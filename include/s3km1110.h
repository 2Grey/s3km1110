
#ifndef s3km1110_h
#define s3km1110_h

#include <Arduino.h>

// #define S3KM1110_DEBUG_COMMANDS
// #define S3KM1110_DEBUG_DATA
// #define S3KM1110_SKIP_READ_CONFIG_ON_BEGIN

struct s3km1110ConfigParameters
{
    uint8_t detectionGatesMin = 0;   // Minimum detection distance gate | 0~15 | 
    uint8_t detectionGatesMax = 0;   // Maximum detection distance gate | 0~15
    uint16_t targetDisappearanceDelay = 0;  // Time (seconds) to confirm absence after target loss | 0~65535
};

class s3km1110 {

    public:
        s3km1110();
        ~s3km1110();

        static constexpr size_t kMaxFrameLength = 45;
        static constexpr size_t kDistanceGateCount = 16;

        bool begin(Stream &dataStream, Stream &debugStream);
        bool isActive();    // Is the sensor sending data regularly
        bool read();        // You must call this frequently in your main loop to process incoming frames from the sensor

        bool readFirmwareVersion(); // Request the firmware version, which is then available on the values below.
        bool readSerialNumber();    // Request the serial number, which is then available on the values below.

        bool readAllRadarConfigs();
        bool readRadarConfigMinimumGates();
        bool readRadarConfigMaximumGates();
        bool readRadarConfigTargetDisappearanceDelay();

        bool setRadarConfigurationMinimumGates(uint8_t);
        bool setRadarConfigurationMaximumGates(uint8_t);
        bool setRadarConfigurationTargetDisappearanceDelay(uint16_t);

        String firmwareVersion;
        String serialNumber;

        s3km1110ConfigParameters radarConfiguration;

        bool isTargetDetected = false;
        int16_t distanceToTarget = -1;  // Distance to the target in centimetres.
        uint16_t distanceGateEnergy[kDistanceGateCount] = {0};

    protected:
        static constexpr uint8_t kFrameCommandSize = 2;
        static constexpr uint8_t kFrameLengthSize = 2;

        enum class RadarCommand : uint8_t {
            ReadFirmwareVersion     = 0x00,
            WriteRegister           = 0x01,
            ReadRegister            = 0x02,
            SetConfig               = 0x07,
            ReadConfig              = 0x08,
            AutoThresholdGen        = 0x09,
            ReadSerialNumber        = 0x11,
            SetMode                 = 0x12,
            ReadMode                = 0x13,
            EnterFactoryTestMode    = 0x24,
            ExitFactoryTextMode     = 0x25,
            SendFactoryTextResult   = 0x26,
            OpenCommandMode         = 0xFF,
            CloseCommandMode        = 0xFE
        };

        enum class ConfigParam : uint8_t {
            MinDistance         = 0x00,
            MaxDistance         = 0x01,
            // 0x02,
            // 0x03,
            DisappearanceDelay  = 0x04,
            PowerSupplyAlarm    = 0x05
            // 0x10 ~ 0x1F      // Motion Trigger Threshold | Sensitivity for initial movement detection (Gates 0-15) | 0 to 2^(32-1)
            // 0x20 ~ 0x2F      // Motion Hold Threshold | Sensitivity for maintaining presence state (Gates 0-15) | 0 to 2^(32-1)
            // 0x30 ~ 0x3F      // Micro-motion Threshold | Sensitivity for stationary/breathing detection (Gates 0-15) | 0 to 2^(32-1)
        };

        enum class RadarMode : uint8_t {
            Debug   = 0x00,
            Report  = 0x04,
            Running = 0x64
        };

    private:
        Stream *_uartRadar = nullptr;
        Stream *_uartDebug = nullptr;

        const uint32_t kRadarUartcommandTimeout = 100;
        uint32_t _radarUartLastPacketTime = 0;
        uint32_t _radarUartLastCommandTime = 0;

        uint8_t _radarDataFrame[kMaxFrameLength];
        int8_t _radarDataFramePosition = 0;

        bool _isFrameStarted = false;
        bool _isCommandFrame = false;

        uint8_t _lastCommand = 0;
        ConfigParam _lastRadarConfigCommand;
        bool _isLatestCommandSuccess = false;

        bool _enableReportMode();
        void _printCurrentFrame();

        bool _read_frame();
        bool _isDataFrameComplete();
        bool _isCommandFrameComplete();
		bool _parseDataFrame();
		bool _parseCommandFrame();
        bool _parseGetConfigCommandFrame(char*, uint8_t);

        bool _sendCommandAndWait(RadarCommand command, uint32_t payload, uint8_t payloadSize, bool isSkipCommandMode = false);
        bool _sendCommandAndWait(uint16_t, uint32_t, uint8_t, uint32_t, uint8_t, bool isSkipCommandMode = false);
        bool _setParameterConfiguration(uint16_t parameter, int value);

        bool _openCommandMode();
        bool _closeCommandMode();

        void _writeLittleEndian(uint8_t *buffer, uint8_t &index, uint32_t value, uint8_t byteCount);
};

#endif // s3km1110_h