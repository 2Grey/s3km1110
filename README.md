# S3KM1110

[![PlatformIO Registry](https://badges.registry.platformio.org/packages/2grey/library/s3km1110.svg)](https://registry.platformio.org/libraries/2grey/s3km1110)

> [!NOTE]  
> I would appreciate your feedback via [Issues](https://github.com/2Grey/s3km1110/issues)

> [!CAUTION]
> Versions starting from 1.0.0 contain breaking changes compared to versions 0.5.1  
> Check the update documentation [here](/MIGRATIONS.md)

## Introduction

An Arduino library for the mmWave Sensor on S3KM1110 chip.\
Is a human micro-motion sensor that can detect and recognize moving, standing, and stationary human body.

![](https://www.waveshare.com/w/A6Y79bcq/Kdy80nYY.php?f=HMMD_mmWave_Sensor.jpg&width=600)

The code in this library is  based heavily off this [LD2410](https://github.com/ncmreynolds/ld2410), [Waveshare](https://www.waveshare.com/wiki/HMMD_mmWave_Sensor) documentations.

This library allows you to configure and use the sensor over serial connection.\
The Sensor communicates over serial at 115200 baud by default.

The modules also provide an 'active high' output on the 'OUT' pin which can be used to signal presence, based off the settings configured. Once the modules are configured it is not necessary to use their UART, but it provides much more information.

## Connections

The module must be powered by 3V3 (3.0~3.6V Wide-range Voltage)

![](https://www.waveshare.com/w/upload/5/5b/HMMD_mmWave_Sensor02.jpg)

If you have the breakout board, you can use the VCC, GND, TX and RX pins to work with the module.

## Reading distances

The S3KM1110 has a number of 'gates', each of which correspond to a distance of about 0.70m and many of the settings/measurements are calculated from this.

## Usage

1. Create radar: `s3km1110 radar`
2. Start radar in `setup()` with help of `begin` method: `radar.begin(Serial2, Serial)`  
First parameter required is radar Serial  
Second parameter optional is debug Serial

3. Call radar's `read` methon in `loop()`: `radar.read()`
4. If `read` is success, then get radar info with following variables:
  * `radar.isTargetDetected` – Check is radar detect something
  * `radar.distanceToTarget` – Get distance to target

## Example

For a detailed example, check out [full example file](https://github.com/2Grey/s3km1110/blob/main/examples/main.cpp)

## Not implemented features
- Work with registers
- Work with factory test mode
- Read data in Debug mode
- Read data in Running mode

## Disclaimer
This library is an independent project and is not affiliated with, endorsed by, or connected to Arduino®.
