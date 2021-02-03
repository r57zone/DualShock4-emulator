[![EN](https://user-images.githubusercontent.com/9499881/33184537-7be87e86-d096-11e7-89bb-f3286f752bc6.png)](https://github.com/r57zone/DualShock4-emulator/) 
[![RU](https://user-images.githubusercontent.com/9499881/27683795-5b0fbac6-5cd8-11e7-929c-057833e01fb1.png)](https://github.com/r57zone/DualShock4-emulator/blob/master/README.RU.md)
# DualShock4 emulator
Simple application to emulate the Sony DualShock 4 gamepad using an Xbox controller or keyboard and mouse. This method is necessary for the fully work of the service [Sony Playstation Now](https://www.playstation.com/en-us/explore/playstation-now/) or Playstation Remote Play. Works based on the driver [ViGEm](https://github.com/ViGEm).

## Setup
1. Install [ViGEmBus](https://github.com/ViGEm/ViGEmBus/releases).
2. Install Microsoft Visual C ++ Redistributable 2017 or newer.
3. Unpack and launch "DualShock4 emulator".
4. Launch "PlayStation Now" or other app.
5. If necessary, you can invert the axes, change the `InvertX` and `InvertY` parameters to `1` in the "Config.ini" configuration file, in the "DS4" section.

## Xbox controller
The "Back" button on the Xbox controller emulating pressing the touchpad on a Sony DualShock 4.

The "Share" button is assigned to the "F12" key.



If necessary, you can swap bumpers and triggers, as well as the "share" button and pressing the touchpad, to do this change the `SwapTriggersShoulders` or `SwapShareTouchPad` parameter to `1` in the "Config.ini" configuration file.

## Touchpad
Game | Action
------------ | -------------
Uncharted 3: Drake’s Deception (2011) | The "Share" button (F12) duplicates pressing the left side of the touchpad.

## Keyboard and mouse
Сan replace button bindings in the "Config.ini" configuration file. Button codes can be found [here](https://github.com/r57zone/Half-Life-Alyx-novr/blob/master/BINDINGS.md).
DualShock 4 | Keyboard and mouse
------------ | -------------
LEFT TRIGGER | Right mouse button
RIGHT TRIGGER | Left mouse button
LEFT SHOULDER | Control
RIGHT SHOULDER | Alt
DPAD UP | 1
DPAD LEFT | 2
DPAD RIGHT | 3
DPAD DOWN | 4
LEFT THUMB | Shift
RIGHT THUMB | Middle mouse button
TRIANGLE | Q
SQUARE | E
CIRCLE | R
CROSS | Space
TOUCHPAD | Enter
OPTIONS | Tab
SHARE | F12

The sensitivity parameters `SensX`, `SensY` for the mouse can also be found in the configuration file "Config.ini", in the section "Mouse". If there is no stick movement, you can also try increasing the "SleepTimeOut" parameter to 5, 7, 10.



You can also enable emulation of analog triggers (L2, R2), change the `EmulateAnalogTriggers` parameter to `1`, and increase step `AnalogTriggerStep` (from 0.1 to 255).

## Download
>Version for Windows 10.

**[Download](https://github.com/r57zone/DualShock4-emulator/releases)**

## Feedback
`r57zone[at]gmail.com`