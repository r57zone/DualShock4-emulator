// XInput headers
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_GUIDE            0x0400
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

#define SkipPollTimeOut 15