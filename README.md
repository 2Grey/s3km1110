# S3KM1110

## Introduction

An Arduino library for the mmWave Sensor on S3KM1110 chip.\
Is a human micro-motion sensor that can detect and recognize moving, standing, and stationary human body.

![](https://www.waveshare.com/w/A6Y79bcq/Kdy80nYY.php?f=HMMD_mmWave_Sensor.jpg&width=600)

The code in this library is  based heavily off this [LD2410](https://github.com/ncmreynolds/ld2410), [Waveshare](https://www.waveshare.com/wiki/HMMD_mmWave_Sensor) and [RD-03](https://docs.ai-thinker.com/_media/en/rd-03_v1.0.0_serial_communication_protocol.pdf) documentations.

This library allows you to configure and use the sensor over serial connection.\
The Sensor communicates over serial at 115200 baud by default.

The modules also provide an 'active high' output on the 'OUT' pin which can be used to signal presence, based off the settings configured. Once the modules are configured it is not necessary to use their UART, but it provides much more information.

## Connections

The module must be powered by 3V3 (3.0~3.6V Wide-range Voltage)

![](https://www.waveshare.com/w/upload/5/5b/HMMD_mmWave_Sensor02.jpg)

If you have the breakout board, you can use the VCC, GND, TX and RX pins to work with the module.

## Reading distances

The S3KM1110 has a number of 'gates', each of which correspond to a distance of about 0.70m and many of the settings/measurements are calculated from this.

## How to use

```cpp
#include <s3km1110.h>

s3km1110 radar;
uint32_t lastReading = 0;

void setup(void) {
    Serial.begin(115200);
    Serial2.begin(115200);

    bool isRadarEnabled = radar.begin(Serial2, Serial);
    Serial.printf("Radar status: %s\n", isRadarEnabled ? "Ok" : "Failed");

    if (isRadarEnabled && radar.readAllRadarConfigs()) {
        auto config = radar.radarConfiguration;
        Serial.printf("[Info] Radar config:\n |- Gates  | Min: %u\t| Max: %u\n |- Frames | Detect: %u\t| Disappear: %u\n |- Disappearance delay: %u\n",
                    config->detectionGatesMin, config->detectionGatesMax, config->activeFrameNum, config->inactiveFrameNum, config->delay);
    }
}

void loop() {
    if (radar.isConnected()) {
        lastReading = millis();
        while (millis() - lastReading < 2000) {
            if (radar.read()) {
                // Get radar info
                bool isDetected = radar.isTargetDetected;
                uint16_t targetDistance = radar.distanceToTarget;
            }
        }
    }
}
```

## Not implemented features
- Work with registers
- Work with factory test mode
- Read data in Debug mode
- Read data in Running mode
