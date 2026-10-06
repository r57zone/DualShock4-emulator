#pragma once

#include <cmath>
#include <string>

// Only the first Xbox stick touch uses these settings. Legacy stays in DS4Emulator.cpp.
namespace XboxTouchpad {
enum class Mode { Legacy, Absolute, Relative };
enum class Curve { Linear, Quadratic };

inline std::string Lower(std::string value) {
    for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}

inline Mode ParseMode(const std::string& value) {
    const std::string mode = Lower(value);
    return mode == "absolute" ? Mode::Absolute : mode == "relative" ? Mode::Relative : Mode::Legacy;
}

inline double Limit(double value, double low, double high) {
    return value < low ? low : value > high ? high : value;
}

struct Settings {
    Mode mode = Mode::Legacy;
    Curve curve = Curve::Linear;
    double sensitivity = 0.60;
    double deadzone = 0.12;
    double smoothing = 0.0; // Absolute mode time constant, seconds; zero disables it.

    void Validate() {
        sensitivity = Limit(std::isfinite(sensitivity) ? sensitivity : 0.60, 0.05, 3.0);
        deadzone = Limit(std::isfinite(deadzone) ? deadzone : 0.12, 0.0, 0.95);
        smoothing = Limit(std::isfinite(smoothing) ? smoothing : 0.0, 0.0, 1.0);
    }
};

inline double Normalize(short value) {
    return value < 0 ? value / 32768.0 : value / 32767.0;
}

// Click suppression has its own threshold; touch coordinates remain unchanged.
inline double ValidateClickDeadzone(double value) {
    return Limit(std::isfinite(value) ? value : 0.12, 0.0, 0.95);
}

inline bool SuppressClick(short stickX, short stickY, unsigned char mappedX, unsigned char mappedY, double deadzone) {
    // Zero explicitly restores the original exact-byte center check.
    if (deadzone <= 0.0) return mappedX != 127 || mappedY != 129;
    const double x = Normalize(stickX), y = Normalize(stickY);
    return x * x + y * y > deadzone * deadzone;
}

struct State {
    bool active = false;
    double x = 959.5, y = 470.5;

    void Release() { active = false; }

    // x/y inputs are normalized, with positive y pointing down the touchpad.
    void Update(double stickX, double stickY, double seconds, const Settings& settings) {
        const bool started = !active;
        if (started) { x = 959.5; y = 470.5; active = true; }
        const double dt = Limit(std::isfinite(seconds) ? seconds : 0.0, 0.0, 0.05);
        const double magnitude = std::sqrt(stickX * stickX + stickY * stickY);
        double dx = 0.0, dy = 0.0;
        if (magnitude > settings.deadzone) {
            double response = (Limit(magnitude, 0.0, 1.0) - settings.deadzone) / (1.0 - settings.deadzone);
            if (settings.curve == Curve::Quadratic) response *= response;
            dx = stickX / magnitude * response;
            dy = stickY / magnitude * response;
        }

        if (settings.mode == Mode::Relative) {
            // Preserve fractional motion; a neutral stick stops immediately, without drift.
            if (!started) {
                x += dx * 1920.0 * settings.sensitivity * dt;
                y += dy * 1920.0 * settings.sensitivity * dt;
            }
        } else {
            const double targetX = Limit(959.5 + dx * 959.5 * settings.sensitivity, 0.0, 1919.0);
            const double targetY = Limit(470.5 + dy * 470.5 * settings.sensitivity, 0.0, 941.0);
            const double alpha = settings.smoothing > 0.0 ? 1.0 - std::exp(-dt / settings.smoothing) : 1.0;
            x += (targetX - x) * alpha;
            y += (targetY - y) * alpha;
        }
        x = Limit(x, 0.0, 1919.0);
        y = Limit(y, 0.0, 941.0);
    }

    unsigned short X() const { return static_cast<unsigned short>(std::lround(x)); }
    unsigned short Y() const { return static_cast<unsigned short>(std::lround(y)); }
};
}
