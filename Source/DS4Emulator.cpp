#include <Windows.h>
#include <mutex>
#include <chrono>
#include "XboxTouchpad.h"
#include "XboxMotion.h"
#include "XboxMotionConsole.h"
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

	KEY_ID_MOTION_SHAKING_NAME = IniFile.ReadString("Keys", "MOTION-SHAKING", "T");
	KEY_ID_MOTION_X_ADD_NAME = IniFile.ReadString("Keys", "MOTION-X-ADD", "NUMPAD6");  // X+
	KEY_ID_MOTION_X_SUB_NAME = IniFile.ReadString("Keys", "MOTION-X-SUB", "NUMPAD4");  // X−
	KEY_ID_MOTION_Y_ADD_NAME = IniFile.ReadString("Keys", "MOTION-Y-ADD", "NUMPAD8");  // Y+
	KEY_ID_MOTION_Y_SUB_NAME = IniFile.ReadString("Keys", "MOTION-Y-SUB", "NUMPAD2");  // Y−
	KEY_ID_MOTION_Z_ADD_NAME = IniFile.ReadString("Keys", "MOTION-Z-ADD", "NUMPAD9"); // Z+
	KEY_ID_MOTION_Z_SUB_NAME = IniFile.ReadString("Keys", "MOTION-Z-SUB", "NUMPAD7"); // Z−

	KEY_ID_MOTION_SHAKING = KeyNameToKeyCode(KEY_ID_MOTION_SHAKING_NAME.c_str());
	KEY_ID_MOTION_X_ADD = KeyNameToKeyCode(KEY_ID_MOTION_X_ADD_NAME.c_str());  // X+
	KEY_ID_MOTION_X_SUB = KeyNameToKeyCode(KEY_ID_MOTION_X_SUB_NAME.c_str());  // X−
	KEY_ID_MOTION_Y_ADD = KeyNameToKeyCode(KEY_ID_MOTION_Y_ADD_NAME.c_str());  // Y+
	KEY_ID_MOTION_Y_SUB = KeyNameToKeyCode(KEY_ID_MOTION_Y_SUB_NAME.c_str());  // Y−
	KEY_ID_MOTION_Z_ADD = KeyNameToKeyCode(KEY_ID_MOTION_Z_ADD_NAME.c_str()); // Z+
	KEY_ID_MOTION_Z_SUB = KeyNameToKeyCode(KEY_ID_MOTION_Z_SUB_NAME.c_str()); // Z−

	KEY_ID_TOUCHPAD_SWIPE_UP_NAME = IniFile.ReadString("Keys", "TOUCHPAD-SWIPE-UP", "7");
	KEY_ID_TOUCHPAD_SWIPE_DOWN_NAME = IniFile.ReadString("Keys", "TOUCHPAD-SWIPE-DOWN", "8");
	KEY_ID_TOUCHPAD_SWIPE_LEFT_NAME = IniFile.ReadString("Keys", "TOUCHPAD-SWIPE-LEFT", "9");
	KEY_ID_TOUCHPAD_SWIPE_RIGHT_NAME = IniFile.ReadString("Keys", "TOUCHPAD-SWIPE-RIGHT", "0");

	KEY_ID_TOUCHPAD_FIRST_UP_NAME = IniFile.ReadString("Keys", "TOUCHPAD-FIRST-UP", "U");
	KEY_ID_TOUCHPAD_FIRST_DOWN_NAME = IniFile.ReadString("Keys", "TOUCHPAD-FIRST-DOWN", "J");
	KEY_ID_TOUCHPAD_FIRST_LEFT_NAME = IniFile.ReadString("Keys", "TOUCHPAD-FIRST-LEFT", "H");
	KEY_ID_TOUCHPAD_FIRST_RIGHT_NAME = IniFile.ReadString("Keys", "TOUCHPAD-FIRST-RIGHT", "K");

	KEY_ID_TOUCHPAD_SECOND_UP_NAME = IniFile.ReadString("Keys", "TOUCHPAD-SECOND-UP", "HOME");
	KEY_ID_TOUCHPAD_SECOND_DOWN_NAME = IniFile.ReadString("Keys", "TOUCHPAD-SECOND-DOWN", "END");
	KEY_ID_TOUCHPAD_SECOND_LEFT_NAME = IniFile.ReadString("Keys", "TOUCHPAD-SECOND-LEFT", "DELETE");
	KEY_ID_TOUCHPAD_SECOND_RIGHT_NAME = IniFile.ReadString("Keys", "TOUCHPAD-SECOND-RIGHT", "PAGE-DOWN");

	KEY_ID_TOUCHPAD_SWIPE_UP = KeyNameToKeyCode(KEY_ID_TOUCHPAD_SWIPE_UP_NAME.c_str());
	KEY_ID_TOUCHPAD_SWIPE_DOWN = KeyNameToKeyCode(KEY_ID_TOUCHPAD_SWIPE_DOWN_NAME.c_str());
	KEY_ID_TOUCHPAD_SWIPE_LEFT = KeyNameToKeyCode(KEY_ID_TOUCHPAD_SWIPE_LEFT_NAME.c_str());
	KEY_ID_TOUCHPAD_SWIPE_RIGHT = KeyNameToKeyCode(KEY_ID_TOUCHPAD_SWIPE_RIGHT_NAME.c_str());

	KEY_ID_TOUCHPAD_FIRST_UP = KeyNameToKeyCode(KEY_ID_TOUCHPAD_FIRST_UP_NAME.c_str());
	KEY_ID_TOUCHPAD_FIRST_DOWN = KeyNameToKeyCode(KEY_ID_TOUCHPAD_FIRST_DOWN_NAME.c_str());
	KEY_ID_TOUCHPAD_FIRST_LEFT = KeyNameToKeyCode(KEY_ID_TOUCHPAD_FIRST_LEFT_NAME.c_str());
	KEY_ID_TOUCHPAD_FIRST_RIGHT = KeyNameToKeyCode(KEY_ID_TOUCHPAD_FIRST_RIGHT_NAME.c_str());

	KEY_ID_TOUCHPAD_SECOND_UP = KeyNameToKeyCode(KEY_ID_TOUCHPAD_SECOND_UP_NAME.c_str());
	KEY_ID_TOUCHPAD_SECOND_DOWN = KeyNameToKeyCode(KEY_ID_TOUCHPAD_SECOND_DOWN_NAME.c_str());
	KEY_ID_TOUCHPAD_SECOND_LEFT = KeyNameToKeyCode(KEY_ID_TOUCHPAD_SECOND_LEFT_NAME.c_str());
	KEY_ID_TOUCHPAD_SECOND_RIGHT = KeyNameToKeyCode(KEY_ID_TOUCHPAD_SECOND_RIGHT_NAME.c_str());
}

