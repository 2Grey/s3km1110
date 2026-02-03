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

uint32_t lastReading = 0;
uint16_t lastDistance = 0;
bool isDetected = false;

#pragma mark - Lyfe cycle

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

    Serial.printf("Radar status: %s\n", isRadarEnabled ? "Ok" : "Failed");  

    if (isRadarEnabled) {
        if (radar.readFirmwareVersion()) {
          MONITOR_SERIAL.printf("[Info] Radar Firmware: %s\n", radar.firmwareVersion);
        }
        if (radar.readSerialNumber()) {
          MONITOR_SERIAL.printf("[Info] Radar Serial number: %s\n", radar.serialNumber);
        }
        
        auto config = radar.radarConfiguration;
        MONITOR_SERIAL.printf("[Info] Radar config:\n");
        MONITOR_SERIAL.printf("|- Gates min: %u\n", config.detectionGatesMin);
        MONITOR_SERIAL.printf("|- Gates max %u\n", config.detectionGatesMax);
        MONITOR_SERIAL.printf("|- Disappearance delay: %u sec\n", config.targetDisappearanceDelay);
    }
}

void loop(void)
{
  if (radar.read()) {        
    bool newIsDetected = radar.isTargetDetected;
    uint16_t newDistance = radar.distanceToTarget;

    if (isDetected && !newIsDetected) {
        MONITOR_SERIAL.printf("[INFO] Target lost (Last known: %ucm)\n", lastDistance);
    } else if (!isDetected && newIsDetected) {
          MONITOR_SERIAL.println("[INFO] Target FOUND!");
    }

    isDetected = newIsDetected;

    if (isDetected && (lastDistance != newDistance)) {
        MONITOR_SERIAL.printf("[INFO] Distance: %ucm\n", newDistance);
    }

    lastDistance = newDistance;
  }

  static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 5000) {
        lastCheck = millis();
        if (!radar.isActive()) {
            MONITOR_SERIAL.println("[WARN] Radar not sending data (Check wiring or power)");
        }
    }
}
