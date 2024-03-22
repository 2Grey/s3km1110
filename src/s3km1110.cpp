#ifndef s3km1110_cpp
#define s3km1110_cpp

#include "s3km1110.h"

#define S3KM1110_FRAME_COMMAND_SIZE 2
#define S3KM1110_FRAME_LENGTH_SIZE 2

#define S3KM1110_RESPONSE_COMMAND_READ_FIRMWARE_VERSION 0x00
// 0x01    // Write register
// 0x02    // Read register
#define S3KM1110_RESPONSE_COMMAND_RADAR_SET_CONFIG 0x07
#define S3KM1110_RESPONSE_COMMAND_RADAR_READ_CONFIG 0x08
#define S3KM1110_RESPONSE_COMMAND_READ_SERIAL_NUMBER 0x11
#define S3KM1110_RESPONSE_COMMAND_SET_MODE 0x12
#define S3KM1110_RESPONSE_COMMAND_READ_MODE 0x13
// 0x24    // Enter factory test mode
// 0x25    // Exit factory test mode
// 0x26    // Send factory test results
#define S3KM1110_RESPONSE_COMMAND_OPEN_COMMAND_MODE 0xFF
#define S3KM1110_RESPONSE_COMMAND_CLOSE_COMMAND_MODE 0xFE

#define S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MIN 0x00
#define S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MAX 0x01
#define S3KM1110_RADAR_CONFIG_TARGET_ACTIVE_FRAMES 0x02
#define S3KM1110_RADAR_CONFIG_TARGET_INACTIVE_FRAMES 0x03
#define S3KM1110_RADAR_CONFIG_DISAPPEARANCE_DELAY 0x04

#define S3KM1110_RADAR_MODE_DEBUG 0x00
#define S3KM1110_RADAR_MODE_REPORT 0x04
#define S3KM1110_RADAR_MODE_RUNNING 0x064

s3km1110::s3km1110() {
    if (radarConfiguration == nullptr) {
        radarConfiguration = new s3km1110ConfigParameters();
    }
}
s3km1110::~s3km1110(){
    _uartRadar = nullptr;
    _uartDebug = nullptr;
    firmwareVersion = nullptr;
    serialNumber = nullptr;
    radarConfiguration = nullptr;
}

#pragma mark - Public

bool s3km1110::begin(Stream &dataStream, Stream &debugStream)
{
    // UARTs
    _uartRadar = &dataStream;
    _uartDebug = &debugStream;
    #if defined(ESP8266)
    if (&debugStream == &Serial) {
        _uartDebug->write(17); //Send an XON to stop the hung terminal after reset on ESP8266
    }
    #endif

    return _enableReportMode();
}

bool s3km1110::isConnected()
{
    if (millis() - _radarUartLastPacketTime < kRadarUartcommandTimeout) { return true; }
    if (_read_frame()) { return true; }
    return false;
}

bool s3km1110::read()
{
    return _read_frame();
}

#pragma mark - Send command

bool s3km1110::_enableReportMode() 
{
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_SET_MODE, 0, 2, S3KM1110_RADAR_MODE_REPORT, 4);
}

bool s3km1110::readFirmwareVersion()
{
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_READ_FIRMWARE_VERSION, 0, 0);
}

bool s3km1110::readSerialNumber()
{
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_READ_SERIAL_NUMBER, 0, 0);
}

#pragma mark * Radar Configuration Set

bool s3km1110::setRadarConfigurationMinimumGates(uint8_t gates)
{
    uint8_t newValue = max((uint8_t)0, min((uint8_t)15, gates));
    bool isSuccess = _setParameterConfiguration(S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MIN, newValue);
    if (isSuccess) { radarConfiguration->detectionGatesMin = (uint8_t*)(void*)(uintptr_t)newValue; }
    return isSuccess;
}

bool s3km1110::setRadarConfigurationMaximumGates(uint8_t gates)
{
    uint8_t newValue = max((uint8_t)0, min((uint8_t)15, gates));
    bool isSuccess = _setParameterConfiguration(S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MAX, newValue);
    if (isSuccess) { radarConfiguration->detectionGatesMax = (uint8_t*)(void*)(uintptr_t)newValue; }
    return isSuccess;
}

