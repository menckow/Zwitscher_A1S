#include "AudioEngine.h"
#include "AppConfig.h"
#include "HardwareConfig.h"
#include "MqttHandler.h"
#include "LedController.h"
#include <Preferences.h>
#include <esp_sleep.h>

extern AppConfig config;
extern Preferences preferences;
extern LedController ledCtrl;

// These are declared in main.cpp
extern AudioBoardStream i2s;
extern EncodedAudioStream decoder;
extern StreamCopy copier;
extern File audioFile;

const unsigned long maxPlaybackDuration = 5 * 60 * 1000UL;
const unsigned long deepSleepInactivityTimeout = 90 * 60 * 1000UL;

AudioEngine::AudioEngine() {
    currentState = PlaybackState::INITIALIZING;
    playbackStartTime = 0;
    lastPirActivityTime = 0;
    currentDirectoryIndex = -1;
    introFileName = "intro.mp3";
    currentVolume = 0.6;
}

void AudioEngine::init() {
    preferences.begin("appState", true);
    currentDirectoryIndex = preferences.getInt("dirIndex", -1);
    float savedVolume = preferences.getFloat("volume", 0.60);
    preferences.end();
    
    if (isnan(savedVolume) || savedVolume < 0.0 || savedVolume > 1.0) {
        currentVolume = 0.60;
    } else {
        currentVolume = savedVolume;
    }

    i2s.setVolume(currentVolume); // Direkte Steuerung des Hardware DAC!
    
    if (directoryList.empty()) {
        Serial.println("Warning: AudioEngine init with empty directoryList");
    } else {
        if (currentDirectoryIndex < 0 || currentDirectoryIndex >= directoryList.size()) {
            currentDirectoryIndex = 0;
        }
        loadFilesFromCurrentDirectory();
    }
    
    currentState = PlaybackState::IDLE;
    lastPirActivityTime = millis();
}

void AudioEngine::update() {
    // audio-tools non-blocking copy
    if (currentState == PlaybackState::PLAYING_RANDOM || currentState == PlaybackState::PLAYING_INTRO) {
        if (!copier.copy()) {
            onAudioEof();
        }
    }
    checkPirAndTimeout();
}

void AudioEngine::findMp3Directories(File dir) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) {
            File subdir = SD.open(entry.path());
            bool hasMp3 = false;
            if(subdir) {
                while(true) {
                    File subEntry = subdir.openNextFile();
                    if(!subEntry) break;
                    String subName = String(subEntry.name());
                    subName.toLowerCase();
                    if (!subEntry.isDirectory() && subName.endsWith(".mp3")) {
                        hasMp3 = true;
                        subEntry.close();
                        break;
                    }
                    subEntry.close();
                }
                subdir.close();
            }
            if (hasMp3) {
                directoryList.push_back(String(entry.path()));
                Serial.println("Found MP3 directory: " + String(entry.path()));
            }
        }
        entry.close();
    }
}

void AudioEngine::loadFilesFromCurrentDirectory() {
    currentMp3Files.clear();
    if (currentDirectoryIndex < 0 || currentDirectoryIndex >= directoryList.size()) {
         if (!directoryList.empty()) currentDirectoryIndex = 0;
         else return;
    }

    String currentDirPath = directoryList[currentDirectoryIndex];
    Serial.println("Loading files from: " + currentDirPath);
    File dir = SD.open(currentDirPath);
    if (!dir || !dir.isDirectory()) {
        if(dir) dir.close();
        return;
    }
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
            String fileName = String(entry.name());
            String lowerCaseFileName = fileName;
            lowerCaseFileName.toLowerCase();
            if (lowerCaseFileName.endsWith(".mp3") && !lowerCaseFileName.endsWith(introFileName)) {
                 if (!fileName.startsWith("._")) {
                     currentMp3Files.push_back(fileName);
                 }
            }
        }
        entry.close();
    }
    dir.close();
    Serial.printf("  Found %d playable MP3 files.\n", currentMp3Files.size());
}

void AudioEngine::playIntro() {
    if (directoryList.empty()) return;
    String currentDirPath = directoryList[currentDirectoryIndex];
    String introPath = currentDirPath + "/" + introFileName;
    
    if (audioFile) audioFile.close();
    audioFile = SD.open(introPath, "r");
    
    if (audioFile) {
        currentState = PlaybackState::PLAYING_INTRO;
        Serial.println("Playing intro: " + introPath);
        decoder.begin();
        copier.begin(decoder, audioFile);
        mqttHandler.publish(config.getTopicPlaying(), introPath);
        mqttHandler.publish(config.getTopicStatus(), "Playing Intro");
    } else {
        Serial.println("intro.mp3 not found: " + introPath);
        mqttHandler.publish(config.getTopicError(), "intro.mp3 not found");
        currentState = PlaybackState::IDLE;
    }
}

void AudioEngine::playRandomTrack() {
    if (currentMp3Files.empty()) {
        Serial.println("No random MP3 files found.");
        return;
    }
    int randomIndex = random(0, currentMp3Files.size());
    String randomFileName = currentMp3Files[randomIndex];
    String currentDirPath = directoryList[currentDirectoryIndex];
    String randomFilePath = currentDirPath + "/" + randomFileName;
    
    if (audioFile) audioFile.close();
    audioFile = SD.open(randomFilePath, "r");
    
    if (audioFile) {
        currentState = PlaybackState::PLAYING_RANDOM;
        playbackStartTime = millis();
        Serial.println("Playing random file: " + randomFilePath);
        decoder.begin();
        copier.begin(decoder, audioFile);
        mqttHandler.publish(config.getTopicPlaying(), randomFilePath);
        mqttHandler.publish(config.getTopicStatus(), "Playing Random File");
    } else {
        Serial.println("Error playing random file.");
        mqttHandler.publish(config.getTopicError(), "File play error");
        currentState = PlaybackState::IDLE;
    }
}

