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
USHORT curTimeStamp = 0;

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
			if (MotionOrientation) { // landscape mapping
				AccelZ = bytesToFloat(freePieIMU[2], freePieIMU[3], freePieIMU[4], freePieIMU[5]);
				AccelX = bytesToFloat(freePieIMU[6], freePieIMU[7], freePieIMU[8], freePieIMU[9]);
				AccelY = bytesToFloat(freePieIMU[10], freePieIMU[11], freePieIMU[12], freePieIMU[13]);

				GyroZ = bytesToFloat(freePieIMU[14], freePieIMU[15], freePieIMU[16], freePieIMU[17]);
				GyroX = bytesToFloat(freePieIMU[18], freePieIMU[19], freePieIMU[20], freePieIMU[21]);
				GyroY = bytesToFloat(freePieIMU[22], freePieIMU[23], freePieIMU[24], freePieIMU[25]);
			} else { // portrait mapping
				AccelX = bytesToFloat(freePieIMU[2], freePieIMU[3], freePieIMU[4], freePieIMU[5]);
				AccelZ = bytesToFloat(freePieIMU[6], freePieIMU[7], freePieIMU[8], freePieIMU[9]);
				AccelY = bytesToFloat(freePieIMU[10], freePieIMU[11], freePieIMU[12], freePieIMU[13]);

				GyroX = bytesToFloat(freePieIMU[14], freePieIMU[15], freePieIMU[16], freePieIMU[17]);
				GyroZ = bytesToFloat(freePieIMU[18], freePieIMU[19], freePieIMU[20], freePieIMU[21]);
				GyroY = bytesToFloat(freePieIMU[22], freePieIMU[23], freePieIMU[24], freePieIMU[25]);
			}
		} else Sleep(SleepTimeOutMotion); // Don't overload the CPU with reading
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
	else if (Value < Min)
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

