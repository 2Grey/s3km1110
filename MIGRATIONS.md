# Migrations

## Radar configuration struct

Configuration is no more stored as pointer

`radarConfiguration` and its fields is not pointers now

## Firmware version and Serial number

`firmwareVersion` and `serialNumber` are also no longer pointers

## Active and inactive frames

I'm not sure I'm naming and using this configuration data correctly, so I decided to remove it from the code for now

### Removed methods

- `bool readRadarConfigActiveFrameNumber()`
- `bool readRadarConfigInactiveFrameNumber()`
- `bool setRadarConfigurationActiveFrameNum(uint8_t)`
- `bool setRadarConfigurationInactiveFrameNum(uint8_t)`

### Removed properties

- `uint8_t *activeFrameNum` in _s3km1110ConfigParameters_
- `uint8_t *inactiveFrameNum` in _s3km1110ConfigParameters_

## Target disappearance delay

The class field and corresponding methods have been renamed to look more logically named

### Renamed methods

- `bool readRadarConfigTargetDisappearanceDelay()`
- `bool setRadarConfigurationTargetDisappearanceDelay(uint16_t)`

### Renamed field

- `uint16_t *delay` to `uint16_t targetDisappearanceDelay` in _s3km1110ConfigParameters_

## Is connected

The `isConnected` method was renamed because it did not correctly reflect the essence

### Renamed method

- `bool isConnected()` to `bool isActive()`

## Other underhood changes

The `begin()` method now calls `readAllRadarConfigs()` internally.

To disable this behavior, define `S3KM1110_SKIP_READ_CONFIG_ON_BEGIN`