# shadPS4: Back/View and touchpad input

## Observed workaround with both controllers visible

On 2026-10-06, the user reported that the inFAMOUS Second Son touchpad interaction
worked after keeping the physical Xbox controller visible as a second controller
and assigning **Touchpad Center** to its **Back/View** button in shadPS4.
The user clarified that **swipes already reach the game without that binding**;
the interaction requiring **Back alone** does not work until the physical Xbox
Back/View is assigned to Touchpad Center. HidHide was disabled.
This is a working configuration reported for that setup,
not a confirmed fix for all shadPS4 versions or a requirement to enable two players.

The observed arrangement was:

- Virtual DS4 in controller slot 0, with motion input enabled.
- Physical Xbox controller in slot 1, available to the input-mapping screen.
- Touchpad Center mapped to Xbox Back/View in the active shadPS4 input profile.
- DS4Emulator using legacy touchpad movement and `SwapShareTouchPad=0`.

To reproduce the arrangement, start DS4Emulator before shadPS4, leave the physical
Xbox visible, and select it when capturing the Back/View binding for Touchpad
Center. Save to the profile actually used by the game. Check the controller order;
reconnecting devices can change it. Test Back alone, Back + right-stick swipe,
release of Back, and simultaneous RT separately.

With unified input configuration enabled, the local `input_config/default.ini`
contained this controller binding:

```ini
touchpad_center = back
```

That line does not explicitly encode a controller slot. The second-controller
description records the observed device arrangement and UI selection; it does
not establish that the saved binding applies exclusively to slot 1. A per-game
configuration can use a different profile.

The exact reason this combination resolves the interaction has not been isolated.
The log confirmed two connected controllers, and the active profile confirmed
the binding, but neither establishes how the game combines their inputs. Avoid
describing this as a proven shadPS4 bug or a universally required workaround.

## Why Back cannot be captured from the virtual DS4

With DS4Emulator's default `SwapShareTouchPad=0`, plain Xbox Back/View becomes the
native DS4 **touchpad click**; Back + Start becomes **Share**. The virtual device
therefore does not emit an ordinary Xbox Back button for that press. See the
[DS4 report mapping](../Source/DS4Emulator.cpp).

The code initializes touch to `(960,471)`, then applies the stick mapping and marks
the finger down. Exact neutral retains the center, but legacy mapping can move
the contact far from center even under small drift. With the previously measured
right stick `(-73,-1238)` and the current non-inverted, zero right-stick-deadzone
settings, the formula produces `(960,135)`. The 12% **click** deadzone preserves
the click but intentionally does not center the **touch coordinate**. These values
come from evaluating the source formula, not a capture of the failing game frame.

The mapper's fixed center coordinate could therefore matter if the interaction
depends on contact location. This is a concrete hypothesis, not a verified game
requirement or explanation of how the second controller contributes. A deliberate
swipe can also suppress click while continuing to send touch coordinates. Report
generation does not confirm that shadPS4 or the game uses the plain Back click.

In the inspected custom shadPS4 build, the native DS4 touchpad button is handled
directly and returns before normal rebinding. Native touch coordinates have a
separate event path. This explains why pressing physical Back while selecting
the virtual DS4 does not supply a normal Back binding in that mapper.
[Source: native event handling](https://github.com/rayhlancaner-png/shadPS4-Infamous-FixBuild/blob/0555e9806ee7f1a6e09846108ede510d34cbb05d/src/sdl_window.cpp#L389-L429).

The mapper's **Touchpad Center** output generates a center touch and a touchpad
button state through its binding-output path. This differs from the virtual DS4's
native touch/button event paths. Different routing, timing or interaction between
controllers are possible explanations, but none has been isolated. In particular,
the mapping code alone does not prove that the DS4 click is delivered to the game.
[Source: center output](https://github.com/rayhlancaner-png/shadPS4-Infamous-FixBuild/blob/0555e9806ee7f1a6e09846108ede510d34cbb05d/src/input/input_handler.cpp#L699-L711).

Swapping Share and touchpad changes the existing Back/Back + Start scheme; it is
not necessary just to record the physical Xbox Back binding. The trigger fix
continues to allow LT/RT during DS4Emulator's modifier.

## Scope and alternative

The inspected shadPS4 source was the custom inFAMOUS build at
`0555e9806ee7f1a6e09846108ede510d34cbb05d`, not every upstream build.
The DS4Emulator binary was Xbox motion preview 3. No controller-report code was
changed to document this workaround.

[HidHide](HidHide.md) is an optional alternative for device isolation. It was
turned off in this working arrangement; enabling it would remove the physical
Xbox input used by this workaround.