bool s3km1110::setRadarConfigurationActiveFrameNum(uint8_t num)
{
    bool isSuccess = _setParameterConfiguration(S3KM1110_RADAR_CONFIG_TARGET_ACTIVE_FRAMES, num);
    if (isSuccess) { radarConfiguration->activeFrameNum = (uint8_t*)(void*)(uintptr_t)num; }
    return isSuccess;
}

bool s3km1110::setRadarConfigurationInactiveFrameNum(uint8_t num)
{
    bool isSuccess = _setParameterConfiguration(S3KM1110_RADAR_CONFIG_TARGET_INACTIVE_FRAMES, num);
    if (isSuccess) { radarConfiguration->inactiveFrameNum = (uint8_t*)(void*)(uintptr_t)num; }
    return isSuccess;
}

bool s3km1110::setRadarConfigurationDelay(uint16_t delay)
{
    bool isSuccess = _setParameterConfiguration(S3KM1110_RADAR_CONFIG_DISAPPEARANCE_DELAY, delay);
    if (isSuccess) { radarConfiguration->delay = (uint16_t*)(void*)(uintptr_t)delay; }
    return isSuccess;
}

#pragma mark * Radar Configuration Read

bool s3km1110::readRadarConfigMinimumGates()
{
    _lastRadarConfigCommand = S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MIN;
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_RADAR_READ_CONFIG, S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MIN, 2);
}

bool s3km1110::readRadarConfigMaximumGates()
{
    _lastRadarConfigCommand = S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MAX;
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_RADAR_READ_CONFIG, S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MAX, 2);
}

bool s3km1110::readRadarConfigActiveFrameNumber()
{
    _lastRadarConfigCommand = S3KM1110_RADAR_CONFIG_TARGET_ACTIVE_FRAMES;
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_RADAR_READ_CONFIG, S3KM1110_RADAR_CONFIG_TARGET_ACTIVE_FRAMES, 2);
}

bool s3km1110::readRadarConfigInactiveFrameNumber()
{
    _lastRadarConfigCommand = S3KM1110_RADAR_CONFIG_TARGET_INACTIVE_FRAMES;
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_RADAR_READ_CONFIG, S3KM1110_RADAR_CONFIG_TARGET_INACTIVE_FRAMES, 2);
}

bool s3km1110::readRadarConfigDelay()
{
    _lastRadarConfigCommand = S3KM1110_RADAR_CONFIG_DISAPPEARANCE_DELAY;
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_RADAR_READ_CONFIG, S3KM1110_RADAR_CONFIG_DISAPPEARANCE_DELAY, 2);
}

bool s3km1110::readAllRadarConfigs()
{
    return
        readRadarConfigMinimumGates() &&
        readRadarConfigMaximumGates() &&
        readRadarConfigActiveFrameNumber() &&
        readRadarConfigInactiveFrameNumber() &&
        readRadarConfigDelay();
}

#pragma mark - Private

bool s3km1110::_read_frame()
{
    if (!_uartRadar->available()) { return false; }

    bool isSuccess = false;
    uint8_t _readData = _uartRadar->read();

    if (_isFrameStarted == false) {
        if (_readData == 0xF4) {
            _radarDataFrame[_radarDataFramePosition++] = _readData;
            _isFrameStarted = true;
            _isCommandFrame = false;
        } else if (_readData == 0xFD) {
            _radarDataFrame[_radarDataFramePosition++] = _readData;
            _isFrameStarted = true;
            _isCommandFrame = true;
        }
    } else {
        if (_radarDataFramePosition >= S3KM1110_MAX_FRAME_LENGTH) {
            #if defined(S3KM1110_DEBUG_COMMANDS) || defined(S3KM1110_DEBUG_DATA)
            if (_uartDebug != nullptr) {
                _uartDebug->println(F("[Error] Frame out of size"));
            }
            #endif
            _radarDataFramePosition = 0;
            _isFrameStarted = false;
        } else {
            _radarDataFrame[_radarDataFramePosition++] = _readData;

            if (_radarDataFramePosition >= 8) {
                if (_isDataFrameComplete()) {
                    isSuccess = _parseDataFrame();
                    _isFrameStarted = false;
                    _radarDataFramePosition = 0;
                } else if (_isCommandFrameComplete()) {
                    isSuccess = _parseCommandFrame();
                    _isFrameStarted = false;
                    _radarDataFramePosition = 0;
                }
            }
        }
    }

    return isSuccess;
}