void DefaultMainText() {
	if (EmulationMode == XboxMode) {
		printf("\n Emulating a DualShock 4 using an Xbox controller.\n");
		if (SwapTriggersShoulders == false)
			printf(" Press \"ALT + 1\" to swap bumpers and triggers.\n");
		else
			printf(" Triggers are bumpers, bumpers are triggers. Press \"ALT + 1\" to swap them.\n");

		if (SwapShareTouchPad == false)
			printf(" Touchpad press: \"BACK\", Share press = \"BACK + START\".");
		else
			printf(" Touchpad press: \"BACK + START\", Share press = \"BACK\".");
		printf(" Press \"ALT + 2\" to change.\n");

		if (TouchPadPressedWhenSwiping)
			printf(" The touchpad is pressed when moving on it. Press \"ALT + 3\" to disable.\n");
		else
			printf(" Touchpad don't click when moving on it. Press \"ALT + 3\" to enable.\n");

		printf(" Touchpad movement: \"BACK + RIGHT-STICK\".\n");
		printf_s(" Touches on the keyboard, first: \"%s/%s/%s/%s\", second: \"%s/%s/%s/%s\", swipes: \"%s/%s/%s/%s\".\n",
			KEY_ID_TOUCHPAD_FIRST_UP_NAME.c_str(),
			KEY_ID_TOUCHPAD_FIRST_DOWN_NAME.c_str(),
			KEY_ID_TOUCHPAD_FIRST_LEFT_NAME.c_str(),
			KEY_ID_TOUCHPAD_FIRST_RIGHT_NAME.c_str(),
			KEY_ID_TOUCHPAD_SECOND_UP_NAME.c_str(),
			KEY_ID_TOUCHPAD_SECOND_DOWN_NAME.c_str(),
			KEY_ID_TOUCHPAD_SECOND_LEFT_NAME.c_str(),
			KEY_ID_TOUCHPAD_SECOND_RIGHT_NAME.c_str(),
			KEY_ID_TOUCHPAD_SWIPE_UP_NAME.c_str(),
			KEY_ID_TOUCHPAD_SWIPE_DOWN_NAME.c_str(),
			KEY_ID_TOUCHPAD_SWIPE_LEFT_NAME.c_str(),
			KEY_ID_TOUCHPAD_SWIPE_RIGHT_NAME.c_str()
		);
		printf_s(" Motion shaking: \"%s + %s\".\n", KEY_ID_XBOX_ACTIVATE_MULTI_NAME.c_str(), KEY_ID_XBOX_MOTION_SHAKING_NAME.c_str());
		printf_s(" Motion on the gamepad - X: \"%s/%s\", Y: \"%s/%s\", Z: \"%s/%s\".\n", KEY_ID_XBOX_MOTION_X_ADD_NAME.c_str(), KEY_ID_XBOX_MOTION_X_SUB_NAME.c_str(), KEY_ID_XBOX_MOTION_Y_ADD_NAME.c_str(), KEY_ID_XBOX_MOTION_Y_SUB_NAME.c_str(), KEY_ID_XBOX_MOTION_Z_ADD_NAME.c_str(), KEY_ID_XBOX_MOTION_Z_SUB_NAME.c_str());
		printf_s(" Motion on the keyboard - X: \"%s/%s\", Y: \"%s/%s\", Z: \"%s/%s\".\n", KEY_ID_MOTION_X_ADD_NAME.c_str(), KEY_ID_MOTION_X_SUB_NAME.c_str(), KEY_ID_MOTION_Y_ADD_NAME.c_str(), KEY_ID_MOTION_Y_SUB_NAME.c_str(), KEY_ID_MOTION_Z_ADD_NAME.c_str(), KEY_ID_MOTION_Z_SUB_NAME.c_str());

		printf("\n Press \"ALT + F9\" to view stick dead zones.\n");
	}
	else {
		printf("\n Emulating a DualShock 4 using an keyboard and mouse.\n");
		if (ActivateInAnyWindow == false)
			printf(" Activate in any window is disabled, so the emulated gamepad work only in \"PS Plus\" and \"PS4 Remote Play\".\n Switch mode with \"ALT + F3\".\n");
		else
			printf(" Activate in any window is enabled. Switch mode with \"ALT + F3\".\n");
		printf_s(" Enable or disable cursor movement on the \"ALT + %s\" button (right stick emulation).\n", KEY_ID_STOP_CENTERING_NAME.c_str());
		printf_s(" Keyboard and mouse profile: \"%s\". Change profiles with \"ALT + Up/Down\".\n\n", KMProfiles[ProfileIndex].substr(0, KMProfiles[ProfileIndex].size() - 4).c_str());

		if (LeftAnalogStick == false)
			printf(" The left stick is digital. Press \"ALT + 1\" to emulate analog mode.\n");
		else
			printf(" The left stick is analog. Press \"ALT + 1\" to make it digital.\n");

		if (EmulateAnalogTriggers == false)
			printf(" Triggers are digital. Press \"ALT + 2\" to simulate analog mode.\n");
		else
			printf(" Triggers are analog. Press \"ALT + 2\" to make digital mode.\n");

		if (SwapSticks == false)
			printf(" The left stick is controlled by the keyboard, the right by the mouse. ");
		else
			printf(" The left stick is controlled by the mouse, the right by the keyboard. ");
		printf("Press \"ALT + 3\" to swap them.\n");
		printf_s(" Motion on the keyboard - X: \"%s/%s\", Y: \"%s/%s\", Z: \"%s/%s\".\n", KEY_ID_MOTION_X_ADD_NAME.c_str(), KEY_ID_MOTION_X_SUB_NAME.c_str(), KEY_ID_MOTION_Y_ADD_NAME.c_str(), KEY_ID_MOTION_Y_SUB_NAME.c_str(), KEY_ID_MOTION_Z_ADD_NAME.c_str(), KEY_ID_MOTION_Z_SUB_NAME.c_str());
	}
	if (EmulationMode == KBMode) {
		printf("\n Press \"ALT + F10\" to switch \"PS Plus\" to full-screen mode or return to normal.\n");
		if (CursorHidden)
			printf(" The cursor is hidden. To display the cursor, press \"ALT + Escape\".\n");
		else
			printf(" The cursor is not hidden. To hide the cursor, press \"ALT + F2\".\n");
	}

	printf(" Press \"ALT + Escape\" to exit.\n");
}

void RussianMainText() {
	if (EmulationMode == XboxMode) {
		printf("\n Эмуляция DualShock 4 с помощью контроллера Xbox.\n");
		if (SwapTriggersShoulders == false)
			printf(" Нажмите \"ALT + 1\", чтобы поменять местами бамперы и триггеры.\n");
		else
			printf(" Триггеры — это бамперы, бамперы — это триггеры. Нажмите \"ALT + 1\", чтобы поменять их местами.\n");

		if (SwapShareTouchPad == false)
			printf(" Нажатие на тачпад: \"BACK\", на Share: \"BACK + START\".");
		else
			printf(" Нажатие на тачпад: \"BACK + START\", на Share: \"BACK\".");
		printf(" Нажмите \"ALT + 2\", чтобы изменить.\n");

		if (TouchPadPressedWhenSwiping)
			printf(" Тачпад нажимается при движении по нему. Нажмите \"ALT + 3\", чтобы не нажималось.\n");
		else
			printf(" Тачпад не нажимается при движении по нему. Нажмите \"ALT + 3\", чтобы нажималось.\n");

		printf(" Прикосновения по тачпаду на геймпаде: \"BACK + RIGHT-STICK\".\n");
		printf_s(" Прикосновения на клавиатуре, первое: \"%s/%s/%s/%s\", второе: \"%s/%s/%s/%s\", свайпы: \"%s/%s/%s/%s\".\n",
			KEY_ID_TOUCHPAD_FIRST_UP_NAME.c_str(),
			KEY_ID_TOUCHPAD_FIRST_DOWN_NAME.c_str(),
			KEY_ID_TOUCHPAD_FIRST_LEFT_NAME.c_str(),
			KEY_ID_TOUCHPAD_FIRST_RIGHT_NAME.c_str(),
			KEY_ID_TOUCHPAD_SECOND_UP_NAME.c_str(),
			KEY_ID_TOUCHPAD_SECOND_DOWN_NAME.c_str(),
			KEY_ID_TOUCHPAD_SECOND_LEFT_NAME.c_str(),
			KEY_ID_TOUCHPAD_SECOND_RIGHT_NAME.c_str(),
			KEY_ID_TOUCHPAD_SWIPE_UP_NAME.c_str(),
			KEY_ID_TOUCHPAD_SWIPE_DOWN_NAME.c_str(),
			KEY_ID_TOUCHPAD_SWIPE_LEFT_NAME.c_str(),
			KEY_ID_TOUCHPAD_SWIPE_RIGHT_NAME.c_str()
		);
		printf_s(" Тряска геймпада: \"%s + %s\".\n", KEY_ID_XBOX_ACTIVATE_MULTI_NAME.c_str(), KEY_ID_XBOX_MOTION_SHAKING_NAME.c_str());
		printf_s(" Движения на геймпаде - X: \"%s/%s\", Y: \"%s/%s\", Z: \"%s/%s\".\n", KEY_ID_XBOX_MOTION_X_ADD_NAME.c_str(), KEY_ID_XBOX_MOTION_X_SUB_NAME.c_str(), KEY_ID_XBOX_MOTION_Y_ADD_NAME.c_str(), KEY_ID_XBOX_MOTION_Y_SUB_NAME.c_str(), KEY_ID_XBOX_MOTION_Z_ADD_NAME.c_str(), KEY_ID_XBOX_MOTION_Z_SUB_NAME.c_str());
		printf_s(" Движения на клавиатуре - X: \"%s/%s\", Y: \"%s/%s\", Z: \"%s/%s\".\n", KEY_ID_MOTION_X_ADD_NAME.c_str(), KEY_ID_MOTION_X_SUB_NAME.c_str(), KEY_ID_MOTION_Y_ADD_NAME.c_str(), KEY_ID_MOTION_Y_SUB_NAME.c_str(), KEY_ID_MOTION_Z_ADD_NAME.c_str(), KEY_ID_MOTION_Z_SUB_NAME.c_str());

		printf("\n Нажмите \"ALT + F9\", чтобы просмотреть мёртвые зоны стиков.\n");
	}
	else {
		printf("\n Эмуляция DualShock 4 с помощью клавиатуры и мыши.\n");
		if (ActivateInAnyWindow == false)
			printf(" Активация в любом окне отключена, эмулируемый геймпад работает только в \"PS Plus\" и \"PS4 Remote Play\".\n Переключить режим: \"ALT + F3\".\n");
		else
			printf(" Activate in any window is enabled. Switch mode with \"ALT + F3\"\n");
		printf_s(" Включение или отключение движения курсора на кнопках \"ALT + %s\" (эмуляция правого стика).\n", KEY_ID_STOP_CENTERING_NAME.c_str());
		printf_s(" Профиль клавиатуры и мыши: \"%s\". Сменить профиль на \"ALT + Up/Down\".\n\n", KMProfiles[ProfileIndex].substr(0, KMProfiles[ProfileIndex].size() - 4).c_str());

		if (LeftAnalogStick == false)
			printf(" Левый стик цифровой. Нажмите \"ALT + 1\", чтобы эмулировать аналоговый режим.\n");
		else
			printf(" Левый стик аналоговый. Нажмите \"ALT + 1\", чтобы сделать его цифровым.\n");

		if (EmulateAnalogTriggers == false)
			printf(" Триггеры цифровые. Нажмите \"ALT + 2\", чтобы включить аналоговый режим.\n");
		else
			printf(" Триггеры аналоговые. Нажмите \"ALT + 2\", чтобы сделать цифровой режим.\n");

		if (SwapSticks == false)
			printf(" Левый стик управляется клавиатурой, правый — мышью.");
		else
			printf(" Левый стик управляется мышью, правый — клавиатурой.");
		printf(" Нажмите \"ALT + 3\", чтобы поменять их местами.\n");
		printf_s(" Движения на клавиатуре - X: \"%s/%s\", Y: \"%s/%s\", Z: \"%s/%s\".\n", KEY_ID_MOTION_X_ADD_NAME.c_str(), KEY_ID_MOTION_X_SUB_NAME.c_str(), KEY_ID_MOTION_Y_ADD_NAME.c_str(), KEY_ID_MOTION_Y_SUB_NAME.c_str(), KEY_ID_MOTION_Z_ADD_NAME.c_str(), KEY_ID_MOTION_Z_SUB_NAME.c_str());

	}
	if (EmulationMode == KBMode) {
		printf("\n Нажмите \"ALT + F10\", чтобы переключить \"PS Plus\" в полноэкранный режим или вернуть обычный.\n");
		if (CursorHidden)
			printf(" Курсор скрыт, чтобы показать курсор, нажмите \"ALT + Escape\".\n");
		else
			printf(" Курсор не скрыт, чтобы скрыть курсор, нажмите \"ALT + F2\".\n");
	}

	printf(" Нажмите \"ALT + Escape\", чтобы выйти.\n");
}

