# Zwitscher Modular Architecture Merge

## Was wurde umgesetzt?
Die gesamte monolithische `.ino`-Datei wurde gelöscht und durch die hochentwickelte, modulare Verzeichnisstruktur Deines Github-Repositories **Zwitscher** ersetzt. Zeitgleich wurde verhindert, dass die kritischen Audio-Probleme des ESP32 Audio Kit V2.2 wieder auftreten.

### 1. Architektur-Portierung 
Folgende Module wurden in das Projekt geholt und auf Deine Hardware angepasst:
- **`WebManager`**: AsyncWebServer basierte Weboberfläche mit LittleFS für das Konfigurations-Interface und für On-The-Air (OTA) Updates.
- **`MqttHandler`**: MQTT-Statustracking und Befehls-Listener (Friendship Lamp API, HomeAssistant Integration).
- **`LedController`**: Steuerung der NeoPixels / Leuchtring für Status-Blitze und den Friendship Lamp Modus.
- **`AppConfig`**: JSON-basiertes Laden und Speichern aller Daten über LittleFS anstelle einzelner NVS-Schreibvorgänge oder der SD-Card Config.

### 2. Audio-Tools Umschreibung (`AudioEngine.cpp`)
Das "alte" `ESP32-audioI2S` Modul aus dem GitHub Repo wurde komplett herausgerissen. Deine `AudioEngine` nutzt nun vollständig die **Phil Schatzmann** (`arduino-audio-tools`) Bibliothek. Dabei wurden folgende Eigenschaften implementiert:
- **Non-Blocking Copier:** Die Funktion `update()` kopiert MP3-Daten stückchenweise aus dem SD-Stream in den Decoder. Der ESP32 kann dadurch parallel Anfragen vom Webserver oder von MQTT bedienen.
- **Hardware-DAC statt Software-Booster:** Ein künstlicher Softwarepegel-Boost (`* 2.5`) wurde entfernt. Die Lautstärke wird jetzt ausschließlich, logarithmisch und sanft per I²C-Signal über den integrierten ES8388-Codec (`i2s.setVolume`) direkt an dem leistungsstarken 3-Watt NS4150-Verstärker reguliert.
- **Hardware Button Callbacks:** Statt extern `analogRead` und Debouncing durchzuführen, abonniert die `AudioEngine` jetzt asynchron Events von den bereits fest auf dem AI Thinker verlöteten Keys (z.B. Vol+, Vol-, Next).
- **AppleDouble Bugfix:** Spezifische macOS System- und Metadatendateien (`._*.mp3`) werden auf der FAT32 SD-Karte vom Scanner konsequent gefiltert, um einen Buffer-Crash des MP3-Decoders zu verhindern.

### 3. ESP32 Audio Kit V2.2 Hardware-Anpassungen (`main.cpp` & `HardwareConfig.h`)
- Der Speaker-Output Fix (`DAC_OUTPUT_ALL` plus *Phil's Volume Hack 1*) blieb erhalten.
- `PA_EN` (Pin 21) wird verlässlich in `main.cpp` hochgeschaltet und ist exklusiv für den Audio-Verstärker reserviert.
- **SD-Karten SPI-Fix:** Der SPI-Bus wird nun via `SPI.begin(14, 2, 15, 13)` vollständig und exakt passend zur Pin-Reihenfolge (MISO/MOSI) initialisiert, bevor die SD-Karte per Arduino-Mount angebunden wird. Die Automatik des ESP32 Audio-Treibers `sd_active=false` wurde abgeschaltet, um Hardware-Lockups zu vermeiden.
- Der **Bewegungsmelder (PIR)** bleibt sicher auf Pin `12`.
- **Friendship Lamp / NeoPixel:** Der RMT-Dienst des ESP32 Core 3.0 ist nun mit der präzisen Boot-Reihenfolge synchronisiert. Der Leuchtring kann ohne störende Artefakte über den freien **Pin 22** angesteuert werden (Tasten 13 und 19 bleiben unbehelligt).

## Verifikation 
✅ Alle Bibliotheken wurden in `platformio.ini` zusammengeführt und C++ Kompilierungs- sowie Linker-Errors (wie Konflikte zwischen den alten und neuen Sound-Klassen) behoben.

**Hinweis zum ersten Start:** Da das "LittleFS" möglicherweise noch keine `config.json` besitzt, wird das Modul beim ersten Boot in den Access Point / Captive Portal (Zwitscherbox AP) Modus gehen. Du kannst dort Deine MQTT / WiFi Einstellungen eingeben!