void s3km1110::_printCurrentFrame()
{
    #if defined(S3KM1110_DEBUG_COMMANDS) || defined(S3KM1110_DEBUG_DATA)
    if (_uartDebug == nullptr) { return; }
    for (uint8_t idx = 0; idx < _radarDataFramePosition; idx++) {
        if (_radarDataFrame[idx] < 0x10) { _uartDebug->print('0'); }
        _uartDebug->print(_radarDataFrame[idx], HEX);
        _uartDebug->print(' ');
    }
    _uartDebug->println(' ');
    #endif
}

bool s3km1110::_isDataFrameComplete()
{
    return 
        _radarDataFrame[0]                              == 0xF4 &&
        _radarDataFrame[1]                              == 0xF3 &&
        _radarDataFrame[2]                              == 0xF2 &&
        _radarDataFrame[3]                              == 0xF1 &&
        _radarDataFrame[_radarDataFramePosition - 4] == 0xF8 &&
        _radarDataFrame[_radarDataFramePosition - 3] == 0xF7 &&
        _radarDataFrame[_radarDataFramePosition - 2] == 0xF6 &&
        _radarDataFrame[_radarDataFramePosition - 1] == 0xF5;
}

bool s3km1110::_isCommandFrameComplete()
{
    return
        _radarDataFrame[0]                              == 0xFD &&
		_radarDataFrame[1]                              == 0xFC &&
		_radarDataFrame[2]                              == 0xFB &&
		_radarDataFrame[3]                              == 0xFA &&
        _radarDataFrame[_radarDataFramePosition - 4] == 0x04 &&
        _radarDataFrame[_radarDataFramePosition - 3] == 0x03 &&
        _radarDataFrame[_radarDataFramePosition - 2] == 0x02 &&
        _radarDataFrame[_radarDataFramePosition - 1] == 0x01;
}

bool s3km1110::_parseDataFrame()
{
    uint8_t frame_data_length = _radarDataFrame[4] + (_radarDataFrame[5] << 8);

    #ifdef S3KM1110_DEBUG_DATA
    if (_uartDebug != nullptr) {
        _uartDebug->println(F("––––––––––––––––––––––––––––––––––––––––"));
        _uartDebug->print(F("RCV DTA: "));
        _printCurrentFrame();
    }
    #endif

    if (frame_data_length == 35) {
        uint8_t detectionResultRaw = _radarDataFrame[6];
        distanceToTarget = _radarDataFrame[7] + (_radarDataFrame[8] << 8);
        isTargetDetected = detectionResultRaw == 0x01;

        #ifdef S3KM1110_DEBUG_DATA
        if (_uartDebug != nullptr) {
            _uartDebug->printf("Detected: %x | Distance: %u\n", detectionResultRaw, distanceToTarget);
            _uartDebug->print(F("Gate energy:\n"));
            for (uint8_t i = 0; i < S3KM1110_DISTANE_GATE_COUNT; i++) {
                _uartDebug->printf("%02u\t", i);
            }
            _uartDebug->print('\n');
        }
        #endif

        uint8_t distanceGateStart = 9;
        for (uint8_t idx = 0; idx < S3KM1110_DISTANE_GATE_COUNT; idx++) {
            uint16_t energy = _radarDataFrame[distanceGateStart + idx] + (_radarDataFrame[distanceGateStart + idx + 1] << 8);
            distanceGateEnergy[idx] = energy;

            #ifdef S3KM1110_DEBUG_DATA
            if (_uartDebug != nullptr) {
                _uartDebug->printf("%02u\t", distanceGateEnergy[idx]);
            }
            #endif
        }
        #ifdef S3KM1110_DEBUG_DATA
        if (_uartDebug != nullptr) {
            _uartDebug->print('\n');
        }
        #endif

        return true;
    } else {
        #ifdef S3KM1110_DEBUG_DATA
        if (_uartDebug != nullptr) {
            _uartDebug->print(F("\nFrame length unexpected: "));
            _uartDebug->print(_radarDataFramePosition);
            _uartDebug->print('\n');
        }
        #endif
    }

    #ifdef S3KM1110_DEBUG_DATA
    if (_uartDebug != nullptr) {
        _uartDebug->println(F("––––––––––––––––––––––––––––––––––––––––"));
    }
    #endif
    return false;
}

