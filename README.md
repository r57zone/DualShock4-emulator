[![EN](https://user-images.githubusercontent.com/9499881/33184537-7be87e86-d096-11e7-89bb-f3286f752bc6.png)](https://github.com/r57zone/DualShock4-emulator/) 
[![RU](https://user-images.githubusercontent.com/9499881/27683795-5b0fbac6-5cd8-11e7-929c-057833e01fb1.png)](https://github.com/r57zone/DualShock4-emulator/blob/master/README.RU.md)
[![FR](https://user-images.githubusercontent.com/9499881/147121779-f90bdadf-8009-4dc4-8682-f15f4bd2008e.png)](https://github.com/r57zone/DualShock4-emulator/blob/master/README.FR.md)
&#8211; Other languages

# DualShock4 emulator
Simple application to emulate the Sony DualShock 4 gamepad using an Xbox controller or keyboard and mouse. This method is necessary for the fully work of the service [Sony Playstation Plus](https://www.playstation.com/en-us/ps-plus/) or [Playstation Remote Play](https://www.playstation.com/remote-play/). Works based on the driver [ViGEm](https://github.com/ViGEm).

## Setup
1. Install [ViGEmBus](https://github.com/ViGEm/ViGEmBus/releases).
2. Install Microsoft Visual C++ Redistributable 2017 or newer.
3. Unpack and launch "DualShock4 emulator" (**Attention!** It is important to run DS4 emulator before starting PS Plus, if you are using an Xbox controller, so that PS Plus gives priority to the DualShock controller).
4. Launch "PlayStation Plus", "PS Remote Play", "xCloud", or another application. Read the FAQ to set up xCloud.
5. If necessary, you can invert the axis, change the `InvertX` and `InvertY` parameters to `1` in the "Config.ini" configuration file.
6. You can also check how the DualShock 4 controller emulation works in the [VSCView](https://github.com/Nielk1/VSCView/releases/) program. To emulate from the keyboard and mouse, you also need to change the `ActivateInAnyWindow` parameter to `1` and restart the program.

## FAQ
**• The program crashes after launch**<br>
"Antivirus" blocks the dynamic loading of the Xbox gamepad library, so the program crashes. You can close it for the duration of use.



**• Touchpad press don't work**<br>
It is possible that the "PS Plus" or "PS Remote Play" apps have given priority to the Xbox controller, so restart the "PS Plus" or "PS Remote Play" apps and the emulated DualShock 4 should take precedence over the Xbox controller.



**• When playing in xCloud, in the browser, the context menu is called up on the right mouse button, how can I remove it in the browser?**<br>
First change the name of the window in the configuration file or change the `ActivateInAnyWindow` parameter to `1`, restarting the program. Next, go to the xCloud website, press "F12", select the console and paste [this code](https://github.com/r57zone/DualShock4-emulator/blob/master/ContextMenuBlock.txt) there, press run and the context menu will no longer be shown.

## Xbox controller
The "Back/View/Select" button (the first button to the right of the left stick) on the Xbox controller emulating pressing the touchpad on a Sony DualShock 4.

The "PS Share" button is binded to the simultaneous pressing of the "Back/View/Select" and "Start/Menu" buttons or to the "F12" key.

The "PS" button is binded to the "Xbox" button, but to use it, you need to disable the use of this button in the "Xbox Game Bar" settings. Also, the "PS" button is tied to the simultaneous pressing of the "Back/View/Select" and "LB" buttons (left bumper) or the "F2" key.

You can shake (gyro) the controller by pressing "Back/View/Select" and "RB" (right bumper). 



If necessary, you can swap bumpers and triggers, as well as the "share" button and pressing the touchpad, to do this change the `SwapTriggersShoulders` or `SwapShareTouchPad` parameter to `1` in the "Config.ini" configuration file.



Changing the dead zone of sticks for drifting sticks is supported. Press "ALT" + "F9" to get the values, paste them into the "Config.ini" configuration file, into the `DeadZone` parameters and restart the program.

## Touchpad
Game | Action
------------ | -------------
Uncharted 3: Drake’s Deception (2011) | The "Share" button (F12) duplicates pressing the left side of the touchpad.
The Last Of Us Part II (2020) | Options -> Accessibility -> "Strumming Settings" instead of vertical and horizontal, put buttons.

On the Xbox gamepad, you need to press the "Back/View/Select" button (touchpad) and move the stick to the sides for swipes. By default, pressing the touchpad during swipes is disabled, it can be enabled in the configuration file by changing the `TouchPadPressedWhenSwiping` parameter to `1`.



You can use swipes for the keyboard, the button codes are described below. 

## Gyroscope
1. Check Windows Firewall to see if incoming connections are allowed on your network type (private) and allow if disabled.
2. Install FreePieIMU on your Android phone by taking the latest version in the [OpenTrack archive](https://github.com/opentrack/opentrack) or in the [releases](https://github.com/r57zone/DualShock4-emulator/releases), enter the IP address of your computer, select "Send raw data", if not selected, select the data rate "Fastest" or "Fast".
3. Reduce the sensitivity if necessary (the `Sens` parameter, in the `Motion` section, where `100` is 100% sensitivity) in configuration file.
4. Invert the axes if necessary (the parameters `InverseX`, `InverseY` and `InverseZ`, in the `Motion` section, where `1` is turning on the inversion, and `0` is turning off).


If you just need to shake (gyro) the gamepad in the game, then there is no need to install Android applications, just press the "shake" button of the gamepad.

## Keyboard and mouse
By default, the mouse and keyboard only work in the windows "PlayStation Plus" and "PS4 Remote Play" (change the `ActivateOnlyInWindow2` parameter to your regional application title). To work only in any other applications or emulators, change the parameters `ActivateOnlyInWindow` and `ActivateOnlyInWindow2` to the headers of these applications You can enable the work in all windows (change the `ActivateInAnyWindow` parameter to `1`, in the "Config.ini" configuration file) or change the name of the window (the `ActivateOnlyInWindow` parameter) in which the actions are captured. This is necessary so that the cursor is centered only in one window and no buttons are pressed when the window is minimized.

To disable cursor centering, hold down the "C" button (can change it in the config - `StopСenteringKey`).

To hide the cursor after startup, change `HideCursorAfterStart` to `1`, to restore the cursor, close the program by pressing "ALT" + "ESCAPE" or "~".

For full-screen Playstation Plus use the keys "ALT" + "F10", the upper black bar, as well as the taskbar will be hidden. To return to the normal window, press these keys again. You can disable hiding the taskbar in the configuration file by changing the `HideTaskBarInFullScreen` parameter to `0`. If the Playstation Plus window changes once, you can change the default top offset, the `FullScreenTopOffset` parameter. 

DualShock 4 | Keyboard and mouse
------------ | -------------
L1 | Control
R1 | Alt
L2 | Right mouse button
R2 | Left mouse button
SHARE | F12
TOUCHPAD (pressing) | Enter
OPTIONS | Tab
DPAD UP | 1
DPAD LEFT | 2
DPAD RIGHT | 3
DPAD DOWN | 4
TRIANGLE | Q
SQUARE | E
CIRCLE | R
CROSS | Space
L3 (pressing the stick) | Shift
R3 (pressing the stick) | Middle mouse button
Touchpad swipe up, down, left, right | 7, 8, 9, 0
Touchpad up, center, left, right, down  | U, J, H, K, N
Shake the gamepad | T
PS | F2

Сan replace button bindings in the "Config.ini" configuration file. Button codes can be found [here](https://github.com/r57zone/Half-Life-Alyx-novr/blob/master/BINDINGS.md).



The sensitivity parameters `SensX`, `SensY` for the mouse can also be found in the configuration file "Config.ini", in the section "Mouse". If there is no stick movement, you can try increasing the "SleepTimeOut" parameter to 2, 4, 8, 10.



You can also enable emulation of analog triggers (L2, R2), change the `EmulateAnalogTriggers` parameter to `1`, and increase step `AnalogTriggerStep` (from 0.1 to 255).

## Download
>Version for Windows 10.

**[Download](https://github.com/r57zone/DualShock4-emulator/releases)**

## Forks
Fork with support for reading gyroscope data from Nintendo Joy-Con - **[More detailed](https://github.com/Okoken/DualShock4-emulator)**.

## Feedback
`r57zone[at]gmail.com`