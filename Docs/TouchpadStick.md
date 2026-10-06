# Xbox touchpad stick controls

Back/View and the right stick control the first DS4 touch. The new settings are opt-in:
without `TouchpadStickMode`, the original mapping remains selected. The left stick's
second touch, Share swap, PS/motion chords, normal buttons, stick suppression and
the earlier LT/RT fix are retained. Plain Back click now has a separate drift
threshold; deliberate swipe click suppression and its existing overrides remain.

Merge these keys into the existing `[Xbox]` section of `Config.ini`, then restart.
They are not keyboard/mouse profile settings.

```ini
[Xbox]
TouchpadStickMode=relative
TouchpadStickSensitivity=0.60
TouchpadStickDeadzone=0.12
TouchpadStickCurve=quadratic
TouchpadStickSmoothing=0
```

This is a starting preset for drawing that consumes DS4 touch coordinates. The
recommendation comes from synthetic controls tests, not an in-game capture.

**inFAMOUS Second Son distinction:** its stencil/graffiti nozzle is aimed by tilting
the controller, as described in [Prima's opening-mission walkthrough](https://primagames.com/news/infamous-second-son-how-beat-mission-1-delsin-rowe).
That is motion input. These settings change touch coordinates only; they leave the
existing gyro/motion mapping intact. They are not a verified fix for native
inFAMOUS graffiti aiming or an emulator's motion path. If your setup explicitly
routes touch coordinates to drawing, the preset applies; otherwise the motion
mapping needs separate testing. Select the virtual DS4 in the game/emulator.

| Setting | Default | Meaning |
| --- | --- | --- |
| TouchpadClickDeadzone | 0.12 | Separate radial threshold for suppressing touchpad click; 0..0.95; zero restores original exact-center check; applies to all modes |
| TouchpadStickMode | legacy | `legacy`, `absolute`, `relative`; unknown values use legacy |
| TouchpadStickSensitivity | 0.60 | Absolute span multiplier or relative speed multiplier; clamped to 0.05..3.0 |
| TouchpadStickDeadzone | 0.12 | Radial normalized deadzone; clamped to 0..0.95 |
| TouchpadStickCurve | linear | `linear` or `quadratic`; unknown values use linear |
| TouchpadStickSmoothing | 0 | Absolute position filter time constant in seconds, 0..1; ignored by relative |

Mode/curve names ignore ASCII case. Non-finite numeric values use defaults.
Stick sensitivity/deadzone/curve/smoothing are ignored by legacy mode. The
separate click deadzone applies to legacy too and does not filter coordinates. Existing
`DeadZoneRightStickX/Y` filtering runs first; the benchmark leaves those at zero.
`InvertX/Y` continues to affect the right stick in both new modes.

## Back click troubleshooting: small right-stick drift

The original condition cancelled touchpad click unless right-stick report bytes
were exactly `127/129`. A measured Xbox sample `(-73,-1238)` is only about 3.8%
radial deflection, yet cancels a plain Back press under that condition. This fork
uses `TouchpadClickDeadzone=0.12` (12%) independently of position mapping. Above
that threshold, swipe clicks are suppressed as before; the existing
`TouchPadPressedWhenSwiping=1` and right-stick-click override remain available.
Set the click deadzone to zero for the exact original check.

Real-source tests cover all three modes, inversion, swapped shoulders, unchanged
touch coordinates/tracking/history, triggers, deliberate swipes and Back release.
The new binary still requires gameplay confirmation for an individual setup.
For physical/virtual controller conflicts, use the [HidHide guide](HidHide.md).

## Swipe troubleshooting: unintended second finger

Back + right stick moves the first touch; Back + left stick controls the second.
With `DeadZoneLeftStickX/Y=0`, a slightly drifting left stick can therefore keep
two touches down even when only the right stick is being used. A live diagnostic
captured changing first-touch coordinates alongside a persistent second touch
from approximately 6% left-stick drift. Replaying 897 recorded Back samples
through the actual report mapping with 8% left-axis deadzones removed the second
finger while retaining the first touch's coordinates/tracking and gyro values.

For a similar controller, merge these existing settings into `[Xbox]` and restart:

```ini
DeadZoneLeftStickX=8
DeadZoneLeftStickY=8
```

These values are percentages (8 means 8%), unlike normalized `MotionStickDeadzone`
and `TouchpadStickDeadzone`. Choose values appropriate to the controller's measured
drift; they also filter normal left-stick controls. The right stick's touchpad and
gyro settings can remain unchanged. This is a configuration adjustment, not a new
touchpad mapping or an in-game validation. If swipes still fail with a single
moving touch, check the receiving emulator/game's touch input separately.

## Behavior and mathematics

Legacy converts the stick to 8-bit report axes first, computes their shared radial
distance from 128, then selects constants using each axis's sign. For rightward X
it uses `200 + trunc(1519*m)`, for leftward X `1719 - trunc(1519*m)`; Y uses
`100 + trunc(741*m)` or `743 - trunc(741*m)`. At the exact neutral report values
127/129 the first touch is `(960,471)`. Small deflections can jump away from that
center, and changing one axis affects both touch coordinates. With the benchmark
settings, a 1% rightward command jumps X by -749 integer coordinate units.

The new modes use the signed 16-bit stick values. Normalize positive values by
32767 and negative values by 32768; invert Y so positive touch Y points down.
For normalized vector `s`, `m=length(s)`, deadzone `d`, sensitivity `S`:

```
u = max(0, (min(m,1)-d)/(1-d))
response = u                 # linear
response = u*u               # quadratic
v = (s/m)*response           # v=(0,0) when m<=d
```

This rescales the deadzone continuously and prevents diagonal speed boost.

Absolute is a position control:

```
targetX = clamp(959.5 + 959.5*S*v.x, 0, 1919)
targetY = clamp(470.5 + 470.5*S*v.y, 0, 941)
alpha = 1-exp(-dt/smoothing)  # alpha=1 when smoothing=0
position += alpha*(target-position)
```

Neutral returns the touch to center, gradually when smoothing is enabled. `S=0.60`
restricts each axis span to its central 60%; the reachable shape is an ellipse.
`S=1` reaches the midpoint of each edge; reaching corners requires higher gain.
The user-requested comparison preset is absolute/linear, S=0.60, d=0.12, smoothing=0.20.
For a quick absolute swipe, a smaller smoothing time or zero smoothing responds faster.

Relative is a velocity control:

```
position += v * 1920*S*dt
```

Activation starts a new touch at `(959.5,470.5)`, transmitted as `(960,471)`;
the activation frame does not move it. Neutral holds the last coordinate with no
inertia. Releasing Back/View sends finger-up; repressing starts a new centered
contact. Bounds are X=0..1919, Y=0..941. Explicit contact tracking keeps `(0,0)` down.
Fractional coordinates accumulate internally, then round to integer report values.

The application uses a monotonic clock for elapsed time. Integration/filter `dt`
is capped at 50 ms to prevent a large jump after a stall; polling below 20 Hz will
therefore slow relative movement intentionally. Tests cover 30/60/250/1000 Hz.
The existing special chords can consume Back/View and end the contact, just as
they did before. Existing touchpad-click behavior is also preserved: Back at
neutral may assert the click; right-stick-click / `TouchPadPressedWhenSwiping`
continues to govern clicking during movement.

## Synthetic comparison

Tests use identical 16-bit-quantized input timelines: small circle, slow diagonal,
fast upward swipe, zig-zag, graffiti-like curves, and small target adjustments.
Position commands and velocity commands intentionally produce different paths.
Do not interpret path differences as an intrinsic accuracy ranking.

All comparison presets have S=0.60 and d=0.12. Absolute uses linear response and a
0.20-second filter; relative variants have no filter. The legacy reference ignores
the new settings, as required for compatibility. Units below are touch coordinates,
not game pixels; relative X changes are distance over one second.

| Measure | Absolute linear | Relative linear | Relative quadratic |
| --- | ---: | ---: | ---: |
| 20% right input, X change after 1s | 51.98 | 104.71 | 9.52 |
| 30% right input, X change after 1s | 116.96 | 235.63 | 48.20 |
| 13% right input, first 60-Hz frame X change | 0.522 | 0.218 | 0.00247 |
| Full reversal, first 60-Hz frame X step | 92.06 | 19.20 | 19.20 |
| Seeded neutral jitter RMS | 0 | 0 | 0 |
| Active noise RMS deviation from clean trace | 1.53 | 2.58 | 1.13 |
| Velocity-target task maximum overshoot | 57.53 | 234.54 | 0.25 |
| Velocity-target task final error | 50.35 | 234.54 | 0.25 |

RMS noise: seed 20261005; two seconds, 60 Hz. Neutral has sigma=0.025 per axis.
Active noise has sigma=0.015 around `(0.25,0)`, compared to the clean mode's own
trace after a 0.5s settling period. Legacy has 707.69 neutral and 134.72 active RMS.

The target experiment commands 30% right for 1.25 seconds, then neutral for 0.5
seconds, aiming at center+60 units. This is deliberately a velocity task. Absolute
would normally use a different deflection to select that target; its overshoot
number is not a universal accuracy result. Returning to neutral explains its
final error. A 100-ms late release at 30% costs approximately 23.56 units with
relative linear, 4.82 with quadratic.

The smallest report coordinate change is one unit in either new mode. Relative
retains smaller increments until they cross a rounding boundary. Legacy typically
steps about 12 X / 6 Y units per 8-bit axis increment away from sign transitions,
with much larger discontinuities near neutral. Absolute step speed depends on
the input trajectory/filter; a full reversal here is about 5524 units/s for its
first 60-Hz frame. Relative's maximum scalar speed is 1152 units/s at S=0.60;
80% gives 687.83 and a full vertical traverse takes 0.817 seconds at full deflection.

| Relative quadratic sensitivity | 20% speed | 80% speed | Full vertical traverse |
| --- | ---: | ---: | ---: |
| 0.40 | 6.35 units/s | 458.55 units/s | 1.225s |
| 0.60 | 9.52 units/s | 687.83 units/s | 0.817s |
| 0.80 | 12.69 units/s | 917.11 units/s | 0.613s |

For long smooth strokes, small direction corrections, and tight corners, start
with relative/quadratic/0.60/0.12. Small deflections move slowly, large ones still
swipe quickly, and neutral stops without pulling the finger back to center.
Use 0.40 if fine control matters more than travel time, 0.80 if it feels too slow.
Absolute is useful when you want stick position to select a location or perform
a quick centered swipe. Legacy remains available without altering old configs.

## Reproduce the checks and GIFs

On Windows with Visual Studio C++ tools and Python with Pillow/numpy:

```powershell
./Tests/run_touchpad_tests.ps1 -Python python -Render
```

Output defaults to `output/touchpad/`: `absolute_mode.gif`, `relative_mode.gif`,
`absolute_vs_relative.gif`, `linear_vs_quadratic.gif`, `legacy_vs_absolute.gif`,
trace CSVs, `metrics.json`, `metrics.md`, and test logs. Each GIF has all six
patterns, identical stick commands, contact/trail/coordinates, settings and a
stick inset. A center detail inset makes small movements visible.

The new-mode simulator executes `Source/XboxTouchpad.h` directly. The real-source
report regression compares to trigger-fix commit
`10d934e10d16ec892e82263cc0103f94a7bea7de` (available in this branch's history).
It verifies the trigger block and stick-suppression block are unchanged, then
checks 16,777,216 legacy full reports and 8,388,608 modern button/stick/motion
reports. It also covers held LT/RT, tracking IDs, `(0,0)`, release/repress, and inversion.

Release x64 was built with VS2022/v143 and SDK 10.0.22621.0. Local build properties
override the old project toolset/SDK. The existing bundled LTCG library requires
a compatible toolset or a local ViGEmClient rebuild; those dependency binaries and
machine-specific build overrides are not committed. Hardware/gameplay verification
remains a separate manual step.