void AudioEngine::stopPlayback() {
    if (currentState == PlaybackState::PLAYING_INTRO || currentState == PlaybackState::PLAYING_RANDOM) {
        if (audioFile) audioFile.close();
        decoder.end(); 
        currentState = PlaybackState::IDLE;
        Serial.println("Stopped current playback.");
        mqttHandler.publish(config.getTopicPlaying(), "STOPPED (Manual)");
    }
}

void AudioEngine::onAudioEof() {
    Serial.println("\n>>> audio_eof_mp3 called <<<");
    mqttHandler.publish(config.getTopicPlaying(), "STOPPED (EOF)");
    
    if (currentState == PlaybackState::PLAYING_INTRO) {
        currentState = PlaybackState::IDLE;
        Serial.println(">>> Intro playback finished. Ready for PIR. <<<");
        mqttHandler.publish(config.getTopicStatus(), "Intro Finished");
    } else if (currentState == PlaybackState::PLAYING_RANDOM) {
        currentState = PlaybackState::IDLE;
        playbackStartTime = 0;
        Serial.println(">>> Random file finished. Ready for PIR. <<<");
        mqttHandler.publish(config.getTopicStatus(), "Random File Finished");
    } else {
        currentState = PlaybackState::IDLE;
    }
}

void AudioEngine::checkPirAndTimeout() {
    bool pirStateHigh = (digitalRead(PIR_PIN) == HIGH);

    if (pirStateHigh || currentState == PlaybackState::PLAYING_INTRO || currentState == PlaybackState::PLAYING_RANDOM) {
        lastPirActivityTime = millis();
    }

    if (currentState == PlaybackState::IDLE || currentState == PlaybackState::STANDBY) {
        if (pirStateHigh) {
            lastPirActivityTime = millis();
            if (currentState == PlaybackState::STANDBY) {
                currentState = PlaybackState::IDLE;
                Serial.println("Woke up from Standby (PIR)");
                mqttHandler.publish(config.getTopicStatus(), "Woke up from Standby");
            }
            Serial.println("\n+++ PIR TRIGGER +++");
            mqttHandler.publish(config.getTopicStatus(), "PIR Triggered");

            if (config.friendlamp_enabled) {
                mqttHandler.publishLamp(config.zwitscherbox_topic, "{\"client_id\":\"" + config.mqtt_client_id + "\",\"color\":\"" + config.friendlamp_color + "\",\"effect\":\"fade\",\"duration\":30000}", false);
            }

            playRandomTrack();
        }
    }

    // Timeout
    if (currentState == PlaybackState::PLAYING_RANDOM) {
        unsigned long elapsedTime = millis() - playbackStartTime;
        if (elapsedTime >= maxPlaybackDuration) {
            Serial.println("\n!!! PIR playback timeout !!!");
            stopPlayback();
        }
    }

    // Standby Check
    if (currentState == PlaybackState::IDLE) {
        unsigned long inactivityDuration = millis() - lastPirActivityTime;
        if (inactivityDuration >= deepSleepInactivityTimeout) {
            preferences.begin("appState", false);
            preferences.putFloat("volume", currentVolume);
            preferences.putInt("dirIndex", currentDirectoryIndex);
            preferences.end();
            
            Serial.println("--- Entering Standby Mode ---");
            mqttHandler.publish(config.getTopicStatus(), "Entering Standby", true);
            delay(500); // Give MQTT time
            
            if (config.friendlamp_enabled) {
                ledCtrl.startFadeOut();
                delay(2000); 
                ledCtrl.update(); 
            }
            
            i2s.end();
            SD.end();

            esp_sleep_enable_timer_wakeup(1 * 1000000ULL); // Beispiel: Wacht zwar nochmal auf, aber prinzipiell Standby
            esp_deep_sleep_start();
        }
    }
}

// Button actions für das ESP32 Audio Kit
void AudioEngine::increaseVolume() {
    currentVolume = min(currentVolume + 0.05f, 1.0f);
    i2s.setVolume(currentVolume);
    Serial.printf("Vol+ %.2f\n", currentVolume);
    mqttHandler.publish(config.getTopicVolume(), String(currentVolume));
    lastPirActivityTime = millis();
}

void AudioEngine::decreaseVolume() {
    currentVolume = max(currentVolume - 0.05f, 0.0f);
    i2s.setVolume(currentVolume);
    Serial.printf("Vol- %.2f\n", currentVolume);
    mqttHandler.publish(config.getTopicVolume(), String(currentVolume));
    lastPirActivityTime = millis();
}

void AudioEngine::nextDirectory() {
    if (currentState == PlaybackState::STANDBY) {
        currentState = PlaybackState::IDLE;
        mqttHandler.publish(config.getTopicStatus(), "Woke up from Standby");
    }
    
    stopPlayback();
    if (!directoryList.empty()) {
        currentDirectoryIndex = (currentDirectoryIndex + 1) % directoryList.size();
        String newDirPath = directoryList[currentDirectoryIndex];
        mqttHandler.publish(config.getTopicDirectory(), newDirPath, true);
        loadFilesFromCurrentDirectory();
        
        playIntro();
        
        preferences.begin("appState", false);
        preferences.putInt("dirIndex", currentDirectoryIndex);
        preferences.end();
    }
    lastPirActivityTime = millis();
}
