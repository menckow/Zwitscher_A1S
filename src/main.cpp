#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <vector>
#include "esp_sleep.h"
#include <Preferences.h> 
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ESPAsyncWebServer.h> 
#include <AsyncTCP.h>          
#include <DNSServer.h>         
#include "freertos/semphr.h"

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/CoreAudio/VolumeStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

// --- Globale Audio Objekte ---
AudioBoardStream i2s(AudioKitEs8388V1);
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix());
StreamCopy copier;
File audioFile;

const char* FW_VERSION = "8.1.0-A1S"; // Major.Minor.Patch

#include "GlobalConfig.h"
#include "HardwareConfig.h"
#include "MqttHandler.h"
#include "LedController.h"
#include "WebManager.h"
#include "AppConfig.h"

Adafruit_NeoPixel strip(DEFAULT_LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
LedController ledCtrl(strip);
AppConfig config;
Preferences preferences; // <--- MISSING NVS OBJECT

#include "AudioEngine.h"
AudioEngine audioEngine;

// --- Callbacks für die ESP32 Audio Kit Buttons ---
void volumeUpAPI(bool active, int repetitions, void *parameter) { 
    if(active) audioEngine.increaseVolume(); 
}
void volumeDownAPI(bool active, int repetitions, void *parameter) { 
    if(active) audioEngine.decreaseVolume(); 
}
void nextDirAPI(bool active, int repetitions, void *parameter) { 
    if(active) audioEngine.nextDirectory(); 
}

void setup() {
    Serial.begin(115200); 
    Serial.println("\nStarting: Directory MP3 Player V8 AudioKit...");

    strip.updateLength(config.led_count);
    ledCtrl.begin();
    pinMode(PIR_PIN, INPUT);

    audioEngine.updatePirActivity();
    randomSeed(esp_random());

    // --- I2S Audio-Stream konfigurieren (VON PHIL SCHATZMANN TREIBERN) ---
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.sd_active = false; // MUSS false bleiben, da wir SD selbst per Arduino-SPI mounten!
    cfg.output_device = audio_driver::DAC_OUTPUT_ALL; 
    i2s.begin(cfg); 
    
    // Hardware-PA erzwingen
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    Serial.println("Audio hardware config complete.");

    Serial.println("Init SD...");
    // Initialize standard SPI with correct ESP32 Audio Kit Pins! 
    // SCK=14, MISO=2, MOSI=15, CS=13
    SPI.begin(14, 2, 15, 13); 
    if (!SD.begin(SD_CS_PIN, SPI)) {
        ledCtrl.setBootStatusLeds(1, false);
        Serial.println("SD FAIL! Halt."); 
        delay(3000);
        while (true); 
    }
    ledCtrl.setBootStatusLeds(1, true);
    Serial.println("SD OK.");

    // --- Konfiguration laden ---
    config.load();

    // --- WLAN verbinden ---
    if (config.homeassistant_mqtt_enabled || config.friendlamp_mqtt_enabled) {
        mqttHandler.setupWifi();
    }

    if (config.friendlamp_enabled) {
        strip.setBrightness(config.led_brightness); 
        Serial.println("Friendship Lamp (LED Ring) configured.");
    }

    Serial.println("Scanning directories...");
    File root = SD.open("/"); if (!root) { Serial.println("ROOT FAIL!"); while (true); }
    audioEngine.findMp3Directories(root);
    root.close();

    audioEngine.init();

    // --- Audio Kit Buttons binden ---
    i2s.addAction(i2s.getKey(4), volumeUpAPI);     // IO19
    i2s.addAction(i2s.getKey(3), volumeDownAPI);   // IO13
    i2s.addAction(i2s.getKey(5), nextDirAPI);      // IO23

    // Boot Status
    if ((config.homeassistant_mqtt_enabled || config.friendlamp_mqtt_enabled) && WiFi.status() == WL_CONNECTED) {
        mqttHandler.forceReconnect(); 
    }
    
    delay(2000); 

    if (config.friendlamp_enabled && config.led_count > 0 && config.led_count != DEFAULT_LED_COUNT) {
        strip.updateLength(config.led_count);
    }
    
    if (!webManager.apMode) {
        strip.clear(); 
        strip.show();
    } else {
        ledCtrl.setApModeLed(true);
    }

    Serial.println("Setup complete.");
    mqttHandler.publish(config.getTopicStatus(), "Initialized", true); 
}

void loop() {
    if (webManager.pendingRestart) {
        if (millis() > webManager.restartTime) {
            ESP.restart();
        }
        return; 
    }
    if (webManager.apMode) {
        webManager.processDns();
        return; 
    }
    
    i2s.processActions(); // Polls the ESP32 Audio Kit Buttons

    ledCtrl.update();
    mqttHandler.update();
    audioEngine.update();
    
    if (config.friendlamp_enabled) {
        if (ledCtrl.ledActive && millis() > ledCtrl.ledTimeout) {
            ledCtrl.ledActive = false;
            ledCtrl.startFadeOut();
        }
    }
}