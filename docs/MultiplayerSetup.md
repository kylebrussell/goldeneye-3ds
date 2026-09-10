# Original multiplayer setup input and launch settings

`ge_original_multiplayer_setup` retains the original `front.c` initializers and
input bodies for MP_OPTIONS, MP_CHAR_SELECT, MP_HANDICAP, MP_CONTROL_STYLE,
MP_STAGE_SELECT, MP_SCENARIO_SELECT and MP_TEAMS. The extractor includes each
body's source SHA256 and the original option tables, portrait texture descriptors,
cursor/tab helpers, scenario defaults and ready checks. It does not replace
those decisions with the diagnostic row editor.

## Integration contract

Before allocating live multiplayer players, configure the actual input owner for
the connected participant seats, then call `ge_original_multiplayer_setup_begin`.
Supply the existing mode-selection folder and cursor, two to four connected
seats, and an explicit returning-from-stage flag. The original frontend options
persist across begin/end. Do not clear character, handicap, control or scenario
history to make different menu paths hash alike.

The mandatory services bind a real borrowed wallet model, its clear/set-switch
operations, portrait texture loading, the file owner's native STAGESTATUS value,
and SFX volume/music volume/music play/SFX play operations. A failed resource
operation stops the owner; it supplies no successful no-op production defaults.
The model remains borrowed until `end`. The fixture uses real typed Model storage
and observable resource callbacks; loading and drawing the wallet asset remain
integration work.

Publish one complete primary/secondary raw-pad frame through the existing input
owner, then call `tick(delta)` once. Native menu cursor coordinates use 440×330.
The adapter captures requested VI parameters for presentation without changing
the live gameplay VI owner. Player one controls shared options. Character,
handicap and control pages read each participant's input. Character scrolling
uses the source 84-unit span and 12-unit steps, and all ready images must reach
size 11 before the page closes. These timings are not shortened by the adapter.

`LAUNCH_PENDING` is the actual `frontChangeMenu(MENU_RUN_STAGE,1)` request.
Random arena selection has already consumed the original RNG, including rejected
locked arenas. Later setup ticks return the same pending state without repeating
selection. `take_launch(request_id,&copy)` succeeds once and deep-copies the
resolved arena/configuration, native time/point limits, all four characters,
handicap indices, control styles, team flags, sight and autoaim values.
The caller owns the original framebuffer-dependent reload/switchscreens/loading
boundary. This module does not simulate those screen flags or the four-frame
VI wait. `RETURN_TO_MODE` likewise requires the real mode-selection owner.

After taking the launch, release setup's borrowed wallet binding with `end`.
Create live multiplayer players, configure the scenario/weapon/stage owner, then
call `apply_launch(&copy)` before intro inventory, model binding and initial spawn.
The adapter applies the actual `lv.c` handicap, character perspective, sight and
autoaim assignments to all four permanent rows. License to Kill uses the native
200.0 handicap. It also applies the copied team flags and the exact original
`options.c` control-type setter for each active physical player. Selection is
restored to the entry slot, and RNG, inventory and kill statistics are untouched.

The player owner stores explicit active control selections for subsequent
`init_player_BONDdata` calls; the input roster receives the same values atomically.
Existing diagnostic callers that never apply a setup launch keep their current
dual-analog/Honey defaults. Kind0 schema version 4 fields 337–338 export the real
explicit-selection flag and all four physical control-storage words. These
fields describe future respawn selection; native player control fields and
input-provider state retain their own existing domains. Aggregate coverage stays
incomplete.

## Validation and remaining presentation work

Run `bash scripts/test_multiplayer_setup.sh` for ASan/UBSan native and short-enum
fixtures, with separate two- and four-player peers and byte-identical copied
launch output. Tests drive the native cursor and all seven pages with raw pads,
exercise duplicate character rejection, source ready cadence, arena lock
fallback, team selection, random Start, one-shot handoff, native all-four-row
permissions, exact per-slot control fields and respawn initialization. Malformed
settings and missing resources fail explicitly. The existing player fixture
and `test_mp_state_session.sh` cover diagnostic defaults, lifecycle reset and
typed mutation sensitivity of the new controls owner.

Source constructors, rotated font text, portrait composition, cursor and wallet
presentation still require the follow-up capture/builder integration. A source
input checkpoint is not a completed visible frontend or an actual wireless
lobby. No emulator or hardware validation is claimed here.
