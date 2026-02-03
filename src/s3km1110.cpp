#ifndef s3km1110_cpp
#define s3km1110_cpp

#include "s3km1110.h"

s3km1110::s3km1110() {};
s3km1110::~s3km1110() = default;

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

    if (!_uartRadar) {
        return false;
    }

    #if !defined(S3KM1110_SKIP_READ_CONFIG_ON_BEGIN)
    if (_enableReportMode()) {
        readAllRadarConfigs();
        return true;
    }
    #else
    return _enableReportMode();
    #endif // S3KM1110_SKIP_READ_CONFIG_ON_BEGIN

    return false;
}

bool s3km1110::isActive()
{
    return (millis() - _radarUartLastPacketTime < kRadarUartcommandTimeout);
}

bool s3km1110::read()
{
    return _read_frame();
}

#pragma mark - Send command

bool s3km1110::_enableReportMode() 
{
    return _sendCommandAndWait(static_cast<uint16_t>(RadarCommand::SetMode), 0, 2, static_cast<uint32_t>(RadarMode::Report), 4);
}

bool s3km1110::readFirmwareVersion()
{
    return _sendCommandAndWait(RadarCommand::ReadFirmwareVersion, 0, 0);
}

bool s3km1110::readSerialNumber()
{
    return _sendCommandAndWait(RadarCommand::ReadSerialNumber, 0, 0);
}

#pragma mark * Radar Configuration Set

bool s3km1110::setRadarConfigurationMinimumGates(uint8_t gates)
{
    uint8_t newValue = max((uint8_t)0, min((uint8_t)15, gates));
    bool isSuccess = _setParameterConfiguration(static_cast<uint16_t>(ConfigParam::MinDistance), newValue);
    if (isSuccess) { radarConfiguration.detectionGatesMin = newValue; }
    return isSuccess;
}

bool s3km1110::setRadarConfigurationMaximumGates(uint8_t gates)
{
    uint8_t newValue = max((uint8_t)0, min((uint8_t)15, gates));
    bool isSuccess = _setParameterConfiguration(static_cast<uint16_t>(ConfigParam::MaxDistance), newValue);
    if (isSuccess) { radarConfiguration.detectionGatesMax = newValue; }
    return isSuccess;
}

bool s3km1110::setRadarConfigurationTargetDisappearanceDelay(uint16_t delay)
{
    bool isSuccess = _setParameterConfiguration(static_cast<uint16_t>(ConfigParam::DisappearanceDelay), delay);
    if (isSuccess) { radarConfiguration.targetDisappearanceDelay = delay; }
    return isSuccess;
}

#pragma mark * Radar Configuration Read

bool s3km1110::readRadarConfigMinimumGates()
{
    _lastRadarConfigCommand = ConfigParam::MinDistance;
    return _sendCommandAndWait(RadarCommand::ReadConfig, static_cast<uint32_t>(ConfigParam::MinDistance), 2);
}

bool s3km1110::readRadarConfigMaximumGates()
{
    _lastRadarConfigCommand = ConfigParam::MaxDistance;
    return _sendCommandAndWait(RadarCommand::ReadConfig, static_cast<uint32_t>(ConfigParam::MaxDistance), 2);
}

bool s3km1110::readRadarConfigTargetDisappearanceDelay()
{
    _lastRadarConfigCommand = ConfigParam::DisappearanceDelay;
    return _sendCommandAndWait(RadarCommand::ReadConfig, static_cast<uint32_t>(ConfigParam::DisappearanceDelay), 2);
}

bool s3km1110::readAllRadarConfigs()
{
    return
        readRadarConfigMinimumGates() &&
        readRadarConfigMaximumGates() &&
        readRadarConfigTargetDisappearanceDelay();
}

#pragma mark - Private

