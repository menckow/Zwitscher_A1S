# Zwitscher_A1S

**Zwitscher_A1S** ist eine Hardware-spezifische Portierung und Weiterentwicklung des originalen [**Zwitscher**](https://github.com/menckow/Zwitscher) Repositories.

Während das ursprüngliche Projekt auf modulare, generische ESP32-Komponenten ausgelegt ist (z.B. externer I2S-Verstärker MAX98357A), wurde diese Variante tiefgreifend für das **ESP32 Audio Kit V2.2 (AI Thinker A1S)** optimiert. Alle kritischen Hardware-Routings des auf dem Board verbauten ES8388 Audio-Codecs sowie des 3-Watt NS4150 Lautsprecher-Verstärkers werden hier nativ und verlustfrei über I2C angesprochen.

Funktional ist die A1S-Variante mit der Hauptlinie auf demselben Stand und spricht das **v2-Familien-Schema** (siehe unten). Die A1S meldet sich im Schema mit dem Geräte-Typ **`box-a1s`** (statt `box`), damit Familien- und Global-OTAs A1S- und V6-Boxen getrennt adressieren können — sonst würde eine Bulk-Aktion die Hälfte der Boxen mit der falschen Firmware bricken.

## Features & Hardware-Spezifikationen
* **Audio-Engine**: Verwendet `arduino-audio-tools` (Phil Schatzmann) mit non-blocking StreamCopy und MP3-Helix Decoder, um flüssiges Multitasking (Webserver, MQTT) während der Wiedergabe zu ermöglichen.
* **Volume Control**: Verwendet den internen I2C ES8388 Codec zur Hardware-Steuerung der Lautstärke (logarithmische Dämpfung direkt auf Chip-Ebene) anstatt einer PCM Software-Skalierung.
* **Friendship Lamp / NeoPixel**: Unterstützt WS2812B LED-Ringe zur visuellen Signalisierung per MQTT. Standardmäßig konfiguriert auf den sauberen Ausgang **Pin 22** unter Nutzung des ESP32 RMT-Treibers.
* **PIR Sensor**: Löst die Audio-Wiedergabe per Bewegungsmelder aus (auf **Pin 12**). Im v2-Schema wird zusätzlich an alle konfigurierten Familienkreise ein Signal (`sender_type:"box"`) gesendet — andere Boxen leuchten dadurch in der Box-Farbe (Solid-Mode), Lampen ignorieren das bewusst.
* **A1S-spezifische Härtungen**: `WiFi.setSleep(false)` und `WiFi.setAutoReconnect(true)` halten WLAN während des Audio-Streams stabil; 30 s WLAN-Timeout statt 15 s, kein `WiFi.disconnect()` beim Reconnect-Fail (erhält die State Machine).
* **Mac OS Kompatibilität**: Beinhaltet einen aktiven Filter zum Ignorieren von `._` AppleDouble-Geisterdateien auf der FAT32 SD-Karte.

## 📡 v2 Familien-Schema (MQTT)

Die A1S nutzt dieselbe Topic-Topologie wie die Hauptbox und die Freundschaftslampe. Konfiguriert wird sie über das Feld **`Familienkreise`** in der Webkonfig (oder via `FAMILY_IDS` in `config.txt`), kommagetrennt — z.B. `schmidt,lieblings`.

| Zweck | Topic | Retained |
|---|---|---|
| Familien-Signal | `fl/family/<familyId>/signal` | nein |
| Gerätestatus (JSON, `type:"box-a1s"`) | `fl/device/<deviceId>/status` | **ja** |
| OTA pro Gerät | `fl/device/<deviceId>/update/trigger` | nein |
| OTA pro Familie (nur A1S-Boxen) | `fl/family/<familyId>/update/trigger/box-a1s` | nein |
| Globaler Notfall-Push (nur A1S-Boxen) | `fl/_global/update/trigger/box-a1s` | nein |
| OTA-Backchannel | `fl/device/<deviceId>/update/status` | nein |

Verhaltensregeln im Empfang:
* **Self-Filter** über `client_id` (Box reagiert nicht auf eigenes Signal).
* **NTP-Schutz**: Signale älter als 60 s werden verworfen.
* **`sender_type`-Aware Rendering**: `"lamp"` → Komplementär-Modus (jede 3. LED in der Komplementärfarbe), `"box"` / `"box-a1s"` oder leer → Solid-Mode. Die Lampe filtert ausgehende Box-Signale per `startsWith("box")`, fängt also auch die A1S-Variante.
* **Defense-in-depth `target_type`**: Bei OTA-Topics wird zusätzlich geprüft, dass das Payload an `"box-a1s"` adressiert ist (sonst wird's verworfen).

## 🔄 OTA & Web Upload

Drei MQTT-Update-Kanäle wie oben gelistet. Daneben gibt es einen **Web-Upload-OTA-Endpoint** (`/update-firmware`) auf der Konfigurationsseite: `.bin` auswählen, optional MD5 eingeben, hochladen — Audio wird gestoppt, LED leuchtet blau während des Schreibvorgangs, danach automatischer Reboot. Sehr nützlich für die Erstprovisionierung oder wenn MQTT temporär nicht verfügbar ist.

## 🔘 Tasten-Funktionen & IP-Adresse Visualisierung

Auf dem ESP32 Audio Kit werden folgende Tasten verwendet:
* **KEY4 (IO23 - Taster für Verzeichniswechsel)**: Ein einfacher Druck stoppt die Wiedergabe, wechselt zum nächsten Verzeichnis auf der SD-Karte und spielt das Intro ab.
* **KEY5 (IO18 - Trenntaste / ehem. Prev)**: Ein einfacher Druck startet direkt die blinkende Visualisierung der **letzten Zahl der lokalen IP-Adresse** auf dem LED-Ring.
  * **Ablauf & Blinkmuster**:
    * Die letzte Zahl der IP-Adresse (z. B. `174` bei `192.168.178.174`) wird Ziffer für Ziffer ausgegeben.
    * Jede Ziffer $D$ (0 bis 9) wird durch **$D + 1$ weißes Blinken** dargestellt (jeweils 500 ms AN / 500 ms AUS).
    * Als Trennzeichen leuchtet der gesamte LED-Ring für **1 Sekunde durchgehend Rot** (gefolgt von 500 ms Pause).

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
