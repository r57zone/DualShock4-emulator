# Xbox right-stick gyro control

Merge these keys into `[Xbox]` in Config.ini, then restart:

```ini
TouchpadStickMode=legacy
MotionStickEnabled=1
MotionStickKey=START
MotionStickSpeed=60
MotionStickDeadzone=0.12
MotionStickCurve=quadratic
MotionStickHorizontalAxis=Y
MotionStickVerticalAxis=X
MotionStickInvertHorizontal=1
MotionStickInvertVertical=0
```

With `MotionStickKey=START`, Start/Menu is held as a pending action. Move the
right stick beyond the motion deadzone while holding Start to use gyro. Returning
to center stops motion; releasing Start after motion never sends Options. A plain
Start press without analog movement sends a 50 ms Options pulse on release, rather
than opening the menu on press. Back + Start still sends Share and cancels the
pending menu action. RB remains normal R1, and RT/LT stay available. Moving the
stick beyond the deadzone during a plain Start press counts as a motion gesture.
After Back + Start, the entire held Start press remains consumed, even if Back
is released first. The same rule applies when a Back chord interrupts analog
motion. Neither centering the stick nor releasing the remaining key can create
a new menu tap; a fresh plain Start press is required after release.

The older RB modifier is still selectable with `MotionStickKey=RIGHT-SHOULDER`:
RB alone retains normal R1 output. RB + right-stick movement beyond the configured
radial deadzone starts an analog motion gesture. RT/LT stay available. Once a
gesture starts, RB is reserved and normal right-stick axes are centered until RB
is released. Centering the stick stops gyro without sending an unintended R1.
Release RB to restore normal camera control and allow a new plain RB/R1 press.
Before stick movement starts, RB is forwarded immediately; this can produce an
initial R1 press if RB is pressed first. For games where that conflicts, configure
a separate motion key such as RIGHT-STICK (R3). Back priority, disconnect and
console editing clear the gesture.
Back + D-pad rotation, Back + RB shake, Back touchpad and explicit keyboard motion
retain priority. There is no automatic 90-degree rotation or orientation reset.
Use the intro rotation that already works before switching to analog drawing.

MotionStickEnabled defaults to 0. Existing configs/reports remain unchanged. The
earlier trigger-fix block and Back stick-suppression block are byte-identical.
Touchpad mode is independent and can stay legacy.

ViGEm's DS4 calibration feature report (0x02) advertises raw gyro pitch bias +1.
SDL subtracts this bias; sending raw zero creates about -0.061 deg/s of calibrated
pitch drift with the measured factory gain. The analog mode adds +1 to outgoing
pitch rates, including neutral while held and after release. Thus calibrated zero
is raw (1,0,0). Released neutral correction applies only in Xbox mode with
MotionStickEnabled and no external IMU socket; disabled legacy mode, digital
rotation/shake and external sensor output retain their behavior. Acceleration is
not changed on release. A live HID read confirmed the bias and old raw-zero output.

## Adjust speed in the console

Focus the DS4Emulator console and press M. Arrow keys change speed by 5 deg/s;
Shift+arrows use 1 deg/s. Enter saves only Xbox/MotionStickSpeed in Config.ini and
exits. Esc or M cancels and restores the speed that was active on entry. A failed
save keeps the editor open for retry. Other keys and Alt/Ctrl combinations are
ignored. Holding M does not repeatedly toggle the editor; arrows support repeat.

These controls read console input events, not global keyboard state, so keys typed
in the game window cannot activate the editor. While editing, all virtual game
inputs are released, gyro is zero, and touches are up; editor arrows cannot become
game inputs. Controls resume on exit. No F6, Alt+F7/F8/F10 or other global shortcut
is added. Console input settings are restored on normal program exit. Redirected
input disables the editor; Config.ini remains usable.

Use the virtual DS4 and enable gamepad motion/sensor reading in shadPS4. Turn F6
mouse gyro off while testing analog so it cannot overwrite the gyro samples.
Default axes/signs match ordinary shadPS4 mouse gyro: mouse up is +X, right is -Y.
Its roll mode uses Z horizontally; try MotionStickHorizontalAxis=Z if necessary.
Adjust inversion after the intro rotation if a drawing direction is reversed.
Main InvertX/Y affects camera/touch; motion has its own inversion settings.

Primary sources:
- [shadPS4 mouse gyro](https://github.com/shadps4-emu/shadPS4/blob/main/src/input/input_mouse.cpp)
- [shadPS4 orientation integration](https://github.com/shadps4-emu/shadPS4/blob/main/src/input/controller.cpp)
- [SDL DS4 sensor scale](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_ps4.c)

The held-mode acceleration reference (0,8192,0) represents mouse gyro's stationary
(0,9.81,0) m/s^2. It is a mouse-like reference, not a physical rotating gravity
simulation. The mode temporarily replaces external IMU output while held; release
restores the existing source. Exact in-game directions require manual validation.

If SwapTriggersShoulders is active, a shoulder modifier would reserve an R2/L2
source. Analog routing is suspended for shoulder keys in that configuration,
preserving the swap. Use MotionStickKey=RIGHT-STICK (R3) or another separate button.
NONE disables activation; Back/the configured multi key cannot be the analog key.

For normalized stick s, magnitude m, deadzone d and full speed V:

```
u = max(0, (min(m,1)-d)/(1-d))
r = u*u                   # quadratic; linear uses u
velocity = (s/m)*r*V      # zero inside deadzone
raw gyro = round(velocity*16)  # add +1 to pitch for ViGEm calibration
```

Positive stick normalization uses /32767; negative uses /32768. Radial response
prevents diagonal speed boost. Gyro is angular velocity; the receiving application
integrates elapsed time. There is no app-side angle reset on release/activation.
Raw DS4 gyro uses 16 counts per degree/second before host calibration, with rate
rounding to 1/16 deg/s. At V=60 and d=.12, quadratic rounded rates are approximately
0.50 deg/s at 20%, 2.50 at 30%, 35.81 at 80%, and 60 at full deflection.
Try speed=30 for slower control or 90 for faster travel.

Speed is clamped to 0.1..2000 deg/s, deadzone to 0..0.95; non-finite values use
defaults. Axis names ignore case; invalid axes fall back to Y/X. Coincident axes
resolve to distinct axes. Curve defaults to quadratic; other values use linear.

`Tests/run_touchpad_tests.ps1 -Python python` checks math/axes/rates and extracts
actual application mapping/packing/motion blocks for regression. 5,242,880 enabled
analog full-report comparisons pass, covering key reservation, normal camera,
RT, Back priority, swap, neutral, release and no unintended touch. The existing
16,777,216 legacy and 8,388,608 modern-touch comparisons also pass. Physical
inFAMOUS gameplay has not been validated by these synthetic tests. Additional
sequence checks cover plain RB with stick drift, motion activation, center hold
without accidental R1, release/repress and Back + RB shake after analog motion.