bool s3km1110::_parseCommandFrame()
{
    uint8_t frame_data_length = _radarDataFrame[4] + (_radarDataFrame[5] << 8);

    _lastCommand = _radarDataFrame[6];
    _isLatestCommandSuccess = (_radarDataFrame[8] == 0x00 && _radarDataFrame[9] == 0x00);

    bool isWithPayloadSize = false;
    if (_lastCommand == S3KM1110_RESPONSE_COMMAND_READ_SERIAL_NUMBER || _lastCommand == S3KM1110_RESPONSE_COMMAND_READ_FIRMWARE_VERSION) {
        isWithPayloadSize = true;
    }

    uint8_t startPayloadPosition = isWithPayloadSize ? 12 : 10;
    int16_t frame_payload_length = _radarDataFramePosition - 4 - startPayloadPosition;

    char payloadBytes[frame_payload_length + 1] = {0};
    if (frame_payload_length > 0) {
        for (uint8_t idx = 0; idx < frame_payload_length; idx++) {
            payloadBytes[idx] = _radarDataFrame[startPayloadPosition + idx];
        }
    }

    #ifdef S3KM1110_DEBUG_COMMANDS
    if (_uartDebug != nullptr) {
        _uartDebug->println(F("–––––––––––––––––––––––––––––––––––––––––––––"));
        _uartDebug->print(F("RCV ACK: "));
        _printCurrentFrame();
        _uartDebug->printf("CMD: 0x%02x | Status: %s | Body: %u | Payload: %u\n", _lastCommand, _isLatestCommandSuccess ? "Ok" : "Failed", frame_data_length, frame_payload_length);
        if (frame_payload_length > 0) {
            _uartDebug->print(F("Raw payload: "));
            for (uint8_t idx = 0; idx < frame_payload_length; idx++) {
                _uartDebug->printf("%02x ", payloadBytes[idx]);
            }
            _uartDebug->print('\n');
        }
    }
    #endif

    bool isSuccess = false;
    if (_lastCommand == S3KM1110_RESPONSE_COMMAND_OPEN_COMMAND_MODE) {
        return _isLatestCommandSuccess;
    }
    else if (_lastCommand == S3KM1110_RESPONSE_COMMAND_CLOSE_COMMAND_MODE) {
        return _isLatestCommandSuccess;
    }
    else if (_lastCommand == S3KM1110_RESPONSE_COMMAND_SET_MODE)
    {
        isSuccess = _isLatestCommandSuccess;
    } 
    else if (_lastCommand == S3KM1110_RESPONSE_COMMAND_READ_SERIAL_NUMBER)
    {
        if (frame_payload_length > 0) {
            serialNumber = new String(payloadBytes);
            isSuccess = true;
        }
    } 
    else if (_lastCommand == S3KM1110_RESPONSE_COMMAND_READ_FIRMWARE_VERSION)
    {
        if (frame_payload_length > 0) {
            firmwareVersion = new String(payloadBytes);
            isSuccess = true;
        }
    }
    else if (_lastCommand == S3KM1110_RESPONSE_COMMAND_RADAR_SET_CONFIG) 
    {
        return _isLatestCommandSuccess;
    }
    else if (_lastCommand == S3KM1110_RESPONSE_COMMAND_RADAR_READ_CONFIG)
    {
        return _parseGetConfigCommandFrame(payloadBytes, frame_payload_length);
    }
    else 
    {
        #ifdef S3KM1110_DEBUG_COMMANDS
        if (_uartDebug != nullptr) {
            _uartDebug->print("[ERROR] Receive Unknown Command\n");
        }
        #endif
    }

    #ifdef S3KM1110_DEBUG_COMMANDS
    if (_uartDebug != nullptr) {
        _uartDebug->println(F("–––––––––––––––––––––––––––––––––––––––––––––"));
    }
    #endif
    return isSuccess;
}

#pragma mark * Parse command helpers

