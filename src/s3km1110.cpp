#include "s3km1110.h"

namespace {
constexpr uint8_t kDataHeader[] = {0xF4, 0xF3, 0xF2, 0xF1};
constexpr uint8_t kDataFooter[] = {0xF8, 0xF7, 0xF6, 0xF5};
constexpr uint8_t kCommandHeader[] = {0xFD, 0xFC, 0xFB, 0xFA};
constexpr uint8_t kCommandFooter[] = {0x04, 0x03, 0x02, 0x01};
constexpr uint8_t kMarkerSize = sizeof(kDataHeader);
constexpr uint8_t kLengthOffset = kMarkerSize;
constexpr uint8_t kBodyOffset = kLengthOffset + 2;
constexpr uint8_t kCommandResponseSize = 4; // Command (2 bytes) and status (2 bytes).
constexpr uint8_t kStringLengthSize = 2;
constexpr uint8_t kReportBodySize = 3 + s3km1110::kDistanceGateCount * 2;
constexpr uint8_t kMaxGate = s3km1110::kDistanceGateCount - 1;

uint8_t clampGate(uint32_t value)
{
    return static_cast<uint8_t>(value > kMaxGate ? kMaxGate : value);
}

String readPayloadString(const uint8_t *payload, uint8_t length)
{
    String result;
    result.reserve(length);
    for (uint8_t i = 0; i < length && payload[i] != 0; ++i) {
        result += static_cast<char>(payload[i]);
    }
    return result;
}
} // namespace

s3km1110::s3km1110() = default;
s3km1110::~s3km1110() = default;

bool s3km1110::begin(Stream &dataStream)
{
    return _begin(dataStream, nullptr);
}

bool s3km1110::begin(Stream &dataStream, Stream &debugStream)
{
    return _begin(dataStream, &debugStream);
}

bool s3km1110::_begin(Stream &dataStream, Stream *debugStream)
{
    _uartRadar = &dataStream;
    _uartDebug = debugStream;
    _resetFrame();
    _hasReceivedPacket = false;
    _isLatestCommandSuccess = false;

#if defined(ESP8266)
    if (_uartDebug == &Serial) {
        _uartDebug->write(17); // Send XON to unfreeze the terminal after reset.
    }
#endif

    if (!_enableReportMode()) {
        return false;
    }
#if !defined(S3KM1110_SKIP_READ_CONFIG_ON_BEGIN)
    return readAllRadarConfigs();
#else
    return true;
#endif
}

bool s3km1110::isActive()
{
    return _hasReceivedPacket && millis() - _radarUartLastPacketTime < kCommandTimeoutMs;
}

bool s3km1110::read()
{
    return _readFrame();
}

bool s3km1110::_enableReportMode()
{
    return _sendCommandAndWait(RadarCommand::SetMode, 0, 2, static_cast<uint32_t>(RadarMode::Report), 4);
}

bool s3km1110::readFirmwareVersion()
{
    return _sendCommandAndWait(RadarCommand::ReadFirmwareVersion, 0, 0);
}

bool s3km1110::readSerialNumber()
{
    return _sendCommandAndWait(RadarCommand::ReadSerialNumber, 0, 0);
}

bool s3km1110::setRadarConfigurationMinimumGates(uint8_t gates)
{
    const uint8_t value = clampGate(gates);
    if (!_setParameterConfiguration(ConfigParam::MinDistance, value)) {
        return false;
    }
    radarConfiguration.detectionGatesMin = value;
    return true;
}

bool s3km1110::setRadarConfigurationMaximumGates(uint8_t gates)
{
    const uint8_t value = clampGate(gates);
    if (!_setParameterConfiguration(ConfigParam::MaxDistance, value)) {
        return false;
    }
    radarConfiguration.detectionGatesMax = value;
    return true;
}

bool s3km1110::setRadarConfigurationTargetDisappearanceDelay(uint16_t delay)
{
    if (!_setParameterConfiguration(ConfigParam::DisappearanceDelay, delay)) {
        return false;
    }
    radarConfiguration.targetDisappearanceDelay = delay;
    return true;
}

bool s3km1110::readRadarConfigMinimumGates()
{
    return _readParameterConfiguration(ConfigParam::MinDistance);
}

bool s3km1110::readRadarConfigMaximumGates()
{
    return _readParameterConfiguration(ConfigParam::MaxDistance);
}

