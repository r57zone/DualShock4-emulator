# Using one virtual DS4 without an emulator launcher

HidHide is optional. The latest [user-reported shadPS4 Back/touchpad workaround](ShadPS4Input.md)
keeps the physical Xbox visible as a second controller and maps its Back/View to
Touchpad Center. Hiding was disabled in that arrangement. The earlier single-DS4
test below records a different configuration, not proof that hiding is required.

The physical Xbox controller and the virtual DS4 are separate devices. A game can
read ordinary buttons from one while touch and motion arrive on another player
slot. Selecting PS4 in a bindings screen does not by itself verify that native
touch events reach the game's player. In a local shadPS4 test, swipes worked after
the virtual DS4 became the sole controller in slot 0; it had previously occupied
slot 1. This diagnosis is independent of the Back-click drift fix below.

HidHide can hide a physical controller from other programs while allowing
DS4Emulator to read it. It operates at the Windows device level, so no game path
or shadPS4 executable path is needed. Use the [official installer](https://github.com/nefarius/HidHide/releases)
and [setup guide](https://docs.nefarius.at/projects/HidHide/Simple-Setup-Guide/).
Finish any requested Windows restart before testing.

Before enabling hiding, verify that the allowlisted DS4Emulator starts in **Xbox**
mode. If it cannot read an Xbox controller at startup, it can fall back to keyboard
and mouse mode. With `EnableCentering=1` and `ActivateInAnyWindow=1` in
`[KeyboardMouse]`, that mode repeatedly centers the desktop cursor. A local
mouse-lock incident occurred during hiding setup; this fallback is a plausible
cause, but the exact triggering state was not captured. Disable hiding and close
DS4Emulator to recover. For a controller-only setup, setting both options to `0`
prevents this cursor-centering behavior if startup falls back to keyboard mode.

## One-time configuration

1. In HidHide Configuration Client, add the exact DS4Emulator.exe location to
   **Applications**. Moving or renaming it requires registering its new path.
2. Under **Devices**, select the physical Xbox controller. Leave the virtual Sony
   DS4 unselected. USB and Bluetooth connections may need separate entries.
3. Leave **Inverse application cloak** off; enable device hiding.
4. Reconnect the controller or restart Windows if the driver installer requires it.
5. Start DS4Emulator, then launch the game/emulator normally. Check that the game
   sees only the virtual DS4 and test Back alone, Back + right-stick swipe, LT/RT,
   and the chosen motion combination separately.

Windows may expose a controller through several related interfaces, particularly
with additional controller drivers. Verify which interfaces are still visible
before adding them; do not hide unrelated devices or the virtual DS4.

**Xbox limitation:** the [official FAQ](https://docs.nefarius.at/projects/HidHide/FAQ/)
states that Xbox/XInput cloaking is not reliable in every setup. A checked device
or an enabled cloak does not prove success. Verify that an ordinary application
cannot read the physical controller while the allowlisted DS4Emulator can.
Raw Input consumers can also remain outside HidHide's coverage. Do not claim
that a launcher or filter is unnecessary until the receiving emulator is checked.

## Returning to normal Xbox input

Disable **Enable device hiding**, close DS4Emulator, and reopen the game. Reconnect
the controller if the game retains its previous device handles. To emulate DS4
again, re-enable hiding, start the allowlisted DS4Emulator and reopen the game.
The normal-input option disables HidHide globally, including any other devices
you previously configured; it does not uninstall the driver.

The portable package provides `Tools/Use-DS4.cmd` and `Tools/Use-Xbox.cmd` for these
two modes. They change HidHide configuration only: they do not launch a game,
install a driver, select devices, reboot Windows or stop running programs.
One-time device selection remains necessary. `Use-DS4.cmd` registers the package's
DS4Emulator path before enabling hiding. In a source checkout, run:

```powershell
./Tools/HidHideMode.ps1 -Mode DS4 -EmulatorPath 'C:\path\DS4Emulator.exe'
./Tools/HidHideMode.ps1 -Mode Xbox
./Tools/HidHideMode.ps1 -Mode Status
```

## Back alone versus a swipe

The original click-suppression condition required exact 8-bit right-stick center
values `127/129`. Small physical stick drift could cancel Back's touchpad click
even when the user intended a plain press. Removing a second input source can
expose that cancellation.

This fork adds `TouchpadClickDeadzone=0.12` in `[Xbox]`. It keeps touchpad click for
sub-threshold drift and suppresses click for meaningful right-stick movement
when `TouchPadPressedWhenSwiping=0`. This does not alter touch coordinates, LT/RT,
motion, Share swapping or normal-stick suppression. Set the click deadzone to
`0` to restore the exact original check. The existing click-during-swipe option
and right-stick-click override remain supported. Report tests reproduce the old
failure with measured right-stick values `(-73,-1238)`; gameplay confirmation of
the new binary is a separate step.
