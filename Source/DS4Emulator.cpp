#include <Windows.h>
#include <mutex>
#include "ViGEm\Client.h"
#include "IniReader\IniReader.h"
#include "DS4Emulator.h"

//#include <winsock2.h>
#pragma comment (lib, "WSock32.Lib")

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
HWND PSPlusWindow = 0;
HWND PSRemotePlayWindow = 0;

// WinSock
SOCKET socketS;
int bytes_read;
struct sockaddr_in from;
int fromlen;
bool SocketActivated = false;
std::thread *pSocketThread = NULL;
unsigned char freePieIMU[50];
float AccelX = 0, AccelY = 0, AccelZ = 0, GyroX = 0, GyroY = 0, GyroZ = 0;
int curTimeStamp;

float bytesToFloat(unsigned char b3, unsigned char b2, unsigned char b1, unsigned char b0)
{
	unsigned char byte_array[] = { b3, b2, b1, b0 };
	float result;
	std::copy(reinterpret_cast<const char*>(&byte_array[0]),
		reinterpret_cast<const char*>(&byte_array[4]),
		reinterpret_cast<char*>(&result));
	return result;
}

int SleepTimeOutMotion = 1;
bool MotionOrientation = true; // true = landscape
void MotionReceiver()
{
	while (SocketActivated) {
		memset(&freePieIMU, 0, sizeof(freePieIMU));
		bytes_read = recvfrom(socketS, (char*)(&freePieIMU), sizeof(freePieIMU), 0, (sockaddr*)&from, &fromlen);
		if (bytes_read > 0) {

			if (MotionOrientation) {
				// landscape mapping
				AccelZ = bytesToFloat(freePieIMU[2], freePieIMU[3], freePieIMU[4], freePieIMU[5]);
				AccelX = bytesToFloat(freePieIMU[6], freePieIMU[7], freePieIMU[8], freePieIMU[9]);
				AccelY = bytesToFloat(freePieIMU[10], freePieIMU[11], freePieIMU[12], freePieIMU[13]);

				GyroZ = bytesToFloat(freePieIMU[14], freePieIMU[15], freePieIMU[16], freePieIMU[17]);
				GyroX = bytesToFloat(freePieIMU[18], freePieIMU[19], freePieIMU[20], freePieIMU[21]);
				GyroY = bytesToFloat(freePieIMU[22], freePieIMU[23], freePieIMU[24], freePieIMU[25]);
			}
			else {
				// portrait mapping
				AccelX = bytesToFloat(freePieIMU[2], freePieIMU[3], freePieIMU[4], freePieIMU[5]);
				AccelZ = bytesToFloat(freePieIMU[6], freePieIMU[7], freePieIMU[8], freePieIMU[9]);
				AccelY = bytesToFloat(freePieIMU[10], freePieIMU[11], freePieIMU[12], freePieIMU[13]);

				GyroX = bytesToFloat(freePieIMU[14], freePieIMU[15], freePieIMU[16], freePieIMU[17]);
				GyroZ = bytesToFloat(freePieIMU[18], freePieIMU[19], freePieIMU[20], freePieIMU[21]);
				GyroY = bytesToFloat(freePieIMU[22], freePieIMU[23], freePieIMU[24], freePieIMU[25]);
			}
		}
		else
			Sleep(SleepTimeOutMotion); // Don't overload the CPU with reading
	}
}

VOID CALLBACK notification(
	PVIGEM_CLIENT Client,
	PVIGEM_TARGET Target,
	UCHAR LargeMotor,
	UCHAR SmallMotor,
	DS4_LIGHTBAR_COLOR LightbarColor,
	LPVOID UserData)
{
	m.lock();

	if (MyXInputSetState != NULL) {
		XINPUT_VIBRATION myVibration;
		myVibration.wLeftMotorSpeed = LargeMotor * 257;
		myVibration.wRightMotorSpeed = SmallMotor * 257;
		MyXInputSetState(XboxUserIndex, &myVibration);
	}

    m.unlock();
}