bool s3km1110::readRadarConfigTargetDisappearanceDelay()
{
    return _readParameterConfiguration(ConfigParam::DisappearanceDelay);
}

bool s3km1110::readAllRadarConfigs()
{
    return readRadarConfigMinimumGates() &&
           readRadarConfigMaximumGates() &&
           readRadarConfigTargetDisappearanceDelay();
}

bool s3km1110::_readParameterConfiguration(ConfigParam parameter)
{
    _lastRadarConfigCommand = parameter;
    return _sendCommandAndWait(RadarCommand::ReadConfig, static_cast<uint32_t>(parameter), 2);
}

bool s3km1110::_readFrame()
{
    if (_uartRadar == nullptr) {
        return false;
    }

    while (_uartRadar->available()) {
        const int nextByte = _uartRadar->read();
        if (nextByte < 0) {
            break;
        }
        const uint8_t data = static_cast<uint8_t>(nextByte);
        if (_radarDataFramePosition == 0) {
            _tryStartFrame(data);
            continue;
        }
        if (_radarDataFramePosition >= kMaxFrameLength) {
            _resetFrame();
            _tryStartFrame(data);
            continue;
        }

        _radarDataFrame[_radarDataFramePosition++] = data;
        const FrameValidation validation = _getCurrentFrameValidation();
        if (validation == FrameValidation::Invalid) {
            _resetFrame();
            _tryStartFrame(data);
        } else if (validation == FrameValidation::Complete) {
            const bool result = _isCommandFrame ? _parseCommandFrame() : _parseDataFrame();
            _resetFrame();
            if (result) {
                _radarUartLastPacketTime = millis();
                _hasReceivedPacket = true;
                return true;
            }
        }
    }
    return false;
}

bool s3km1110::_tryStartFrame(uint8_t data)
{
    if (data != kDataHeader[0] && data != kCommandHeader[0]) {
        return false;
    }
    _radarDataFrame[0] = data;
    _radarDataFramePosition = 1;
    _isCommandFrame = data == kCommandHeader[0];
    return true;
}

void s3km1110::_resetFrame()
{
    _radarDataFramePosition = 0;
    _isCommandFrame = false;
}

void s3km1110::_printCurrentFrame()
{
#if defined(S3KM1110_DEBUG_COMMANDS) || defined(S3KM1110_DEBUG_DATA)
    if (_uartDebug == nullptr) {
        return;
    }
    for (uint8_t i = 0; i < _radarDataFramePosition; ++i) {
        if (_radarDataFrame[i] < 0x10) {
            _uartDebug->print('0');
        }
        _uartDebug->print(_radarDataFrame[i], HEX);
        _uartDebug->print(' ');
    }
    _uartDebug->println();
#endif
}

uint16_t s3km1110::_readLittleEndian16(uint8_t offset) const
{
    return static_cast<uint16_t>(_radarDataFrame[offset]) |
           (static_cast<uint16_t>(_radarDataFrame[offset + 1]) << 8);
}

uint32_t s3km1110::_readLittleEndian32(const uint8_t *buffer) const
{
    return static_cast<uint32_t>(buffer[0]) |
           (static_cast<uint32_t>(buffer[1]) << 8) |
           (static_cast<uint32_t>(buffer[2]) << 16) |
           (static_cast<uint32_t>(buffer[3]) << 24);
}

s3km1110::FrameValidation s3km1110::_getCurrentFrameValidation() const
{
    return _isCommandFrame ? _validateFrame(kCommandHeader, kCommandFooter)
                           : _validateFrame(kDataHeader, kDataFooter);
}

s3km1110::FrameValidation s3km1110::_validateFrame(const uint8_t *header, const uint8_t *footer) const
{
    for (uint8_t i = 0; i < kMarkerSize && i < _radarDataFramePosition; ++i) {
        if (_radarDataFrame[i] != header[i]) {
            return FrameValidation::Invalid;
        }
    }
    if (_radarDataFramePosition < kBodyOffset) {
        return FrameValidation::Incomplete;
    }

    const uint32_t expectedLength = kBodyOffset + static_cast<uint32_t>(_readLittleEndian16(kLengthOffset)) + kMarkerSize;
    if (expectedLength > kMaxFrameLength || _radarDataFramePosition > expectedLength) {
        return FrameValidation::Invalid;
    }
    if (_radarDataFramePosition < expectedLength) {
        return FrameValidation::Incomplete;
    }
    const uint8_t footerOffset = static_cast<uint8_t>(expectedLength - kMarkerSize);
    for (uint8_t i = 0; i < kMarkerSize; ++i) {
        if (_radarDataFrame[footerOffset + i] != footer[i]) {
            return FrameValidation::Invalid;
        }
    }
    return FrameValidation::Complete;
}

