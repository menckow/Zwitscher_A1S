#pragma once
#include <Arduino.h>

// --- Central Hardware Pin Definitions ---
// Keeping pins centralized prevents duplicate values and bugs
// when changing hardware connections.

const int POT_PIN = -1;       // Wird auf dem Audio Kit nicht per analogRead gelesen
const int PIR_PIN = 12;       // Standard PIR-Pin auf dem ESP32 Audio Kit V2.2
const int BUTTON_PIN = -1;    // Buttons werden über die interne API des Audio Kits abgefragt
const int LED_PIN = 22;       // Friendship Lamp Dummy-Pin (Pin 22 ist frei verfügbar auf der Header-Leiste)
const int SD_CS_PIN = 13;     // SD Card CS Pin auf dem A1S Board

// Default count, dynamic value is stored in AppConfig.led_count
const int DEFAULT_LED_COUNT = 16;