void GetMouseState()
{
	POINT mousePos;
	if (firstCP) { SetCursorPos(m_HalfWidth, m_HalfHeight); firstCP = false; }
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
		StickAxis -= trunc(DeadZoneValue);
		if (StickAxis < 0)
			StickAxis = 0;
	}
	else if (StickAxis < 0) {
		StickAxis += trunc(DeadZoneValue);
		if (StickAxis > 0)
			StickAxis = 0;
	}

	return trunc(StickAxis + StickAxis * Percent * 0.01);
}

int main(int argc, char **argv)
{
	SetConsoleTitle("DS4Emulator 1.7.7");

	CIniReader IniFile("Config.ini"); // Config

	if (IniFile.ReadBoolean("Motion", "Activate", true)) {
		WSADATA wsaData;
		int iResult;
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult == 0) {
			struct sockaddr_in local;
			fromlen = sizeof(from);
			local.sin_family = AF_INET;
			local.sin_port = htons(IniFile.ReadInteger("Motion", "Port", 5555));
			local.sin_addr.s_addr = INADDR_ANY;

			socketS = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

			u_long nonblocking_enabled = true;
			ioctlsocket(socketS, FIONBIO, &nonblocking_enabled);

			if (socketS != INVALID_SOCKET) {
				iResult = bind(socketS, (sockaddr*)&local, sizeof(local));
				if (iResult != SOCKET_ERROR) {
					SocketActivated = true;
					pSocketThread = new std::thread(MotionReceiver);
				} else {
					WSACleanup();
					SocketActivated = false;
				}
			} else {
				WSACleanup();
				SocketActivated = false;
			}
		} else {
			WSACleanup();
			SocketActivated = false;
		}
	}
	float MotionSens = IniFile.ReadFloat("Motion", "Sens", 100) * 0.01;
	int InverseXStatus = IniFile.ReadBoolean("Motion", "InverseX", false) == false ? 1 : -1 ;
	int InverseYStatus = IniFile.ReadBoolean("Motion", "InverseY", false) == false ? 1 : -1 ;
	int InverseZStatus = IniFile.ReadBoolean("Motion", "InverseZ", false) == false ? 1 : -1 ;
	MotionOrientation = IniFile.ReadBoolean("Motion", "Orientation", true);

	SleepTimeOutMotion = IniFile.ReadInteger("Motion", "SleepTimeOut", 1);

	#define OCR_NORMAL 32512
	HCURSOR CurCursor = CopyCursor(LoadCursor(0, IDC_ARROW));
	HCURSOR CursorEmpty = LoadCursorFromFile("EmptyCursor.cur");
	bool CursorHidden = IniFile.ReadBoolean("KeyboardMouse", "HideCursorAfterStart", false);
	if (CursorHidden) { SetSystemCursor(CursorEmpty, OCR_NORMAL); CursorHidden = true; }

	#define KBMode 0
	#define XboxMode 1
	int EmulationMode = KBMode;

	// Config parameters
	int KEY_ID_EXIT = IniFile.ReadInteger("Main", "ExitBtn", 192); // "~" by default for RU, US and not for UK
	int KEY_ID_STOP_CENTERING = IniFile.ReadInteger("KeyboardMouse", "StopCenteringKey", 'C');

	bool InvertX = IniFile.ReadBoolean("Main", "InvertX", false);
	bool InvertY = IniFile.ReadBoolean("Main", "InvertY", false);

	int SleepTimeOutXbox = IniFile.ReadInteger("Xbox", "SleepTimeOut", 1);
	bool SwapTriggersShoulders = IniFile.ReadBoolean("Xbox", "SwapTriggersShoulders", false);
	bool SwapShareTouchPad = IniFile.ReadBoolean("Xbox", "SwapShareTouchPad", false);
	bool TouchPadPressedWhenSwiping = IniFile.ReadBoolean("Xbox", "TouchPadPressedWhenSwiping", true);
	bool EnableXboxButton = IniFile.ReadBoolean("Xbox", "EnableXboxButton", true);

	float DeadZoneLeftStickX = IniFile.ReadFloat("Xbox", "DeadZoneLeftStickX", 0);
	float DeadZoneLeftStickY = IniFile.ReadFloat("Xbox", "DeadZoneLeftStickY", 0);
	float DeadZoneRightStickX = IniFile.ReadFloat("Xbox", "DeadZoneRightStickX", 0);
	float DeadZoneRightStickY = IniFile.ReadFloat("Xbox", "DeadZoneRightStickY", 0);

	int SleepTimeOutKB = IniFile.ReadInteger("KeyboardMouse", "SleepTimeOut", 1);
	std::string WindowTitle = IniFile.ReadString("KeyboardMouse", "ActivateOnlyInWindow", "PlayStation Plus");
	std::string WindowTitle2 = IniFile.ReadString("KeyboardMouse", "ActivateOnlyInWindow2", "PS4 Remote Play");
	int FullScreenTopOffset = IniFile.ReadInteger("KeyboardMouse", "FullScreenTopOffset", -50);
	bool HideTaskBar = IniFile.ReadBoolean("KeyboardMouse", "HideTaskBarInFullScreen", true);
	bool FullScreenMode = false;
	bool ActivateInAnyWindow = IniFile.ReadBoolean("KeyboardMouse", "ActivateInAnyWindow", false);
	bool EmulateAnalogTriggers = IniFile.ReadBoolean("KeyboardMouse", "EmulateAnalogTriggers", false);
	float LeftTriggerValue = 0;
	float RightTriggerValue = 0;
	float StepTriggerValue = IniFile.ReadFloat("KeyboardMouse", "AnalogTriggerStep", 15);
	mouseSensetiveX = IniFile.ReadFloat("KeyboardMouse", "SensX", 15);
	mouseSensetiveY = IniFile.ReadFloat("KeyboardMouse", "SensY", 15);
	int DeadZoneDS4 = IniFile.ReadInteger("KeyboardMouse", "DeadZone", 0); // 25, makes mouse movement smoother when moving slowly (12->25)

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
	int KEY_ID_SHARE = IniFile.ReadInteger("Keys", "SHARE", VK_F12);
	int KEY_ID_TOUCHPAD = IniFile.ReadInteger("Keys", "TOUCHPAD", VK_RETURN);
	int KEY_ID_OPTIONS = IniFile.ReadInteger("Keys", "OPTIONS", VK_TAB);
	int KEY_ID_PS = IniFile.ReadInteger("Keys", "PS", VK_F2);

	int KEY_ID_SHAKING = IniFile.ReadInteger("Keys", "SHAKING", 'T');

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
	bool MotionShaking = false, MotionShakingSwap = false;

	int SkipPollCount = 0;

	// Load library and scan Xbox gamepads
	hDll = LoadLibrary("xinput1_3.dll"); // x360ce support
	if (hDll == NULL) hDll = LoadLibrary("xinput1_4.dll");
	if (hDll != NULL) {
		MyXInputGetState = (_XInputGetState)GetProcAddress(hDll, (LPCSTR)100); // "XInputGetState"); // Ordinal 100 is the same as XInputGetState but supports the Guide button. Taken here https://github.com/speps/XInputDotNet/blob/master/XInputInterface/GamePad.cpp
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

	// Write current mode
	if (EmulationMode == XboxMode)
		printf("\n Emulation with Xbox controller.\n");
	else {
		printf("\r\n Emulation with keyboard and mouse.\n");
		if (ActivateInAnyWindow == false)
			printf(" Activate in any window is disabled, so the emulated gamepad work only in \"PS Plus\" and \"PS4 Remote Play\".\n");
		printf(" Hold down \"C\" to for cursor movement.\n");
	}
	if (EmulationMode == KBMode) {
		printf(" Press \"ALT\" + \"F10\" to switch \"PS Plus\" to full-screen mode or return to normal.\n");
		if (CursorHidden)
			printf(" The cursor is hidden. To display the cursor, press \"ALT\" + \"Escape\" or \"exit key\".\n");
		else
			printf(" The cursor is not hidden. To hide the cursor, press \"ALT\" + \"F2\".\n");
	}
	printf(" Press \"ALT\" + \"Escape\" or \"exit key\" to exit.\n");

	DS4_TOUCH BuffPreviousTouch[2] = { 0, 0 };
	BuffPreviousTouch[0].bIsUpTrackingNum1 = 0x80;
	BuffPreviousTouch[1].bIsUpTrackingNum1 = 0x80;
	unsigned char TouchIndex = 0;
	bool AllowIncTouchIndex;
	bool DeadZoneMode = false;

	while (!((GetAsyncKeyState(KEY_ID_EXIT) & 0x8000) || ((GetAsyncKeyState(VK_LMENU) & 0x8000) && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) )) // "~" by default
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
				if ((GetAsyncKeyState(VK_F9) & 0x8000) != 0 && ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) && SkipPollCount == 0)
				{
					DeadZoneMode = !DeadZoneMode;
					if (DeadZoneMode == false) {
						system("cls");
						printf("\n Emulation with Xbox controller.\n");
						printf(" Press \"ALT\" + \"Escape\" or \"exit key\" to exit.\n");
					}
					SkipPollCount = SkipPollTimeOut;
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
					myPState.Gamepad.wButtons &= ~XINPUT_GAMEPAD_BACK; myPState.Gamepad.wButtons &= ~XINPUT_GAMEPAD_START;
					if (SwapShareTouchPad)
						report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
					else
						report.wButtons |= DS4_BUTTON_SHARE;
				}

				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					report.wButtons |= DS4_BUTTON_OPTIONS;

				// PS button
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) {
					myPState.Gamepad.wButtons &= ~XINPUT_GAMEPAD_BACK; myPState.Gamepad.wButtons &= ~XINPUT_GAMEPAD_LEFT_SHOULDER;
					report.bSpecial |= DS4_SPECIAL_BUTTON_PS;
				}

				if (EnableXboxButton && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_GUIDE) report.bSpecial |= DS4_SPECIAL_BUTTON_PS;

				// Motion shaking
				if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK && myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
					myPState.Gamepad.wButtons &= ~XINPUT_GAMEPAD_BACK; myPState.Gamepad.wButtons &= ~XINPUT_GAMEPAD_RIGHT_SHOULDER;
					MotionShaking = true;
				} else MotionShaking = false;

				// Swap share and touchpad
				if (SwapShareTouchPad == false) {
					if ((GetAsyncKeyState(KEY_ID_SHARE) & 0x8000) != 0)
						report.wButtons |= DS4_BUTTON_SHARE;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
						report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
				} else {
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
					if (!TouchPadPressedWhenSwiping && (report.bThumbRX != 127 || report.bThumbRY != 129) ) {
						report.bSpecial &= ~DS4_SPECIAL_BUTTON_TOUCHPAD;
						if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) { report.wButtons &= ~DS4_BUTTON_THUMB_RIGHT; report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD; }
					}
					TouchX = 960; TouchY = 471;
					if (report.bThumbRX > 127)
						TouchX = 320 + (report.bThumbRX - 127) * 10;
					if (report.bThumbRX < 127)
						TouchX = 1600 - (-report.bThumbRX + 127) * 10;
					if (report.bThumbRY > 129)
						TouchY = trunc(100 + (report.bThumbRY - 129) * 5.8);
					if (report.bThumbRY < 129)
						TouchY = trunc(743 - (-report.bThumbRY + 129) * 5.8);
					report.bThumbRX = 127;
					report.bThumbRY = 129;
				}
			}
		}
		// Mouse and keyboard mode
		else if (EmulationMode == KBMode) {
			
			PSPlusWindow = FindWindow(NULL, WindowTitle.c_str());
			bool PSNowFound = (PSPlusWindow != 0) && (IsWindowVisible(PSPlusWindow)) && (PSPlusWindow == GetForegroundWindow());

			PSRemotePlayWindow = FindWindow(NULL, WindowTitle2.c_str());
			bool PSRemotePlayFound = (PSRemotePlayWindow != 0) && (IsWindowVisible(PSRemotePlayWindow)) && (PSRemotePlayWindow == GetForegroundWindow());

			if ((GetAsyncKeyState(VK_LMENU) & 0x8000) && (GetAsyncKeyState(VK_F10) & 0x8000) && SkipPollCount == 0) {
				if (PSPlusWindow != 0)
					if (FullScreenMode) {
						if (HideTaskBar) ShowWindow(FindWindow("Shell_TrayWnd", NULL), SW_SHOW);
						SetWindowPos(PSPlusWindow, HWND_TOP, m_HalfWidth - 640, m_HalfHeight - 360, 1280, 720, SWP_FRAMECHANGED);
					} else {
						SetForegroundWindow(PSPlusWindow);
						SetActiveWindow(PSPlusWindow);
						if (HideTaskBar) ShowWindow(FindWindow("Shell_TrayWnd", NULL), SW_HIDE);
						SetWindowPos(PSPlusWindow, HWND_TOP, 0, FullScreenTopOffset, m_HalfWidth * 2, m_HalfHeight * 2 + (-FullScreenTopOffset), SWP_FRAMECHANGED);
					}
				FullScreenMode = !FullScreenMode;
				SkipPollCount = SkipPollTimeOut;
			}

			if (CursorHidden == false && (GetAsyncKeyState(VK_LMENU) & 0x8000) && (GetAsyncKeyState(VK_F2) & 0x8000) && SkipPollCount == 0) {
				SetSystemCursor(CursorEmpty, OCR_NORMAL); CursorHidden = true;
				printf(" The cursor is hidden. To display the cursor, press \"ALT\" + \"Escape\" or \"exit key\".\n");
				SkipPollCount = SkipPollTimeOut;
			}

			if (ActivateInAnyWindow || PSNowFound || PSRemotePlayFound) {
				if ((GetAsyncKeyState(KEY_ID_STOP_CENTERING) & 0x8000) == 0) GetMouseState();

				if (InvertX)
					DeltaMouseX = DeltaMouseX * -1;
				if (InvertY)
					DeltaMouseY = DeltaMouseY * -1;

				// Are there better options? / Есть вариант лучше?
				if (DeltaMouseX > 0)
					report.bThumbRX = 128 + DeadZoneDS4 + trunc( Clamp(DeltaMouseX * mouseSensetiveX, 0, 127 - DeadZoneDS4) );
				if (DeltaMouseX < 0)
					report.bThumbRX = 128 - DeadZoneDS4 + trunc( Clamp(DeltaMouseX * mouseSensetiveX, -127 + DeadZoneDS4, 0) );
				if (DeltaMouseY < 0)
					report.bThumbRY = 128 - DeadZoneDS4 + trunc( Clamp(DeltaMouseY * mouseSensetiveY, -127 + DeadZoneDS4, 0) );
				if (DeltaMouseY > 0)
					report.bThumbRY = 128 + DeadZoneDS4 + trunc( Clamp(DeltaMouseY * mouseSensetiveY, 0, 127 - DeadZoneDS4) );
			
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
					
					report.bTriggerL = trunc( Clamp(LeftTriggerValue, 0, 255) );

					if ((GetAsyncKeyState(KEY_ID_RIGHT_TRIGGER) & 0x8000) != 0) {
						if (RightTriggerValue < 255)
							RightTriggerValue += StepTriggerValue;
					}
					else {
						if (RightTriggerValue > 0)
							RightTriggerValue -= StepTriggerValue;
					}

					report.bTriggerR = trunc( Clamp(RightTriggerValue, 0, 255) );
				}
				
				if (report.bTriggerL > 0) // Specific of DualShock 
					report.wButtons |= DS4_BUTTON_TRIGGER_LEFT; 
				if (report.bTriggerR > 0) // Specific of DualShock
					report.wButtons |= DS4_BUTTON_TRIGGER_RIGHT;

				if ((GetAsyncKeyState(KEY_ID_SHARE) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_SHARE;
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD) & 0x8000) != 0)
					report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
				if ((GetAsyncKeyState(KEY_ID_OPTIONS) & 0x8000) != 0)
					report.wButtons |= DS4_BUTTON_OPTIONS;

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
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_LEFT) & 0x8000) != 0) {TouchX = 320; TouchY = 471; }
				// Center
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_CENTER) & 0x8000) != 0) { TouchX = 960; TouchY = 471; }
				// Right
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_RIGHT) & 0x8000) != 0) { TouchX = 1600; TouchY = 471; }
				// Center up
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_UP) & 0x8000) != 0) { if (TouchX == 0) TouchX = 960; TouchY = 157; }
				// Center down
				if ((GetAsyncKeyState(KEY_ID_TOUCHPAD_DOWN) & 0x8000) != 0) { if (TouchX == 0) TouchX = 960; TouchY = 785; }

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

				// Motion shaking
				MotionShaking = (GetAsyncKeyState(KEY_ID_SHAKING) & 0x8000) != 0;
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

		report.wAccelX = trunc( Clamp(AccelX * 1638.35 * 1.0479 / 2, -32767, 32767) ) * InverseXStatus * MotionSens; // freepie accel max 19.61, min -20, short -32,768 to 32,767
		report.wAccelY = trunc( Clamp(AccelY * 1638.35 * 1.0479 / 2, -32767, 32767) ) * InverseYStatus * MotionSens;
		report.wAccelZ = trunc( Clamp(AccelZ * 1638.35 * 1.0479 / 2, -32767, 32767) ) * InverseZStatus * MotionSens;
		report.wGyroX = trunc( Clamp(GyroX * 2376.7 * 0.8 / 2, -32767, 32767) ) * InverseXStatus * MotionSens; // freepie max gyro 10, min -10.09
		report.wGyroY = trunc( Clamp(GyroY * 2376.7 * 0.8 / 2, -32767, 32767) ) * InverseYStatus * MotionSens;
		report.wGyroZ = trunc( Clamp(GyroZ * 2376.7 * 0.8 / 2, -32767, 32767) ) * InverseZStatus * MotionSens; // if ((GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0) printf("%d\t%d\t%d\t%d\t%d\t%d\t\n", report.wAccelX, report.wAccelY, report.wAccelZ, report.wGyroX, report.wGyroY, report.wGyroZ);

		// Motion shaking
		if (MotionShaking) {
			MotionShakingSwap = !MotionShakingSwap;
			if (MotionShakingSwap) {
				report.wAccelX = -6530;		report.wAccelY = 6950;		report.wAccelZ = -710;
				report.wGyroX = 2300;		report.wGyroY = 5000;		report.wGyroZ = 10;
			} else {
				report.wAccelX = 6830;		report.wAccelY = 7910;		report.wAccelZ = 1360;
				report.wGyroX = 2700;		report.wGyroY = -5000;		report.wGyroZ = 140;
			}
		}

		// Multi mode keys
		if ((GetAsyncKeyState(KEY_ID_PS) & 0x8000) != 0)
			report.bSpecial |= DS4_SPECIAL_BUTTON_PS;

		// if ((GetAsyncKeyState(VK_NUMPAD0) & 0x8000) != 0) system("cls");

		curTimeStamp++; if (curTimeStamp > 65535) curTimeStamp = 0; // Is it right? / Правильно ли это?
		report.wTimestamp = curTimeStamp;

		ret = vigem_target_ds4_update_ex(client, ds4, report);

		// Don't overload the CPU with reading
		if (EmulationMode == XboxMode)
			Sleep(SleepTimeOutXbox);
		else
			Sleep(SleepTimeOutKB);

		if (SkipPollCount > 0) SkipPollCount--;
	}

	if (CursorHidden) SetSystemCursor(CurCursor, OCR_NORMAL);

	vigem_target_remove(client, ds4);
	vigem_target_free(ds4);
	vigem_free(client);
	FreeLibrary(hDll);
	hDll = nullptr;

	if (SocketActivated) {
		SocketActivated = false;
		if (pSocketThread) {
			pSocketThread->join();
			delete pSocketThread;
			pSocketThread = nullptr;
		}
		closesocket(socketS);
		WSACleanup();
	}
}