bool s3km1110::_parseGetConfigCommandFrame(char *payload, uint8_t count)
{
    if (count != 4) { return false; }
    uint32_t result = payload[0] + (payload[1] << 8) + (payload[2] << 16) + (payload[3] << 24);

    if (_lastRadarConfigCommand == S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MIN) {
        
        radarConfiguration->detectionGatesMin = (uint8_t*)(void*)(uintptr_t)result;
    }
    else if (_lastRadarConfigCommand == S3KM1110_RADAR_CONFIG_DETECTION_DISTANE_MAX) {
        radarConfiguration->detectionGatesMax = (uint8_t*)(void*)(uintptr_t)result;
    }
    else if (_lastRadarConfigCommand == S3KM1110_RADAR_CONFIG_TARGET_ACTIVE_FRAMES) {
        radarConfiguration->activeFrameNum = (uint8_t*)(void*)(uintptr_t)result;
    }
    else if (_lastRadarConfigCommand == S3KM1110_RADAR_CONFIG_TARGET_INACTIVE_FRAMES) {
        radarConfiguration->inactiveFrameNum = (uint8_t*)(void*)(uintptr_t)result;
    }
    else if (_lastRadarConfigCommand == S3KM1110_RADAR_CONFIG_DISAPPEARANCE_DELAY) {
        radarConfiguration->delay = (uint16_t*)(void*)(uintptr_t)result;
    } else {
        return false;
    }

    return true;
}

bool s3km1110::_setParameterConfiguration(uint16_t parameterCode, int value)
{
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_RADAR_SET_CONFIG, parameterCode, 2, value, 4);
}

#pragma mark - Command mode

bool s3km1110::_openCommandMode()
{
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_OPEN_COMMAND_MODE, 1, 2, true);
}

bool s3km1110::_closeCommandMode()
{
    return _sendCommandAndWait(S3KM1110_RESPONSE_COMMAND_CLOSE_COMMAND_MODE, 0, 0, true);
}

#pragma mark * Helpers

void s3km1110::_sendHexData(String rawData)
{
    #ifdef S3KM1110_DEBUG_COMMANDS
    if (_uartDebug != nullptr) {
        _uartDebug->print(F("SND: "));
        _uartDebug->println(rawData);
    }
    #endif

    unsigned int count = rawData.length();
    byte bytes[count / 2];
    for (byte idx = 0; idx < count; idx += 2) {
        bytes[idx / 2] = strtoul(rawData.substring(idx, idx + 2).c_str(), NULL, HEX);
    }
    _uartRadar->write(bytes, sizeof(bytes));
}

bool s3km1110::_sendCommandAndWait(uint16_t command, uint32_t payload, uint8_t payloadSize, bool isSkipCommandMode)
{
    return _sendCommandAndWait(command, 0, 0, payload, payloadSize, isSkipCommandMode);
}

bool s3km1110::_sendCommandAndWait(
    uint16_t command, 
    uint32_t subCommand, 
    uint8_t subCommandSize, 
    uint32_t payload, 
    uint8_t payloadSize,
    bool isSkipCommandMode)
{
    String commandStr = _intToHex(command, S3KM1110_FRAME_COMMAND_SIZE);
    String subCommandStr = subCommandSize > 0 ? _intToHex(subCommand, subCommandSize) : "";
    String payloadStr = payloadSize > 0 ? _intToHex(payload, payloadSize) : "";
    String totalSizeStr = _intToHex(S3KM1110_FRAME_COMMAND_SIZE + subCommandSize + payloadSize, S3KM1110_FRAME_LENGTH_SIZE);
    
    if (isSkipCommandMode || _openCommandMode()) {
        delay(50);
        _sendHexData(_getCommandPrefix() + totalSizeStr + commandStr + subCommandStr + payloadStr + _getCommandPostfix());

        _radarUartLastCommandTime = millis();
        while (millis() - _radarUartLastCommandTime < kRadarUartcommandTimeout) {
            if (_read_frame()) {
                if (_isLatestCommandSuccess && _lastCommand == command) {
                    delay(50);
                    if (!isSkipCommandMode) { _closeCommandMode(); }
                    return true;
                }
            }
        }
    }

    delay(50);
    if (!isSkipCommandMode) { _closeCommandMode(); }
    return false;
}

String s3km1110::_intToHex(int value, uint8_t byteCount)
{
    uint8_t width = byteCount * 2;
    char result[width];
    uint32_t littleIndian = __htonl(value);
    snprintf(result, width + 1, "%08x", littleIndian);
    return String(result);
}

#endif //s3km1110_cpp