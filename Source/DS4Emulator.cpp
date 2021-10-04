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

SHORT DeadZoneXboxAxis(SHORT StickAxis, float Percent)
{
	float DeadZoneValue = Percent * 327.67;
	if (StickAxis > 0)
	{
		StickAxis -= round(DeadZoneValue);
		if (StickAxis < 0)
			StickAxis = 0;
	}
	else if (StickAxis < 0) {
		StickAxis += round(DeadZoneValue);
		if (StickAxis > 0)
			StickAxis = 0;
	}

	return round(StickAxis + StickAxis * Percent * 0.01);
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

	float DeadZoneLeftStickX = IniFile.ReadFloat("Xbox", "DeadZoneLeftStickX", 0);
	float DeadZoneLeftStickY = IniFile.ReadFloat("Xbox", "DeadZoneLeftStickY", 0);
	float DeadZoneRightStickX = IniFile.ReadFloat("Xbox", "DeadZoneRightStickX", 0);
	float DeadZoneRightStickY = IniFile.ReadFloat("Xbox", "DeadZoneRightStickY", 0);

	int SleepTimeOutKB = IniFile.ReadInteger("KeyboardMouse", "SleepTimeOut", 2);
	std::string WindowTitle = IniFile.ReadString("KeyboardMouse", "ActivateOnlyInWindow", "PlayStation™Now");
	bool ActivateInAnyWindow = IniFile.ReadBoolean("KeyboardMouse", "ActivateInAnyWindow", false);
	bool EmulateAnalogTriggers = IniFile.ReadBoolean("KeyboardMouse", "EmulateAnalogTriggers", false);
	float LeftTriggerValue = 0;
	float RightTriggerValue = 0;
	float StepTriggerValue = IniFile.ReadFloat("KeyboardMouse", "AnalogTriggerStep", 15);
	mouseSensetiveX = IniFile.ReadFloat("KeyboardMouse", "SensX", 15);
	mouseSensetiveY = IniFile.ReadFloat("KeyboardMouse", "SensY", 15);

	int KEY_ID_LEFT_STICK_UP = IniFile.ReadInteger("Keys", "LS_UP", 'W');
	int KEY_ID_LEFT_STICK_LEFT = IniFile.ReadInteger("Keys", "LS_LEFT", 'A');
	int KEY_ID_LEFT_STICK_RIGHT = IniFile.ReadInteger("Keys", "LS_RIGHT", 'D');
	int KEY_ID_LEFT_STICK_DOWN = IniFile.ReadInteger("Keys", "LS_DOWN", 'S');
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
	
	int KEY_ID_TOUCHPAD_SWIPE_UP = IniFile.ReadInteger("Keys", "TOUCHPAD_SWIPE_UP", '7');
	int KEY_ID_TOUCHPAD_SWIPE_DOWN = IniFile.ReadInteger("Keys", "TOUCHPAD_SWIPE_DOWN", '8');
	int KEY_ID_TOUCHPAD_SWIPE_LEFT = IniFile.ReadInteger("Keys", "TOUCHPAD_SWIPE_LEFT", '9');
	int KEY_ID_TOUCHPAD_SWIPE_RIGHT = IniFile.ReadInteger("Keys", "TOUCHPAD_SWIPE_RIGHT", '0');

	int KEY_ID_TOUCHPAD_UP = IniFile.ReadInteger("Keys", "TOUCHPAD_UP", 'U');
	int KEY_ID_TOUCHPAD_DOWN = IniFile.ReadInteger("Keys", "TOUCHPAD_DOWN", 'N');
	int KEY_ID_TOUCHPAD_LEFT = IniFile.ReadInteger("Keys", "TOUCHPAD_LEFT", 'H');
	int KEY_ID_TOUCHPAD_RIGHT = IniFile.ReadInteger("Keys", "TOUCHPAD_RIGHT", 'K');
	int KEY_ID_TOUCHPAD_CENTER = IniFile.ReadInteger("Keys", "TOUCHPAD_CENTER", 'J');

    const auto client = vigem_alloc();
    auto ret = vigem_connect(client);
	const auto ds4 = vigem_target_ds4_alloc();
	ret = vigem_target_add(client, ds4);
	ret = vigem_target_ds4_register_notification(client, ds4, &notification, nullptr);
	DS4_REPORT_EX report;
	bool TouchpadSwipeUp = false, TouchpadSwipeDown = false;
	bool TouchpadSwipeLeft = false, TouchpadSwipeRight = false;

	printf("\r\n Press exit key to exit, by default it is \"~\" (can be changed in config file).\r\n");

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
	if (EmulationMode == XboxMode) {
		SetConsoleTitle("DS4Emulator: Xbox controller mode");
		printf(" Xbox controller mode.\r\n");

	}
	else {
		SetConsoleTitle("DS4Emulator: keyboard and mouse mode");
		printf(" Keyboard and mouse mode.\r\n");
	}

	DS4_TOUCH BuffPreviousTouch[2] = { 0, 0 };
	BuffPreviousTouch[0].bIsUpTrackingNum1 = 0x80;
	BuffPreviousTouch[1].bIsUpTrackingNum1 = 0x80;
	unsigned char TouchIndex = 0;
	bool AllowIncTouchIndex;
	bool DeadZoneMode = false;


	while (!(GetAsyncKeyState(KEY_ID_EXIT) & 0x8000)) // "~" by default
	{
		DS4_REPORT_INIT_EX(&report);

		uint16_t TouchX = 0;
		uint16_t TouchY = 0;

		report.bTouchPacketsN = 0;
		report.sCurrentTouch = { 0 };
		report.sPreviousTouch[0] = { 0 };
		report.sPreviousTouch[1] = { 0 };
		report.sCurrentTouch.bIsUpTrackingNum1 = 0x80;
		report.sCurrentTouch.bIsUpTrackingNum2 = 0x80;

		// Xbox mode
		if (EmulationMode == XboxMode) {
			DWORD myStatus = ERROR_DEVICE_NOT_CONNECTED;
			if (hDll != NULL)
				myStatus = MyXInputGetState(XboxUserIndex, &myPState);
			
			if (myStatus == ERROR_SUCCESS) {

				// Dead zones
				if ((GetAsyncKeyState(VK_F10) & 0x8000) != 0 && ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0))
				{
					DeadZoneMode = !DeadZoneMode;
					if (DeadZoneMode == false) {
						system("cls");
						printf("\r\n Press exit key to exit, by default it is \"~\" (can be changed in config file).\r\n");
						printf(" Xbox controller mode.\r\n");
					}
				}

				if (DeadZoneMode) {
					printf(" Left Stick X=%6.2f, ", abs(myPState.Gamepad.sThumbLX / (32767 / 100.0f)));
					printf("Y=%6.2f\t", abs(myPState.Gamepad.sThumbLY / (32767 / 100.0f)));
					printf("Right Stick X=%6.2f ", abs(myPState.Gamepad.sThumbRX / (32767 / 100.0f)));
					printf("Y=%6.2f\n", abs(myPState.Gamepad.sThumbRY / (32767 / 100.0f)));
				}

				myPState.Gamepad.sThumbLX = DeadZoneXboxAxis(myPState.Gamepad.sThumbLX, DeadZoneLeftStickX);
				myPState.Gamepad.sThumbLY = DeadZoneXboxAxis(myPState.Gamepad.sThumbLY, DeadZoneLeftStickY);
				myPState.Gamepad.sThumbRX = DeadZoneXboxAxis(myPState.Gamepad.sThumbRX, DeadZoneRightStickX);
				myPState.Gamepad.sThumbRY = DeadZoneXboxAxis(myPState.Gamepad.sThumbRY, DeadZoneRightStickY);

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

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_START) {
					myPState.Gamepad.wButtons &= XINPUT_GAMEPAD_BACK; myPState.Gamepad.wButtons &= XINPUT_GAMEPAD_START;
					if (SwapShareTouchPad)
						report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
					else
						report.wButtons |= DS4_BUTTON_SHARE;
				}

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
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_NORTH);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_SOUTH);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_WEST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_EAST);

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_NORTHEAST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_SOUTHWEST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_NORTHWEST);
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_SOUTHEAST);

				// Touchpad swipes
				if (report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD) {
					TouchX = 960; TouchY = 471;
					if (report.bThumbRX > 127)
						TouchX = 320 + (report.bThumbRX - 127) * 10;
					if (report.bThumbRX < 127)
						TouchX = 1600 - (-report.bThumbRX + 127) * 10;
					if (report.bThumbRY > 129)
						TouchY = round(100 + (report.bThumbRY - 129) * 5.8);
					if (report.bThumbRY < 129)
						TouchY = round(743 - (-report.bThumbRY + 129) * 5.8);
					report.bThumbRX = 127;
					report.bThumbRY = 129;
				}
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

				// Are there better options? / Есть вариант лучше?
				if (DeltaMouseX > 0)
					report.bThumbRX = 128 + round( Clamp(DeltaMouseX * mouseSensetiveX, 0, 127) );
				if (DeltaMouseX < 0)
					report.bThumbRX = 128 + round( Clamp(DeltaMouseX * mouseSensetiveX, -127, 0) );
				if (DeltaMouseY < 0)
					report.bThumbRY = 128 + round( Clamp(DeltaMouseY * mouseSensetiveY, -127, 0) );
				if (DeltaMouseY > 0)
					report.bThumbRY = 128 + round( Clamp(DeltaMouseY * mouseSensetiveY, 0, 127) );
			
				if ((GetAsyncKeyState(KEY_ID_LEFT_STICK_UP) & 0x8000) != 0) report.bThumbLY = 0;
				if ((GetAsyncKeyState(KEY_ID_LEFT_STICK_DOWN) & 0x8000) != 0) report.bThumbLY = 255;
				if ((GetAsyncKeyState(KEY_ID_LEFT_STICK_LEFT) & 0x8000) != 0) report.bThumbLX = 0;
				if ((GetAsyncKeyState(KEY_ID_LEFT_STICK_RIGHT) & 0x8000) != 0) report.bThumbLX = 255;

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
						if (LeftTriggerValue > 0)
							LeftTriggerValue -= StepTriggerValue;
					}
					
					report.bTriggerL = Clamp(round(LeftTriggerValue), 0, 255);

					if ((GetAsyncKeyState(KEY_ID_RIGHT_TRIGGER) & 0x8000) != 0) {
						if (RightTriggerValue < 255)
							RightTriggerValue += StepTriggerValue;
					}
					else {
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
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_NORTH);
				if ((GetAsyncKeyState(KEY_ID_DPAD_DOWN) & 0x8000) != 0)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_SOUTH);
				if ((GetAsyncKeyState(KEY_ID_DPAD_LEFT) & 0x8000) != 0)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_WEST);
				if ((GetAsyncKeyState(KEY_ID_DPAD_RIGHT) & 0x8000) != 0)
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_EAST);
			
				// Touchpad, left
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_LEFT) & 0x8000) != 0) {
					TouchX = 320; TouchY = 471; }

				// Center
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_CENTER) & 0x8000) != 0) {
					TouchX = 960; TouchY = 471; }

				// Right
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_RIGHT) & 0x8000) != 0) {
					TouchX = 1600; TouchY = 471; }

				// Center up
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_UP) & 0x8000) != 0) {
					if (TouchX == 0) TouchX = 960; TouchY = 157; }

				// Center down
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_DOWN) & 0x8000) != 0) {
					if (TouchX == 0) TouchX = 960; TouchY = 785; }

				// Touchpad swipes, last
				if (TouchpadSwipeUp) { TouchpadSwipeUp = false; TouchX = 960; TouchY = 100; }
				if (TouchpadSwipeDown) { TouchpadSwipeDown = false; TouchX = 960; TouchY = 843; }
				if (TouchpadSwipeLeft) { TouchpadSwipeLeft = false; TouchX = 320; TouchY = 471; }
				if (TouchpadSwipeRight) { TouchpadSwipeRight = false; TouchX = 1600; TouchY = 471; }

				// Swipes
				if (TouchpadSwipeUp == false && (GetAsyncKeyState(KEY_ID_TOUCHPAD_SWIPE_UP) & 0x8000) != 0) { TouchX = 960; TouchY = 843; TouchpadSwipeUp = true; }
				if (TouchpadSwipeDown == false && (GetAsyncKeyState(KEY_ID_TOUCHPAD_SWIPE_DOWN) & 0x8000) != 0) { TouchX = 960; TouchY = 100; TouchpadSwipeDown = true; }
				if (TouchpadSwipeLeft == false && (GetAsyncKeyState(KEY_ID_TOUCHPAD_SWIPE_LEFT) & 0x8000) != 0) { TouchX = 1600; TouchY = 471; TouchpadSwipeLeft = true; }
				if (TouchpadSwipeRight == false && (GetAsyncKeyState(KEY_ID_TOUCHPAD_SWIPE_RIGHT) & 0x8000) != 0) { TouchX = 320; TouchY = 471; TouchpadSwipeRight = true; }
			}
		}

		if (BuffPreviousTouch[1].bIsUpTrackingNum1 == 0) {
			//printf("2: prev 2 touched\r\n");
			report.sPreviousTouch[1] = BuffPreviousTouch[1];
			BuffPreviousTouch[1] = { 0 };
			BuffPreviousTouch[1].bIsUpTrackingNum1 = 0x80;
		}

		if (BuffPreviousTouch[0].bIsUpTrackingNum1 == 0) {
			//printf("1: prev 1 touched\r\n");
			report.sPreviousTouch[0] = BuffPreviousTouch[0];
			BuffPreviousTouch[1] = BuffPreviousTouch[0];
			BuffPreviousTouch[0] = { 0 };
			BuffPreviousTouch[0].bIsUpTrackingNum1 = 0x80;
		}

		// Probably the wrong way, but it works, temporary workaround / Вероятно неверный путь, но он работает, подойдет как временное решение
		report.sCurrentTouch.bIsUpTrackingNum1 = 0x80;
		if (TouchX != 0 || TouchY != 0) { 
			report.bTouchPacketsN = 1;
			
			if (AllowIncTouchIndex) {
				if (TouchIndex < 255) // Is this the right way? / Верный ли это путь?
					TouchIndex++;
				else
					TouchIndex = 0;

				AllowIncTouchIndex = false;
			}
			report.sCurrentTouch.bIsUpTrackingNum1 = 0;

			//printf(" %d: touched\r\n", TouchIndex);

			report.sCurrentTouch.bTouchData1[0] = TouchX & 0xFF;
			report.sCurrentTouch.bTouchData1[1] = ((TouchX >> 8) & 0x0F) | ((TouchY & 0x0F) << 4);
			report.sCurrentTouch.bTouchData1[2] = (TouchY >> 4) & 0xFF;

			BuffPreviousTouch[0] = report.sCurrentTouch;
		}

		if (TouchX == 0 && TouchY == 0)
			AllowIncTouchIndex = true;

		report.sCurrentTouch.bPacketCounter = TouchIndex;

		// if ((GetAsyncKeyState(VK_NUMPAD0) & 0x8000) != 0) system("cls");

		ret = vigem_target_ds4_update_ex(client, ds4, report);

		// Don't overload the CPU with reading
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
