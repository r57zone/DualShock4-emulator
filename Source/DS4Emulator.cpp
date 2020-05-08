#include <Windows.h>
#include <ViGEm/Client.h>
#include <mutex>
#include <algorithm>
#include "IniReader\IniReader.h"

//XInput headers
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y				0x8000

#define BATTERY_TYPE_DISCONNECTED		0x00

#define XUSER_MAX_COUNT                 4
#define XUSER_INDEX_ANY					0x000000FF

#define ERROR_DEVICE_NOT_CONNECTED		1167
#define ERROR_SUCCESS					0

//XInput structures
typedef struct _XINPUT_GAMEPAD
{
	WORD                                wButtons;
	BYTE                                bLeftTrigger;
	BYTE                                bRightTrigger;
	SHORT                               sThumbLX;
	SHORT                               sThumbLY;
	SHORT                               sThumbRX;
	SHORT                               sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
	DWORD                               dwPacketNumber;
	XINPUT_GAMEPAD                      Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION
{
	WORD                                wLeftMotorSpeed;
	WORD                                wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;

typedef struct _XINPUT_CAPABILITIES
{
	BYTE                                Type;
	BYTE                                SubType;
	WORD                                Flags;
	XINPUT_GAMEPAD                      Gamepad;
	XINPUT_VIBRATION                    Vibration;
} XINPUT_CAPABILITIES, *PXINPUT_CAPABILITIES;

typedef struct _XINPUT_BATTERY_INFORMATION
{
	BYTE BatteryType;
	BYTE BatteryLevel;
} XINPUT_BATTERY_INFORMATION, *PXINPUT_BATTERY_INFORMATION;

typedef struct _XINPUT_KEYSTROKE
{
	WORD    VirtualKey;
	WCHAR   Unicode;
	WORD    Flags;
	BYTE    UserIndex;
	BYTE    HidCode;
} XINPUT_KEYSTROKE, *PXINPUT_KEYSTROKE;

typedef DWORD(__stdcall *_XInputGetState)(_In_ DWORD dwUserIndex, _Out_ XINPUT_STATE *pState);
typedef DWORD(__stdcall *_XInputSetState)(_In_ DWORD dwUserIndex, _In_ XINPUT_VIBRATION *pVibration);

_XInputGetState MyXInputGetState;
_XInputSetState MyXInputSetState;
_XINPUT_STATE myPState;
HMODULE hDll;
DWORD XboxUserIndex = 0;

static std::mutex m;

//Key bindings
int KEY_ID_LEFT_TRIGGER;
int KEY_ID_RIGHT_TRIGGER;
int KEY_ID_LEFT_SHOULDER;
int KEY_ID_RIGHT_SHOULDER;
int KEY_ID_DPAD_UP;
int KEY_ID_DPAD_DOWN;
int KEY_ID_DPAD_LEFT;
int KEY_ID_DPAD_RIGHT;
int KEY_ID_LEFT_THUMB;
int KEY_ID_RIGHT_THUMB;
int KEY_ID_TRIANGLE;
int KEY_ID_SQUARE;
int KEY_ID_CIRCLE;
int KEY_ID_CROSS;
int KEY_ID_TOUCHPAD;
int KEY_ID_OPTIONS;
int KEY_ID_SHARE;

int m_HalfWidth = 1920 / 2;
int m_HalfHeight = 1080 / 2;
float mouseSensetiveY;
float mouseSensetiveX;
bool firstCP = true;
int mouseX, mouseY;
HWND PSNowWindow = 0;

VOID CALLBACK notification(
	PVIGEM_CLIENT Client,
	PVIGEM_TARGET Target,
	UCHAR LargeMotor,
	UCHAR SmallMotor,
	DS4_LIGHTBAR_COLOR LightbarColor,
	LPVOID UserData)
{
    m.lock();

	XINPUT_VIBRATION myVibration;
	myVibration.wLeftMotorSpeed = LargeMotor * 257;
	myVibration.wRightMotorSpeed = SmallMotor * 257;
	MyXInputSetState(XboxUserIndex, &myVibration);

    m.unlock();
}

void GetMouseState()
{
	POINT mousePos;

	if (firstCP) {
		SetCursorPos(m_HalfWidth, m_HalfHeight);
		firstCP = false;
	}

	GetCursorPos(&mousePos);

	mouseX = round((mousePos.x - m_HalfWidth) * mouseSensetiveX);
	mouseY = round((mousePos.y - m_HalfHeight) * mouseSensetiveY);

	SetCursorPos(m_HalfWidth, m_HalfHeight);
}

int main(int argc, char **argv)
{
	SetConsoleTitle("DS4Emulator");
	
	#define XboxMode 0
	#define KBMode 1
	unsigned char EmulationMode = XboxMode;

	//Config
	CIniReader IniFile("Config.ini");
	bool SwapTriggersShoulders = IniFile.ReadBoolean("Xbox", "SwapTriggersShoulders", false);

	mouseSensetiveX = IniFile.ReadFloat("Mouse", "SensX", 20);
	mouseSensetiveY = IniFile.ReadFloat("Mouse", "SensY", 20);

	KEY_ID_LEFT_TRIGGER = IniFile.ReadInteger("Keys", "LEFT_TRIGGER", VK_RBUTTON);
	KEY_ID_RIGHT_TRIGGER = IniFile.ReadInteger("Keys", "RIGHT_TRIGGER", VK_LBUTTON);
	KEY_ID_LEFT_SHOULDER = IniFile.ReadInteger("Keys", "LEFT_SHOULDER", VK_CONTROL);
	KEY_ID_RIGHT_SHOULDER = IniFile.ReadInteger("Keys", "RIGHT_SHOULDER", VK_MENU);
	KEY_ID_DPAD_UP = IniFile.ReadInteger("Keys", "DPAD_UP", '1');
	KEY_ID_DPAD_LEFT = IniFile.ReadInteger("Keys", "DPAD_LEFT", '2');
	KEY_ID_DPAD_RIGHT = IniFile.ReadInteger("Keys", "DPAD_RIGHT", '3'); 
	KEY_ID_DPAD_DOWN = IniFile.ReadInteger("Keys", "DPAD_DOWN", '4'); 
	KEY_ID_LEFT_THUMB = IniFile.ReadInteger("Keys", "LEFT_THUMB", VK_LSHIFT);
	KEY_ID_RIGHT_THUMB = IniFile.ReadInteger("Keys", "RIGHT_THUMB", VK_MBUTTON);
	KEY_ID_TRIANGLE = IniFile.ReadInteger("Keys", "TRIANGLE", 'Q'); 
	KEY_ID_SQUARE = IniFile.ReadInteger("Keys", "SQUARE", 'E'); 
	KEY_ID_CIRCLE = IniFile.ReadInteger("Keys", "CIRCLE", 'R'); 
	KEY_ID_CROSS = IniFile.ReadInteger("Keys", "CROSS", VK_SPACE);
	KEY_ID_TOUCHPAD = IniFile.ReadInteger("Keys", "TOUCHPAD", VK_RETURN);
	KEY_ID_OPTIONS = IniFile.ReadInteger("Keys", "OPTIONS", VK_TAB);
	KEY_ID_SHARE = IniFile.ReadInteger("Keys", "SHARE", VK_F12);

    const auto client = vigem_alloc();
    auto ret = vigem_connect(client);
	const auto ds4 = vigem_target_ds4_alloc();
	ret = vigem_target_add(client, ds4);
	ret = vigem_target_ds4_register_notification(client, ds4, &notification, nullptr);
	DS4_REPORT report;
	DS4_REPORT_INIT(&report);

	printf("Press \"~\" key to exit\r\n");

	//Forced keyboard and mouse mode
	if (argv[1] != NULL && strcmp(argv[1], "-mk") == 0)
		EmulationMode = KBMode;

	//Load library and scan Xbox gamepads only in Xbox mode (default)
	if (EmulationMode == XboxMode) {
		//printf("Xbox mode\r\n");
		//TCHAR XInputPath[MAX_PATH] = { 0 };
		//GetSystemWindowsDirectory(XInputPath, sizeof(XInputPath));
		//_tcscat_s(XInputPath, sizeof(XInputPath), _T("\\System32\\xinput1_3.dll")); //Separate paths for different architectures are not required. Windows does it by itself.
		hDll = LoadLibrary("xinput1_3.dll"); //x360ce support
		if (hDll != NULL) {
			MyXInputGetState = (_XInputGetState)GetProcAddress(hDll, "XInputGetState");
			MyXInputSetState = (_XInputSetState)GetProcAddress(hDll, "XInputSetState");
			if (MyXInputGetState == NULL || MyXInputSetState == NULL) 
				hDll = NULL;
		}

		for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
			if (MyXInputGetState(XboxUserIndex, &myPState) == ERROR_SUCCESS)
			{
				XboxUserIndex = i;
				break;
			} else
				if (i == 3) //if don't have gamepad 
					EmulationMode = KBMode;
	}
	
	if (EmulationMode == KBMode) {
		m_HalfWidth = GetSystemMetrics(SM_CXSCREEN) / 2;
		m_HalfHeight = GetSystemMetrics(SM_CYSCREEN) / 2;
	}

	//Title
	if (EmulationMode == XboxMode)
		SetConsoleTitle("DS4Emulator: Xbox controller mode");
	else
		SetConsoleTitle("DS4Emulator: keyboard and mouse mode");

	while (!(GetAsyncKeyState(192) & 0x8000)) //~
	{
		ret = vigem_target_ds4_update(client, ds4, report);
		
		report.wButtons = 0;
		DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NONE);
		report.bTriggerL = 0;
		report.bTriggerR = 0;
		report.bSpecial = 0;

		//Xbox mode
		if (EmulationMode == XboxMode) {
			DWORD myStatus = ERROR_DEVICE_NOT_CONNECTED;
			if (hDll != NULL)
				myStatus = MyXInputGetState(XboxUserIndex, &myPState);

			if (myStatus == ERROR_SUCCESS) {

				//Convert axis from - https://github.com/sam0x17/XJoy/blob/236b5539cc15ea1c83e1e5f0260937f69a78866d/Include/ViGEmUtil.h
				report.bThumbLX = ((myPState.Gamepad.sThumbLX + ((USHRT_MAX / 2) + 1)) / 257);
				report.bThumbLY = (-(myPState.Gamepad.sThumbLY + ((USHRT_MAX / 2) - 1)) / 257);
				report.bThumbLY = (report.bThumbLY == 0) ? 0xFF : report.bThumbLY;
				report.bThumbRX = ((myPState.Gamepad.sThumbRX + ((USHRT_MAX / 2) + 1)) / 257);
				report.bThumbRY = (-(myPState.Gamepad.sThumbRY + ((USHRT_MAX / 2) + 1)) / 257);
				report.bThumbRY = (report.bThumbRY == 0) ? 0xFF : report.bThumbRY;

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					report.wButtons |= DS4_BUTTON_OPTIONS;

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
					report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;

				if ((GetAsyncKeyState(KEY_ID_SHARE) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_SHARE;

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
					report.wButtons |= DS4_BUTTON_TRIANGLE;
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_X)
					report.wButtons |= DS4_BUTTON_SQUARE;
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_B)
					report.wButtons |= DS4_BUTTON_CIRCLE;
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_A)
					report.wButtons |= DS4_BUTTON_CROSS;

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
					report.wButtons |= DS4_BUTTON_THUMB_LEFT;
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
					report.wButtons |= DS4_BUTTON_THUMB_RIGHT;

				//Swap triggers and shoulders
				if (SwapTriggersShoulders == false) {
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
						report.wButtons |= DS4_BUTTON_SHOULDER_LEFT;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
						report.wButtons |= DS4_BUTTON_SHOULDER_RIGHT;
						
					report.bTriggerL = myPState.Gamepad.bLeftTrigger;
					report.bTriggerR = myPState.Gamepad.bRightTrigger;
				}
				else {
					if (myPState.Gamepad.bLeftTrigger > 0)
						report.wButtons |= DS4_BUTTON_SHOULDER_LEFT;
					if (myPState.Gamepad.bRightTrigger > 0)
						report.wButtons |= DS4_BUTTON_SHOULDER_RIGHT;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
						report.bTriggerL = 255;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
						report.bTriggerR = 255;
				}

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTH);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTH);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_WEST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_EAST);
			}
		} else
		
		//Mouse and keyboard mode
		if (EmulationMode == KBMode) {

			PSNowWindow = FindWindow(NULL, "PlayStation™Now");
			if (PSNowWindow != 0 && PSNowWindow == GetForegroundWindow() && IsWindowVisible(PSNowWindow)) {
				
				GetMouseState();
				report.bThumbRX = 0x80;
				report.bThumbRY = 0x80;

				//Are there better options? / Åñòü âàðèàíò ëó÷øå?
				if (mouseX > 0)
					report.bThumbRX = 128 + std::clamp(mouseX, 0, 127);
				if (mouseX < 0)
					report.bThumbRX = std::clamp(mouseX, 0, 127);
				if (mouseY < 0)
					report.bThumbRY = 128 + std::clamp(mouseY, -127, 0);
				if (mouseY > 0)
					report.bThumbRY = 128 + std::clamp(mouseY, 0, 127);

				report.bThumbLY = 0x80;
				if ((GetAsyncKeyState('W') & 0x8000) != 0) report.bThumbLY = 0;
				if ((GetAsyncKeyState('S') & 0x8000) != 0) report.bThumbLY = 255;
				report.bThumbLX = 0x80;
				if ((GetAsyncKeyState('A') & 0x8000) != 0) report.bThumbLX = 0;
				if ((GetAsyncKeyState('D') & 0x8000) != 0) report.bThumbLX = 255;

				
				if ((GetAsyncKeyState(KEY_ID_LEFT_TRIGGER) & 0x8000) != 0)
					report.bTriggerL = 255;
				if ((GetAsyncKeyState(KEY_ID_RIGHT_TRIGGER) & 0x8000) != 0)
					report.bTriggerR = 255;

				if ((GetAsyncKeyState(KEY_ID_OPTIONS) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_OPTIONS;

				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD) & 0x8000) != 0)
					report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;

				if ((GetAsyncKeyState(KEY_ID_SHARE) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_SHARE;

				if ((GetAsyncKeyState(KEY_ID_TRIANGLE) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_TRIANGLE;
				if ((GetAsyncKeyState(KEY_ID_SQUARE) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_SQUARE;
				if ((GetAsyncKeyState(KEY_ID_CIRCLE) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_CIRCLE;
				if ((GetAsyncKeyState(KEY_ID_CROSS) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_CROSS;

				if ((GetAsyncKeyState(KEY_ID_LEFT_THUMB) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_THUMB_LEFT;
				if ((GetAsyncKeyState(KEY_ID_RIGHT_THUMB) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_THUMB_RIGHT;

				if ((GetAsyncKeyState(KEY_ID_LEFT_SHOULDER) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_SHOULDER_LEFT;
				if ((GetAsyncKeyState(KEY_ID_RIGHT_SHOULDER) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_SHOULDER_RIGHT;

				if ((GetAsyncKeyState(KEY_ID_DPAD_UP) & 0x8000) != 0)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTH);
				if ((GetAsyncKeyState(KEY_ID_DPAD_DOWN) & 0x8000) != 0)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTH);
				if ((GetAsyncKeyState(KEY_ID_DPAD_LEFT) & 0x8000) != 0)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_WEST);
				if ((GetAsyncKeyState(KEY_ID_DPAD_RIGHT) & 0x8000) != 0)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_EAST);
			}
		}

		//Don't over load the CPU with reading
		Sleep(10);
	}

	vigem_target_remove(client, ds4);
	vigem_target_free(ds4);
    vigem_free(client);
}