void LoadKMProfile(std::string ProfileFile) {
	CIniReader IniFile("Profiles\\" + ProfileFile);

	KEY_ID_LEFT_STICK_UP = KeyNameToKeyCode(IniFile.ReadString("Keys", "LEFT-STICK-UP", "W"));
	KEY_ID_LEFT_STICK_LEFT = KeyNameToKeyCode(IniFile.ReadString("Keys", "LEFT-STICK-LEFT", "A"));
	KEY_ID_LEFT_STICK_RIGHT = KeyNameToKeyCode(IniFile.ReadString("Keys", "LEFT-STICK-RIGHT", "D"));
	KEY_ID_LEFT_STICK_DOWN = KeyNameToKeyCode(IniFile.ReadString("Keys", "LEFT-STICK-DOWN", "S"));
	KEY_ID_LEFT_TRIGGER = KeyNameToKeyCode(IniFile.ReadString("Keys", "L2", "MOUSE-RIGHT-BTN"));
	KEY_ID_RIGHT_TRIGGER = KeyNameToKeyCode(IniFile.ReadString("Keys", "R2", "MOUSE-LEFT-BTN"));
	KEY_ID_LEFT_SHOULDER = KeyNameToKeyCode(IniFile.ReadString("Keys", "L1", "ALT"));
	KEY_ID_RIGHT_SHOULDER = KeyNameToKeyCode(IniFile.ReadString("Keys", "R1", "CTRL"));
	KEY_ID_DPAD_UP = KeyNameToKeyCode(IniFile.ReadString("Keys", "DPAD-UP", "1"));
	KEY_ID_DPAD_LEFT = KeyNameToKeyCode(IniFile.ReadString("Keys", "DPAD-LEFT", "2"));
	KEY_ID_DPAD_RIGHT = KeyNameToKeyCode(IniFile.ReadString("Keys", "DPAD-RIGHT", "3"));
	KEY_ID_DPAD_DOWN = KeyNameToKeyCode(IniFile.ReadString("Keys", "DPAD-DOWN", "4"));
	KEY_ID_LEFT_THUMB = KeyNameToKeyCode(IniFile.ReadString("Keys", "L3", "SHIFT"));
	KEY_ID_RIGHT_THUMB = KeyNameToKeyCode(IniFile.ReadString("Keys", "R3", "MOUSE-MIDDLE-BTN"));
	KEY_ID_TRIANGLE = KeyNameToKeyCode(IniFile.ReadString("Keys", "TRIANGLE", "E"));
	KEY_ID_SQUARE = KeyNameToKeyCode(IniFile.ReadString("Keys", "SQUARE", "R"));
	KEY_ID_CIRCLE = KeyNameToKeyCode(IniFile.ReadString("Keys", "CIRCLE", "Q"));
	KEY_ID_CROSS = KeyNameToKeyCode(IniFile.ReadString("Keys", "CROSS", "SPACE"));
	KEY_ID_SHARE = KeyNameToKeyCode(IniFile.ReadString("Keys", "SHARE", "F12"));
	KEY_ID_TOUCHPAD = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD", "ENTER"));
	KEY_ID_OPTIONS = KeyNameToKeyCode(IniFile.ReadString("Keys", "OPTIONS", "TAB"));
	KEY_ID_PS = KeyNameToKeyCode(IniFile.ReadString("Keys", "PS", "F2"));

	KEY_ID_SHAKING = KeyNameToKeyCode(IniFile.ReadString("Keys", "SHAKING", "T"));
	KEY_ID_MOTION_UP = KeyNameToKeyCode(IniFile.ReadString("Keys", "MOTION-UP", "NUMPAD8"));
	KEY_ID_MOTION_DOWN = KeyNameToKeyCode(IniFile.ReadString("Keys", "MOTION-DOWN", "NUMPAD2"));
	KEY_ID_MOTION_LEFT = KeyNameToKeyCode(IniFile.ReadString("Keys", "MOTION-LEFT", "NUMPAD4"));
	KEY_ID_MOTION_RIGHT = KeyNameToKeyCode(IniFile.ReadString("Keys", "MOTION-RIGHT", "NUMPAD6"));

	KEY_ID_TOUCHPAD_SWIPE_UP = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-SWIPE-UP", "7"));
	KEY_ID_TOUCHPAD_SWIPE_DOWN = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-SWIPE-DOWN", "8"));
	KEY_ID_TOUCHPAD_SWIPE_LEFT = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-SWIPE-LEFT", "9"));
	KEY_ID_TOUCHPAD_SWIPE_RIGHT = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-SWIPE-RIGHT", "0"));

	KEY_ID_TOUCHPAD_UP = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-UP", "U"));
	KEY_ID_TOUCHPAD_DOWN = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-DOWN", "N"));
	KEY_ID_TOUCHPAD_LEFT = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-LEFT", "H"));
	KEY_ID_TOUCHPAD_RIGHT = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-RIGHT", "K"));
	KEY_ID_TOUCHPAD_CENTER = KeyNameToKeyCode(IniFile.ReadString("Keys", "TOUCHPAD-CENTER", "J"));
}