bool s3km1110::_read_frame()
{
    while (_uartRadar->available()) {
        uint8_t _readData = _uartRadar->read();
        _radarUartLastPacketTime = millis();
        
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
            if (_radarDataFramePosition >= kMaxFrameLength) {
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
                        bool result = _parseDataFrame();
                        _isFrameStarted = false;
                        _radarDataFramePosition = 0;
                        if (result) return true;
                    } else if (_isCommandFrameComplete()) {
                        bool result = _parseCommandFrame();
                        _isFrameStarted = false;
                        _radarDataFramePosition = 0;
                        if (result) return true;
                    }
                }
            }
        }
    }

    return false;
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
            for (uint8_t i = 0; i < kDistanceGateCount; i++) {
                _uartDebug->printf("%02u\t", i);
            }
            _uartDebug->print('\n');
        }
        #endif

        uint8_t distanceGateStart = 9;
        for (uint8_t idx = 0; idx < kDistanceGateCount; idx++) {
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
    if (_lastCommand == static_cast<uint8_t>(RadarCommand::ReadSerialNumber) || _lastCommand == static_cast<uint8_t>(RadarCommand::ReadFirmwareVersion)) {
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
    if (_lastCommand == static_cast<uint8_t>(RadarCommand::OpenCommandMode)) {
        return _isLatestCommandSuccess;
    }
    else if (_lastCommand == static_cast<uint8_t>(RadarCommand::CloseCommandMode)) {
        return _isLatestCommandSuccess;
    }
    else if (_lastCommand == static_cast<uint8_t>(RadarCommand::SetMode))
    {
        isSuccess = _isLatestCommandSuccess;
    } 
    else if (_lastCommand == static_cast<uint8_t>(RadarCommand::ReadSerialNumber))
    {
        if (frame_payload_length > 0) {
            serialNumber = String(payloadBytes);
            isSuccess = true;
        }
    } 
    else if (_lastCommand == static_cast<uint8_t>(RadarCommand::ReadFirmwareVersion))
    {
        if (frame_payload_length > 0) {
            firmwareVersion = String(payloadBytes);
            isSuccess = true;
        }
    }
    else if (_lastCommand == static_cast<uint8_t>(RadarCommand::SetConfig)) 
    {
        return _isLatestCommandSuccess;
    }
    else if (_lastCommand == static_cast<uint8_t>(RadarCommand::ReadConfig))
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

    if (_lastRadarConfigCommand == ConfigParam::MinDistance) {
        radarConfiguration.detectionGatesMin = result;
    }
    else if (_lastRadarConfigCommand == ConfigParam::MaxDistance) {
        radarConfiguration.detectionGatesMax = result;
    }
    else if (_lastRadarConfigCommand == ConfigParam::DisappearanceDelay) {
        radarConfiguration.targetDisappearanceDelay = result;
    } else {
        return false;
    }

    return true;
}

bool s3km1110::_setParameterConfiguration(uint16_t parameterCode, int value)
{
    return _sendCommandAndWait(static_cast<uint8_t>(RadarCommand::SetConfig), parameterCode, 2, value, 4);
}

#pragma mark - Command mode

bool s3km1110::_openCommandMode()
{
    return _sendCommandAndWait(RadarCommand::OpenCommandMode, 1, 2, true);
}

bool s3km1110::_closeCommandMode()
{
    return _sendCommandAndWait(RadarCommand::CloseCommandMode, 0, 0, true);
}

#pragma mark * Helpers

void s3km1110::_writeLittleEndian(uint8_t *buffer, uint8_t &index, uint32_t value, uint8_t byteCount)
{
    for (uint8_t i = 0; i < byteCount; i++) {
        buffer[index++] = (value >> (i * 8)) & 0xFF;
    }
}

bool s3km1110::_sendCommandAndWait(RadarCommand command, uint32_t payload, uint8_t payloadSize, bool isSkipCommandMode)
{
    return _sendCommandAndWait(static_cast<uint16_t>(command), 0, 0, payload, payloadSize, isSkipCommandMode);
}

bool s3km1110::_sendCommandAndWait(
    uint16_t command, 
    uint32_t subCommand, 
    uint8_t subCommandSize, 
    uint32_t payload, 
    uint8_t payloadSize,
    bool isSkipCommandMode)
{

    uint16_t dataLen = kFrameCommandSize + subCommandSize + payloadSize;
    
    uint8_t idx = 0;
    uint8_t buffer[64]; 
    buffer[idx++] = 0xFD; buffer[idx++] = 0xFC; buffer[idx++] = 0xFB; buffer[idx++] = 0xFA;

    _writeLittleEndian(buffer, idx, dataLen, kFrameLengthSize);
    _writeLittleEndian(buffer, idx, command, kFrameCommandSize);

    if (subCommandSize > 0) {
        _writeLittleEndian(buffer, idx, subCommand, subCommandSize);
    }

    if (payloadSize > 0) {
        _writeLittleEndian(buffer, idx, payload, payloadSize);
    }

    buffer[idx++] = 0x04; buffer[idx++] = 0x03; buffer[idx++] = 0x02; buffer[idx++] = 0x01;
    
    
    if (isSkipCommandMode || _openCommandMode()) {
        
        #ifdef S3KM1110_DEBUG_COMMANDS
        if (_uartDebug != nullptr) {
            _uartDebug->print(F("SND HEX: "));
            for(uint8_t i=0; i<idx; i++) {
                if(buffer[i] < 0x10) _uartDebug->print('0');
                _uartDebug->print(buffer[i], HEX);
            }
            _uartDebug->println();
        }
        #endif

        _uartRadar->write(buffer, idx);

        _radarUartLastCommandTime = millis();
        while (millis() - _radarUartLastCommandTime < kRadarUartcommandTimeout) {
            if (_read_frame()) {
                if (_isLatestCommandSuccess && _lastCommand == (uint8_t)command) { 
                    if (!isSkipCommandMode) { _closeCommandMode(); }
                    return true;
                }
            }
        }
    }

    if (!isSkipCommandMode) { _closeCommandMode(); }
    return false;
}

#endif //s3km1110_cpp