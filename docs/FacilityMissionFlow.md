# Facility mission-flow audit — 2026-09-10

Facility completion through a recorded controller route remains unverified.
The production ARM build now uses `GE_PORT_FULL_CAMPAIGN_INTERPRETER` for
`chrai.o`. The former 13-case Dam plus 26-case Facility slice is only the
focused host fixture; it is not the current production dependency boundary.

## Authored flow and current bindings

The frontend maps `SP_LEVEL_FACILITY` to `LEVELID_FACILITY` (34), publishes
four Agent objectives and five Secret/00/007 objectives, restarts Facility
after failure, and advances a successful report to Runway. The setup loader
retains `UsetuparkZ`, normal intro pad 167 (`p1682a1`), and the authored graph.
These are source/host-verified boundaries, not evidence of a completed route.

The exit owner is background list `ai_47`, setup ID `0x1006`:

1. Wait while Bond is alive until he enters the room containing decoded pad
   **309 (`0x0135`)** or **135 (`0x0087`)**. The setup macros spell these as
   `0x3501` and `0x8700`; those are serialized macro operands, not native pad
   indices. The second branch sets flags2 bit 1.
2. Set stage flag `0x00800000`, disable damage/pickups, hide the HUD, lock
   controls, pause mission time, fade to black, and wait for fade completion.
3. If an enabled objective is incomplete, jump to global list **15 (`0x000f`)**
   and its `AI_EndLevel`. The macro spells that list operand `0x0f00`.
4. Otherwise hide characters, arm `stop_time_flag`, choose the authored
   cinematic equipment, and issue a tagged camera switch. Pad 309 selects
   camera tag `0x25` and Bond-cinema list `ai_38` (`0x0427`); pad 135 selects
   tag `0x24` and `ai_37` (`0x0426`).
5. `ai_37` opens tagged doors `0x22/0x23`, teleports to decoded pad 134,
   runs the authored animations/timer, and fades out. `ai_38` teleports to
   decoded pad 310, runs its authored animation/timer and held-item removal,
   and fades out. Both issue `AI_EndLevel` and then the global dead list.

Current runtime owners are present in source:

| Boundary | Current binding | Evidence limit |
| --- | --- | --- |
| Objective flags, inventory, key state, enter/deposit/photo events | `initialize_stage_objectives` binds the generic registry and exact providers | Facility fixture has 15/17 enabled criteria at Agent/higher difficulties; providers are controlled |
| Authored guards, equipment and doors | Generic guard and interactive materializers; objective tags refer to native live definitions | Live construction/pickup/interaction during a full Facility route remains unverified |
| Background AI | `initialize_stage_active_props` composes props, then `ge_original_stage_mission_runtime_begin` allocates authored background actors; canonical `chrpropTick` is their scheduler | No parallel Facility AI tick is needed; host flow fixture manually schedules its actors |
| Bond cinematic body | `initialize_stage_guard_objects` binds player-body construction and held-item attachment; POSEND invokes `solo_char_load` | Body creation and full animation progress in an actual Facility exit need a capture |
| Tagged camera | Native camera resolver/conversion described below | Both real `ai_47` branches now publish authored camera definitions in the host fixture |
| HUD/control/time/fade | Full canonical command bodies and shared mission-exit services | Host command ordering/fade fixtures; device presentation remains unverified |
| Title/results/save | Shared `AI_EndLevel` service, live objective evaluation, mission-result adapter and CRC save provider | Campaign fixtures verify Facility at all four difficulties; no complete controller route |

## Repaired native camera boundary

The previous production `AI_CameraSwitch` obtained a native sidecar
`TagObjectRecord`, then called `tagGetCommandIndex`, which searches the
serialized `g_CurrentSetup.propDefs` array by address. A sidecar is never a
member of that array. The camera was skipped. The existing Facility fixture
returned null tags, so it could pass without testing camera activation.

Camera records also still contained authored integer position/angle words;
they had not received the original `prop.c` `PROPDEF_CAMERAPOS` conversion.
Facility's camera records are setup commands **519/521**, referenced backward
by tags **0x24/0x25** at commands **520/522**. Their positions require signed
integer division by `100.0f`; theta/verta require signed division by
`65536.0f`. For example, tag `0x24` must begin at
`(7969.43, -311.01, -5450.34)`, with pad 135.

The objective runtime now owns native `CutsceneRecord` storage and resolves
camera tags through the already-validated authored command relation. It
preserves original duplicate-tag precedence, keeps raw setup words intact,
and releases the native storage at teardown. Camera tags remain distinct
from health-bearing gameplay objects. Teardown clears `gBondViewCutscene`
only when it points into the closing runtime, preserving a foreign owner's
camera; missing or truncated camera records are rejected before conversion.
The port's `AI_CameraSwitch` uses this
boundary and preserves the original camera/flag/mode publication order; the
N64 preprocessor branch remains unchanged.

The positive Facility fixture fails against pre-repair `chrai.c` because no
camera is published, then passes with this repair. Across the campaign, **30
tagged authored camera commands** resolve and convert, with non-camera,
missing-tag, restart and unbound lookups checked under ASan/UBSan.

## Executable checks

```sh
# Requires the private generated build/u/src/game/chraidata.o for global AI.
# Passing a stage pack additionally enables both positive native-camera cases.
bash scripts/test_facility_mission_flow.sh /path/to/stages.gepack

# All 80 mission/difficulty objective→terminal-service→result→save fixtures.
bash scripts/test_campaign_acceptance.sh /path/to/stages.gepack

# Shared fade/death/title, exact result, save/unlock and source-contract tests.
bash scripts/test_dam_mission_exit.sh
```

The Facility fixture executes actual `UsetuparkZ` lists and unchanged focused
interpreter cases, resolves the extracted 18-list global AI table, sustains
`0x1006` across yields, runs both `ai_37/ai_38` branches, and checks the
incomplete-objective global exit. The positive camera cases bind the actual
serialized Facility setup and the real native camera owner. Door services,
body/model animation completion, objective completion and fade completion
remain controlled fixtures. A passing fixture is not a controller route.
Without a pack, the script explicitly reports that positive camera cases
were not run. `test_stage_assets.sh` supplies its generated pack to both
campaign and Facility fixtures.

## Remaining route and diagnostic work

No other live Facility completion failure is established by these host runs.
The camera defect was a demonstrated activation failure; fixing it does not
prove that all earlier mission events can be reached. The next authorized
emulator run must record the actual initial materializer/actor/objective
statuses, normal interactions, scientist/contact and 006 AI, bottling-tank
destruction, exit-room entry, cinematic-body construction, objective results,
Facility save mutation, and the successful Runway handoff. Secret/00 routes
must additionally reach the double agent and decoder through canonical
inventory/action code. Abort/death/restart need separate recorded attempts.

An arbitrary route to either exit pad would at best test an incomplete exit.
No controller route candidate is provided here: the available authored room
and exit fixtures do not specify a tested path through the scientist/decoder,
006 and tank objectives. Calling that path a successful route would hide the
remaining gameplay work. The coordinator owns emulator execution and capture
collection; these host scripts do not launch Azahar or modify its files.

Also, existing `native_dam_mission_exit_ai_offset` diagnostic fields query
AI ID `0x1004`. Facility's actual exit owner is `0x1006`; do not interpret the
Dam-named offset as Facility exit progress. A Facility route capture needs
an explicit `0x1006` actor observation or equivalent ordered AI evidence.
See [the campaign acceptance matrix](CampaignAcceptance.md).
