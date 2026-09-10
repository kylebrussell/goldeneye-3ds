# New Nintendo 3DS controls and touchscreen

The normal mission view now uses the bottom screen for a touch control panel.
Input probes, visual tours and RAMROM recordings bypass the panel. Original
menus retain their previous analog shaping, with D-pad navigation added.

The default game layout remains the original two-controller 2.2 Galore style:
Circle Pad walks/strafes and C-Stick turns/looks. R or ZR fires, L or ZL aims,
B uses, Y reloads, A selects the next weapon, X selects the previous weapon,
and Start opens/closes the original watch. Previous Weapon uses the canonical
inventory-plus-fire chord (N64 A + rising Z); the original body suppresses
firing during that chord. Release Fire before tapping Previous if Fire is
already held. The former D-pad Down mapping did not cycle in 2.2.

Human gameplay now presents ±80 virtual stick travel. The former ±60 maximum
could move the crosshair but never crossed the original manual-aim camera-pan
threshold (>60). Scripted probes and frontend input keep their ±60 calibration;
RAMROM replay samples retain their authored raw bytes. Hold Aim and push the
C-Stick to its edge for sustained camera aiming. The original
aim-plus-Circle-Pad-down/up crouch/stand and scoped zoom gestures still apply.
With Remote Mines equipped, A + B (or A + Y) invokes the original detonation
chord once per rising edge; the canonical body suppresses use and weapon cycling
for this gesture. D-pad movement is available when
its corresponding Circle Pad axis is neutral.

The touch panel provides large Use, Reload, Previous Weapon, Next Weapon,
Watch and Hold Aim areas. These feed the same logical input path as physical
buttons; they do not directly change inventory, objectives or simulation state.
In particular, **Use and Reload both invoke GoldenEye's context-sensitive N64
B action**. Reload can therefore activate a nearby object, just as Y/B can.
Watch opens the original interface containing inventory, mission objectives
and settings. Hold Aim lasts while the stylus remains inside its captured button.

The top row has separate **Controls**, **Objectives**, and **Inventory** tabs. The
objectives page shows original authored objective names, filtered for the
selected difficulty, with the difficulty name and live Incomplete/Complete/Failed status. A blocked
status provider is shown as Unavailable; it cannot falsely mark an objective
complete. Five objectives fit each page; Previous/Next Page and Watch occupy
the bottom row. Objective text wraps to two lines and shows an ellipsis when
truncated. The original watch retains the full text.

This read-only page does not pause the mission or change objective state. It
polls original evaluation providers at most four times per second while visible,
refreshes immediately when opened, and redraws only when content changes.
Objective snapshots use fixed-size copied storage without per-frame allocation.
No status-change HUD notifications are consumed by this additional reader.

The Inventory page copies the original watch's authored names and ordering.
Five large rows fit a page; keys and unavailable actions remain visible with
selection disabled. Touch and release on an available row to equip it. Moving
out of the captured row cancels the selection. Inventory refresh pauses during
a held stylus, and the selection boundary rechecks player, inventory ownership,
row identity, weapon mapping and gameplay state before applying it. Removal,
pickup reordering, stage restart, death, watch/cinematic transitions, active
firing/reloading/throwing and locked controls can reject the old gesture.

Selection follows the original watch's right-weapon/left-unarmed behavior.
Loaded ammo is returned through the original un-equip wrapper; gadgets preserve
the prior weapons for canonical cycling. Dual pairs remain available through
weapon cycling. The lower screen never grants items or calls a synthetic
inventory mutation. See [the tested inventory boundary](CanonicalInventory.md).

Touching another button during a drag cancels the original button. Lift and
tap again to activate a different button. Held buttons and touch carried into a mission cannot activate game actions
until released. `ge_3ds_controls_resume` exposes the same release gate for the
platform suspend/resume hook, preserving settings and the selected page. Simultaneous physical
and touch presses of the same action produce one rising edge. Swapping
shoulders while holding one suppresses it until released, preventing an aim
hold from becoming a shot.

The bottom row changes pitch inversion, look sensitivity (0.25x steps within
0.25x–2x), and shoulder assignment. Each gesture changes a setting once.
The controls page redraws only when entering a mission, resuming, or changing
a setting; information pages also redraw when their cached content changes. Settings
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
`scripts/tests/test_3ds_control_gestures.py` executes the actual 2.x control
branch after physical/touch mapping and the original joy sample ring. It
reproduces the old unreachable aim-pan threshold, checks the full-range repair,
crouch/stand, scoped zoom, previous/next/fire suppression, remote-mine rising
edges, touch-held aim and control locks. This is input-dispatch evidence; it
does not replace playing those gestures on a console.

Inventory UI tests cover five-row paging, disabled rows, release selection,
drag cancellation, snapshot freezing/replacement, redraw gating, and restart/
resume release handling. Physical-console ergonomics, touchscreen readability,
C-Stick comfort and saved-settings reboot acceptance remain to be validated.
