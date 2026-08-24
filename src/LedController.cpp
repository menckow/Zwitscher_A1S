#include "LedController.h"

// --- LedController Implementation ---

LedController::LedController(Adafruit_NeoPixel& stripRef) 
    : strip(stripRef), mutex(NULL), currentEffect(nullptr) {}

LedController::~LedController() {
    clearCurrentEffect();
    // Do not delete the mutex, as it lives for the app lifecycle
}

void LedController::begin() {
    mutex = xSemaphoreCreateMutex();
}

void LedController::clearCurrentEffect() {
    if (currentEffect) {
        delete currentEffect;
        currentEffect = nullptr;
    }
}

void LedController::setEffect(ILedEffect* newEffect) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        currentEffect = newEffect;
        xSemaphoreGive(mutex);
    } else {
        // If mutex fails, prevent memory leak
        delete newEffect;
    }
}

void LedController::update() {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        if (currentEffect) {
            bool finished = currentEffect->update(strip);
            if (finished) {
                clearCurrentEffect();
            }
        }
        xSemaphoreGive(mutex);
    }
}

void LedController::turnOff() {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        strip.clear();
        strip.show();
        xSemaphoreGive(mutex);
    }
}

void LedController::setBootStatusLeds(int step, bool success) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect(); // Halt active effects
        if (step >= 0 && step < strip.numPixels()) {
            uint32_t color = success ? strip.Color(0, 255, 0) : strip.Color(255, 0, 0);
            strip.setPixelColor(step, color);
            strip.show();
        }
        xSemaphoreGive(mutex);
    }
}

void LedController::setApModeLed(bool active) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        if (active) {
            for (int i = 0; i < strip.numPixels(); i++) {
                strip.setPixelColor(i, strip.Color(255, 0, 0));
            }
        } else {
            strip.clear();
        }
        strip.show();
        xSemaphoreGive(mutex);
    }
}

void LedController::setSolidColor(uint32_t color) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, color);
        }
        strip.show();
        xSemaphoreGive(mutex);
    }
}

void LedController::setOtaProgress(int percent) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        uint16_t numLedsToLight = (percent * strip.numPixels()) / 100;
        
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
            if (i < numLedsToLight) {
                strip.setPixelColor(i, strip.Color(0, 0, 255)); // Blue ring
            } else {
                strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
            }
        }
        strip.show();
        xSemaphoreGive(mutex);
    }
}

void LedController::showIpDigit(int numLeds, uint32_t color) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
            if (i < numLeds) {
                strip.setPixelColor(i, color);
            } else {
                strip.setPixelColor(i, 0); // Off
            }
        }
        strip.show();
        xSemaphoreGive(mutex);
    }
}

bool LedController::isLedActive() const { return ledActive; }
void LedController::setLedActive(bool active) { ledActive = active; }
void LedController::setTimeout(unsigned long timeout) { ledTimeout = timeout; }
unsigned long LedController::getTimeout() const { return ledTimeout; }

// --- Legacy Wrappers ---
ILedEffect* createLedEffect(const String& effectName, uint32_t color, int mode) {
    String eff = effectName;
    eff.toLowerCase();
    if (eff == "rainbow") {
        return new RainbowEffect(false);
    } else if (eff == "blink") {
        return new BlinkEffect(color);
    } else if (eff == "pulse") {
        return new PulseEffect(color);
    } else if (eff == "chase") {
        return new ChaseEffect(color);
    } else if (eff == "sparkle") {
        return new SparkleEffect(color);
    } else if (eff == "aurora") {
        return new AuroraEffect(color);
    } else if (eff == "heartbeat") {
        return new HeartbeatEffect(color);
    } else if (eff == "scanner") {
        return new ScannerEffect(color);
    } else if (eff == "radar") {
        return new RadarEffect(color);
    } else if (eff == "beacon") {
        return new BeaconEffect(color);
    } else if (eff == "wave") {
        return new WaveEffect(color);
    } else if (eff == "drift") {
        return new DriftEffect(color);
    } else if (eff == "burst") {
        return new BurstEffect(color);
    } else {
        return new FadeEffect(color, true, mode);
    }
}

