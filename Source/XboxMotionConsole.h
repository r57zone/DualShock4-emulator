#pragma once

#include <Windows.h>
#include <cstdio>
#include "XboxMotion.h"

namespace XboxMotion {
enum class ConsoleAction { None, Opened, Changed, Saved, Cancelled, SaveFailed };

// Reads the console input queue only. No global hotkeys or async keyboard polling.
class ConsoleTuner {
    HANDLE input = INVALID_HANDLE_VALUE;
    DWORD originalMode = 0;
    double entrySpeed = 60.0;
    bool toggleHeld = false;
public:
    bool available = false, active = false;
    explicit ConsoleTuner(bool enabled) {
        if (!enabled) return;
        input = GetStdHandle(STD_INPUT_HANDLE);
        available = GetConsoleMode(input, &originalMode) && SetConsoleMode(input,
            (originalMode | ENABLE_EXTENDED_FLAGS) &
            ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_QUICK_EDIT_MODE));
    }
    ~ConsoleTuner() { if (available) SetConsoleMode(input, originalMode); }
    ConsoleTuner(const ConsoleTuner&) = delete;
    ConsoleTuner& operator=(const ConsoleTuner&) = delete;

    ConsoleAction Handle(const KEY_EVENT_RECORD& key, Settings& settings, const char* configPath) {
        const bool toggle = key.uChar.UnicodeChar == L'm' || key.uChar.UnicodeChar == L'M';
        if (toggle && !key.bKeyDown) { toggleHeld = false; return ConsoleAction::None; }
        if (!key.bKeyDown || (key.dwControlKeyState &
            (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) return ConsoleAction::None;
        if (toggle) { if (toggleHeld) return ConsoleAction::None; toggleHeld = true; }
        if (!active) {
            if (!toggle) return ConsoleAction::None;
            entrySpeed = settings.speed; active = true;
            return ConsoleAction::Opened;
        }
        if (toggle || key.wVirtualKeyCode == VK_ESCAPE) {
            settings.speed = entrySpeed; active = false;
            return ConsoleAction::Cancelled;
        }
        if (key.wVirtualKeyCode == VK_RETURN) {
            const std::string value = std::to_string(settings.speed);
            if (!WritePrivateProfileStringA("Xbox", "MotionStickSpeed", value.c_str(), configPath))
                return ConsoleAction::SaveFailed;
            active = false;
            return ConsoleAction::Saved;
        }
        int direction = key.wVirtualKeyCode == VK_UP || key.wVirtualKeyCode == VK_RIGHT ? 1
            : key.wVirtualKeyCode == VK_DOWN || key.wVirtualKeyCode == VK_LEFT ? -1 : 0;
        if (!direction) return ConsoleAction::None;
        const double step = key.dwControlKeyState & SHIFT_PRESSED ? 1.0 : 5.0;
        const double previous = settings.speed;
        settings.speed += direction * step * (key.wRepeatCount ? key.wRepeatCount : 1);
        settings.Validate();
        return settings.speed == previous ? ConsoleAction::None : ConsoleAction::Changed;
    }

    void Poll(Settings& settings, const char* configPath) {
        if (!available) return;
        DWORD pending = 0;
        if (!GetNumberOfConsoleInputEvents(input, &pending) || !pending) return;
        INPUT_RECORD events[32]; DWORD count = 0;
        if (!ReadConsoleInputW(input, events, pending < 32 ? pending : 32, &count)) return;
        for (DWORD i = 0; i < count; ++i) {
            if (events[i].EventType != KEY_EVENT) continue;
            const auto action = Handle(events[i].Event.KeyEvent, settings, configPath);
            if (action == ConsoleAction::Opened)
                std::printf("\n Motion speed editor: arrows +/-5, Shift+arrows +/-1 deg/s.\n Enter: save and exit. Esc/M: cancel. Controller input paused while editing.\n");
            if (action == ConsoleAction::Opened || action == ConsoleAction::Changed)
                std::printf("\r Motion speed: %7.1f deg/s (not saved)    ", settings.speed);
            if (action == ConsoleAction::Saved)
                std::printf("\n Saved MotionStickSpeed=%.1f to Config.ini. Controls resumed.\n", settings.speed);
            if (action == ConsoleAction::Cancelled)
                std::printf("\n Cancelled. Motion speed restored to %.1f deg/s. Controls resumed.\n", settings.speed);
            if (action == ConsoleAction::SaveFailed)
                std::printf("\n Could not save Config.ini (Windows error %lu). Enter retries; Esc cancels.\n", GetLastError());
            if (action != ConsoleAction::None) std::fflush(stdout);
        }
    }
};
}
