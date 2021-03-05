#include <Windows.h>
#include <ViGEm/Client.h>
#include <mutex>
#include "IniReader\IniReader.h"

// XInput headers
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

// XInput structures
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
HMODULE hDll = NULL;
DWORD XboxUserIndex = 0;

static std::mutex m;

int m_HalfWidth = 1920 / 2;
int m_HalfHeight = 1080 / 2;
float mouseSensetiveY;
float mouseSensetiveX;
bool firstCP = true;
int DeltaMouseX, DeltaMouseY;
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

	DeltaMouseX = mousePos.x - m_HalfWidth;
	DeltaMouseY = mousePos.y - m_HalfHeight;

	SetCursorPos(m_HalfWidth, m_HalfHeight);
}

float Clamp(float Value, float Min, float Max)
{
	if (Value > Max)
		Value = Max;
	else
		if (Value < Min)
			Value = Min;
	return Value;
}

int main(int argc, char **argv)
{
	SetConsoleTitle("DS4Emulator");
	
	#define XboxMode 1
	#define KBMode 0
	int EmulationMode = KBMode;

	// Config
	CIniReader IniFile("Config.ini");
	int KEY_ID_EXIT = IniFile.ReadInteger("Main", "ExitBtn", 192); // "~" by default

	bool InvertX = IniFile.ReadBoolean("DS4", "InvertX", false);
	bool InvertY = IniFile.ReadBoolean("DS4", "InvertY", false);

	int SleepTimeOutXbox = IniFile.ReadInteger("Xbox", "SleepTimeOut", 1);
	bool SwapTriggersShoulders = IniFile.ReadBoolean("Xbox", "SwapTriggersShoulders", false);
	bool SwapShareTouchPad = IniFile.ReadBoolean("Xbox", "SwapShareTouchPad", false);

	int SleepTimeOutKB = IniFile.ReadInteger("KeyboardMouse", "SleepTimeOut", 2);
	std::string WindowTitle = IniFile.ReadString("KeyboardMouse", "ActivateOnlyInWindow", "PlayStation™Now");
	bool ActivateInAnyWindow = IniFile.ReadBoolean("KeyboardMouse", "ActivateInAnyWindow", false);
	bool EmulateAnalogTriggers = IniFile.ReadBoolean("KeyboardMouse", "EmulateAnalogTriggers", false);
	float LeftTriggerValue = 0;
	float RightTriggerValue = 0;
	float StepTriggerValue = IniFile.ReadFloat("KeyboardMouse", "AnalogTriggerStep", 15);
	mouseSensetiveX = IniFile.ReadFloat("KeyboardMouse", "SensX", 15);
	mouseSensetiveY = IniFile.ReadFloat("KeyboardMouse", "SensY", 15);

	int KEY_ID_LEFT_TRIGGER = IniFile.ReadInteger("Keys", "L2", VK_RBUTTON);
	int KEY_ID_RIGHT_TRIGGER = IniFile.ReadInteger("Keys", "R2", VK_LBUTTON);
	int KEY_ID_LEFT_SHOULDER = IniFile.ReadInteger("Keys", "L1", VK_CONTROL);
	int KEY_ID_RIGHT_SHOULDER = IniFile.ReadInteger("Keys", "R1", VK_MENU);
	int KEY_ID_DPAD_UP = IniFile.ReadInteger("Keys", "DPAD_UP", '1');
	int KEY_ID_DPAD_LEFT = IniFile.ReadInteger("Keys", "DPAD_LEFT", '2');
	int KEY_ID_DPAD_RIGHT = IniFile.ReadInteger("Keys", "DPAD_RIGHT", '3');
	int KEY_ID_DPAD_DOWN = IniFile.ReadInteger("Keys", "DPAD_DOWN", '4');
	int KEY_ID_LEFT_THUMB = IniFile.ReadInteger("Keys", "L3", VK_LSHIFT);
	int KEY_ID_RIGHT_THUMB = IniFile.ReadInteger("Keys", "R3", VK_MBUTTON);
	int KEY_ID_TRIANGLE = IniFile.ReadInteger("Keys", "TRIANGLE", 'Q');
	int KEY_ID_SQUARE = IniFile.ReadInteger("Keys", "SQUARE", 'E');
	int KEY_ID_CIRCLE = IniFile.ReadInteger("Keys", "CIRCLE", 'R');
	int KEY_ID_CROSS = IniFile.ReadInteger("Keys", "CROSS", VK_SPACE);
	int KEY_ID_TOUCHPAD = IniFile.ReadInteger("Keys", "TOUCHPAD", VK_RETURN);
	int KEY_ID_OPTIONS = IniFile.ReadInteger("Keys", "OPTIONS", VK_TAB);
	int KEY_ID_SHARE = IniFile.ReadInteger("Keys", "SHARE", VK_F12);

    const auto client = vigem_alloc();
    auto ret = vigem_connect(client);
	const auto ds4 = vigem_target_ds4_alloc();
	ret = vigem_target_add(client, ds4);
	ret = vigem_target_ds4_register_notification(client, ds4, &notification, nullptr);
	DS4_REPORT report;
	DS4_REPORT_INIT(&report);

	printf("Press \"~\" key to exit\r\n");

	// Load library and scan Xbox gamepads
	hDll = LoadLibrary("xinput1_3.dll"); // x360ce support
	if (hDll != NULL) {
		MyXInputGetState = (_XInputGetState)GetProcAddress(hDll, "XInputGetState");
		MyXInputSetState = (_XInputSetState)GetProcAddress(hDll, "XInputSetState");
		if (MyXInputGetState == NULL || MyXInputSetState == NULL) 
			hDll = NULL;
	}

	if (hDll != NULL)
		for (int i = 0; i < XUSER_MAX_COUNT; ++i)
			if (MyXInputGetState(XboxUserIndex, &myPState) == ERROR_SUCCESS)
			{
				XboxUserIndex = i;
				EmulationMode = XboxMode;
				break;
			}
	
	if (EmulationMode == KBMode) {
		m_HalfWidth = GetSystemMetrics(SM_CXSCREEN) / 2;
		m_HalfHeight = GetSystemMetrics(SM_CYSCREEN) / 2;
	}

	// Title
	if (EmulationMode == XboxMode)
		SetConsoleTitle("DS4Emulator: Xbox controller mode");
	else
		SetConsoleTitle("DS4Emulator: keyboard and mouse mode");

	while (!(GetAsyncKeyState(KEY_ID_EXIT) & 0x8000)) // "~" by default
	{
		DS4_REPORT_INIT(&report);

		// Xbox mode
		if (EmulationMode == XboxMode) {
			DWORD myStatus = ERROR_DEVICE_NOT_CONNECTED;
			if (hDll != NULL)
				myStatus = MyXInputGetState(XboxUserIndex, &myPState);
			
			if (myStatus == ERROR_SUCCESS) {

				// Convert axis from - https://github.com/sam0x17/XJoy/blob/236b5539cc15ea1c83e1e5f0260937f69a78866d/Include/ViGEmUtil.h
				report.bThumbLX = ((myPState.Gamepad.sThumbLX + ((USHRT_MAX / 2) + 1)) / 257);
				report.bThumbLY = (-(myPState.Gamepad.sThumbLY + ((USHRT_MAX / 2) - 1)) / 257);
				report.bThumbLY = (report.bThumbLY == 0) ? 0xFF : report.bThumbLY;
				
				// Inverting X
				if (InvertX == false)
					report.bThumbRX = ((myPState.Gamepad.sThumbRX + ((USHRT_MAX / 2) + 1)) / 257);
				else
					report.bThumbRX = ((-myPState.Gamepad.sThumbRX + ((USHRT_MAX / 2) + 1)) / 257);
				
				// Inverting Y
				if (InvertY == false)
					report.bThumbRY = (-(myPState.Gamepad.sThumbRY + ((USHRT_MAX / 2) + 1)) / 257);
				else
					report.bThumbRY = (-(-myPState.Gamepad.sThumbRY + ((USHRT_MAX / 2) + 1)) / 257);
				
				report.bThumbRY = (report.bThumbRY == 0) ? 0xFF : report.bThumbRY;

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					report.wButtons |= DS4_BUTTON_OPTIONS;

				// Swap share and touchpad
				if (SwapShareTouchPad == false) {
					if ((GetAsyncKeyState(KEY_ID_SHARE) & 0x8000) != 0)
						report.wButtons |= DS4_BUTTON_SHARE;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
						report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
				}
				else {
					if ((GetAsyncKeyState(KEY_ID_SHARE) & 0x8000) != 0)
						report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
						report.wButtons |= DS4_BUTTON_SHARE;
				}

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

				// Swap triggers and shoulders
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

				if (report.bTriggerL > 0) // Specific of DualShock
					report.wButtons |= DS4_BUTTON_TRIGGER_LEFT; 
				if (report.bTriggerR > 0) // Specific of DualShock
					report.wButtons |= DS4_BUTTON_TRIGGER_RIGHT;

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTH);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTH);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_WEST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_EAST);

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTHEAST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTHWEST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTHWEST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
					DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTHEAST);
			}
		}
		// Mouse and keyboard mode
		else if (EmulationMode == KBMode) {
			
			PSNowWindow = FindWindow(NULL, WindowTitle.c_str());
			bool PSNowFound = (PSNowWindow != 0) && (IsWindowVisible(PSNowWindow)) && (PSNowWindow == GetForegroundWindow());

			if (ActivateInAnyWindow || PSNowFound) {
				GetMouseState();

				if (InvertX)
					DeltaMouseX = DeltaMouseX * -1;
				if (InvertY)
					DeltaMouseY = DeltaMouseY * -1;

				// Are there better options? / Åñòü âàðèàíò ëó÷øå?
				if (DeltaMouseX > 0)
					report.bThumbRX = 128 + round( Clamp( DeltaMouseX * mouseSensetiveX, 0, 127) );
				if (DeltaMouseX < 0)
					report.bThumbRX = 128 + round( Clamp(DeltaMouseX * mouseSensetiveX, -127, 0) );
				if (DeltaMouseY < 0)
					report.bThumbRY = 128 + round( Clamp(DeltaMouseY * mouseSensetiveY, -127, 0) );
				if (DeltaMouseY > 0)
					report.bThumbRY = 128 + round( Clamp(DeltaMouseY * mouseSensetiveY, 0, 127) );
			
				if ((GetAsyncKeyState('W') & 0x8000) != 0) report.bThumbLY = 0;
				if ((GetAsyncKeyState('S') & 0x8000) != 0) report.bThumbLY = 255;
				if ((GetAsyncKeyState('A') & 0x8000) != 0) report.bThumbLX = 0;
				if ((GetAsyncKeyState('D') & 0x8000) != 0) report.bThumbLX = 255;

				if (EmulateAnalogTriggers == false) {

					if ((GetAsyncKeyState(KEY_ID_LEFT_TRIGGER) & 0x8000) != 0)
						report.bTriggerL = 255;
					if ((GetAsyncKeyState(KEY_ID_RIGHT_TRIGGER) & 0x8000) != 0)
						report.bTriggerR = 255;
				}
				else { // With emulate analog triggers

					if ((GetAsyncKeyState(KEY_ID_LEFT_TRIGGER) & 0x8000) != 0) {
						if (LeftTriggerValue < 255)
							LeftTriggerValue += StepTriggerValue;
					}
					else {
						// LeftTriggerValue = 0;
						if (LeftTriggerValue > 0)
							LeftTriggerValue -= StepTriggerValue;
					}
					
					report.bTriggerL = Clamp(round(LeftTriggerValue), 0, 255);

					if ((GetAsyncKeyState(KEY_ID_RIGHT_TRIGGER) & 0x8000) != 0) {
						if (RightTriggerValue < 255)
							RightTriggerValue += StepTriggerValue;
					}
					else {
						// RightTriggerValue = 0;
						if (RightTriggerValue > 0)
							RightTriggerValue -= StepTriggerValue;
					}

					report.bTriggerR = Clamp(round(RightTriggerValue), 0, 255);
				}

				
				if (report.bTriggerL > 0) // Specific of DualShock 
					report.wButtons |= DS4_BUTTON_TRIGGER_LEFT; 
				if (report.bTriggerR > 0) // Specific of DualShock
					report.wButtons |= DS4_BUTTON_TRIGGER_RIGHT; 

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

		ret = vigem_target_ds4_update(client, ds4, report);

		//Don't overload the CPU with reading
		if (EmulationMode == XboxMode)
			Sleep(SleepTimeOutXbox);
		else
			Sleep(SleepTimeOutKB);
	}

	vigem_target_remove(client, ds4);
	vigem_target_free(ds4);
    vigem_free(client);
	FreeLibrary(hDll);
	hDll = nullptr;
}
