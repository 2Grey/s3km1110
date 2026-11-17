# Migrations

## 1.0.0

### Radar configuration struct

Configuration is no more stored as pointer

`radarConfiguration` and its fields is not pointers now

### Firmware version and Serial number

`firmwareVersion` and `serialNumber` are also no longer pointers

### Active and inactive frames

I'm not sure I'm naming and using this configuration data correctly, so I decided to remove it from the code for now

#### Removed methods

- `bool readRadarConfigActiveFrameNumber()`
- `bool readRadarConfigInactiveFrameNumber()`
- `bool setRadarConfigurationActiveFrameNum(uint8_t)`
- `bool setRadarConfigurationInactiveFrameNum(uint8_t)`

#### Removed properties

- `uint8_t *activeFrameNum` in _s3km1110ConfigParameters_
- `uint8_t *inactiveFrameNum` in _s3km1110ConfigParameters_

### Target disappearance delay

The class field and corresponding methods have been renamed to look more logically named

#### Renamed methods

- `bool readRadarConfigTargetDisappearanceDelay()`
- `bool setRadarConfigurationTargetDisappearanceDelay(uint16_t)`

#### Renamed field

- `uint16_t *delay` to `uint16_t targetDisappearanceDelay` in _s3km1110ConfigParameters_

### Is connected

The `isConnected` method was renamed because it did not correctly reflect the essence

#### Renamed method

- `bool isConnected()` to `bool isActive()`

### Other underhood changes

The `begin()` method now calls `readAllRadarConfigs()` internally.

To disable this behavior, define `S3KM1110_SKIP_READ_CONFIG_ON_BEGIN`

## 1.0.3

### Optional debug stream

`begin()` now supports initialization without a debug stream:

```cpp
radar.begin(Serial2);
/// Or use the two-argument overload when debug output is needed:
radar.begin(Serial2, Serial);
```

### begin return value

`begin()` now returns `false` if the library cannot enable report mode, read the radar configuration, or confirm exit from command mode.

To skip reading configuration during `begin()` in PlatformIO, add this option to the environment in `platformio.ini`:

```ini
build_flags = -DS3KM1110_SKIP_READ_CONFIG_ON_BEGIN
```

The macro must be defined when compiling the library itself. A `#define` in the
sketch alone does not affect the library's separately compiled `.cpp` file.

### Frame parsing

Data and command frames are now validated against their declared payload length before parsing. This makes frame parsing more reliable when binary payload data contains bytes that look like frame footers.

### Gate energy parsing
Distance gate energy values are now parsed as 16-bit little-endian values without overlapping bytes between gates.

### Config parsing

Configuration payload values are now parsed from unsigned bytes to avoid signed char issues on platforms where char is signed.

### Initialization and command safety

Calling `read()` or a command method before `begin()` now returns `false`.
`isActive()` remains `false` until a valid frame is received. Repeating `begin()`
discards the previous stream's partial frame and resets activity tracking.

Short command responses and failed ACKs are rejected before updating firmware,
serial number, or configuration values. Command frame construction and response
waiting are separated internally; public method signatures remain unchanged.
Firmware and serial number responses must match their declared string length.
Commands return `false` if exiting command mode fails or its ACK is missing.

### Portable serial output

Debug output and the example use Arduino `print()` / `println()` instead of
`printf()`. The example preserves signed distance values and throttles warnings
correctly across `millis()` wraparound.