bool s3km1110::_parseDataFrame()
{
#ifdef S3KM1110_DEBUG_DATA
    if (_uartDebug != nullptr) {
        _uartDebug->print(F("RCV DTA: "));
        _printCurrentFrame();
    }
#endif
    if (_readLittleEndian16(kLengthOffset) != kReportBodySize) {
        return false;
    }

    isTargetDetected = _radarDataFrame[kBodyOffset] == 0x01;
    distanceToTarget = static_cast<int16_t>(_readLittleEndian16(kBodyOffset + 1));
    for (uint8_t i = 0; i < kDistanceGateCount; ++i) {
        distanceGateEnergy[i] = _readLittleEndian16(kBodyOffset + 3 + i * 2);
    }
#ifdef S3KM1110_DEBUG_DATA
    if (_uartDebug != nullptr) {
        _uartDebug->print(F("Detected: "));
        _uartDebug->print(isTargetDetected);
        _uartDebug->print(F(" | Distance: "));
        _uartDebug->println(distanceToTarget);
        _uartDebug->print(F("Gate energy: "));
        for (uint8_t i = 0; i < kDistanceGateCount; ++i) {
            _uartDebug->print(distanceGateEnergy[i]);
            _uartDebug->print(' ');
        }
        _uartDebug->println();
    }
#endif
    return true;
}

bool s3km1110::_parseCommandFrame()
{
    const uint16_t bodyLength = _readLittleEndian16(kLengthOffset);
    if (bodyLength < kCommandResponseSize) {
        return false;
    }

    _lastCommand = _radarDataFrame[kBodyOffset];
    _isLatestCommandSuccess = false;
    const RadarCommand command = static_cast<RadarCommand>(_lastCommand);
    const bool hasStringLength = command == RadarCommand::ReadSerialNumber || command == RadarCommand::ReadFirmwareVersion;
    const uint8_t responseSize = kCommandResponseSize + (hasStringLength ? kStringLengthSize : 0);
    if (bodyLength < responseSize) {
        return false;
    }
    const uint8_t payloadLength = static_cast<uint8_t>(bodyLength - responseSize);
    if (hasStringLength && _readLittleEndian16(kBodyOffset + kCommandResponseSize) != payloadLength) {
        return false;
    }
    const uint8_t *payload = _radarDataFrame + kBodyOffset + responseSize;
    const bool statusOk = _readLittleEndian16(kBodyOffset + 2) == 0;
#ifdef S3KM1110_DEBUG_COMMANDS
    if (_uartDebug != nullptr) {
        _uartDebug->print(F("RCV ACK: "));
        _printCurrentFrame();
        _uartDebug->print(F("Status: "));
        _uartDebug->println(statusOk ? F("Ok") : F("Failed"));
    }
#endif
    if (!statusOk) {
        return false;
    }

    bool parsed = false;
    switch (command) {
        case RadarCommand::OpenCommandMode:
        case RadarCommand::CloseCommandMode:
        case RadarCommand::SetMode:
        case RadarCommand::SetConfig:
            parsed = true;
            break;
        case RadarCommand::ReadSerialNumber:
            if (payloadLength > 0) {
                serialNumber = readPayloadString(payload, payloadLength);
                parsed = true;
            }
            break;
        case RadarCommand::ReadFirmwareVersion:
            if (payloadLength > 0) {
                firmwareVersion = readPayloadString(payload, payloadLength);
                parsed = true;
            }
            break;
        case RadarCommand::ReadConfig:
            parsed = _parseGetConfigCommandFrame(payload, payloadLength);
            break;
        default:
            break;
    }
    _isLatestCommandSuccess = parsed;
    return parsed;
}