void MainTextUpdate() {
	system("cls");

	if (EmulationMode == XboxMode) {
		printf("\n Emulation with Xbox controller.\n");
		if (SwapShareTouchPad == false)
			printf(" Touchpad press: \"BACK\", Share press = \"BACK\" + \"START\".\n");
		else
			printf(" Touchpad press: \"BACK\" + \"START\", Share press = \"BACK\".\n");
		printf_s(" Touchpad movement: \"BACK\" + \"RIGHT-STICK\".\n Motion shaking: \"%s\" + \"%s\".\n", KEY_ID_XBOX_ACTIVATE_MOTION_NAME.c_str(), KEY_ID_XBOX_SHAKING_NAME.c_str());
		printf_s(" Motion up, down, left, right: \"%s\", \"%s\", \"%s\", \"%s\".\n", KEY_ID_XBOX_MOTION_UP_NAME.c_str(), KEY_ID_XBOX_MOTION_DOWN_NAME.c_str(), KEY_ID_XBOX_MOTION_LEFT_NAME.c_str(), KEY_ID_XBOX_MOTION_RIGHT_NAME.c_str());

	}
	else {
		printf("\r\n Emulation with keyboard and mouse.\n");
		if (ActivateInAnyWindow == false)
			printf(" Activate in any window is disabled, so the emulated gamepad work only in \"PS Plus\" and \"PS4 Remote Play\".\n Switch mode with \"ALT + F3\"\n");
		else
			printf(" Activate in any window is enabled. Switch mode with \"ALT + F3\"\n");
		printf_s(" Enable or disable cursor movement on the \"ALT + %s\" button (right stick emulation).\n", KEY_ID_STOP_CENTERING_NAME.c_str());
		printf_s(" Keyboard and mouse profile: \"%s\". Change profiles with \"ALT\" + \"Up | Down\".\n", KMProfiles[ProfileIndex].c_str());
	}
	if (EmulationMode == KBMode) {
		printf(" Press \"ALT\" + \"F10\" to switch \"PS Plus\" to full-screen mode or return to normal.\n");
		if (CursorHidden)
			printf(" The cursor is hidden. To display the cursor, press \"ALT\" + \"Escape\".\n");
		else
			printf(" The cursor is not hidden. To hide the cursor, press \"ALT\" + \"F2\".\n");
	}
	printf(" Press \"ALT\" + \"Escape\" to exit.\n");
}

