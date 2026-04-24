# Merge Zwitscher Features into Audio Kit Project

Das Ziel ist es, die Architektur und alle fortgeschrittenen Funktionen aus Deinem `Zwitscher`-Repository (Web-GUI über LittleFS, OTA Updates, Friendship Lamp / NeoPixels, modulares Design) auf das aktuelle **ESP32 Audio Kit V2.2** Projekt (`ZwitscherA1s`) zu portieren, **ohne** dabei die `audio-tools` / `audio-driver` Treiber-Basis von Phil Schatzmann zu verlieren.

## User Review Required

> [!WARNING]
> Der ESP32 Audio Kit (A1S) hat im Gegensatz zum ESP32-S3 sehr viele blockierte Pins (durch den Codec, SD-Karte und SDRAM). Wir müssen NeoPixel und Sensoren sorgfältig belegen. 

## Proposed Changes

### `platformio.ini`
- **[MODIFY]** `platformio.ini`: Erweitern um die WebServer, DNS, ArduinoJson und NeoPixel Bibliotheken aus dem Zwitscher-Repo. Der Board-Typ bleibt das Standard `esp32dev` mit allen unseren AudioKit und PSRAM-Flags.

---

### `src/` (Quellcode)
- **[DELETE]** `AnsagedesVerz_Flash_MQTT_V4.ino`: Der Monolith wird entfernt.
- **[NEW]** Ordner-Struktur aus `Zwitscher_ref`: Übernahme von `src/main.cpp`, `src/WebManager.cpp` usw.
- **[MODIFY]** `src/AudioEngine.cpp` & `include/AudioEngine.h`:
  Das Herzstück! Die alte `ESP32-audioI2S` (`Audio.h`) Bibliothek im Hintergrund wird komplett durch Phil Schatzmanns Streams (`AudioBoardStream`, `VolumeStream`, `EncodedAudioStream` und `StreamCopy`) ausgetauscht. Das blockfreie Kopieren der SD-Lese-Daten erfolgt dann künftig innerhalb von `AudioEngine::update()`.

- **[MODIFY]** `src/main.cpp`: 
  Das Setup des Audio-Treibers (mit `DAC_OUTPUT_ALL` und PA Enable an Pin 21) wird in die modulare Struktur eingewoben.

- **[MODIFY]** Button-Logik:
  Das `Zwitscher_ref` Repo geht von einem physischen Analog-Poti und einem Taster aus. Das Audio Kit hat Taster verbaut. Ich werde die Audio Kit Taster konfigurieren, anstatt dedizierte externe GPIO-Taster abzufragen.

## Open Questions

> [!IMPORTANT]  
> Bitte kläre folgende Pin/Hardware-Fragen, bevor wir den Code umschreiben:
> 1. **NeoPixels (Friendship Lamp):** Hast Du am Audio Kit V2.2 physikalisch einen LED-Ring angeschlossen? Wenn ja, an **welchen GPIO Pin**?
> 2. **PIR Sensor:** In der alten Zwitscherbox war er an Pin 18, in dem `.ino`-Code für das Audio-Kit hattest Du **Pin 12**. Sollen wir Pin 12 als Standard belassen?
> 3. **Potentiometer:** Hast Du ein Poti angeschlossen, oder steuerst Du die Lautstärke rein über die Audio Kit Buttons (IO13 `Vol-`, IO19 `Vol+`) / MQTT / Web-UI?

## Verification Plan
1. **Automated Compilation:** Plattform-Kompilierung mittels `pio run`, um C++ Linking-Fehler der kombinierten Libs auszuschließen.
2. **Testing:** Flashen des Boards. Die SD-Karte muss reagieren, der Access Point gestartet werden (LittleFS Captive Portal) und die NeoPixel leuchten (sofern angeschlossen). Die Speaker-Routing Fixes müssen erhalten bleiben.