void LedController::startFadeIn(uint32_t color, int mode, const String& effect) {
    setEffect(createLedEffect(effect, color, mode));
}

void LedController::startFadeOut() {
    // Determine what to fade out based on currentEffect class type if necessary, 
    // but the legacy logic just triggered standard FadeOut or RainbowOut.
    // For simplicity, we just trigger standard fade out unless it's rainbow.
    // Given OOP restrictions with RTTI, we'll try a dynamic_cast if enabled, 
    // or just assume a standard fade out. We'll use a hack to know if it's rainbow by checking if we have rainbow.
    if (config.led_fade_effect) {
        setEffect(new FadeEffect(currentLedColor, false, 0)); // mode 0 for normal fade out
    } else {
        turnOff();
    }
}

// --- Effects Implementation ---

// 1. Fade Effect
FadeEffect::FadeEffect(uint32_t color, bool in, int mode) 
    : targetColor(color), fadeIn(in), ringMode(mode) {
    startTime = millis();
}

bool FadeEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long currentTime = millis();
    float progress = 1.0;
    
    if (config.led_fade_effect && config.fadeDuration > 0) {
        progress = (float)(currentTime - startTime) / config.fadeDuration;
        if (progress > 1.0f) progress = 1.0f;
    }

    strip.clear();
    uint8_t r = (targetColor >> 16) & 0xFF;
    uint8_t g = (targetColor >> 8) & 0xFF;
    uint8_t b = targetColor & 0xFF;
    
    uint8_t r_comp = 255 - r;
    uint8_t g_comp = 255 - g;
    uint8_t b_comp = 255 - b;

    float alpha = fadeIn ? progress : (1.0f - progress);

    uint32_t currentColor = strip.Color((uint8_t)(r * alpha), (uint8_t)(g * alpha), (uint8_t)(b * alpha));
    uint32_t currentCompColor = strip.Color((uint8_t)(r_comp * alpha), (uint8_t)(g_comp * alpha), (uint8_t)(b_comp * alpha));

    for (int i = 0; i < strip.numPixels(); i++) {
        if (ringMode == 1 && (i % 3 == 0)) {
            strip.setPixelColor(i, currentCompColor);
        } else {
            strip.setPixelColor(i, currentColor);
        }
    }
    strip.show();

    return (progress >= 1.0f); // Return true when animation completes
}

// 2. Rainbow Effect
RainbowEffect::RainbowEffect(bool out) : fadeOut(out) {
    startTime = millis();
}

bool RainbowEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long currentTime = millis();
    float brightness = 1.0f;

    if (fadeOut) {
        float progress = (float)(currentTime - startTime) / config.fadeDuration;
        if (progress >= 1.0f) {
            strip.clear();
            strip.show();
            return true; // Finished
        }
        brightness = 1.0f - progress;
    }

    for (int i = 0; i < strip.numPixels(); i++) {
        int pixelHue = (currentTime * 10) + (i * 65536L / strip.numPixels());
        uint32_t c = strip.gamma32(strip.ColorHSV(pixelHue));
        uint8_t r = ((c >> 16) & 0xFF) * brightness * config.led_brightness / 255;
        uint8_t g = ((c >> 8) & 0xFF) * brightness * config.led_brightness / 255;
        uint8_t b = (c & 0xFF) * brightness * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r,g,b));
    }
    strip.show();

    return false; // Spin forever unless fadeOut finishes
}

// 3. Blink Effect
BlinkEffect::BlinkEffect(uint32_t c) : color(c) {
    startTime = millis();
}