int main(int argc, char **argv)
{
	SetConsoleTitle("DS4Emulator 2.0");

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
	int MotionSens = IniFile.ReadInteger("Motion", "Sens", 100); // sampling(timestamp) speed of motion sensor
	// min & max programmable ds4 gyro +/-125 degrees/s = 2 rad/s to 2000 degrees/s = 35 rad/s
	float GyroSens = 35 - IniFile.ReadInteger("Motion", "GyroSens", 100)/100 * 33; // 33 = max - min
	// min & max programmable ds4 accel +/-2g = 20 m/s^2  to 16g = 160 m/s^2
	float AccelSens = 160 - IniFile.ReadInteger("Motion", "AccelSens", 100)/100 * 140; // 140 = max - min
	int InverseXStatus = IniFile.ReadBoolean("Motion", "InverseX", false) == false ? 1 : -1 ;
	int InverseYStatus = IniFile.ReadBoolean("Motion", "InverseY", false) == false ? 1 : -1 ;
	int InverseZStatus = IniFile.ReadBoolean("Motion", "InverseZ", false) == false ? 1 : -1 ;
	MotionOrientation = IniFile.ReadBoolean("Motion", "Orientation", true);

	SleepTimeOutMotion = IniFile.ReadInteger("Motion", "SleepTimeOut", 1);

	#define OCR_NORMAL 32512
	HCURSOR CurCursor = CopyCursor(LoadCursor(0, IDC_ARROW));
	HCURSOR CursorEmpty = LoadCursorFromFile("EmptyCursor.cur");
	CursorHidden = IniFile.ReadBoolean("KeyboardMouse", "HideCursorAfterStart", false);
	if (CursorHidden) { SetSystemCursor(CursorEmpty, OCR_NORMAL); CursorHidden = true; }

	// Config parameters
	KEY_ID_STOP_CENTERING_NAME = IniFile.ReadString("KeyboardMouse", "StopCenteringKey", "C");
	int KEY_ID_STOP_CENTERING = KeyNameToKeyCode(KEY_ID_STOP_CENTERING_NAME);
	bool CenteringEnable = true;

	bool InvertX = IniFile.ReadBoolean("Main", "InvertX", false);
	bool InvertY = IniFile.ReadBoolean("Main", "InvertY", false);

	int SleepTimeOutXbox = IniFile.ReadInteger("Xbox", "SleepTimeOut", 1);
	bool SwapTriggersShoulders = IniFile.ReadBoolean("Xbox", "SwapTriggersShoulders", false);
	SwapShareTouchPad = IniFile.ReadBoolean("Xbox", "SwapShareTouchPad", false);
	bool TouchPadPressedWhenSwiping = IniFile.ReadBoolean("Xbox", "TouchPadPressedWhenSwiping", true);
	bool EnableXboxButton = IniFile.ReadBoolean("Xbox", "EnableXboxButton", true);

	KEY_ID_XBOX_ACTIVATE_MOTION_NAME = IniFile.ReadString("Xbox", "MotionActivateKey", "BACK");
	int KEY_ID_XBOX_ACTIVATE_MOTION = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_ACTIVATE_MOTION_NAME);
	KEY_ID_XBOX_SHAKING_NAME = IniFile.ReadString("Xbox", "MotionShakingKey", "RIGHT-SHOULDER");
	int KEY_ID_XBOX_SHAKING = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_SHAKING_NAME);
	
	KEY_ID_XBOX_MOTION_UP_NAME = IniFile.ReadString("Xbox", "MotionUp", "DPAD-UP");
	KEY_ID_XBOX_MOTION_DOWN_NAME = IniFile.ReadString("Xbox", "MotionDown", "DPAD-DOWN");
	KEY_ID_XBOX_MOTION_LEFT_NAME = IniFile.ReadString("Xbox", "MotionLeft", "DPAD-LEFT");
	KEY_ID_XBOX_MOTION_RIGHT_NAME = IniFile.ReadString("Xbox", "MotionRight", "DPAD-RIGHT");
	int KEY_ID_XBOX_MOTION_UP = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_UP_NAME);
	int KEY_ID_XBOX_MOTION_DOWN = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_DOWN_NAME);
	int KEY_ID_XBOX_MOTION_LEFT = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_LEFT_NAME);
	int KEY_ID_XBOX_MOTION_RIGHT = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_RIGHT_NAME);

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
	ActivateInAnyWindow = IniFile.ReadBoolean("KeyboardMouse", "ActivateInAnyWindow", false);
	bool EmulateAnalogTriggers = IniFile.ReadBoolean("KeyboardMouse", "EmulateAnalogTriggers", false);
	float LeftTriggerValue = 0;
	float RightTriggerValue = 0;
	float StepTriggerValue = IniFile.ReadFloat("KeyboardMouse", "AnalogTriggerStep", 15);
	mouseSensetiveX = IniFile.ReadFloat("KeyboardMouse", "SensX", 15);
	mouseSensetiveY = IniFile.ReadFloat("KeyboardMouse", "SensY", 15);
	int DeadZoneDS4 = IniFile.ReadInteger("KeyboardMouse", "DeadZone", 0); // 25, makes mouse movement smoother when moving slowly (12->25)

	// Search keyboard and mouses profiles
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	hFind = FindFirstFile("Profiles\\*.ini", &ffd);
	KMProfiles.push_back("Default.ini");
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (strcmp(ffd.cFileName, "Default.ini")) // Already added to the top of the list
				KMProfiles.push_back(ffd.cFileName);
		} while (FindNextFile(hFind, &ffd) != 0);
		FindClose(hFind);
	}

	// Load default profile (Shaking key for Xbox)
	LoadKMProfile(KMProfiles[ProfileIndex]);

	const auto client = vigem_alloc();
	auto ret = vigem_connect(client);
	const auto ds4 = vigem_target_ds4_alloc();
	ret = vigem_target_add(client, ds4);
	ret = vigem_target_ds4_register_notification(client, ds4, &notification, nullptr);
	DS4_REPORT_EX report;
	bool TouchpadSwipeUp = false, TouchpadSwipeDown = false;
	bool TouchpadSwipeLeft = false, TouchpadSwipeRight = false;
	bool MotionShaking = false, MotionShakingSwap = false;
	bool MotionUp = false, MotionDown = false, MotionLeft = false, MotionRight = false;

	int MotionKeyOnlyCheckCount = 0;
	int MotionKeyReleasedCount = 0;
	bool MotionKeyOnlyPressed = false;

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
			if (MyXInputGetState(i, &myPState) == ERROR_SUCCESS)
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
	MainTextUpdate();

	DS4_TOUCH BuffPreviousTouch[2] = { 0, 0 };
	BuffPreviousTouch[0].bIsUpTrackingNum1 = 0x80;
	BuffPreviousTouch[1].bIsUpTrackingNum1 = 0x80;
	unsigned char TouchIndex = 0;
	bool AllowIncTouchIndex;
	bool DeadZoneMode = false;

	while ( !( (GetAsyncKeyState(VK_LMENU) & 0x8000 && GetAsyncKeyState(VK_ESCAPE) & 0x8000) ) )
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
		
		//report.bBatteryLvl = 8;
		//report.bBatteryLvlSpecial = 32; ??? not working

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
					if (DeadZoneMode == false)
						MainTextUpdate();
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
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MOTION && myPState.Gamepad.wButtons & KEY_ID_XBOX_SHAKING) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MOTION; myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_SHAKING;
					MotionShaking = true;
				} else
					MotionShaking = false;

				// Motion up
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MOTION && myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_UP) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MOTION; myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_UP;
					MotionUp = true;
				}
				else
					MotionUp = false;

				// Motion down
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MOTION && myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_DOWN) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MOTION; myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_DOWN;
					MotionDown = true;
				}
				else
					MotionDown = false;

				// Motion left
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MOTION && myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_LEFT) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MOTION; myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_LEFT;
					MotionLeft = true;

				}
				else
					MotionLeft = false;

				// Motion right
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MOTION && myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_RIGHT) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MOTION; myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_RIGHT;
					MotionRight = true;
				}
				else
					MotionRight = false;

				// Motion key exclude
				if (myPState.Gamepad.wButtons == KEY_ID_XBOX_ACTIVATE_MOTION && MotionKeyOnlyCheckCount == 0) { MotionKeyOnlyCheckCount = 20; MotionKeyOnlyPressed = true; }
				if (MotionKeyOnlyCheckCount > 0) {
					if (MotionKeyOnlyCheckCount == 1 && MotionKeyOnlyPressed)
						MotionKeyReleasedCount = 30; // Timeout to release the motion key button and don't execute other buttons
					MotionKeyOnlyCheckCount--;
					if (myPState.Gamepad.wButtons != KEY_ID_XBOX_ACTIVATE_MOTION && myPState.Gamepad.wButtons != 0) { MotionKeyOnlyPressed = false; MotionKeyOnlyCheckCount = 0; }
				}
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MOTION && myPState.Gamepad.wButtons != KEY_ID_XBOX_ACTIVATE_MOTION) MotionKeyReleasedCount = 30;
				if (MotionKeyReleasedCount > 0) MotionKeyReleasedCount--;
				 
				//printf("%d %d %d\n", MotionKeyOnlyCheckCount, MotionKeyReleasedCount, MotionKeyOnlyPressed);

				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MOTION) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MOTION;
				}
				if (MotionKeyOnlyCheckCount == 1 && MotionKeyOnlyPressed)
					myPState.Gamepad.wButtons = KEY_ID_XBOX_ACTIVATE_MOTION; //|=

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

				// Exclude multi keys
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MOTION == false) {
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
				}

				// Touchpad swipes
				if (report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD) {
					if (!TouchPadPressedWhenSwiping && (report.bThumbRX != 127 || report.bThumbRY != 129)) {
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

			// Switch profiles
			if (((GetAsyncKeyState(VK_MENU) & 0x8000) != 0 && ((GetAsyncKeyState(VK_UP) & 0x8000) != 0 || (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0)) && SkipPollCount == 0) {
				SkipPollCount = SkipPollTimeOut;
				if ((GetAsyncKeyState(VK_UP) & 0x8000) != 0) if (ProfileIndex > 0) ProfileIndex--; else ProfileIndex = KMProfiles.size() - 1;
				if ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) if (ProfileIndex < KMProfiles.size() - 1) ProfileIndex++; else ProfileIndex = 0;
				MainTextUpdate();
				LoadKMProfile(KMProfiles[ProfileIndex]);
			}

			// Custom fullscreen mode
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

			// Cursor hide
			if (CursorHidden == false && (GetAsyncKeyState(VK_LMENU) & 0x8000) && (GetAsyncKeyState(VK_F2) & 0x8000) && SkipPollCount == 0) {
				SetSystemCursor(CursorEmpty, OCR_NORMAL); CursorHidden = true;
				printf(" The cursor is hidden. To display the cursor, press \"ALT\" + \"Escape\".\n");
				SkipPollCount = SkipPollTimeOut;
			}

			if ((GetAsyncKeyState(VK_LMENU) & 0x8000) && (GetAsyncKeyState(VK_F3) & 0x8000) && SkipPollCount == 0) {
				ActivateInAnyWindow = !ActivateInAnyWindow;
				MainTextUpdate();
				SkipPollCount = SkipPollTimeOut;
			}

			if (ActivateInAnyWindow || PSNowFound || PSRemotePlayFound) {
				if ((GetAsyncKeyState(VK_LMENU) & 0x8000) && GetAsyncKeyState(KEY_ID_STOP_CENTERING) & 0x8000 &&  SkipPollCount == 0) { CenteringEnable = !CenteringEnable;  SkipPollCount = SkipPollTimeOut;}
				if (CenteringEnable) GetMouseState();

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
				
				MotionUp = (GetAsyncKeyState(KEY_ID_MOTION_UP) & 0x8000) != 0;
				MotionDown = (GetAsyncKeyState(KEY_ID_MOTION_DOWN) & 0x8000) != 0;
				MotionLeft = (GetAsyncKeyState(KEY_ID_MOTION_LEFT) & 0x8000) != 0;
				MotionRight = (GetAsyncKeyState(KEY_ID_MOTION_RIGHT) & 0x8000) != 0;
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

		// freePIE gyro and accel in rad/s and m/s^2, need to scale with accel and gyro configs
		report.wAccelX = trunc(Clamp(AccelX / AccelSens * 32767, -32767, 32767)) * InverseXStatus;
		report.wAccelY = trunc(Clamp(AccelY / AccelSens * 32767, -32767, 32767)) * InverseYStatus;
		report.wAccelZ = trunc(Clamp(AccelZ / AccelSens * 32767, -32767, 32767)) * InverseZStatus;
		report.wGyroX = trunc(Clamp(GyroX / GyroSens * 32767, -32767, 32767)) * InverseXStatus;
		report.wGyroY = trunc(Clamp(GyroY / GyroSens * 32767, -32767, 32767)) * InverseYStatus;
		report.wGyroZ = trunc(Clamp(GyroZ / GyroSens * 32767, -32767, 32767)) * InverseZStatus;
		// if ((GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0) printf("%d\t%d\t%d\t%d\t%d\t%d\t\n", report.wAccelX, report.wAccelY, report.wAccelZ, report.wGyroX, report.wGyroY, report.wGyroZ);

		// Def motion???
		//report.wAccelX = 0;		report.wAccelY = 32767;		report.wAccelZ = 0;
		//report.wGyroX = 0;		report.wGyroY = 32767;		report.wGyroZ = 0;

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
		} else if (MotionUp) {
			report.wAccelX = 0;		report.wAccelY = 0;		report.wAccelZ = 32767;
			report.wGyroX = 0;		report.wGyroY = 0;		report.wGyroZ = 32767;
		} else if (MotionDown) {
			report.wAccelX = 0;		report.wAccelY = 0;		report.wAccelZ = -32767;
			report.wGyroX = 0;		report.wGyroY = 0;		report.wGyroZ = -32767;
		} else if (MotionLeft) {
			report.wAccelX = 32767;		report.wAccelY = 0;		report.wAccelZ = 0;
			report.wGyroX = 32767;		report.wGyroY = 0;		report.wGyroZ = 0;
		} else if (MotionRight) {
			report.wAccelX = -32767;		report.wAccelY = 0;		report.wAccelZ = 0;
			report.wGyroX = -32767;		report.wGyroY = 0;		report.wGyroZ = 0;
		}

		// Multi mode keys
		if ((GetAsyncKeyState(KEY_ID_PS) & 0x8000) != 0)
			report.bSpecial |= DS4_SPECIAL_BUTTON_PS;

		// if ((GetAsyncKeyState(VK_NUMPAD0) & 0x8000) != 0) system("cls");

		curTimeStamp = curTimeStamp + MotionSens * 100;
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
