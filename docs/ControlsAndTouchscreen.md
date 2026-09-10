# New Nintendo 3DS controls and touchscreen

The normal mission view now uses the bottom screen for a touch control panel.
Input probes, visual tours and RAMROM recordings bypass the panel. Original
menus retain their previous analog shaping, with D-pad navigation added.

The default game layout remains the original two-controller 2.2 Galore style:
Circle Pad walks/strafes and C-Stick turns/looks. R or ZR fires, L or ZL aims,
B uses, Y reloads, A selects the next weapon, X selects the previous weapon,
and Start opens/closes the original watch. The original aim-plus-Circle-Pad-down
crouch and scoped zoom gestures still apply. D-pad movement is available when
its corresponding Circle Pad axis is neutral.

The touch panel provides large Use, Reload, Previous Weapon, Next Weapon,
Watch and Hold Aim areas. These feed the same logical input path as physical
buttons; they do not directly change inventory, objectives or simulation state.
In particular, **Use and Reload both invoke GoldenEye's context-sensitive N64
B action**. Reload can therefore activate a nearby object, just as Y/B can.
Watch opens the original interface containing inventory, mission objectives
and settings. Hold Aim lasts while the stylus remains inside its captured button.

Tap the top row to switch between Controls and **Mission Objectives**. The
objectives page shows original authored objective names, filtered for the
selected difficulty, with live Incomplete/Complete/Failed status. A blocked
status provider is shown as Unavailable; it cannot falsely mark an objective
complete. Five objectives fit each page; Previous/Next Page and Watch occupy
the bottom row. Objective text wraps to two lines and shows an ellipsis when
truncated. The original watch retains the full text.

This read-only page does not pause the mission or change objective state. It
polls original evaluation providers at most four times per second while visible,
refreshes immediately when opened, and redraws only when content changes.
Objective snapshots use fixed-size copied storage without per-frame allocation.
No status-change HUD notifications are consumed by this additional reader.

Touching another button during a drag cancels the original button. Lift and
tap again to activate a different button. A held touch carried from a menu into
a mission cannot activate a game button until released. Simultaneous physical
and touch presses of the same action produce one rising edge. Swapping
shoulders while holding one suppresses it until released, preventing an aim
hold from becoming a shot.

The bottom row changes pitch inversion, look sensitivity (0.25x steps within
0.25x–2x), and shoulder assignment. Each gesture changes a setting once.
The panel redraws only when entering a mission or changing a setting. Settings
save on mission cleanup / normal application exit, outside the render loop.
Abrupt power loss before cleanup can lose changes. A failed save leaves the
previous config intact and reports the failure on the console.

Settings live in `sdmc:/3ds/goldeneye-3ds/controls.cfg`:

```text
dual_analog 1
invert_look_y 0
swap_shoulders 0
move_deadzone 0.12
look_deadzone 0.12
look_sensitivity 1.0
```

Set `dual_analog 0` to retain the original single-stick 1.1 Honey style. Deadzones
accept values from 0 to 0.45. Gameplay axes smoothly rescale the range outside
the deadzone to full travel, avoiding the prior jump at the threshold. Values
are clamped to the canonical input range. Inversion/sensitivity affect gameplay
look only; frontend cursor controls remain independent. Unknown config keys
are ignored; malformed known values reject the entire file and preserve the
currently loaded settings (defaults at startup).

Verification: `scripts/test_3ds_controls.sh` compiles with AddressSanitizer and
UndefinedBehaviorSanitizer. It tests analog monotonicity/saturation/deadzone
continuity, frontend independence, D-pad fallback, all action regions and their
boundaries, drag cancellation, physical/touch edge merging, shoulder-change
release handling, settings limits, legacy config compatibility, invalid config
rejection, save failure preservation, roundtrip persistence, objective page navigation and bounds, status caching,
refresh throttling, text normalization/truncation, and redraw gating.
`scripts/tests/test_3ds_touch_objectives.py` additionally executes the actual
main-loop snapshot helper with original-provider fixtures to verify difficulty
filtering, status mapping, unavailable data and menu bounds.
Physical-console ergonomics and touchscreen usability remain to be validated.
