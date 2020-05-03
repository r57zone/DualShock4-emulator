#include <Windows.h>
#include <ViGEm/Client.h>
#include <mutex>
#include <algorithm>

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

int main()
{
	SetConsoleTitle("DS4Emulator");

    const auto client = vigem_alloc();

    auto ret = vigem_connect(client);

	const auto ds4 = vigem_target_ds4_alloc();
	
	ret = vigem_target_add(client, ds4);

	ret = vigem_target_ds4_register_notification(client, ds4, &notification, nullptr);

	DS4_REPORT report;
	DS4_REPORT_INIT(&report);

	printf("Press \"~\" key to exit\r\n");

	hDll = LoadLibrary("xinput1_3.dll");

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
		}


	while (!(GetAsyncKeyState(192) & 0x8000)) //~
	{
		ret = vigem_target_ds4_update(client, ds4, report);
		
		report.wButtons = 0;
		DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NONE);
		report.bTriggerL = 0;
		report.bTriggerR = 0;
		report.bSpecial = 0;

		DWORD myStatus = ERROR_DEVICE_NOT_CONNECTED;
		if (hDll != NULL)
			myStatus = MyXInputGetState(XboxUserIndex, &myPState);

		if (myStatus == ERROR_SUCCESS) {

			report.bTriggerL = myPState.Gamepad.bLeftTrigger;
			report.bTriggerR = myPState.Gamepad.bRightTrigger;

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

			if ((GetAsyncKeyState(VK_F12) & 0x8000) != 0)
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
			
			if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
				report.wButtons |= DS4_BUTTON_SHOULDER_LEFT;
			if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
				report.wButtons |= DS4_BUTTON_SHOULDER_RIGHT;

			if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
				DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTH);
			if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
				DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTH);
			if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
				DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_WEST);
			if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
				DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_EAST);
		}

		Sleep(10);
	}

	vigem_target_remove(client, ds4);
	vigem_target_free(ds4);
	
    vigem_free(client);
}