bool s3km1110::_parseGetConfigCommandFrame(const uint8_t *payload, uint8_t count)
{
    if (count != sizeof(uint32_t)) {
        return false;
    }
    const uint32_t value = _readLittleEndian32(payload);
    switch (_lastRadarConfigCommand) {
        case ConfigParam::MinDistance:
            radarConfiguration.detectionGatesMin = clampGate(value);
            return true;
        case ConfigParam::MaxDistance:
            radarConfiguration.detectionGatesMax = clampGate(value);
            return true;
        case ConfigParam::DisappearanceDelay:
            radarConfiguration.targetDisappearanceDelay = static_cast<uint16_t>(value > 65535UL ? 65535UL : value);
            return true;
        default:
            return false;
    }
}

bool s3km1110::_setParameterConfiguration(ConfigParam parameter, uint32_t value)
{
    return _sendCommandAndWait(RadarCommand::SetConfig, static_cast<uint32_t>(parameter), 2, value, 4);
}

bool s3km1110::_openCommandMode()
{
    return _sendCommandAndWait(RadarCommand::OpenCommandMode, 1, 2, true);
}

bool s3km1110::_closeCommandMode()
{
    return _sendCommandAndWait(RadarCommand::CloseCommandMode, 0, 0, true);
}

void s3km1110::_writeLittleEndian(uint8_t *buffer, uint8_t &index, uint32_t value, uint8_t byteCount)
{
    for (uint8_t i = 0; i < byteCount; ++i) {
        buffer[index++] = static_cast<uint8_t>(value >> (i * 8));
    }
}

void s3km1110::_writeCommandFrame(RadarCommand command, uint32_t subCommand, uint8_t subCommandSize,
                                 uint32_t payload, uint8_t payloadSize)
{
    // The largest supported request has a 2-byte command and two 32-bit fields.
    uint8_t buffer[kMarkerSize + kFrameLengthSize + kFrameCommandSize + 2 * sizeof(uint32_t) + kMarkerSize];
    uint8_t index = 0;
    for (uint8_t byte : kCommandHeader) {
        buffer[index++] = byte;
    }
    _writeLittleEndian(buffer, index, kFrameCommandSize + subCommandSize + payloadSize, kFrameLengthSize);
    _writeLittleEndian(buffer, index, static_cast<uint16_t>(command), kFrameCommandSize);
    _writeLittleEndian(buffer, index, subCommand, subCommandSize);
    _writeLittleEndian(buffer, index, payload, payloadSize);
    for (uint8_t byte : kCommandFooter) {
        buffer[index++] = byte;
    }
#ifdef S3KM1110_DEBUG_COMMANDS
    if (_uartDebug != nullptr) {
        _uartDebug->print(F("SND HEX: "));
        for (uint8_t i = 0; i < index; ++i) {
            if (buffer[i] < 0x10) {
                _uartDebug->print('0');
            }
            _uartDebug->print(buffer[i], HEX);
            _uartDebug->print(' ');
        }
        _uartDebug->println();
    }
#endif
    _uartRadar->write(buffer, index);
}

bool s3km1110::_waitForCommandResponse(RadarCommand command)
{
    const uint32_t startTime = millis();
    while (millis() - startTime < kCommandTimeoutMs) {
        if (_readFrame() && _isLatestCommandSuccess && _lastCommand == static_cast<uint8_t>(command)) {
            return true;
        }
    }
    return false;
}

bool s3km1110::_sendCommandAndWait(RadarCommand command, uint32_t payload, uint8_t payloadSize, bool skipCommandMode)
{
    return _sendCommandAndWait(command, 0, 0, payload, payloadSize, skipCommandMode);
}

bool s3km1110::_sendCommandAndWait(RadarCommand command, uint32_t subCommand, uint8_t subCommandSize,
                                 uint32_t payload, uint8_t payloadSize, bool skipCommandMode)
{
    if (_uartRadar == nullptr || subCommandSize > sizeof(uint32_t) || payloadSize > sizeof(uint32_t)) {
        return false;
    }

    bool success = false;
    if (skipCommandMode || _openCommandMode()) {
        _isLatestCommandSuccess = false;
        _writeCommandFrame(command, subCommand, subCommandSize, payload, payloadSize);
        success = _waitForCommandResponse(command);
    }
    if (!skipCommandMode) {
        const bool closed = _closeCommandMode();
        success = success && closed;
    }
    return success;
}
