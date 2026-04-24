# Zwitscher_A1S

**Zwitscher_A1S** ist eine Hardware-spezifische Portierung und Weiterentwicklung des originalen [**Zwitscher**](https://github.com/menckow/Zwitscher) Repositories.

Während das ursprüngliche Projekt auf modulare, generische ESP32-Komponenten ausgelegt ist (z.B. externer I2S-Verstärker MAX98357A), wurde diese Variante tiefgreifend für das **ESP32 Audio Kit V2.2 (AI Thinker A1S)** optimiert. Alle kritischen Hardware-Routings des auf dem Board verbauten ES8388 Audio-Codecs sowie des 3-Watt NS4150 Lautsprecher-Verstärkers werden hier nativ und verlustfrei über I2C angesprochen.

## Features & Hardware-Spezifikationen
* **Audio-Engine**: Verwendet `arduino-audio-tools` (Phil Schatzmann) mit non-blocking StreamCopy und MP3-Helix Decoder, um flüssiges Multitasking (Webserver, MQTT) während der Wiedergabe zu ermöglichen.
* **Volume Control**: Verwendet den internen I2C ES8388 Codec zur Hardware-Steuerung der Lautstärke (logarithmische Dämpfung direkt auf Chip-Ebene) anstatt einer PCM Software-Skalierung.
* **Friendship Lamp / NeoPixel**: Unterstützt WS2812B LED-Ringe zur visuellen Signalisierung per MQTT. Standardmäßig konfiguriert auf den sauberen Ausgang **Pin 22** unter Nutzung des ESP32 RMT-Treibers.
* **PIR Sensor**: Löst die Audio-Wiedergabe per Bewegungsmelder aus (auf **Pin 12**).
* **Mac OS Kompatibilität**: Beinhaltet einen aktiven Filter zum Ignorieren von `._` AppleDouble-Geisterdateien auf der FAT32 SD-Karte.

## Architektur (identisch zum Original)
Die Software-Architektur bleibt dem Hauptprojekt (`Zwitscher`) treu:
* `WebManager`: AsyncWebServer mit LittleFS für das Captive Portal und die Konfigurations-Infrastruktur.
* `MqttHandler`: Kommunikation mit Home Assistant (Status-Tracking) und dem globalen HiveMQ-Server (Friendship Lamp Funktionalität).
* `AppConfig`: JSON gespeicherte Einstellungen (OTA-fähig).

## Dokumentation
Unter `/docs` findest du tiefergehende Dokumentationen zur durchgeführten Code-Migration, der ESP32-Pinbelegung (insbesondere SPI und I2S) sowie zur Lösung von Hardware-Lockups.

## Aufbau & Deployment
Das Projekt wird vollständig über `PlatformIO` verwaltet. Um es zu kompilieren:
1. Das Projekt in VS Code / PlatformIO öffnen.
2. Das passende Environment für das ESP32 Audio Kit (z.B. `esp32-audio-kit`) ist in der `platformio.ini` inklusive der nötigen Build-Flags (wie `AI_THINKER_ES8388_VOLUME_HACK`) hinterlegt.
3. Build und Upload durchführen (die `huge_app.csv` Partition Table stellt hierbei ausreichend Flashspeicher für den Code zur Verfügung).
