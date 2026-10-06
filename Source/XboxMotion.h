#pragma once

#include "XboxTouchpad.h"

// Angular velocity, not a per-frame angle. The receiving application integrates it.
namespace XboxMotion {
// ViGEm's DS4 feature report 0x02 advertises a +1 raw pitch zero offset.
// SDL subtracts it during calibration: raw (1,0,0), not (0,0,0), means no rotation.
constexpr short GyroPitchZero = 1;
enum class Axis { X, Y, Z };
inline Axis ParseAxis(const std::string& value, Axis fallback) {
    const std::string axis = XboxTouchpad::Lower(value);
    return axis == "x" ? Axis::X : axis == "y" ? Axis::Y : axis == "z" ? Axis::Z : fallback;
}
struct Settings {
    double speed = 60.0; // degrees/second at full deflection
    double deadzone = 0.12;
    bool quadratic = true;
    Axis horizontal = Axis::Y, vertical = Axis::X;
    bool invertHorizontal = true, invertVertical = false;
    void Validate() {
        speed = XboxTouchpad::Limit(std::isfinite(speed) ? speed : 60.0, 0.1, 2000.0);
        deadzone = XboxTouchpad::Limit(std::isfinite(deadzone) ? deadzone : 0.12, 0.0, 0.95);
        if (horizontal == vertical) vertical = horizontal == Axis::X ? Axis::Y : Axis::X;
    }
};
struct Rate {
    short gyro[3] = {0, 0, 0};
    template<class Report> void Apply(Report& report) const {
        report.wGyroX = static_cast<short>(XboxTouchpad::Limit(gyro[0] + GyroPitchZero, -32767, 32767));
        report.wGyroY = gyro[1]; report.wGyroZ = gyro[2];
        // Same stationary reference as shadPS4's mouse gyro: +1g on Y.
        report.wAccelX = 0; report.wAccelY = 8192; report.wAccelZ = 0;
    }
};
struct Gesture {
    bool active = false;
    bool Update(bool held, short rightX, short rightY, const Settings& settings) {
        if (!held) active = false;
        else if (std::hypot(XboxTouchpad::Normalize(rightX), XboxTouchpad::Normalize(rightY)) > settings.deadzone)
            active = true;
        // Once drawing starts, keep reserving the key until release. Returning
        // to center must stop gyro without unexpectedly pressing R1 in the game.
        return active;
    }
};
struct MenuButton {
    bool held = false, consumed = false;
    double pulse = 0.0;
    bool Update(bool allowed, bool pressed, bool motionActive, double seconds) {
        if (!allowed) {
            // A chord owns the whole physical press. Releasing Back first must
            // not turn the still-held Start into a new pending menu tap.
            held = pressed; consumed = pressed; pulse = 0.0;
            return false;
        }
        pulse = std::fmax(0.0, pulse - XboxTouchpad::Limit(seconds, 0.0, 0.05));
        if (pressed) {
            if (!held) { consumed = false; pulse = 0.0; }
            held = true;
            consumed = consumed || motionActive;
            return false;
        }
        if (held) {
            if (!consumed) pulse = 0.05; // Remain visible across host polling intervals.
            held = consumed = false;
        }
        return pulse > 0.0;
    }
};
inline Rate Calculate(short rightX, short rightY, const Settings& settings) {
    const double x = XboxTouchpad::Normalize(rightX), y = XboxTouchpad::Normalize(rightY);
    const double magnitude = std::sqrt(x * x + y * y);
    Rate rate;
    if (magnitude > settings.deadzone) {
        double response = (XboxTouchpad::Limit(magnitude, 0.0, 1.0) - settings.deadzone) / (1.0 - settings.deadzone);
        if (settings.quadratic) response *= response;
        // DS4 raw gyro scale: 16 counts per degree/second (SDL HIDAPI fallback).
        const double scale = response * settings.speed * 16.0 / magnitude;
        rate.gyro[static_cast<int>(settings.horizontal)] = static_cast<short>(std::lround(x * scale * (settings.invertHorizontal ? -1 : 1)));
        rate.gyro[static_cast<int>(settings.vertical)] = static_cast<short>(std::lround(y * scale * (settings.invertVertical ? -1 : 1)));
    }
    return rate;
}
}
