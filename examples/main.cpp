#include <Arduino.h>
#include <s3km1110.h>

#if defined(ESP32)
  #ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
    #if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
      #define MONITOR_SERIAL Serial
      #define RADAR_SERIAL Serial2
      #define RADAR_RX_PIN 16
      #define RADAR_TX_PIN 17
    #elif CONFIG_IDF_TARGET_ESP32S2
      #define MONITOR_SERIAL Serial
      #define RADAR_SERIAL Serial1
      #define RADAR_RX_PIN 9
      #define RADAR_TX_PIN 8
    #elif CONFIG_IDF_TARGET_ESP32C3
      #define MONITOR_SERIAL Serial
      #define RADAR_SERIAL Serial1
      #define RADAR_RX_PIN 4
      #define RADAR_TX_PIN 5
    #else
      #error Target CONFIG_IDF_TARGET is not supported
    #endif
  #else // ESP32 Before IDF 4.0
    #define MONITOR_SERIAL Serial
    #define RADAR_SERIAL Serial1
    #define RADAR_RX_PIN 32
    #define RADAR_TX_PIN 33
  #endif
#elif defined(__AVR_ATmega32U4__)
  #define MONITOR_SERIAL Serial
  #define RADAR_SERIAL Serial1
  #define RADAR_RX_PIN 0
  #define RADAR_TX_PIN 1
#endif

s3km1110 radar;

int16_t lastDistance = -1;
bool isDetected = false;

void setup(void)
{
    MONITOR_SERIAL.begin(115200);

    MONITOR_SERIAL.println("Waiting for radar to boot...");
    delay(2000);

    #if defined(ESP32)
    RADAR_SERIAL.begin(115200, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN); //UART for monitoring the radar
    #elif defined(__AVR_ATmega32U4__)
    RADAR_SERIAL.begin(115200); //UART for monitoring the radar
    #endif

    bool isRadarEnabled = false;
    for(int i=0; i<3; i++) {
        if(radar.begin(RADAR_SERIAL, MONITOR_SERIAL)) {
            isRadarEnabled = true;
            break;
        }
        MONITOR_SERIAL.println("Retrying connection...");
        delay(1000);
    }

    MONITOR_SERIAL.print(F("Radar status: "));
    MONITOR_SERIAL.println(isRadarEnabled ? F("Ok") : F("Failed"));

    if (isRadarEnabled) {
        if (radar.readFirmwareVersion()) {
            MONITOR_SERIAL.print(F("[Info] Radar Firmware: "));
            MONITOR_SERIAL.println(radar.firmwareVersion);
        }
        if (radar.readSerialNumber()) {
            MONITOR_SERIAL.print(F("[Info] Radar Serial number: "));
            MONITOR_SERIAL.println(radar.serialNumber);
        }

        const auto &config = radar.radarConfiguration;
        MONITOR_SERIAL.println(F("[Info] Radar config:"));
        MONITOR_SERIAL.print(F("|- Gates min: "));
        MONITOR_SERIAL.println(config.detectionGatesMin);
        MONITOR_SERIAL.print(F("|- Gates max: "));
        MONITOR_SERIAL.println(config.detectionGatesMax);
        MONITOR_SERIAL.print(F("|- Disappearance delay: "));
        MONITOR_SERIAL.print(config.targetDisappearanceDelay);
        MONITOR_SERIAL.println(F(" sec"));
    }
}

void loop(void)
{
  if (radar.read()) {
    const bool newIsDetected = radar.isTargetDetected;
    const int16_t newDistance = radar.distanceToTarget;

    if (isDetected && !newIsDetected) {
        MONITOR_SERIAL.print(F("[INFO] Target lost (Last known: "));
        MONITOR_SERIAL.print(lastDistance);
        MONITOR_SERIAL.println(F("cm)"));
    } else if (!isDetected && newIsDetected) {
        MONITOR_SERIAL.println(F("[INFO] Target FOUND!"));
    }

    isDetected = newIsDetected;

    if (isDetected && (lastDistance != newDistance)) {
        MONITOR_SERIAL.print(F("[INFO] Distance: "));
        MONITOR_SERIAL.print(newDistance);
        MONITOR_SERIAL.println(F("cm"));
    }

    lastDistance = newDistance;
  }

  static uint32_t lastWarning = 0;
  static bool hasWarned = false;
  const uint32_t currentTime = millis();
  if (!radar.isActive() && (!hasWarned || currentTime - lastWarning >= 5000)) {
    // Throttle warnings even when millis() wraps around.
    lastWarning = currentTime;
    hasWarned = true;
    MONITOR_SERIAL.println("[WARN] Radar not sending data (Check wiring or power)");
  }
}