bool BlinkEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long currentTime = millis();
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    bool isOn = ((currentTime - startTime) % 1000) < 500;
    if (isOn) {
        uint32_t c = strip.Color(r * config.led_brightness / 255, g * config.led_brightness / 255, b * config.led_brightness / 255);
        strip.fill(c);
    } else {
        strip.clear();
    }
    strip.show();

    return false; // Blink forever until replaced
}


// 4. Pulse Effect
PulseEffect::PulseEffect(uint32_t c) : color(c) { startTime = millis(); }
bool PulseEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    float wave = (sin(elapsed * 2.0f * PI / 3000.0f) + 1.0f) / 2.0f;
    float minBrightness = 0.1f;
    float factor = minBrightness + (1.0f - minBrightness) * wave;
    uint8_t r = ((color >> 16) & 0xFF) * factor * config.led_brightness / 255;
    uint8_t g = ((color >> 8) & 0xFF) * factor * config.led_brightness / 255;
    uint8_t b = (color & 0xFF) * factor * config.led_brightness / 255;
    strip.fill(strip.Color(r, g, b));
    strip.show();
    return false;
}

// 5. Chase Effect
ChaseEffect::ChaseEffect(uint32_t c) : color(c) { startTime = millis(); }
bool ChaseEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    int numLeds = strip.numPixels();
    if (numLeds == 0) return false;
    int head = (elapsed / (1500 / numLeds)) % numLeds;
    uint8_t r_base = (color >> 16) & 0xFF;
    uint8_t g_base = (color >> 8) & 0xFF;
    uint8_t b_base = color & 0xFF;
    for (int i = 0; i < numLeds; i++) {
        int diff = (head - i + numLeds) % numLeds;
        float intensity = 0.0f;
        if (diff == 0) {
            intensity = 1.0f;
        } else if (diff < 5) {
            intensity = 1.0f - (diff * 0.2f);
        }
        uint8_t r = r_base * intensity * config.led_brightness / 255;
        uint8_t g = g_base * intensity * config.led_brightness / 255;
        uint8_t b = b_base * intensity * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    return false;
}

// 6. Sparkle Effect
SparkleEffect::SparkleEffect(uint32_t c) : color(c) { startTime = millis(); }
bool SparkleEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    int numLeds = strip.numPixels();
    uint8_t r_base = (color >> 16) & 0xFF;
    uint8_t g_base = (color >> 8) & 0xFF;
    uint8_t b_base = color & 0xFF;
    strip.clear();
    for (int i = 0; i < numLeds; i++) {
        float wave = (sin((elapsed + i * 733) * 0.007f) + 1.0f) / 2.0f;
        float intensity = 0.0f;
        if (wave > 0.85f) {
            intensity = (wave - 0.85f) / 0.15f;
        }
        uint8_t r = r_base * intensity * config.led_brightness / 255;
        uint8_t g = g_base * intensity * config.led_brightness / 255;
        uint8_t b = b_base * intensity * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    return false;
}

// 7. Aurora Effect
AuroraEffect::AuroraEffect(uint32_t c) : color(c) { startTime = millis(); }
bool AuroraEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    int numLeds = strip.numPixels();
    uint8_t r1 = (color >> 16) & 0xFF;
    uint8_t g1 = (color >> 8) & 0xFF;
    uint8_t b1 = color & 0xFF;
    uint8_t r2 = (r1 + 100) > 255 ? (r1 / 2) : (r1 + 100);
    uint8_t g2 = (g1 + 50) > 255 ? (g1 / 2) : (g1 + 50);
    uint8_t b2 = (b1 + 150) > 255 ? (b1 / 2) : (b1 + 150);
    for (int i = 0; i < numLeds; i++) {
        float wave = (sin(elapsed * 0.0015f + i * 2.0f * PI / numLeds) + 1.0f) / 2.0f;
        uint8_t r = (uint8_t)(r1 * wave + r2 * (1.0f - wave)) * config.led_brightness / 255;
        uint8_t g = (uint8_t)(g1 * wave + g2 * (1.0f - wave)) * config.led_brightness / 255;
        uint8_t b = (uint8_t)(b1 * wave + b2 * (1.0f - wave)) * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    return false;
}

