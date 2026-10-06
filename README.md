[![EN](https://user-images.githubusercontent.com/9499881/33184537-7be87e86-d096-11e7-89bb-f3286f752bc6.png)](https://github.com/r57zone/DualShock4-emulator/) 
[![RU](https://user-images.githubusercontent.com/9499881/27683795-5b0fbac6-5cd8-11e7-929c-057833e01fb1.png)](https://github.com/r57zone/DualShock4-emulator/blob/master/README.RU.md)
[![FR](https://user-images.githubusercontent.com/9499881/147121779-f90bdadf-8009-4dc4-8682-f15f4bd2008e.png)](https://github.com/r57zone/DualShock4-emulator/blob/master/README.FR.md)
← Choose language | Выберите язык

# DualShock4 emulator
Simple application to emulate the Sony DualShock 4 gamepad using an Xbox controller or keyboard and mouse. This method is necessary for the fully work of the service [Sony Playstation Plus](https://www.playstation.com/en-us/ps-plus/) or [Playstation Remote Play](https://www.playstation.com/remote-play/). Works based on the driver [ViGEm](https://github.com/ViGEm).

## Xbox input improvements in this branch

This development branch extends the original project by [r57zone](https://github.com/r57zone/DualShock4-emulator). The changes are proposed upstream in [PR #107](https://github.com/r57zone/DualShock4-emulator/pull/107).

- **Simultaneous triggers:** LT/L2 and RT/R2 remain available while Back/View activates touchpad or motion emulation. Shoulder/trigger swapping is preserved.
- **Optional analog motion:** hold a configurable button and move the right stick beyond its motion deadzone to start DS4 gyro control. The preset below uses Start/Menu: a plain press opens the menu on release, while a motion gesture never sends Options. RB remains normal R1. Speed, deadzone, response curve, axes and inversion are configurable. Releasing the modifier restores normal right-stick control; existing Back + D-pad rotation and Back + RB shake retain priority.
- **Console sensitivity editor:** focus the emulator console and press M. Arrows adjust speed, Shift + arrows make finer adjustments, Enter saves, and Esc/M cancels. The editor adds no global shortcut and releases virtual game inputs while editing.
- **Gyro neutral correction:** opt-in analog motion accounts for ViGEm's pitch calibration offset, including after releasing the modifier, to avoid a small calibrated rotation at rest.
- **Reliable plain Back click:** a separate 12% click deadzone prevents small right-stick drift from cancelling touchpad press. Swipe coordinates are unchanged; setting `TouchpadClickDeadzone=0` restores the exact original check.
- **Optional touchpad modes:** legacy, improved absolute and relative/velocity mapping. Existing configs retain legacy touchpad controls and have analog motion disabled.

### Original behavior and our changes

| Original 2.2 behavior or limitation | Change in this fork |
| --- | --- |
| LT/RT mapping was inside the block skipped while the Xbox motion/touchpad modifier was held. This prevented simultaneous touchpad input and R2/L2. | Trigger values and DS4 trigger flags are mapped independently of that block; shoulder/trigger swapping and Back stick suppression are retained. |
| Controller motion used digital rotation/shake combinations, with no proportional right-stick gyro control. | Optional analog angular velocity with radial deadzone, linear/quadratic curve, configurable speed and axes. The release preset uses Start + right stick and keeps RB as R1. |
| Legacy touchpad mapping converts to 8-bit axes and uses shared stick magnitude; small commands can jump away from center. | Optional 16-bit absolute mapping with smoothing, or elapsed-time relative movement that holds position at stick center. Legacy remains selectable; the release preset keeps it. |
| Touchpad click was suppressed whenever right-stick report bytes differed from exact center `127/129`, including small stick drift. | `TouchpadClickDeadzone=0.12` separates a plain Back click from deliberate stick movement without changing touch coordinates; zero restores the original condition. |
| There was no dedicated console editor for analog motion speed. | Console-only M editor, coarse/fine speed steps and save/cancel; no new global shortcut to conflict with the game/emulator. |
| Raw zero gyro differs from calibrated zero with the measured ViGEm pitch bias (+1), producing a small nonzero calibrated rate. | Enabled analog motion sends the calibrated neutral value, including after modifier release. Digital motion and external IMU output retain their behavior. |

Two refinements address conflicts introduced during this fork's development: the first RB modifier reserved R1 even for a plain press, so plain RB output was restored; the recommended Start modifier now defers Options until release and cancels it after an analog gesture, avoiding an initial menu press. These are fork refinements, not additional defects attributed to the original project.

Back touchpad/rotation/shake combinations remain available. Meaningful right-stick movement still suppresses click by default, while the separate click deadzone preserves plain Back under small drift. Share swapping and click-during-swipe/right-stick-click options are retained. Touchpad and gyro are different input paths: native inFAMOUS graffiti aiming motivated the analog motion feature; the touchpad experiments alone do not solve it. Reproducible touchpad comparisons are documented in [TouchpadStick.md](Docs/TouchpadStick.md).

For a slower starting point, merge these keys into the existing `[Xbox]` section of `Config.ini` and restart:

```ini
TouchpadStickMode=legacy
TouchpadClickDeadzone=0.12
MotionStickEnabled=1
MotionStickKey=START
MotionStickSpeed=30
MotionStickDeadzone=0.12
MotionStickCurve=quadratic
MotionStickHorizontalAxis=Y
MotionStickVerticalAxis=X
MotionStickInvertHorizontal=1
MotionStickInvertVertical=0
```

With this preset, Start/Menu alone opens the game menu on release. Start + right-stick movement starts gyro and never sends Options, including when the stick centers or Start is released. RB remains normal R1. Back + Start still sends Share (or touchpad click with the existing swap setting). A chord consumes the entire held Start press: releasing Back first, centering the stick or returning from a chord to analog control does not rearm a menu tap. The original RB modifier remains selectable; its plain press is forwarded immediately before analog movement.

This motion input is intended for tasks such as inFAMOUS Second Son graffiti aiming; slowing touchpad input alone does not address that motion control. Use the existing Back + D-pad rotation for the initial controller rotation, then Start + right stick for analog motion, with RT available for spraying. In shadPS4, select the virtual DS4, enable motion input and turn off F6 mouse gyro while testing. Directions may need adjustment for the game's current orientation or emulator build. Start also works with `SwapTriggersShoulders=1`; a shoulder modifier is suspended in that configuration.

See [motion controls and console tuning](Docs/MotionStick.md) and [touchpad settings and comparisons](Docs/TouchpadStick.md). Set `MotionStickEnabled=0` and `TouchpadStickMode=legacy` to use the original control scheme while retaining the trigger fix.

**shadPS4 Back/touchpad workaround:** one user reports that swipes work, but the interaction requiring Back alone works only after keeping the physical Xbox visible as a second controller and mapping its Back/View to **Touchpad Center**, with HidHide off. DS4Emulator generates native click and center-touch data for plain Back; why the game needs the additional Xbox binding remains unconfirmed. See [the observed configuration and why virtual DS4 Back cannot be captured as Xbox Back](Docs/ShadPS4Input.md).

Release x64 builds and synthetic report/regression tests passed. The gyro calibration offset was also measured from a live virtual DS4. These checks do not establish gameplay results; the latest drift correction still needs in-game confirmation. Reproduce tests with `Tests/run_touchpad_tests.ps1 -Python python`; add `-Render` to generate the touchpad comparison GIFs.

## Setup
1. Install [ViGEmBus](https://github.com/ViGEm/ViGEmBus/releases).
2. Install [Microsoft Visual C++ Redistributable 2017](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist) or newer.
3. Unpack and launch `DualShock 4 emulator` (**Attention!** It is important to run DS4 emulator before starting `PS Plus`, if you are using an Xbox controller, so that `PS Plus` gives priority to the DualShock 4 controller).
4. Launch `PlayStation Plus`, `PS Remote Play`, `xCloud`, or another application. Read the FAQ to set up `xCloud`.
5. If necessary, you can invert the axis, change the `InvertX` and `InvertY` parameters to `1` in the `Config.ini` configuration file.
6. You can also check how the DualShock 4 controller emulation works in the [VSCView](https://github.com/Nielk1/VSCView/releases/) program. 
7. Study the configuration `Config.ini` and the description below, perhaps something can be configured more conveniently.
8. When using Steam games, in the controller settings, disable `PlayStation Controller Support`.

## Download
>Version for Windows 10, 11.

**[Download this fork: Xbox motion preview 3 (Windows x64)](https://github.com/emribilemir/DualShock4-emulator/releases/tag/v2.2-xbox-motion-preview.3)**

Use the portable ZIP for the EXE, Start motion preset (30 deg/s), Back click deadzone (12%), controller profiles, cursor, instructions, optional HidHide mode-switch tools and licenses. The release also provides the standalone EXE, SHA-256 checksums and build information. Install ViGEmBus separately. This is a prerelease: Release x64 builds and automated tests passed; the latest Start routing and drift correction still need in-game confirmation. It is a fork build, not an official upstream release. Code and discussion are tracked in [PR #107](https://github.com/r57zone/DualShock4-emulator/pull/107).

[Original upstream releases](https://github.com/r57zone/DualShock4-emulator/releases) remain available; those binaries do not include this fork's changes. The project's [Unlicense](LICENSE) permits source and compiled binary distribution; the package also includes the ViGEmClient MIT license and attribution.

## FAQ
**• The program crashes after launch**<br>
"Antivirus" blocks the dynamic loading of the Xbox gamepad library, so the program crashes. You can close it for the duration of use.



**• Touchpad press don't work**<br>
It is possible that the "PS Plus" or "PS Remote Play" apps have given priority to the Xbox controller, so restart the "PS Plus" or "PS Remote Play" apps and the emulated DualShock 4 should take precedence over the Xbox controller.



**• When playing in xCloud, in the browser, the context menu is called up on the right mouse button, how can I remove it in the browser?**<br>
First change the name of the window in the configuration file or change the `ActivateInAnyWindow` parameter to `1`, restarting the program. Next, go to the xCloud website, press "F12", select the console and paste [this code](https://github.com/r57zone/DualShock4-emulator/blob/master/ContextMenuBlock.txt) there, press run and the context menu will no longer be shown.



• **The game sees 2 controllers at the same time (DualSense / DualShock 4 / Nintendo Pro controller or JoyCons and Xbox)**<br>
For shadPS4, first see the [reported two-controller Back/touchpad workaround](Docs/ShadPS4Input.md). For optional device isolation, see the [HidHide setup and normal-Xbox mode guide](Docs/HidHide.md) to hide the physical controller while allowing DS4Emulator to read it. Hiding removes the physical input used by that workaround. The guide includes DS4/Xbox mode switches and the documented Xbox/XInput limitations.

## Xbox controller
The "Back/View/Select" button (the first button to the right of the left stick) on the Xbox controller emulating pressing the touchpad on a Sony DualShock 4.

The `Share` button is binded to the simultaneous pressing of the `Back/View/Select` and `Start/Menu` buttons or to the `F12` key.

The `PS` button is binded to the `Xbox` button, but to use it, you need to disable the use of this button in the "Xbox Game Bar" settings. Also, the `PS` button is tied to the simultaneous pressing of the `Back/View/Select` and `LB` buttons (left bumper) or the `F2` key.

You can shake (gyro) the controller by pressing `Back/View/Select` and `RB` (right bumper). You can change the combinations in the configuration file.


You can rotate the gamepad (gyroscope) by pressing `Back` and the `DPAD ←↑↓→` (you can change the combinations in the configuration file).

Optional [right-stick gyro control](Docs/MotionStick.md) uses a separate held button for analog motion while preserving Back touchpad/rotation/shake combinations.


By default, the `RB` and `DPAD ←↑↓→` buttons also work in the game, if they interfere with movement activation, you can try disabling them using the `DisableButtonOnMotion` parameter.


If necessary, you can swap bumpers and triggers, as well as the `Share` button and pressing the touchpad, to do this change the `SwapTriggersShoulders` or `SwapShareTouchPad` parameter to `1` in the "Config.ini" configuration file.


The movement activation button, by default `Back`, can be reassigned to other buttons, for example, to the `Xbox` button. In this case, you will need to turn off `EnableXboxButton` and activation of the "Xbox Game Bar" in Windows.


Changing the dead zone of sticks for drifting sticks is supported. Press `ALT + F9` to get the values, paste them into the "Config.ini" configuration file, into the `DeadZone` parameters and restart the program.

## Keys for emulating touchpad, motion, etc.

DualShock 4 | Keyboard and mouse
------------ | -------------
Touchpad swipe up, down, left, right | `Home, End, Delete, Page down`
Touchpad first touch: up, down, left, right, down | `U, J, H, K`
Touchpad second touch: up, down, left, right, down | `↑, ↓, ←, →`
Shake the gamepad | `T`
Rotate gamepad forward, backward, right, left (motion, gyroscope) | `Numpad 8, 2, 4, 6, 7, 9`
PS | `F2`

In the `Default.ini` profile configuration file, in the `Profiles` folder, you can change the button bindings.

## Keyboard and mouse
By default, the mouse and keyboard only work in the windows `PlayStation Plus` and `PS4 Remote Play` (change the `ActivateOnlyInWindow2` parameter to your regional application title). To work only in any other applications or emulators, change the parameters `ActivateOnlyInWindow` and `ActivateOnlyInWindow2` to the headers of these applications You can enable the work in all windows (change the `ActivateInAnyWindow` parameter to `1`, in the "Config.ini" configuration file) or change the name of the window (the `ActivateOnlyInWindow` parameter) in which the actions are captured. This is necessary so that the cursor is centered only in one window and no buttons are pressed when the window is minimized.

To disable cursor centering, hold down the `C` button (can change it in the config - `StopСenteringKey`).

To hide the cursor after startup, change `HideCursorAfterStart` to `1`, to restore the cursor, close the program by pressing `ALT + ESCAPE` or `~`.

For full-screen Playstation Plus use the keys `ALT + F10`, the upper black bar, as well as the taskbar will be hidden. To return to the normal window, press these keys again. You can disable hiding the taskbar in the configuration file by changing the `HideTaskBarInFullScreen` parameter to `0`. If the Playstation Plus window changes once, you can change the default top offset, the `FullScreenTopOffset` parameter. 

DualShock 4 | Keyboard and mouse
------------ | -------------
L1 | `Alt`
R1 | `Control`
L2 | `Right mouse button`
R2 | `Left mouse button`
SHARE | `F12`
TOUCHPAD (pressing) | `Enter`
OPTIONS | `Tab`
DPAD UP | `1`
DPAD LEFT | `2`
DPAD RIGHT | `3`
DPAD DOWN | `4`
TRIANGLE | `E`
SQUARE | `R`
CIRCLE | `Q`
CROSS | `Space`
L3 (pressing the stick) | `Shift`
R3 (pressing the stick) | `Middle mouse button`


In the profile configuration file, in the `Profiles` folder, you can change the button bindings or create a new one based on `Default.ini`. Button names can be found [here](https://github.com/r57zone/DualShock4-emulator/blob/master/BINDINGS.md). You can choose from standard profiles. Send more convenient bindings for a variety of games.



The sensitivity parameters `SensX`, `SensY` for the mouse can also be found in the configuration file `Config.ini`, in the section `Mouse`. If there is no stick movement, you can try increasing the "SleepTimeOut" parameter to 2, 4, 8, 10.



You can also enable emulation of analog triggers (L2, R2), change the `EmulateAnalogTriggers` parameter to `1`, and increase step `AnalogTriggerStep` (from 0.1 to 255).

## Touchpad in games
Game | Action
------------ | -------------
Uncharted 3: Drake’s Deception (2011) | The `Share` button (F12) duplicates pressing the left side of the touchpad.
The Last Of Us Part II (2020) | Options -> Accessibility -> "Strumming Settings" instead of vertical and horizontal, put buttons.

On the Xbox gamepad, you need to press the `Back/View/Select` button (touchpad) and move the stick to the sides for swipes. By default, pressing the touchpad during swipes is disabled, it can be enabled in the configuration file by changing the `TouchPadPressedWhenSwiping` parameter to `1`.


Configurable Xbox right-stick touchpad controls (legacy, improved absolute, or relative/velocity) and drawing presets are documented in [Xbox touchpad stick settings](Docs/TouchpadStick.md). Existing configs keep the legacy mapping.

## Motion with Android phone (Gyroscope)
1. Enable the `Activate` parameter in the `Config.ini` configuration file, changing `0` to `1`, in the `Motion` section.
2. Check Windows Firewall to see if incoming connections are allowed on your network type (private) and allow if disabled.
3. Install FreePieIMU on your Android phone by taking the latest version in the [OpenTrack archive](https://github.com/opentrack/opentrack) or in the [releases](https://github.com/r57zone/DualShock4-emulator/releases), enter the IP address of your computer, select "Send raw data", if not selected, select the data rate "Fastest" or "Fast".
4. Reduce the general sensitivity if necessary (the `Sens` parameter, in the `Motion` section, where `100` is 100% sensitivity) in configuration file.
5. Reduce individual sensor sensitivity if necessary (the `AccelSens` and `GyroSense`, in the `Motion` section,  where `100` is 100% sensitivity) in configuration file.
6. Invert the axes if necessary (the parameters `InverseX`, `InverseY` and `InverseZ`, in the `Motion` section, where `1` is turning on the inversion, and `0` is turning off).
7. Change phone orientation (the parameter `Orientation`, in the `Motion` section. where `1` is landscape and `0` is portrait).

If you just need to shake (gyro) the gamepad in the game, then there is no need to install Android applications, just press the "shake" button of the gamepad.

## Feedback
`r57zone[at]gmail.com`