void MainTextUpdate() {
	system("cls");
	if (Lang == LANG_RUSSIAN)
		RussianMainText();
	else
		DefaultMainText();
	//system("cls"); DefaultMainText();
}

int main(int argc, char **argv)
{
	SetConsoleTitle("DS4Emulator 2.2");
	WindowToCenter();

	if (PRIMARYLANGID(GetUserDefaultLangID()) == LANG_RUSSIAN) { // Resave cpp file with UTF8 BOM
		Lang = LANG_RUSSIAN;
		setlocale(LC_ALL, ""); // Output locale
		setlocale(LC_NUMERIC, "C"); // Numbers with a dot
		system("chcp 65001 > nul"); // Console UTF8 output 
	}

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
	bool InvertX = IniFile.ReadBoolean("Main", "InvertX", false);
	bool InvertY = IniFile.ReadBoolean("Main", "InvertY", false);

	int SleepTimeOutXbox = IniFile.ReadInteger("Xbox", "SleepTimeOut", 1);
	SwapTriggersShoulders = IniFile.ReadBoolean("Xbox", "SwapTriggersShoulders", false);
	SwapShareTouchPad = IniFile.ReadBoolean("Xbox", "SwapShareTouchPad", false);
	TouchPadPressedWhenSwiping = IniFile.ReadBoolean("Xbox", "TouchPadPressedWhenSwiping", false);
	bool EnableXboxButton = IniFile.ReadBoolean("Xbox", "EnableXboxButton", true);
	XboxTouchpad::Settings TouchpadStickSettings;
	TouchpadStickSettings.mode = XboxTouchpad::ParseMode(IniFile.ReadString("Xbox", "TouchpadStickMode", "legacy"));
	TouchpadStickSettings.sensitivity = IniFile.ReadFloat("Xbox", "TouchpadStickSensitivity", 0.60f);
	TouchpadStickSettings.deadzone = IniFile.ReadFloat("Xbox", "TouchpadStickDeadzone", 0.12f);
	TouchpadStickSettings.smoothing = IniFile.ReadFloat("Xbox", "TouchpadStickSmoothing", 0.0f);
	if (XboxTouchpad::Lower(IniFile.ReadString("Xbox", "TouchpadStickCurve", "linear")) == "quadratic")
		TouchpadStickSettings.curve = XboxTouchpad::Curve::Quadratic;
	TouchpadStickSettings.Validate();
	const double TouchpadClickDeadzone = XboxTouchpad::ValidateClickDeadzone(IniFile.ReadFloat("Xbox", "TouchpadClickDeadzone", 0.12f));

	const bool MotionStickEnabled = IniFile.ReadBoolean("Xbox", "MotionStickEnabled", false);
	const std::string MotionStickKeyName = IniFile.ReadString("Xbox", "MotionStickKey", "RIGHT-SHOULDER");
	const int MotionStickKey = XboxKeyNameToXboxKeyCode(MotionStickKeyName);
	XboxMotion::Settings MotionStickSettings;
	MotionStickSettings.speed = IniFile.ReadFloat("Xbox", "MotionStickSpeed", 60.0f);
	MotionStickSettings.deadzone = IniFile.ReadFloat("Xbox", "MotionStickDeadzone", 0.12f);
	MotionStickSettings.quadratic = XboxTouchpad::Lower(IniFile.ReadString("Xbox", "MotionStickCurve", "quadratic")) == "quadratic";
	MotionStickSettings.horizontal = XboxMotion::ParseAxis(IniFile.ReadString("Xbox", "MotionStickHorizontalAxis", "Y"), XboxMotion::Axis::Y);
	MotionStickSettings.vertical = XboxMotion::ParseAxis(IniFile.ReadString("Xbox", "MotionStickVerticalAxis", "X"), XboxMotion::Axis::X);
	MotionStickSettings.invertHorizontal = IniFile.ReadBoolean("Xbox", "MotionStickInvertHorizontal", true);
	MotionStickSettings.invertVertical = IniFile.ReadBoolean("Xbox", "MotionStickInvertVertical", false);
	MotionStickSettings.Validate();

	KEY_ID_XBOX_ACTIVATE_MULTI_NAME = IniFile.ReadString("Xbox", "MultiActivateKey", "BACK");
	int KEY_ID_XBOX_ACTIVATE_MULTI = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_ACTIVATE_MULTI_NAME);
	KEY_ID_XBOX_MOTION_SHAKING_NAME = IniFile.ReadString("Xbox", "MotionShakingKey", "RIGHT-SHOULDER");
	int KEY_ID_XBOX_MOTION_SHAKING = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_SHAKING_NAME);
	
	bool DisableButtonOnMotion = IniFile.ReadBoolean("Xbox", "DisableButtonOnMotion", false);
	KEY_ID_XBOX_MOTION_X_ADD_NAME = IniFile.ReadString("Xbox", "MotionXAdd", "DPAD-UP");
	KEY_ID_XBOX_MOTION_X_SUB_NAME = IniFile.ReadString("Xbox", "MotionXSub", "DPAD-DOWN");
	KEY_ID_XBOX_MOTION_Y_ADD_NAME = IniFile.ReadString("Xbox", "MotionYAdd", "NONE");
	KEY_ID_XBOX_MOTION_Y_SUB_NAME = IniFile.ReadString("Xbox", "MotionYSub", "NONE");
	KEY_ID_XBOX_MOTION_Z_ADD_NAME = IniFile.ReadString("Xbox", "MotionZAdd", "DPAD-LEFT");
	KEY_ID_XBOX_MOTION_Z_SUB_NAME = IniFile.ReadString("Xbox", "MotionZSub", "DPAD-RIGHT");
	int KEY_ID_XBOX_MOTION_X_ADD = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_X_ADD_NAME);
	int KEY_ID_XBOX_MOTION_X_SUB = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_X_SUB_NAME);
	int KEY_ID_XBOX_MOTION_Y_ADD = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_Y_ADD_NAME);
	int KEY_ID_XBOX_MOTION_Y_SUB = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_Y_SUB_NAME);
	int KEY_ID_XBOX_MOTION_Z_ADD = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_Z_ADD_NAME);
	int KEY_ID_XBOX_MOTION_Z_SUB = XboxKeyNameToXboxKeyCode(KEY_ID_XBOX_MOTION_Z_SUB_NAME);

	float DeadZoneLeftStickX = IniFile.ReadFloat("Xbox", "DeadZoneLeftStickX", 0);
	float DeadZoneLeftStickY = IniFile.ReadFloat("Xbox", "DeadZoneLeftStickY", 0);
	float DeadZoneRightStickX = IniFile.ReadFloat("Xbox", "DeadZoneRightStickX", 0);
	float DeadZoneRightStickY = IniFile.ReadFloat("Xbox", "DeadZoneRightStickY", 0);

	bool KMActivateAlways = IniFile.ReadBoolean("KeyboardMouse", "ActivateAlways", false);
	KEY_ID_STOP_CENTERING_NAME = IniFile.ReadString("KeyboardMouse", "StopCenteringKey", "C");
	int KEY_ID_STOP_CENTERING = KeyNameToKeyCode(KEY_ID_STOP_CENTERING_NAME);
	bool CenteringEnable = IniFile.ReadBoolean("KeyboardMouse", "EnableCentering", true);
	std::string DefaultProfile = IniFile.ReadString("KeyboardMouse", "DefaultProfile", "Default.ini");

	int SleepTimeOutKB = IniFile.ReadInteger("KeyboardMouse", "SleepTimeOut", 1);
	std::string WindowTitle = IniFile.ReadString("KeyboardMouse", "ActivateOnlyInWindow", "PlayStation Plus");
	std::string WindowTitle2 = IniFile.ReadString("KeyboardMouse", "ActivateOnlyInWindow2", "PS4 Remote Play");
	int FullScreenTopOffset = IniFile.ReadInteger("KeyboardMouse", "FullScreenTopOffset", -50);
	bool HideTaskBar = IniFile.ReadBoolean("KeyboardMouse", "HideTaskBarInFullScreen", true);
	bool FullScreenMode = false;
	ActivateInAnyWindow = IniFile.ReadBoolean("KeyboardMouse", "ActivateInAnyWindow", false);
	EmulateAnalogTriggers = IniFile.ReadBoolean("KeyboardMouse", "EmulateAnalogTriggers", false);
	float LeftTriggerValue = 0;
	float RightTriggerValue = 0;
	float StepTriggerValue = IniFile.ReadFloat("KeyboardMouse", "AnalogTriggerStep", 15);
	mouseSensetiveX = IniFile.ReadFloat("KeyboardMouse", "SensX", 15);
	mouseSensetiveY = IniFile.ReadFloat("KeyboardMouse", "SensY", 15);
	SwapSticks = IniFile.ReadBoolean("KeyboardMouse", "SwapSticks", false);
	LeftAnalogStick = IniFile.ReadBoolean("KeyboardMouse", "EmulateLeftAnalogStick", false);
	int LeftAnalogX = 128, LeftAnalogY = 128;
	int AnalogStickLeft = IniFile.ReadFloat("KeyboardMouse", "AnalogStickStep", 15);
	int DeadZoneDS4 = IniFile.ReadInteger("KeyboardMouse", "DeadZone", 0); // 25, makes mouse movement smoother when moving slowly (12->25)

	static auto start = std::chrono::steady_clock::now();

	// Search keyboard and mouses profiles
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	hFind = FindFirstFile("Profiles\\*.ini", &ffd);
	KMProfiles.push_back("Default.ini");
	int ProfileCount = 0;
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (strcmp(ffd.cFileName, "Default.ini")) {
				KMProfiles.push_back(ffd.cFileName);
				ProfileCount++;
				if(strcmp(ffd.cFileName, DefaultProfile.c_str()) == 0)
					ProfileIndex = ProfileCount;
			}
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
	bool MotionShakingSwap = false;

	//int MotionKeyOnlyCheckCount = 0;
	//int MotionKeyReleasedCount = 0;
	//bool MotionKeyOnlyPressed = false;

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

	if (KMActivateAlways == false && hDll != NULL)
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

	XboxMotion::ConsoleTuner MotionSpeedConsole(MotionStickEnabled);

	// Write current mode
	MainTextUpdate();
	if (MotionStickEnabled)
		printf_s(" Analog motion: %s + RIGHT-STICK, max %.1f deg/s. Button alone works normally; movement reserves it until release. Back combinations take priority.\n", MotionStickKeyName.c_str(), MotionStickSettings.speed);
	if (MotionStickEnabled && MotionStickKey == XINPUT_GAMEPAD_START)
		printf(" Start alone sends Options on release; a motion gesture never opens the menu.\n");
	if (MotionSpeedConsole.available)
		printf(" Focus this console and press M to edit motion speed. No global shortcut is registered.\n");

	DS4_TOUCH BuffPreviousTouch[2] = { 0, 0 };
	BuffPreviousTouch[0].bIsUpTrackingNum1 = 0x80;
	BuffPreviousTouch[1].bIsUpTrackingNum1 = 0x80;
	unsigned char TouchIndex = 0;
	bool AllowIncTouchIndex = false;
	bool DeadZoneMode = false;

	Touch2.LeftMode = 2;
	static uint8_t TouchPacket = 0;
	XboxTouchpad::State TouchpadStickState;
	XboxMotion::Gesture MotionStickGesture;
	XboxMotion::MenuButton MotionMenuButton;
	auto TouchpadStickLastFrame = std::chrono::steady_clock::now();

	while ( !(IsKeyPressed(VK_LMENU) && IsKeyPressed(VK_ESCAPE) ) )
	{
		DS4_REPORT_INIT_EX(&report);
		MotionSpeedConsole.Poll(MotionStickSettings, IniFile.GetIniPath().c_str());

		const auto TouchpadStickNow = std::chrono::steady_clock::now();
		const double TouchpadStickDelta = std::chrono::duration<double>(TouchpadStickNow - TouchpadStickLastFrame).count();
		TouchpadStickLastFrame = TouchpadStickNow;
		bool XboxStickTouchActive = false;
		bool XboxStickMotionActive = false;
		XboxMotion::Rate XboxStickMotionRate;

		ResetTouchData(Touch1);
		ResetTouchData(Touch2); 

		report.bTouchPacketsN = 0;
		report.sCurrentTouch = { 0 };
		report.sPreviousTouch[0] = { 0 };
		report.sPreviousTouch[1] = { 0 };

		// previous-пакет (держим один последний)
		static decltype(report.sCurrentTouch) LastTouch = { 0 };
		static bool LastTouchValid = false;

		// по умолчанию оба "up" с текущими track-id
		report.sCurrentTouch.bIsUpTrackingNum1 = uint8_t(0x80 | (Touch1.ID & 0x7F));
		report.sCurrentTouch.bIsUpTrackingNum2 = uint8_t(0x80 | (Touch2.ID & 0x7F));
		
		report.bBatteryLvl = 11;

		bool MotionShaking = false, MotionXAdd = false, MotionXSub = false, MotionYAdd = false, MotionYSub = false, MotionZAdd = false, MotionZSub = false;

		// While editing in the console, release all game inputs so editor keys cannot become touches.
		if (MotionSpeedConsole.active) {
			MotionStickGesture.active = false;
			MotionMenuButton = {};
			XboxMotion::Rate{}.Apply(report);
			report.bTouchPacketsN = 1;
			report.sCurrentTouch.bPacketCounter = ++TouchPacket;
			Touch1.PrevTouched = Touch2.PrevTouched = false;
			LastTouchValid = false; TouchpadStickState.Release();
			report.wTimestamp = (USHORT)(std::chrono::duration_cast<std::chrono::milliseconds>(TouchpadStickNow - start).count() & 0xFFFF);
			ret = vigem_target_ds4_update_ex(client, ds4, report);
			Sleep(10);
			continue;
		}

		// Xbox mode
		if (EmulationMode != XboxMode) { MotionStickGesture.active = false; MotionMenuButton = {}; }
		if (EmulationMode == XboxMode) {
			DWORD myStatus = ERROR_DEVICE_NOT_CONNECTED;
			if (hDll != NULL)
				myStatus = MyXInputGetState(XboxUserIndex, &myPState);
			if (myStatus != ERROR_SUCCESS) { MotionStickGesture.active = false; MotionMenuButton = {}; }
			
			if (myStatus == ERROR_SUCCESS) {

				if (SkipPollCount == 0 && IsKeyPressed(VK_MENU)) {
					if (IsKeyPressed('1'))
					{
						SwapTriggersShoulders = !SwapTriggersShoulders;
						MainTextUpdate();
						SkipPollCount = SkipPollTimeOut;
					}

					if (IsKeyPressed('2'))
					{
						SwapShareTouchPad = !SwapShareTouchPad;
						MainTextUpdate();
						SkipPollCount = SkipPollTimeOut;
					}

					if (IsKeyPressed('3'))
					{
						TouchPadPressedWhenSwiping = !TouchPadPressedWhenSwiping;
						MainTextUpdate();
						SkipPollCount = SkipPollTimeOut;
					}

					// Dead zones
					if (IsKeyPressed(VK_F9))
					{
						DeadZoneMode = !DeadZoneMode;
						if (DeadZoneMode == false)
							MainTextUpdate();
						SkipPollCount = SkipPollTimeOut;
					}
				}	

				if (DeadZoneMode) {
					printf(" Left Stick X=%.2f, ", abs(myPState.Gamepad.sThumbLX / (32767 / 100.0f)));
					printf("Y=%3.2f | ", abs(myPState.Gamepad.sThumbLY / (32767 / 100.0f)));
					printf("Right Stick X=%.2f ", abs(myPState.Gamepad.sThumbRX / (32767 / 100.0f)));
					printf("Y=%3.2f\n", abs(myPState.Gamepad.sThumbRY / (32767 / 100.0f)));
				}

				myPState.Gamepad.sThumbLX = DeadZoneXboxAxis(myPState.Gamepad.sThumbLX, DeadZoneLeftStickX);
				myPState.Gamepad.sThumbLY = DeadZoneXboxAxis(myPState.Gamepad.sThumbLY, DeadZoneLeftStickY);
				myPState.Gamepad.sThumbRX = DeadZoneXboxAxis(myPState.Gamepad.sThumbRX, DeadZoneRightStickX);
				myPState.Gamepad.sThumbRY = DeadZoneXboxAxis(myPState.Gamepad.sThumbRY, DeadZoneRightStickY);

				// Convert axis from - https://github.com/sam0x17/XJoy/blob/236b5539cc15ea1c83e1e5f0260937f69a78866d/Include/ViGEmUtil.h
				// Reserve the key only after stick movement starts; a plain press stays a normal button.
				const bool MotionStickHeld = MotionStickEnabled && MotionStickKey != 0
					&& (myPState.Gamepad.wButtons & MotionStickKey)
					&& !(myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI)
					&& !(myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
					&& !(SwapTriggersShoulders && (MotionStickKey & (XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER)));
				XboxStickMotionActive = MotionStickGesture.Update(MotionStickHeld,
					myPState.Gamepad.sThumbRX, myPState.Gamepad.sThumbRY, MotionStickSettings);
				const bool MotionMenuAllowed = MotionStickEnabled && MotionStickKey == XINPUT_GAMEPAD_START
					&& !(myPState.Gamepad.wButtons & (KEY_ID_XBOX_ACTIVATE_MULTI | XINPUT_GAMEPAD_BACK));
				const bool MotionMenuPulse = MotionMenuButton.Update(MotionMenuAllowed,
					(myPState.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0, XboxStickMotionActive, TouchpadStickDelta);
				if (MotionMenuAllowed) {
					myPState.Gamepad.wButtons &= ~XINPUT_GAMEPAD_START;
					if (MotionMenuPulse) report.wButtons |= DS4_BUTTON_OPTIONS;
				}
				if (XboxStickMotionActive) {
					XboxStickMotionRate = XboxMotion::Calculate(myPState.Gamepad.sThumbRX, myPState.Gamepad.sThumbRY, MotionStickSettings);
					myPState.Gamepad.wButtons &= ~MotionStickKey;
				}
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
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI && myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_SHAKING) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MULTI; if (DisableButtonOnMotion) myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_SHAKING;
					MotionShaking = true;
				};

				// Motion X+
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI && (myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_X_ADD)) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MULTI; if (DisableButtonOnMotion) myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_X_ADD;
					MotionXAdd = true;
					//printf("right\n");
				}

				// Motion X-
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI && (myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_X_SUB)) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MULTI; if (DisableButtonOnMotion) myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_X_SUB;
					MotionXSub = true;
					//printf("left\n");
				}

				// Motion Y+
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI && (myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_Y_ADD)) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MULTI; if (DisableButtonOnMotion) myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_Y_ADD;
					MotionYAdd = true;
					//printf("up\n");
				}

				// Motion Y-
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI && (myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_Y_SUB)) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MULTI; if (DisableButtonOnMotion) myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_Y_SUB;
					MotionYSub = true;
					//printf("down\n");
				}

				// Motion Z+
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI && (myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_Z_ADD)) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MULTI; if (DisableButtonOnMotion) myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_Z_ADD;
					MotionZAdd = true;
					//printf("right\n");
				}

				// Motion Z-
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI && (myPState.Gamepad.wButtons & KEY_ID_XBOX_MOTION_Z_SUB)) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MULTI; if (DisableButtonOnMotion) myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_MOTION_Z_SUB;
					MotionZSub = true;
					//printf("left\n");
				}

				// Motion key exclude
				bool XboxActivateMotionPressed = myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI ? true : false;
				/*if (myPState.Gamepad.wButtons == KEY_ID_XBOX_ACTIVATE_MULTI && MotionKeyOnlyCheckCount == 0) { MotionKeyOnlyCheckCount = 20; MotionKeyOnlyPressed = true; }
				if (MotionKeyOnlyCheckCount > 0) {
					MotionKeyOnlyCheckCount--;
					if (myPState.Gamepad.wButtons != KEY_ID_XBOX_ACTIVATE_MULTI && (myPState.Gamepad.wButtons != 0 || (report.bThumbLX != 127 || report.bThumbLY != 129) || (report.bThumbRX != 127 || report.bThumbRY != 129) )) {
						//printf("pressed another button\n"); 
						MotionKeyOnlyPressed = false; MotionKeyOnlyCheckCount = 0; 
					}
				}
				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI && (myPState.Gamepad.wButtons != KEY_ID_XBOX_ACTIVATE_MULTI || (report.bThumbLX != 127 || report.bThumbLY != 129) || (report.bThumbRX != 127 || report.bThumbRY != 129))) MotionKeyReleasedCount = 30;
				if (MotionKeyReleasedCount > 0) MotionKeyReleasedCount--;
				 
				//printf("%d %d %d\n", MotionKeyOnlyCheckCount, MotionKeyReleasedCount, MotionKeyOnlyPressed);
				//printf("%d\n", MotionKeyOnlyCheckCount);

				if (myPState.Gamepad.wButtons & KEY_ID_XBOX_ACTIVATE_MULTI) {
					myPState.Gamepad.wButtons &= ~KEY_ID_XBOX_ACTIVATE_MULTI;
				}
				if (MotionKeyOnlyCheckCount == 1 && MotionKeyOnlyPressed && MotionKeyReleasedCount == 0) {
					myPState.Gamepad.wButtons |= KEY_ID_XBOX_ACTIVATE_MULTI; //|=
					MotionKeyReleasedCount = 30; // Timeout to release the motion key button and don't execute other buttons
					//printf("Motion Key pressed\n");
				}*/

				// Swap share and touchpad
				if (SwapShareTouchPad == false) {
					if (IsKeyPressed(KEY_ID_SHARE))
						report.wButtons |= DS4_BUTTON_SHARE;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
						report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
				} else {
					if (IsKeyPressed(KEY_ID_SHARE))
						report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
						report.wButtons |= DS4_BUTTON_SHARE;
				}

				// Exclude multi keys
				if (XboxActivateMotionPressed == false) {
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
					}
					else {
						if (myPState.Gamepad.bLeftTrigger > 0)
							report.wButtons |= DS4_BUTTON_SHOULDER_LEFT;
						if (myPState.Gamepad.bRightTrigger > 0)
							report.wButtons |= DS4_BUTTON_SHOULDER_RIGHT;
					}

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

				// Allow triggers while the touchpad/motion modifier is held.
				if (SwapTriggersShoulders == false) {
					report.bTriggerL = myPState.Gamepad.bLeftTrigger;
					report.bTriggerR = myPState.Gamepad.bRightTrigger;
				}
				else {
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
						report.bTriggerL = 255;
					if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
						report.bTriggerR = 255;
				}

				if (report.bTriggerL > 0) // Specific of DualShock
					report.wButtons |= DS4_BUTTON_TRIGGER_LEFT;
				if (report.bTriggerR > 0) // Specific of DualShock
					report.wButtons |= DS4_BUTTON_TRIGGER_RIGHT;

				// Touchpad swipes
				if (report.bSpecial & DS4_SPECIAL_BUTTON_TOUCHPAD) {
						if (!TouchPadPressedWhenSwiping && XboxTouchpad::SuppressClick(myPState.Gamepad.sThumbRX, myPState.Gamepad.sThumbRY, report.bThumbRX, report.bThumbRY, TouchpadClickDeadzone)) {
							report.bSpecial &= ~DS4_SPECIAL_BUTTON_TOUCHPAD;
							if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) { report.wButtons &= ~DS4_BUTTON_THUMB_RIGHT; report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD; }
						}
					Touch1.X = 960; Touch1.Y = 471;
					/*if (report.bThumbRX > 127)
						Touch1.X = 320 + (report.bThumbRX - 127) * 10;
					if (report.bThumbRX < 127)
						Touch1.X = 1600 - (-report.bThumbRX + 127) * 10;
					if (report.bThumbRY > 129)
						Touch1.Y = trunc(100 + (report.bThumbRY - 129) * 5.8);
					if (report.bThumbRY < 129)
						Touch1.Y = trunc(743 - (-report.bThumbRY + 129) * 5.8);

					if (report.bThumbLX > 127)
						Touch2.X = 320 + (report.bThumbLX - 127) * 10;
					if (report.bThumbLX < 127)
						Touch2.X = 1600 - (-report.bThumbLX + 127) * 10;
					if (report.bThumbLY > 129)
						Touch2.Y = trunc(100 + (report.bThumbLY - 129) * 5.8);
					if (report.bThumbLY < 129)
						Touch2.Y = trunc(743 - (-report.bThumbLY + 129) * 5.8);*/

					double LeftStickValue = StickDeviationPercent(report.bThumbLX, report.bThumbLY);
					double RightStickValue = StickDeviationPercent(report.bThumbRX, report.bThumbRY);

					//printf(" %.2f %.2f %d %d \n", LeftStickValue, RightStickValue, report.bThumbRX, report.bThumbRY);

					if (TouchpadStickSettings.mode == XboxTouchpad::Mode::Legacy) {
					if (report.bThumbRX > 127)
						Touch1.X = 200 + trunc(1519 * RightStickValue);
					if (report.bThumbRX < 127)
						Touch1.X = 1719 - trunc(1519 * RightStickValue);

					if (report.bThumbRY > 129)
						Touch1.Y = 100 + trunc(741 * RightStickValue);
					if (report.bThumbRY < 129)
						Touch1.Y = 743 - trunc(741 * RightStickValue);
					} else {
						const double stickX = XboxTouchpad::Normalize(myPState.Gamepad.sThumbRX) * (InvertX ? -1.0 : 1.0);
						const double stickY = XboxTouchpad::Normalize(myPState.Gamepad.sThumbRY) * (InvertY ? 1.0 : -1.0);
						TouchpadStickState.Update(stickX, stickY, TouchpadStickDelta, TouchpadStickSettings);
						Touch1.X = TouchpadStickState.X(); Touch1.Y = TouchpadStickState.Y();
						XboxStickTouchActive = true;
					}

					if (report.bThumbLX > 127)
						Touch2.X = 200 + trunc(1519 * LeftStickValue);
					if (report.bThumbLX < 127)
						Touch2.X = 1719 - trunc(1519 * LeftStickValue);

					if (report.bThumbLY > 129)
						Touch2.Y = 100 + trunc(741 * LeftStickValue);
					if (report.bThumbLY < 129)
						Touch2.Y = 743 - trunc(741 * LeftStickValue);

				}

				if (XboxStickMotionActive) {
					report.bThumbRX = 128; report.bThumbRY = 128;
				}

				if (XboxActivateMotionPressed) {
					report.bThumbLX = 128; report.bThumbLY = 128;
					report.bThumbRX = 128; report.bThumbRY = 128;
				}
				
			}
		}
		// Mouse and keyboard mode
		else if (EmulationMode == KBMode) {
			
			PSPlusWindow = FindWindow(NULL, WindowTitle.c_str());
			bool PSNowFound = (PSPlusWindow != 0) && (IsWindowVisible(PSPlusWindow)) && (PSPlusWindow == GetForegroundWindow());

			PSRemotePlayWindow = FindWindow(NULL, WindowTitle2.c_str());
			bool PSRemotePlayFound = (PSRemotePlayWindow != 0) && (IsWindowVisible(PSRemotePlayWindow)) && (PSRemotePlayWindow == GetForegroundWindow());

			if (SkipPollCount == 0 && IsKeyPressed(VK_MENU)) {
				// Switch profiles
				if (IsKeyPressed(VK_UP) || IsKeyPressed(VK_DOWN)) {
					SkipPollCount = SkipPollTimeOut;
					if (IsKeyPressed(VK_UP)) if (ProfileIndex > 0) ProfileIndex--; else ProfileIndex = KMProfiles.size() - 1;
					if (IsKeyPressed(VK_DOWN)) if (ProfileIndex < KMProfiles.size() - 1) ProfileIndex++; else ProfileIndex = 0;
					MainTextUpdate();
					LoadKMProfile(KMProfiles[ProfileIndex]);
				}

				if (IsKeyPressed('1'))
				{
					LeftAnalogStick = !LeftAnalogStick;
					MainTextUpdate();
					SkipPollCount = SkipPollTimeOut;
				}

				if (IsKeyPressed('2'))
				{
					EmulateAnalogTriggers = !EmulateAnalogTriggers;
					MainTextUpdate();
					SkipPollCount = SkipPollTimeOut;
				}

				if (IsKeyPressed('3'))
				{
					SwapSticks = !SwapSticks;
					MainTextUpdate();
					SkipPollCount = SkipPollTimeOut;
				}

				// Custom fullscreen mode
				if (IsKeyPressed(VK_F10)) {
					if (PSPlusWindow != 0)
						if (FullScreenMode) {
							if (HideTaskBar) ShowWindow(FindWindow("Shell_TrayWnd", NULL), SW_SHOW);
							SetWindowPos(PSPlusWindow, HWND_TOP, m_HalfWidth - 640, m_HalfHeight - 360, 1280, 720, SWP_FRAMECHANGED);
						}
						else {
							SetForegroundWindow(PSPlusWindow);
							SetActiveWindow(PSPlusWindow);
							if (HideTaskBar) ShowWindow(FindWindow("Shell_TrayWnd", NULL), SW_HIDE);
							SetWindowPos(PSPlusWindow, HWND_TOP, 0, FullScreenTopOffset, m_HalfWidth * 2, m_HalfHeight * 2 + (-FullScreenTopOffset), SWP_FRAMECHANGED);
						}
					FullScreenMode = !FullScreenMode;
					SkipPollCount = SkipPollTimeOut;
				}

				// Cursor hide
				if (CursorHidden == false && IsKeyPressed(VK_F2)) {
					SetSystemCursor(CursorEmpty, OCR_NORMAL); CursorHidden = true;
					printf(" The cursor is hidden. To display the cursor, press \"ALT\" + \"Escape\".\n");
					SkipPollCount = SkipPollTimeOut;
				}

				if (IsKeyPressed(VK_F3)) {
					ActivateInAnyWindow = !ActivateInAnyWindow;
					MainTextUpdate();
					SkipPollCount = SkipPollTimeOut;
				}
			}

			if (ActivateInAnyWindow || PSNowFound || PSRemotePlayFound) {
				if (IsKeyPressed(VK_MENU) && IsKeyPressed(KEY_ID_STOP_CENTERING) && SkipPollCount == 0) { CenteringEnable = !CenteringEnable;  SkipPollCount = SkipPollTimeOut;}
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
			
				if (LeftAnalogStick == false) {
					if (IsKeyPressed(KEY_ID_LEFT_STICK_UP)) report.bThumbLY = 0;
					if (IsKeyPressed(KEY_ID_LEFT_STICK_DOWN)) report.bThumbLY = 255;
					if (IsKeyPressed(KEY_ID_LEFT_STICK_LEFT)) report.bThumbLX = 0;
					if (IsKeyPressed(KEY_ID_LEFT_STICK_RIGHT)) report.bThumbLX = 255;
				} else {
					if (IsKeyPressed(KEY_ID_LEFT_STICK_UP)) {
						if (LeftAnalogY > 0) LeftAnalogY -= AnalogStickLeft;
					} else if (IsKeyPressed(KEY_ID_LEFT_STICK_DOWN)) {
						if (LeftAnalogY < 255) LeftAnalogY += AnalogStickLeft;
					} else {
						if (LeftAnalogY > 128 + AnalogStickLeft) LeftAnalogY -= AnalogStickLeft;
						else if (LeftAnalogY < 128 - AnalogStickLeft) LeftAnalogY += AnalogStickLeft;
						else LeftAnalogY = 128;
					}

					if (IsKeyPressed(KEY_ID_LEFT_STICK_LEFT)) {
						if (LeftAnalogX > 0) LeftAnalogX -= AnalogStickLeft;
					}
					else if (IsKeyPressed(KEY_ID_LEFT_STICK_RIGHT)) {
						if (LeftAnalogX < 255) LeftAnalogX += AnalogStickLeft;
					}
					else {
						if (LeftAnalogX > 128 + AnalogStickLeft) LeftAnalogX -= AnalogStickLeft;
						else if (LeftAnalogX < 128 - AnalogStickLeft) LeftAnalogX += AnalogStickLeft;
						else LeftAnalogX = 128;
					}
					if (LeftAnalogX > 255) LeftAnalogX = 255;
					if (LeftAnalogX < 0) LeftAnalogX = 0;
					if (LeftAnalogY > 255) LeftAnalogY = 255;
					if (LeftAnalogY < 0) LeftAnalogY = 0;
					report.bThumbLY = LeftAnalogY;
					report.bThumbLX = LeftAnalogX;
					//printf("%d %d\n", LeftAnalogX, LeftAnalogY);
				}

				if (SwapSticks) {
					std::swap(report.bThumbLX, report.bThumbRX);
					std::swap(report.bThumbLY, report.bThumbRY);
				}

				if (EmulateAnalogTriggers == false) {

					if (IsKeyPressed(KEY_ID_LEFT_TRIGGER))
						report.bTriggerL = 255;
					if (IsKeyPressed(KEY_ID_RIGHT_TRIGGER))
						report.bTriggerR = 255;
				}
				else { // With emulate analog triggers

					if (IsKeyPressed(KEY_ID_LEFT_TRIGGER)) {
						if (LeftTriggerValue < 255)
							LeftTriggerValue += StepTriggerValue;
					}
					else {
						if (LeftTriggerValue > 0)
							LeftTriggerValue -= StepTriggerValue;
					}
					
					report.bTriggerL = trunc( Clamp(LeftTriggerValue, 0, 255) );

					if (IsKeyPressed(KEY_ID_RIGHT_TRIGGER)) {
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

				if (IsKeyPressed(KEY_ID_SHARE))
					report.wButtons |= DS4_BUTTON_SHARE;
				if (IsKeyPressed(KEY_ID_TOUCHPAD))
					report.bSpecial |= DS4_SPECIAL_BUTTON_TOUCHPAD;
				if (IsKeyPressed(KEY_ID_OPTIONS))
					report.wButtons |= DS4_BUTTON_OPTIONS;

				if (IsKeyPressed(KEY_ID_TRIANGLE))
					report.wButtons |= DS4_BUTTON_TRIANGLE;
				if (IsKeyPressed(KEY_ID_SQUARE))
					report.wButtons |= DS4_BUTTON_SQUARE;
				if (IsKeyPressed(KEY_ID_CIRCLE))
					report.wButtons |= DS4_BUTTON_CIRCLE;
				if (IsKeyPressed(KEY_ID_CROSS))
					report.wButtons |= DS4_BUTTON_CROSS;

				if (IsKeyPressed(KEY_ID_LEFT_THUMB))
					report.wButtons |= DS4_BUTTON_THUMB_LEFT;
				if (IsKeyPressed(KEY_ID_RIGHT_THUMB))
					report.wButtons |= DS4_BUTTON_THUMB_RIGHT;

				if (IsKeyPressed(KEY_ID_LEFT_SHOULDER))
					report.wButtons |= DS4_BUTTON_SHOULDER_LEFT;
				if (IsKeyPressed(KEY_ID_RIGHT_SHOULDER))
					report.wButtons |= DS4_BUTTON_SHOULDER_RIGHT;

				if (IsKeyPressed(KEY_ID_DPAD_UP))
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_NORTH);
				if (IsKeyPressed(KEY_ID_DPAD_DOWN))
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_SOUTH);
				if (IsKeyPressed(KEY_ID_DPAD_LEFT))
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_WEST);
				if (IsKeyPressed(KEY_ID_DPAD_RIGHT))
					DS4_SET_DPAD_EX(&report, DS4_BUTTON_DPAD_EAST);
			}
		}

		// Any mode
		// Touchpad, Finger 1
		// Left
		if (IsKeyPressed(KEY_ID_TOUCHPAD_FIRST_LEFT)) {
			Touch1.X = TouchLeftMode(Touch1.LeftMode);
			Touch1.Y = TouchTopMode(Touch1.TopMode);
			if (Touch1.SkipPollLeft == 0) { if (Touch1.LeftMode > 0) --Touch1.LeftMode; Touch1.SkipPollLeft = 10; }
		}
		// Right
		if (IsKeyPressed(KEY_ID_TOUCHPAD_FIRST_RIGHT)) {
			Touch1.X = TouchLeftMode(Touch1.LeftMode);
			Touch1.Y = TouchTopMode(Touch1.TopMode);
			if (Touch1.SkipPollLeft == 0) { if (Touch1.LeftMode < TOUCH_LEFT_MODE_MAX) ++Touch1.LeftMode; Touch1.SkipPollLeft = 10; }
		}
		// Up
		if (IsKeyPressed(KEY_ID_TOUCHPAD_FIRST_UP)) {
			Touch1.X = TouchLeftMode(Touch1.LeftMode);
			Touch1.Y = TouchTopMode(Touch1.TopMode);
			if (Touch1.SkipPollTop == 0) { if (Touch1.TopMode > 0) --Touch1.TopMode; Touch1.SkipPollTop = SkipPollTimeOut; }
		}
		// Down
		if (IsKeyPressed(KEY_ID_TOUCHPAD_FIRST_DOWN)) {
			Touch1.X = TouchLeftMode(Touch1.LeftMode);
			Touch1.Y = TouchTopMode(Touch1.TopMode);
			if (Touch1.SkipPollTop == 0) { if (Touch1.TopMode < TOUCH_TOP_MODE_MAX) ++Touch1.TopMode; Touch1.SkipPollTop = SkipPollTimeOut; }
		}


		// Finger 2
		// Left
		if (IsKeyPressed(KEY_ID_TOUCHPAD_SECOND_LEFT)) {
			Touch2.X = TouchLeftMode(Touch2.LeftMode);
			Touch2.Y = TouchTopMode(Touch2.TopMode);
			if (Touch2.SkipPollLeft == 0) { if (Touch2.LeftMode > 0) --Touch2.LeftMode; Touch2.SkipPollLeft = 10; }
		}
		// Right
		if (IsKeyPressed(KEY_ID_TOUCHPAD_SECOND_RIGHT)) {
			Touch2.X = TouchLeftMode(Touch2.LeftMode);
			Touch2.Y = TouchTopMode(Touch2.TopMode);
			if (Touch2.SkipPollLeft == 0) { if (Touch2.LeftMode < TOUCH_LEFT_MODE_MAX) ++Touch2.LeftMode; Touch2.SkipPollLeft = 10; }
		}
		// Up
		if (IsKeyPressed(KEY_ID_TOUCHPAD_SECOND_UP)) {
			Touch2.X = TouchLeftMode(Touch2.LeftMode);
			Touch2.Y = TouchTopMode(Touch2.TopMode);
			if (Touch2.SkipPollTop == 0) { if (Touch2.TopMode > 0) --Touch2.TopMode; Touch2.SkipPollTop = SkipPollTimeOut; }
		}
		// Down
		if (IsKeyPressed(KEY_ID_TOUCHPAD_SECOND_DOWN)) {
			Touch2.X = TouchLeftMode(Touch2.LeftMode);
			Touch2.Y = TouchTopMode(Touch2.TopMode);
			if (Touch2.SkipPollTop == 0) { if (Touch2.TopMode < TOUCH_TOP_MODE_MAX) ++Touch2.TopMode; Touch2.SkipPollTop = SkipPollTimeOut; }
		}

		// Touchpad swipes, last
		if (TouchpadSwipeUp) { TouchpadSwipeUp = false; Touch1.X = 960; Touch1.Y = 100; }
		if (TouchpadSwipeDown) { TouchpadSwipeDown = false; Touch1.X = 960; Touch1.Y = 841; }
		if (TouchpadSwipeLeft) { TouchpadSwipeLeft = false; Touch1.X = 200; Touch1.Y = 471; }
		if (TouchpadSwipeRight) { TouchpadSwipeRight = false; Touch1.X = 1719; Touch1.Y = 471; }

		// Swipes
		if (TouchpadSwipeUp == false && IsKeyPressed(KEY_ID_TOUCHPAD_SWIPE_UP)) { Touch1.X = 960; Touch1.Y = 841; TouchpadSwipeUp = true; }
		if (TouchpadSwipeDown == false && IsKeyPressed(KEY_ID_TOUCHPAD_SWIPE_DOWN)) { Touch1.X = 960; Touch1.Y = 100; TouchpadSwipeDown = true; }
		if (TouchpadSwipeLeft == false && IsKeyPressed(KEY_ID_TOUCHPAD_SWIPE_LEFT)) { Touch1.X = 1719; Touch1.Y = 471; TouchpadSwipeLeft = true; }
		if (TouchpadSwipeRight == false && IsKeyPressed(KEY_ID_TOUCHPAD_SWIPE_RIGHT)) { Touch1.X = 200; Touch1.Y = 471; TouchpadSwipeRight = true; }

		// Motion shaking
		if (IsKeyPressed(KEY_ID_MOTION_SHAKING)) MotionShaking = true;
		if (IsKeyPressed(KEY_ID_MOTION_Y_ADD)) MotionYAdd = true;
		if (IsKeyPressed(KEY_ID_MOTION_Y_SUB)) MotionYSub = true;
		if (IsKeyPressed(KEY_ID_MOTION_X_SUB)) MotionXAdd = true;
		if (IsKeyPressed(KEY_ID_MOTION_X_ADD)) MotionXSub = true;
		if (IsKeyPressed(KEY_ID_MOTION_Z_ADD)) MotionZAdd = true;
		if (IsKeyPressed(KEY_ID_MOTION_Z_SUB)) MotionZSub = true;

		// состояние пальцев сейчас
		if (!XboxStickTouchActive) TouchpadStickState.Release();
		Touch1.IsChanged = XboxStickTouchActive || (Touch1.X | Touch1.Y) != 0;
		Touch2.IsChanged = (Touch2.X | Touch2.Y) != 0;

		// фронт касания -> новый track-id (0..127)
		if (Touch1.IsChanged && !Touch1.PrevTouched) Touch1.ID = uint8_t((Touch1.ID + 1) & 0x7F);
		if (Touch2.IsChanged && !Touch2.PrevTouched) Touch2.ID = uint8_t((Touch2.ID + 1) & 0x7F);

		// previous, если есть прошлый пакет
		report.bTouchPacketsN = LastTouchValid ? 2 : 1;
		if (LastTouchValid) report.sPreviousTouch[0] = LastTouch;

		// палец 1
		if (Touch1.IsChanged) {
			report.sCurrentTouch.bIsUpTrackingNum1 = uint8_t(Touch1.ID & 0x7F); // down
			report.sCurrentTouch.bTouchData1[0] = uint8_t(Touch1.X & 0xFF);
			report.sCurrentTouch.bTouchData1[1] = uint8_t(((Touch1.X >> 8) & 0x0F) | ((Touch1.Y & 0x0F) << 4));
			report.sCurrentTouch.bTouchData1[2] = uint8_t((Touch1.Y >> 4) & 0xFF);
		}

		// палец 2
		if (Touch2.IsChanged) {
			report.sCurrentTouch.bIsUpTrackingNum2 = uint8_t(Touch2.ID & 0x7F); // down
			report.sCurrentTouch.bTouchData2[0] = uint8_t(Touch2.X & 0xFF);
			report.sCurrentTouch.bTouchData2[1] = uint8_t(((Touch2.X >> 8) & 0x0F) | ((Touch2.Y & 0x0F) << 4));
			report.sCurrentTouch.bTouchData2[2] = uint8_t((Touch2.Y >> 4) & 0xFF);
		}

		// счетчик пакетов 0..255
		report.sCurrentTouch.bPacketCounter = ++TouchPacket;

		// сохранить текущий как previous на следующий тик
		LastTouch = report.sCurrentTouch;
		LastTouchValid = true;

		// Запоминаем, что было нажатие
		Touch1.PrevTouched = Touch1.IsChanged;
		Touch2.PrevTouched = Touch2.IsChanged;

		// freePIE gyro and accel in rad/s and m/s^2, need to scale with accel and gyro configs
		report.wAccelX = trunc(Clamp(AccelX / AccelSens * 32767, -32767, 32767)) * InverseXStatus;
		report.wAccelY = trunc(Clamp(AccelY / AccelSens * 32767, -32767, 32767)) * InverseYStatus;
		report.wAccelZ = trunc(Clamp(AccelZ / AccelSens * 32767, -32767, 32767)) * InverseZStatus;
		report.wGyroX = trunc(Clamp(GyroX / GyroSens * 32767, -32767, 32767)) * InverseXStatus;
		report.wGyroY = trunc(Clamp(GyroY / GyroSens * 32767, -32767, 32767)) * InverseYStatus;
		report.wGyroZ = trunc(Clamp(GyroZ / GyroSens * 32767, -32767, 32767)) * InverseZStatus;
		// if ((IsKeyPressed(VK_NUMPAD1)) printf("%d\t%d\t%d\t%d\t%d\t%d\t\n", report.wAccelX, report.wAccelY, report.wAccelZ, report.wGyroX, report.wGyroY, report.wGyroZ);

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
		}
		else if (MotionXAdd) {        // движение вправо по X (roll +)
			report.wAccelX = 32767; report.wAccelY = 0;      report.wAccelZ = 0;
			report.wGyroX = 32767; report.wGyroY = 0;      report.wGyroZ = 0;
		}
		else if (MotionXSub) {      // движение влево по X (roll -)
			report.wAccelX = -32767; report.wAccelY = 0;     report.wAccelZ = 0;
			report.wGyroX = -32767; report.wGyroY = 0;     report.wGyroZ = 0;
		}
		else if (MotionYAdd) {      // наклон вперёд по Y (pitch +)
			report.wAccelX = 0;      report.wAccelY = 32767; report.wAccelZ = 0;
			report.wGyroX = 0;      report.wGyroY = 32767; report.wGyroZ = 0;
		}
		else if (MotionYSub) {      // наклон назад по Y (pitch -)
			report.wAccelX = 0;      report.wAccelY = -32767; report.wAccelZ = 0;
			report.wGyroX = 0;      report.wGyroY = -32767; report.wGyroZ = 0;
		}
		else if (MotionZAdd) {      // поворот вправо по Z (yaw +)
			report.wAccelX = 0;      report.wAccelY = 0;      report.wAccelZ = 32767;
			report.wGyroX = 0;      report.wGyroY = 0;      report.wGyroZ = 32767;
		}
		else if (MotionZSub) {      // поворот влево по Z (yaw -)
			report.wAccelX = 0;      report.wAccelY = 0;      report.wAccelZ = -32767;
			report.wGyroX = 0;      report.wGyroY = 0;      report.wGyroZ = -32767;
		}

		// Explicit shake/keyboard/D-pad motion retains priority over analog motion.
		if (!(MotionShaking || MotionXAdd || MotionXSub || MotionYAdd || MotionYSub || MotionZAdd || MotionZSub)) {
			if (XboxStickMotionActive)
				XboxStickMotionRate.Apply(report);
			else if (MotionStickEnabled && EmulationMode == XboxMode && !SocketActivated) {
				// Keep calibrated gyro neutral after releasing the analog modifier.
				// Leave external IMU, legacy/digital motion and acceleration behavior intact.
				report.wGyroX = XboxMotion::GyroPitchZero;
				report.wGyroY = 0; report.wGyroZ = 0;
			}
		}

		// Special keys
		if (IsKeyPressed(KEY_ID_PS))
			report.bSpecial |= DS4_SPECIAL_BUTTON_PS;

		// if (IsKeyPressed(VK_NUMPAD0)) system("cls");

		auto now = std::chrono::steady_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
		report.wTimestamp = (USHORT)(ms & 0xFFFF);

		ret = vigem_target_ds4_update_ex(client, ds4, report);

		// Don't overload the CPU with reading
		if (EmulationMode == XboxMode)
			Sleep(SleepTimeOutXbox);
		else
			Sleep(SleepTimeOutKB);

		if (Touch1.SkipPollLeft > 0) Touch1.SkipPollLeft--;
		if (Touch1.SkipPollTop > 0) Touch1.SkipPollTop--;
		if (Touch2.SkipPollLeft > 0) Touch2.SkipPollLeft--;
		if (Touch2.SkipPollTop > 0) Touch2.SkipPollTop--;
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
