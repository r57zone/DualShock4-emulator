"""Extract actual Xbox/report blocks and compare against the existing trigger-fix commit.

Generates C++ to compile with MSVC. No replacement implementation of input mapping.
"""
import argparse
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASELINE = "10d934e10d16ec892e82263cc0103f94a7bea7de"


def between(text, start, end):
    a = text.index(start)
    return text[a:text.index(end, a)]


def mapper(source, name):
    mapping = between(source, "// Convert axis from", "\n\t\t\t}\n\t\t}\n\t\t// Mouse and keyboard mode")
    start = "if (!XboxStickTouchActive)" if "if (!XboxStickTouchActive)" in source else "Touch1.IsChanged ="
    packing = between(source, start, "// freePIE gyro")
    motion_start = source.rindex("// Motion shaking")
    motion = source[motion_start:source.index("// Special keys", motion_start)]
    setup = """
Result NAME(XINPUT_STATE myPState, const Config& c, State& state) {
    DS4_REPORT_EX report; DS4_REPORT_INIT_EX(&report);
    if (c.externalMotion) { report.wGyroX = 123; report.wGyroY = -456; report.wGyroZ = 789; }
    auto& Touch1 = state.t1; auto& Touch2 = state.t2;
    Touch1.X = Touch1.Y = Touch2.X = Touch2.Y = 0;
    auto& LastTouch = state.last; auto& LastTouchValid = state.valid;
    auto& TouchPacket = state.packet;
    report.sCurrentTouch.bIsUpTrackingNum1 = uint8_t(0x80 | Touch1.ID);
    report.sCurrentTouch.bIsUpTrackingNum2 = uint8_t(0x80 | Touch2.ID);
    bool InvertX = c.ix, InvertY = c.iy, EnableXboxButton = true;
    bool SwapTriggersShoulders = c.swap, DisableButtonOnMotion = c.disable;
    bool SwapShareTouchPad = c.share, TouchPadPressedWhenSwiping = c.click;
    int KEY_ID_XBOX_ACTIVATE_MULTI = XINPUT_GAMEPAD_BACK;
    int KEY_ID_XBOX_MOTION_SHAKING = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    int KEY_ID_XBOX_MOTION_X_ADD = XINPUT_GAMEPAD_DPAD_UP, KEY_ID_XBOX_MOTION_X_SUB = XINPUT_GAMEPAD_DPAD_DOWN;
    int KEY_ID_XBOX_MOTION_Y_ADD = 0, KEY_ID_XBOX_MOTION_Y_SUB = 0;
    int KEY_ID_XBOX_MOTION_Z_ADD = XINPUT_GAMEPAD_DPAD_LEFT, KEY_ID_XBOX_MOTION_Z_SUB = XINPUT_GAMEPAD_DPAD_RIGHT;
    bool MotionShaking = false, MotionXAdd = false, MotionXSub = false, MotionYAdd = false;
    bool MotionYSub = false, MotionZAdd = false, MotionZSub = false, MotionShakingSwap = false;
    auto& TouchpadStickState = state.stick;
    const auto& TouchpadStickSettings = c.touch;
    const double TouchpadClickDeadzone = c.clickDeadzone;
    const double TouchpadStickDelta = 0.01;
    bool XboxStickTouchActive = false;
    bool XboxStickMotionActive = false;
    XboxMotion::Rate XboxStickMotionRate;
    auto& MotionStickGesture = state.motionGesture;
    auto& MotionMenuButton = state.menuButton;
    bool MotionStickEnabled = c.motionEnabled;
    const int XboxMode = 1; int EmulationMode = 1; bool SocketActivated = c.externalMotion;
    int MotionStickKey = c.motionKey;
    const auto& MotionStickSettings = c.motion;
""".replace("NAME", name)
    return setup + mapping + packing + motion + """
    unsigned motions = MotionShaking | (MotionXAdd << 1) | (MotionXSub << 2) | (MotionYAdd << 3)
        | (MotionYSub << 4) | (MotionZAdd << 5) | (MotionZSub << 6);
    return {report, motions, XboxActivateMotionPressed};
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    old = subprocess.check_output(["git", "show", BASELINE + ":Source/DS4Emulator.cpp"], cwd=ROOT).decode("utf-8-sig").replace("\r\n", "\n")
    new = (ROOT / "Source/DS4Emulator.cpp").read_text(encoding="utf-8-sig")
    trigger_start = "// Allow triggers while"
    assert between(old, trigger_start, "// Touchpad swipes") == between(new, trigger_start, "// Touchpad swipes"), "Trigger fix was modified"
    suppression = "if (XboxActivateMotionPressed) {\n\t\t\t\t\treport.bThumbLX = 128; report.bThumbLY = 128;\n\t\t\t\t\treport.bThumbRX = 128; report.bThumbRY = 128;\n\t\t\t\t}"
    assert suppression in new
    header = (ROOT / "Source/DS4Emulator.h").read_text(encoding="cp1251")
    preamble = """