// 8. Heartbeat Effect
HeartbeatEffect::HeartbeatEffect(uint32_t c) : color(c) { startTime = millis(); }
bool HeartbeatEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    unsigned long ms = elapsed % 2000;
    float factor = 0.0f;
    if (ms < 200) {
        factor = (float)ms / 200.0f;
    } else if (ms < 400) {
        factor = 1.0f - (float)(ms - 200) / 200.0f;
    } else if (ms < 600) {
        factor = (float)(ms - 400) / 200.0f;
    } else if (ms < 900) {
        factor = 1.0f - (float)(ms - 600) / 300.0f;
    } else {
        factor = 0.0f;
    }
    factor = 0.05f + factor * 0.95f;
    uint8_t r = ((color >> 16) & 0xFF) * factor * config.led_brightness / 255;
    uint8_t g = ((color >> 8) & 0xFF) * factor * config.led_brightness / 255;
    uint8_t b = (color & 0xFF) * factor * config.led_brightness / 255;
    strip.fill(strip.Color(r, g, b));
    strip.show();
    return false;
}

// 9. Scanner Effect
ScannerEffect::ScannerEffect(uint32_t c) : color(c) { startTime = millis(); }
bool ScannerEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    int numLeds = strip.numPixels();
    if (numLeds <= 1) return false;
    int maxPos = 2 * (numLeds - 1);
    int rawPos = (elapsed / (1600 / maxPos)) % maxPos;
    int head = rawPos < numLeds ? rawPos : maxPos - rawPos;
    uint8_t r_base = (color >> 16) & 0xFF;
    uint8_t g_base = (color >> 8) & 0xFF;
    uint8_t b_base = color & 0xFF;
    for (int i = 0; i < numLeds; i++) {
        float intensity = 0.0f;
        int diff = abs(head - i);
        if (diff == 0) {
            intensity = 1.0f;
        } else if (diff == 1) {
            intensity = 0.5f;
        } else if (diff == 2) {
            intensity = 0.2f;
        }
        uint8_t r = r_base * intensity * config.led_brightness / 255;
        uint8_t g = g_base * intensity * config.led_brightness / 255;
        uint8_t b = b_base * intensity * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    return false;
}

// 10. Radar Effect
RadarEffect::RadarEffect(uint32_t c) : color(c) { startTime = millis(); }
bool RadarEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    int numLeds = strip.numPixels();
    if (numLeds == 0) return false;
    int head = (elapsed / (2000 / numLeds)) % numLeds;
    uint8_t r_base = (color >> 16) & 0xFF;
    uint8_t g_base = (color >> 8) & 0xFF;
    uint8_t b_base = color & 0xFF;
    for (int i = 0; i < numLeds; i++) {
        int diff = (head - i + numLeds) % numLeds;
        float intensity = 1.0f - ((float)diff / numLeds);
        intensity = pow(intensity, 3.0f);
        uint8_t r = r_base * intensity * config.led_brightness / 255;
        uint8_t g = g_base * intensity * config.led_brightness / 255;
        uint8_t b = b_base * intensity * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    return false;
}

// 11. Beacon Effect
BeaconEffect::BeaconEffect(uint32_t c) : color(c) { startTime = millis(); }
bool BeaconEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    unsigned long ms = elapsed % 3000;
    bool isOn = false;
    if (ms < 100) {
        isOn = true;
    } else if (ms < 250) {
        isOn = false;
    } else if (ms < 350) {
        isOn = true;
    } else {
        isOn = false;
    }
    if (isOn) {
        uint8_t r = ((color >> 16) & 0xFF) * config.led_brightness / 255;
        uint8_t g = ((color >> 8) & 0xFF) * config.led_brightness / 255;
        uint8_t b = (color & 0xFF) * config.led_brightness / 255;
        strip.fill(strip.Color(r, g, b));
    } else {
        strip.clear();
    }
    strip.show();
    return false;
}

