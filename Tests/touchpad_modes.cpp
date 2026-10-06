#include "../Source/XboxTouchpad.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace XboxTouchpad;

void Check(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

void UnitTests() {
    Settings settings; settings.mode = Mode::Relative; settings.curve = Curve::Quadratic;
    State state;
    state.Update(1, 0, 10, settings);
    Check(state.X() == 960 && state.Y() == 471 && state.active, "relative activation at center");
    state.Update(0.3, 0, 0.01, settings);
    Check(std::abs(state.x - 959.5 - 0.4819834710743802) < 1e-10, "quadratic velocity math");
    const double held = state.x;
    for (int i = 0; i < 100; ++i) state.Update(0.1, -0.05, 0.01, settings);
    Check(state.x == held && state.y == 470.5, "deadzone holds last position");
    for (int i = 0; i < 100; ++i) state.Update(0, 0, 0.01, settings);
    Check(state.x == held, "neutral has no drift");
    state.Release(); Check(!state.active, "release contact");
    state.Update(0, 0, 0.01, settings);
    Check(state.x == 959.5 && state.y == 470.5, "repress recenters");
    double distances[4]; const int rates[] = {30, 60, 250, 1000};
    for (int i = 0; i < 4; ++i) {
        State sample; sample.Update(0, 0, 0, settings);
        for (int j = 0; j < rates[i] * 2; ++j) sample.Update(0.3, 0, 1.0 / rates[i], settings);
        distances[i] = sample.x;
        Check(std::abs(sample.x - (959.5 + 96.39669421487604)) < 1e-8, "polling independent velocity");
    }
    state.x = 1000; state.Update(1, 0, 10, settings);
    Check(std::abs(state.x - 1057.6) < 1e-10, "stall dt capped at 50 ms");
    for (int i = 0; i < 200; ++i) state.Update(-1, -1, 0.01, settings);
    Check(state.X() == 0 && state.Y() == 0 && state.active, "top left remains a live touch");
    for (int i = 0; i < 400; ++i) state.Update(1, 1, 0.01, settings);
    Check(state.X() == 1919 && state.Y() == 941, "bounds clamp");
    State fractional; fractional.Update(0, 0, 0, settings);
    for (int i = 0; i < 1000; ++i) fractional.Update(0.15, 0, 0.001, settings);
    Check(fractional.X() > 960, "fractional motion accumulates");
    settings.mode = Mode::Absolute; settings.curve = Curve::Linear;
    settings.sensitivity = 1; settings.smoothing = 0;
    state = {}; state.Update(1, 0, 0.01, settings);
    Check(state.X() == 1919 && state.Y() == 471, "absolute right edge");
    state.Update(0, -1, 0.01, settings);
    Check(state.X() == 960 && state.Y() == 0, "absolute upper edge");
    state.Update(0, 0, 0.01, settings);
    Check(state.X() == 960 && state.Y() == 471, "absolute neutral center");
    state.Update(0.120001, 0, 0.01, settings);
    Check(std::abs(state.x - 959.5) < 0.002, "continuous deadzone transition");
    settings.smoothing = 0.20;
    double smoothEnd = 0;
    for (int rate : rates) {
        State sample;
        for (int i = 0; i < rate; ++i) sample.Update(0.4, 0, 1.0 / rate, settings);
        if (!smoothEnd) smoothEnd = sample.x;
        Check(std::abs(sample.x - smoothEnd) < 1e-9, "time based smoothing");
    }
    settings.sensitivity = std::numeric_limits<double>::quiet_NaN();
    settings.deadzone = 1; settings.smoothing = -1; settings.Validate();
    Check(settings.sensitivity == 0.60 && settings.deadzone == 0.95 && settings.smoothing == 0,
        "invalid config constrained");
    Check(ParseMode("RELATIVE") == Mode::Relative && ParseMode("typo") == Mode::Legacy, "config parsing");
    std::puts("PASS: velocity, polling 30/60/250/1000 Hz, smoothing, deadzone, fractional motion, bounds, release/repress, config.");
}

// Batch simulator reads active, normalized stick-x, stick-y (down positive), seconds.
// The improved modes run the exact header used by the application, not a Python rewrite.
int main(int argc, char** argv) {
    if (argc == 1) { UnitTests(); return 0; }
    Settings settings; settings.mode = ParseMode(argv[1]);
    if (argc > 2) settings.curve = Lower(argv[2]) == "quadratic" ? Curve::Quadratic : Curve::Linear;
    if (argc > 3) settings.sensitivity = std::atof(argv[3]);
    if (argc > 4) settings.deadzone = std::atof(argv[4]);
    if (argc > 5) settings.smoothing = std::atof(argv[5]);
    settings.Validate();
    State state; int active, frame = 0; double x, y, dt;
    std::puts("frame,x,y,wire_x,wire_y,active");
    while (std::scanf("%d,%lf,%lf,%lf", &active, &x, &y, &dt) == 4) {
        const short rawX = static_cast<short>(Limit(x, -1.0, 1.0) * (x < 0 ? 32768 : 32767));
        const short rawY = static_cast<short>(-Limit(y, -1.0, 1.0) * (y > 0 ? 32768 : 32767));
        if (!active) state.Release();
        else if (settings.mode == Mode::Legacy) {
            // Original 2.2 conversion and radial/sign mapping, with InvertX/Y disabled.
            const unsigned char rx = static_cast<unsigned char>((rawX + 32768) / 257);
            unsigned char ry = static_cast<unsigned char>(-(rawY + 32768) / 257);
            if (!ry) ry = 255;
            const double ax = (rx - 128.0) / 128, ay = (ry - 128.0) / 128;
            const double magnitude = Limit(std::sqrt(ax * ax + ay * ay), 0, 1);
            state.x = 960; state.y = 471; state.active = true;
            if (rx > 127) state.x = 200 + std::trunc(1519 * magnitude);
            if (rx < 127) state.x = 1719 - std::trunc(1519 * magnitude);
            if (ry > 129) state.y = 100 + std::trunc(741 * magnitude);
            if (ry < 129) state.y = 743 - std::trunc(741 * magnitude);
        } else state.Update(Normalize(rawX), -Normalize(rawY), dt, settings);
        std::printf("%d,%.12f,%.12f,%u,%u,%d\n", frame++, state.x, state.y, state.X(), state.Y(), state.active);
    }
}