#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../Source/ViGEm/Common.h"
#include "../../Source/XboxTouchpad.h"
#include "../../Source/XboxMotion.h"
bool IsKeyPressed(int) { return false; }
const int KEY_ID_SHARE = 0;
"""
    preamble = preamble.replace("../../Source/", (ROOT / "Source").as_posix() + "/")
    preamble += between(header, "// XInput headers", "unsigned short Lang")
    preamble += between(header, "struct _TouchData", "_TouchData Touch1;")
    preamble += header[header.index("double StickDeviationPercent"):]
    preamble += """
struct State { _TouchData t1{}, t2{}; DS4_TOUCH last{}; bool valid = false; uint8_t packet = 0; XboxTouchpad::State stick; XboxMotion::Gesture motionGesture; XboxMotion::MenuButton menuButton; };
struct Config { bool swap = false, disable = false, share = false, click = false, ix = false, iy = false; XboxTouchpad::Settings touch; double clickDeadzone = 0.12; bool motionEnabled = false, externalMotion = false; int motionKey = XINPUT_GAMEPAD_RIGHT_SHOULDER; XboxMotion::Settings motion; };
struct Result { DS4_REPORT_EX report; unsigned motions; bool modifier; };
void check(bool ok, const char* message) { if (!ok) { std::fprintf(stderr, "FAIL: %s\\n", message); std::exit(1); } }
"""
    tests = r"""