// 12. Wave Effect
WaveEffect::WaveEffect(uint32_t c) : color(c) { startTime = millis(); }
bool WaveEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    int numLeds = strip.numPixels();
    uint8_t r_base = (color >> 16) & 0xFF;
    uint8_t g_base = (color >> 8) & 0xFF;
    uint8_t b_base = color & 0xFF;
    for (int i = 0; i < numLeds; i++) {
        float angle = (i * 2.0f * PI / numLeds) - (elapsed * 0.004f);
        float factor = (sin(angle) + 1.0f) / 2.0f;
        factor = 0.2f + 0.8f * factor;
        uint8_t r = r_base * factor * config.led_brightness / 255;
        uint8_t g = g_base * factor * config.led_brightness / 255;
        uint8_t b = b_base * factor * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    return false;
}

// 13. Drift Effect
DriftEffect::DriftEffect(uint32_t c) : color(c) { startTime = millis(); }
bool DriftEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    int numLeds = strip.numPixels();
    if (numLeds == 0) return false;
    float center = (elapsed * numLeds) / 10000.0f;
    uint8_t r_base = (color >> 16) & 0xFF;
    uint8_t g_base = (color >> 8) & 0xFF;
    uint8_t b_base = color & 0xFF;
    for (int i = 0; i < numLeds; i++) {
        float dist = abs(i - center);
        while (dist > numLeds / 2.0f) dist = numLeds - dist;
        float intensity = exp(-pow(dist / 1.5f, 2.0f));
        if (intensity < 0.05f) intensity = 0.05f;
        uint8_t r = r_base * intensity * config.led_brightness / 255;
        uint8_t g = g_base * intensity * config.led_brightness / 255;
        uint8_t b = b_base * intensity * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    return false;
}

// 14. Burst Effect
BurstEffect::BurstEffect(uint32_t c) : color(c) { startTime = millis(); }
bool BurstEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long elapsed = millis() - startTime;
    int numLeds = strip.numPixels();
    uint8_t r_base = (color >> 16) & 0xFF;
    uint8_t g_base = (color >> 8) & 0xFF;
    uint8_t b_base = color & 0xFF;
    if (elapsed < 300) {
        strip.clear();
        float p = (float)elapsed / 300.0f;
        uint8_t r = r_base * p * config.led_brightness / 255;
        uint8_t g = g_base * p * config.led_brightness / 255;
        uint8_t b = b_base * p * config.led_brightness / 255;
        if (numLeds > 0) strip.setPixelColor(0, strip.Color(r, g, b));
    } else if (elapsed < 600) {
        float p = (float)(elapsed - 300) / 300.0f;
        for (int i = 0; i < numLeds; i++) {
            float delayVal = (float)i / numLeds;
            float pixelIntensity = 0.0f;
            if (p > delayVal) {
                pixelIntensity = 1.0f - (p - delayVal);
            }
            uint8_t r = r_base * pixelIntensity * config.led_brightness / 255;
            uint8_t g = g_base * pixelIntensity * config.led_brightness / 255;
            uint8_t b = b_base * pixelIntensity * config.led_brightness / 255;
            strip.setPixelColor(i, strip.Color(r, g, b));
        }
    } else {
        float p = (float)(elapsed - 600) / 5400.0f;
        if (p >= 1.0f) {
            strip.clear();
            strip.show();
            return true;
        }
        float factor = 1.0f - p;
        uint8_t r = r_base * factor * config.led_brightness / 255;
        uint8_t g = g_base * factor * config.led_brightness / 255;
        uint8_t b = b_base * factor * config.led_brightness / 255;
        strip.fill(strip.Color(r, g, b));
    }
    strip.show();
    return false;
}