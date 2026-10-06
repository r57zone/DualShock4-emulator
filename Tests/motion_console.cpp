#include "../Source/XboxMotionConsole.h"
#include <fstream>
#include <cstdlib>
#include <iterator>
using XboxMotion::ConsoleAction;
void check(bool ok, const char* message) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
KEY_EVENT_RECORD key(WORD vk, wchar_t character=0, DWORD modifiers=0, WORD repeat=1, bool down=true) {
    KEY_EVENT_RECORD event{}; event.bKeyDown=down; event.wVirtualKeyCode=vk;
    event.uChar.UnicodeChar=character; event.dwControlKeyState=modifiers; event.wRepeatCount=repeat;
    return event;
}
int main() {
    // Decoder/state tests don't attach to a console or inject keyboard events.
    XboxMotion::ConsoleTuner console(false); XboxMotion::Settings s;
    char temp[MAX_PATH], path[MAX_PATH];
    check(GetTempPathA(MAX_PATH,temp) && GetTempFileNameA(temp,"ds4",0,path),"temporary test config");
    const char* initial="# preserved comment\r\n[Xbox]\r\nMotionStickSpeed=60\r\nMotionStickCurve=quadratic\r\n[Motion]\r\nActivate=0\r\n";
    { std::ofstream file(path,std::ios::binary); file<<initial; }
    check(console.Handle(key(VK_UP),s,path)==ConsoleAction::None && s.speed==60,"arrows ignored outside editor");
    check(console.Handle(key('M',L'm',LEFT_ALT_PRESSED),s,path)==ConsoleAction::None,"Alt combinations ignored");
    check(console.Handle(key(VK_F6),s,path)==ConsoleAction::None,"emulator F6 ignored");
    check(console.Handle(key('M',L'm'),s,path)==ConsoleAction::Opened && console.active,"M opens local editor");
    check(console.Handle(key('M',L'm'),s,path)==ConsoleAction::None && console.active,"held M does not toggle repeatedly");
    console.Handle(key('M',L'm',0,1,false),s,path);
    check(console.Handle(key(VK_RIGHT),s,path)==ConsoleAction::Changed && s.speed==65,"coarse increase");
    check(console.Handle(key(VK_DOWN,0,SHIFT_PRESSED,3),s,path)==ConsoleAction::Changed && s.speed==62,"fine decrease and repeat");
    check(console.Handle(key(VK_ESCAPE),s,path)==ConsoleAction::Cancelled && !console.active && s.speed==60,"cancel restores entry speed");
    { std::ifstream file(path,std::ios::binary); std::string text((std::istreambuf_iterator<char>(file)),{}); check(text==initial,"cancel never writes config"); }
    console.Handle(key('M',L'M'),s,path); console.Handle(key('M',L'M',0,1,false),s,path);
    console.Handle(key(VK_UP,0,0,4),s,path);
    check(console.Handle(key(VK_RETURN),s,path)==ConsoleAction::Saved && !console.active && s.speed==80,"save commits and exits");
    char value[64]; GetPrivateProfileStringA("Xbox","MotionStickSpeed","",value,64,path);
    check(std::atof(value)==80,"saved speed reloads");
    { std::ifstream file(path,std::ios::binary); std::string text((std::istreambuf_iterator<char>(file)),{});
      check(text.find("# preserved comment")!=std::string::npos && text.find("MotionStickCurve=quadratic")!=std::string::npos
          && text.find("Activate=0")!=std::string::npos,"save preserves unrelated keys and comments"); }
    console.Handle(key('M',L'm'),s,path); console.Handle(key('M',L'm',0,1,false),s,path);
    console.Handle(key(VK_DOWN,0,0,500),s,path); check(s.speed==0.1,"lower rate clamp");
    console.Handle(key(VK_UP,0,0,500),s,path); check(s.speed==2000,"upper rate clamp");
    const std::string invalid=std::string(path)+"/missing.ini";
    check(console.Handle(key(VK_RETURN),s,invalid.c_str())==ConsoleAction::SaveFailed && console.active,"failed save keeps editor open");
    check(console.Handle(key('M',L'm'),s,path)==ConsoleAction::Cancelled && s.speed==80,"M cancels and restores runtime entry speed");
    DeleteFileA(path);
    std::puts("PASS: console key decoding, no Alt/F6 actions, repeat, cancel, save/reload, preserved settings, save failure, limits.");
}