int main() {
    unsigned long long cases = 0;
    const BYTE values[] = {0,1,127,255};
    for (int flags = 0; flags < 16; ++flags) for (unsigned buttons = 0; buttons < 65536; ++buttons)
        for (BYTE lt : values) for (BYTE rt : values) {
            Config c; c.swap = flags & 1; c.disable = flags & 2; c.share = flags & 4; c.click = flags & 8;
            XINPUT_STATE x{}; x.Gamepad.wButtons = WORD(buttons); x.Gamepad.bLeftTrigger = lt; x.Gamepad.bRightTrigger = rt;
            x.Gamepad.sThumbLX = 18000; x.Gamepad.sThumbLY = -12000; x.Gamepad.sThumbRX = -24000; x.Gamepad.sThumbRY = 32767;
            State a,b; auto old = original(x,c,a), now = updated(x,c,b);
            check(std::memcmp(&old.report,&now.report,sizeof(DS4_REPORT_EX)) == 0 && old.motions == now.motions,
                "legacy entire report byte identical to trigger fix baseline");
            ++cases;
        }
    unsigned long long modern = 0;
    for (auto mode : {XboxTouchpad::Mode::Absolute, XboxTouchpad::Mode::Relative})
    for (int flags = 0; flags < 64; ++flags) for (unsigned buttons = 0; buttons < 65536; ++buttons) {
        Config c; c.swap = flags & 1; c.disable = flags & 2; c.share = flags & 4; c.click = flags & 8;
        c.ix = flags & 16; c.iy = flags & 32; c.touch.mode = mode; c.touch.curve = XboxTouchpad::Curve::Quadratic;
        XINPUT_STATE x{}; x.Gamepad.wButtons = WORD(buttons); x.Gamepad.bLeftTrigger = 89; x.Gamepad.bRightTrigger = 207;
        x.Gamepad.sThumbLX = 12000; x.Gamepad.sThumbRX = -24000; x.Gamepad.sThumbRY = 32767;
        State a,b; auto old = original(x,c,a), now = updated(x,c,b);
        check(old.motions == now.motions && old.modifier == now.modifier, "motion activation unchanged");
        check(old.report.wButtons == now.report.wButtons && old.report.bSpecial == now.report.bSpecial
            && old.report.bTriggerL == now.report.bTriggerL && old.report.bTriggerR == now.report.bTriggerR,
            "all buttons and triggers unchanged in modern modes");
        check(old.report.bThumbLX == now.report.bThumbLX && old.report.bThumbLY == now.report.bThumbLY
            && old.report.bThumbRX == now.report.bThumbRX && old.report.bThumbRY == now.report.bThumbRY,
            "normal and suppressed sticks unchanged");
        check(std::memcmp(&old.report.wGyroX,&now.report.wGyroX,12) == 0, "gyro and accel unchanged");
        check(std::memcmp(&old.report.sCurrentTouch.bIsUpTrackingNum2,&now.report.sCurrentTouch.bIsUpTrackingNum2,4) == 0,
            "left stick second touch unchanged");
        ++modern;
    }
    Config c; c.touch.mode = XboxTouchpad::Mode::Relative; c.touch.curve = XboxTouchpad::Curve::Quadratic;
    XINPUT_STATE x{}; x.Gamepad.wButtons = XINPUT_GAMEPAD_BACK; x.Gamepad.bRightTrigger = 200;
    x.Gamepad.bLeftTrigger = 77; x.Gamepad.sThumbRX = -32768; x.Gamepad.sThumbRY = 32767;
    State state;
    for (int i = 0; i < 300; ++i) {
        auto now = updated(x,c,state);
        check(now.report.bTriggerL == 77 && now.report.bTriggerR == 200, "held LT/RT during relative motion");
        check((now.report.wButtons & (DS4_BUTTON_TRIGGER_LEFT|DS4_BUTTON_TRIGGER_RIGHT))
            == (DS4_BUTTON_TRIGGER_LEFT|DS4_BUTTON_TRIGGER_RIGHT), "held trigger flags");
        check(!(now.report.sCurrentTouch.bIsUpTrackingNum1 & 0x80), "continuous touch down");
        check(state.t1.ID == 1, "stable tracking id");
    }
    check(state.t1.X == 0 && state.t1.Y == 0 && state.t1.IsChanged, "(0,0) contact stays down");
    x.Gamepad.sThumbRX = x.Gamepad.sThumbRY = 0;
    auto held = updated(x,c,state);
    check(state.t1.X == 0 && state.t1.Y == 0, "neutral preserves top left");
    x.Gamepad.wButtons = 0; auto released = updated(x,c,state);
    check((released.report.sCurrentTouch.bIsUpTrackingNum1 & 0x80) && !state.stick.active, "finger up on modifier release");
    x.Gamepad.wButtons = XINPUT_GAMEPAD_BACK; auto restarted = updated(x,c,state);
    check(state.t1.X == 960 && state.t1.Y == 471 && state.t1.ID == 2, "repress starts new center contact");
    c.touch.mode = XboxTouchpad::Mode::Absolute;
    x.Gamepad.sThumbRX = 16000; x.Gamepad.sThumbRY = 8000;
    State direction; auto direct = updated(x,c,direction);
    check(direction.t1.X > 960 && direction.t1.Y < 471, "normal absolute X/Y direction");
    c.ix = c.iy = true; direction = {}; auto inverted = updated(x,c,direction);
    check(direction.t1.X < 960 && direction.t1.Y > 471, "existing InvertX/Y settings honored");

    // Raw drift measured on the physical Xbox must not swallow a plain Back press.
    for (auto mode : {XboxTouchpad::Mode::Legacy, XboxTouchpad::Mode::Absolute, XboxTouchpad::Mode::Relative})
    for (bool invert : {false,true}) for (bool swap : {false,true}) {
        Config plainBack; plainBack.touch.mode = mode; plainBack.ix=plainBack.iy=invert;
        plainBack.swap=swap; plainBack.motionEnabled=true; plainBack.motionKey=XINPUT_GAMEPAD_START;
        XINPUT_STATE drift{}; drift.Gamepad.wButtons=XINPUT_GAMEPAD_BACK;
        drift.Gamepad.sThumbRX=-73; drift.Gamepad.sThumbRY=-1238;
        drift.Gamepad.bLeftTrigger=77; drift.Gamepad.bRightTrigger=215;
        State safeState, originalState; Config exact=plainBack; exact.clickDeadzone=0;
        for (int i=0; i<20; ++i) {
            auto safe=updated(drift,plainBack,safeState), oldClick=updated(drift,exact,originalState);
            check(safe.report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD,"plain Back remains a touchpad click under measured stick drift");
            check(!(oldClick.report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD),"zero click deadzone reproduces swallowed Back click");
            check(!std::memcmp(&safe.report.sCurrentTouch,&oldClick.report.sCurrentTouch,sizeof(DS4_TOUCH))
                && !std::memcmp(safe.report.sPreviousTouch,oldClick.report.sPreviousTouch,sizeof(safe.report.sPreviousTouch)),
                "click deadzone does not change touch coordinates, tracking or history in any mode");
            check(safe.report.bTriggerL==(swap?0:77) && safe.report.bTriggerR==(swap?0:215),"plain Back keeps existing trigger and swap mapping");
        }
        drift.Gamepad.sThumbRY=32767;
        auto swipe=updated(drift,plainBack,safeState);
        check(!(swipe.report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD)
            && !(swipe.report.sCurrentTouch.bIsUpTrackingNum1 & 128),"meaningful swipe sends touch without click");
        check(swipe.report.bThumbRX==128 && swipe.report.bThumbRY==128,"Back swipe keeps normal sticks suppressed");
        drift.Gamepad.wButtons|=XINPUT_GAMEPAD_RIGHT_THUMB;
        check(updated(drift,plainBack,safeState).report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD,"right-stick click during swipe is preserved");
        drift.Gamepad.wButtons=XINPUT_GAMEPAD_BACK; plainBack.click=true;
        check(updated(drift,plainBack,safeState).report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD,"click-while-swiping option is preserved");
        drift.Gamepad.wButtons=0;
        auto up=updated(drift,plainBack,safeState);
        check(!(up.report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD)
            && (up.report.sCurrentTouch.bIsUpTrackingNum1 & 128)
            && (up.report.sCurrentTouch.bIsUpTrackingNum2 & 128),"Back release lifts both fingers and releases click");
    }
    check(XboxTouchpad::ValidateClickDeadzone(NAN)==0.12 && XboxTouchpad::ValidateClickDeadzone(2)==0.95
        && XboxTouchpad::ValidateClickDeadzone(-1)==0,"click deadzone config is validated");
    check(!XboxTouchpad::SuppressClick(3932,0,142,129,0.12)
        && XboxTouchpad::SuppressClick(3933,0,142,129,0.12),"radial click threshold boundary");
    Config compatibility; compatibility.clickDeadzone=0;
    for (SHORT rx : {SHORT(-32768),SHORT(-1238),SHORT(0),SHORT(1000),SHORT(32767)})
    for (SHORT ry : {SHORT(-32768),SHORT(-1238),SHORT(0),SHORT(1000),SHORT(32767)}) {
        State originalState, newState; XINPUT_STATE heldBack{};
        heldBack.Gamepad.wButtons=XINPUT_GAMEPAD_BACK; heldBack.Gamepad.sThumbRX=rx; heldBack.Gamepad.sThumbRY=ry;
        auto previous=original(heldBack,compatibility,originalState), current=updated(heldBack,compatibility,newState);
        check(!std::memcmp(&previous.report,&current.report,sizeof(DS4_REPORT_EX)),"zero click deadzone retains exact original full report");
    }
    std::puts("PASS: Back click under measured drift, unchanged swipe packets, thresholds, legacy compatibility and Back release.");

    unsigned long long analog = 0;
    for (int key : {XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_THUMB, XINPUT_GAMEPAD_START, 0, XINPUT_GAMEPAD_BACK})
    for (int flags = 0; flags < 16; ++flags) for (unsigned buttons = 0; buttons < 65536; ++buttons) {
        Config cfg; cfg.motionEnabled = true; cfg.motionKey = key;
        cfg.swap = flags & 1; cfg.disable = flags & 2; cfg.share = flags & 4; cfg.click = flags & 8;
        XINPUT_STATE input{}; input.Gamepad.wButtons = WORD(buttons);
        input.Gamepad.bLeftTrigger = 77; input.Gamepad.bRightTrigger = 200;
        input.Gamepad.sThumbLX = 12000; input.Gamepad.sThumbLY = -9000;
        input.Gamepad.sThumbRX = 32767; input.Gamepad.sThumbRY = 0;
        State before, after;
        auto actual = updated(input,cfg,after);
        bool eligible = key != 0 && (buttons & key) && !(buttons & XINPUT_GAMEPAD_BACK)
            && !(cfg.swap && key == XINPUT_GAMEPAD_RIGHT_SHOULDER);
        if (eligible) input.Gamepad.wButtons &= ~key;
        auto expected = original(input,cfg,before);
        if (eligible) {
            expected.report.bThumbRX = expected.report.bThumbRY = 128;
            expected.report.wGyroX = 1; expected.report.wGyroY = -960; expected.report.wGyroZ = 0;
            expected.report.wAccelX = 0; expected.report.wAccelY = 8192; expected.report.wAccelZ = 0;
        }
        if (!eligible && !expected.motions) expected.report.wGyroX = 1;
        check(std::memcmp(&expected.report,&actual.report,sizeof(DS4_REPORT_EX)) == 0,
            "analog routing, reserved key, right-stick suppression, triggers, Back priority and swap compatibility");
        ++analog;
    }
    Config motion; motion.motionEnabled = true;
    XINPUT_STATE plain{}; plain.Gamepad.wButtons = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    plain.Gamepad.sThumbRX = 1000; plain.Gamepad.sThumbRY = -900;
    plain.Gamepad.bLeftTrigger = 77; plain.Gamepad.bRightTrigger = 231;
    State plainState;
    for (int i=0; i<30; ++i) {
        auto r = updated(plain,motion,plainState);
        check((r.report.wButtons & DS4_BUTTON_SHOULDER_RIGHT) && !plainState.motionGesture.active,
            "plain RB and sub-deadzone drift retain R1 before a motion gesture");
        check(r.report.bTriggerL == 77 && r.report.bTriggerR == 231, "plain RB retains triggers");
        check(r.report.wGyroX == 1 && r.report.wGyroY == 0 && r.report.wGyroZ == 0,
            "plain RB retains calibrated neutral gyro");
    }
    plain.Gamepad.sThumbRX = 6553; plain.Gamepad.sThumbRY = 0;
    auto beginGesture = updated(plain,motion,plainState);
    check(!(beginGesture.report.wButtons & DS4_BUTTON_SHOULDER_RIGHT) && plainState.motionGesture.active
        && beginGesture.report.bThumbRX == 128 && beginGesture.report.bThumbRY == 128
        && beginGesture.report.wGyroY == -8 && beginGesture.report.bTriggerR == 231,
        "movement switches a plain held RB from R1 to gyro while preserving RT");
    plain.Gamepad.sThumbRX = 0;
    auto centerGesture = updated(plain,motion,plainState);
    check(!(centerGesture.report.wButtons & DS4_BUTTON_SHOULDER_RIGHT) && centerGesture.report.wGyroX == 1
        && centerGesture.report.wGyroY == 0 && centerGesture.report.bThumbRX == 128,
        "centering after a plain RB to motion transition keeps R1 released and gyro neutral");
    plain.Gamepad.wButtons = 0;
    check(!(updated(plain,motion,plainState).report.wButtons & DS4_BUTTON_SHOULDER_RIGHT), "plain RB release releases R1");
    XINPUT_STATE heldMotion{}; heldMotion.Gamepad.wButtons = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    heldMotion.Gamepad.bRightTrigger = 231;
    State contact;
    for (int i=0; i<20; ++i) {
        heldMotion.Gamepad.sThumbRX = i % 2 ? 32767 : -32768;
        auto r = updated(heldMotion,motion,contact);
        check(r.report.bTriggerR == 231 && (r.report.wButtons & DS4_BUTTON_TRIGGER_RIGHT), "RT stays active during analog gyro");
        check(r.report.wGyroY == (i % 2 ? -960 : 960), "analog horizontal sign and repeated samples");
        check(r.report.sCurrentTouch.bIsUpTrackingNum1 & 0x80, "motion does not create a touch");
    }
    heldMotion.Gamepad.sThumbRX = 0; auto stopped = updated(heldMotion,motion,contact);
    check(!(stopped.report.wButtons & DS4_BUTTON_SHOULDER_RIGHT) && contact.motionGesture.active,
        "centering during motion does not cause an unintended R1 press");
    check(stopped.report.wGyroX == 1 && stopped.report.wGyroY == 0 && stopped.report.wGyroZ == 0,
        "neutral stops calibrated gyro immediately");
    heldMotion.Gamepad.wButtons = 0; heldMotion.Gamepad.sThumbRX = 32767;
    auto normal = updated(heldMotion,motion,contact);
    check(normal.report.bThumbRX == 255 && normal.report.wAccelY == 0 && normal.report.wGyroX == 1,
        "key release restores normal stick and acceleration, keeping calibrated neutral pitch");
    for (int i=0; i<1000; ++i) {
        auto idle = updated(heldMotion,motion,contact);
        check(idle.report.wGyroX == 1 && idle.report.wGyroY == 0 && idle.report.wGyroZ == 0,
            "released modifier stays at calibrated gyro zero even with stick deflection");
    }
    heldMotion.Gamepad.sThumbRX = 0; heldMotion.Gamepad.wButtons = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    check(updated(heldMotion,motion,contact).report.wButtons & DS4_BUTTON_SHOULDER_RIGHT,
        "plain RB works again after a motion gesture is released");
    heldMotion.Gamepad.sThumbRX = 32767;
    updated(heldMotion,motion,contact);
    heldMotion.Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;
    auto backShake = updated(heldMotion,motion,contact);
    check(backShake.motions & 1, "Back plus RB still shakes after analog motion");
    check(!contact.motionGesture.active, "Back priority clears analog gesture");
    heldMotion.Gamepad.wButtons = 0;
    motion.externalMotion = true;
    auto external = updated(heldMotion,motion,contact);
    check(external.report.wGyroX == 123 && external.report.wGyroY == -456 && external.report.wGyroZ == 789, "external IMU path is not offset or replaced");
    Config menu; menu.motionEnabled = true; menu.motionKey = XINPUT_GAMEPAD_START;
    XINPUT_STATE menuInput{}; State menuState;
    menuInput.Gamepad.wButtons = XINPUT_GAMEPAD_START;
    for (int i=0; i<20; ++i)
        check(!(updated(menuInput,menu,menuState).report.wButtons & DS4_BUTTON_OPTIONS), "held Start does not open menu before deciding gesture");
    menuInput.Gamepad.wButtons = 0;
    for (int i=0; i<5; ++i)
        check(updated(menuInput,menu,menuState).report.wButtons & DS4_BUTTON_OPTIONS, "plain Start release sends polling-visible Options pulse");
    for (int i=0; i<10; ++i)
        check(!(updated(menuInput,menu,menuState).report.wButtons & DS4_BUTTON_OPTIONS), "Options pulse ends without repeat");
    menuInput.Gamepad.wButtons = XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_RIGHT_SHOULDER;
    menuInput.Gamepad.bRightTrigger = 215;
    updated(menuInput,menu,menuState);
    menuInput.Gamepad.sThumbRX = 6553;
    auto menuMotion = updated(menuInput,menu,menuState);
    check(!(menuMotion.report.wButtons & DS4_BUTTON_OPTIONS) && (menuMotion.report.wButtons & DS4_BUTTON_SHOULDER_RIGHT)
        && menuMotion.report.bTriggerR == 215 && menuMotion.report.wGyroY == -8 && menuMotion.report.bThumbRX == 128,
        "Start motion reserves Options while RB and RT retain normal functions");
    menuInput.Gamepad.sThumbRX = 0;
    check(!(updated(menuInput,menu,menuState).report.wButtons & DS4_BUTTON_OPTIONS), "centered Start gesture does not open menu");
    menuInput.Gamepad.wButtons = 0;
    for (int i=0; i<10; ++i)
        check(!(updated(menuInput,menu,menuState).report.wButtons & DS4_BUTTON_OPTIONS), "motion release never sends Options");
    menuInput.Gamepad.wButtons = XINPUT_GAMEPAD_START;
    updated(menuInput,menu,menuState);
    menuInput.Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;
    auto share = updated(menuInput,menu,menuState);
    check((share.report.wButtons & DS4_BUTTON_SHARE) && !(share.report.wButtons & DS4_BUTTON_OPTIONS), "Back plus Start Share keeps priority");
    menuInput.Gamepad.wButtons = 0;
    check(!(updated(menuInput,menu,menuState).report.wButtons & DS4_BUTTON_OPTIONS), "Share release does not send Options");
    menuInput.Gamepad.wButtons = XINPUT_GAMEPAD_BACK;
    auto backClick = updated(menuInput,menu,menuState);
    check(backClick.report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD, "plain Back touchpad click unchanged with centered stick");
    menuInput.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    check(updated(menuInput,menu,menuState).motions & (1 << 6), "Back plus D-pad rotation unchanged with Start motion key");
    menuInput.Gamepad.wButtons = XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_RIGHT_SHOULDER;
    check(updated(menuInput,menu,menuState).motions & 1, "Back plus RB shake unchanged with Start motion key");
    menu.motionEnabled = false; menuInput.Gamepad.wButtons = XINPUT_GAMEPAD_START;
    check(updated(menuInput,menu,menuState).report.wButtons & DS4_BUTTON_OPTIONS, "disabled motion keeps immediate held Options");
    for (bool swapped : {false,true}) for (bool startFirst : {false,true}) for (bool backReleasedFirst : {false,true}) {
        Config chord; chord.motionEnabled = true; chord.motionKey = XINPUT_GAMEPAD_START; chord.share = swapped;
        State chordState; XINPUT_STATE chordInput{};
        chordInput.Gamepad.wButtons = startFirst ? XINPUT_GAMEPAD_START : XINPUT_GAMEPAD_BACK;
        updated(chordInput,chord,chordState);
        chordInput.Gamepad.wButtons = XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_START;
        auto combined = updated(chordInput,chord,chordState);
        check(!(combined.report.wButtons & DS4_BUTTON_OPTIONS), "Back/Start chord does not send Options");
        check(swapped ? (combined.report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD) != 0
            : (combined.report.wButtons & DS4_BUTTON_SHARE) != 0, "Back/Start Share or swapped touchpad click retained");
        chordInput.Gamepad.wButtons = backReleasedFirst ? XINPUT_GAMEPAD_START : XINPUT_GAMEPAD_BACK;
        for (int i=0; i<20; ++i)
            check(!(updated(chordInput,chord,chordState).report.wButtons & DS4_BUTTON_OPTIONS),
                "partial chord release must not rearm a plain Start tap");
        chordInput.Gamepad.wButtons = 0;
        for (int i=0; i<20; ++i)
            check(!(updated(chordInput,chord,chordState).report.wButtons & DS4_BUTTON_OPTIONS),
                "either Back/Start release order must not open menu");
        chordInput.Gamepad.wButtons = XINPUT_GAMEPAD_START;
        updated(chordInput,chord,chordState);
        chordInput.Gamepad.wButtons = 0;
        check(updated(chordInput,chord,chordState).report.wButtons & DS4_BUTTON_OPTIONS,
            "a fresh plain Start tap works after the entire chord is released");
    }
    for (bool backDuringMotion : {false,true}) for (bool centerFirst : {false,true}) {
        Config gyro; gyro.motionEnabled = true; gyro.motionKey = XINPUT_GAMEPAD_START;
        State gyroState; XINPUT_STATE gyroInput{};
        gyroInput.Gamepad.bLeftTrigger = 77; gyroInput.Gamepad.bRightTrigger = 215;
        gyroInput.Gamepad.wButtons = XINPUT_GAMEPAD_START;
        updated(gyroInput,gyro,gyroState);
        gyroInput.Gamepad.sThumbRX = 6553;
        for (int i=0; i<10; ++i) {
            auto drawing = updated(gyroInput,gyro,gyroState);
            check(!(drawing.report.wButtons & DS4_BUTTON_OPTIONS) && drawing.report.bTriggerL == 77
                && drawing.report.bTriggerR == 215, "gyro gesture never sends Options while preserving both triggers");
        }
        if (backDuringMotion) {
            gyroInput.Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;
            updated(gyroInput,gyro,gyroState);
            gyroInput.Gamepad.sThumbRX = 0;
            gyroInput.Gamepad.wButtons = XINPUT_GAMEPAD_START;
            for (int i=0; i<10; ++i)
                check(!(updated(gyroInput,gyro,gyroState).report.wButtons & DS4_BUTTON_OPTIONS),
                    "Back chord during gyro does not rearm centered Start");
        }
        if (centerFirst) {
            gyroInput.Gamepad.sThumbRX = 0;
            updated(gyroInput,gyro,gyroState);
        }
        gyroInput.Gamepad.wButtons = 0;
        for (int i=0; i<20; ++i)
            check(!(updated(gyroInput,gyro,gyroState).report.wButtons & DS4_BUTTON_OPTIONS),
                "gyro release with centered or deflected stick never opens menu, including Back interruption");
    }
    std::puts("PASS: Back/Start chord press/release orders, Share/touchpad swap, gyro/chord interruptions and fresh menu taps.");
    std::puts("PASS: Start tap/menu pulse, analog motion without Options, normal RB/RT, Back touch/rotation/shake and Share.");
    std::printf("PASS: %llu enabled analog full-report comparisons, plain RB, gesture latch, RT, Back, touch-up, neutral and release.\n", analog);
    std::printf("PASS: %llu legacy full reports, %llu modern button/stick/motion comparisons; corner, release, tracking, LT/RT.\n", cases,modern);
}
"""
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "xbox_report_regression.cpp").write_text(preamble + mapper(old, "original") + mapper(new, "updated") + tests, encoding="utf-8-sig")
    print("PASS: trigger-fix block and stick-suppression block unchanged; generated real-source report regression.")


if __name__ == "__main__":
    main()
