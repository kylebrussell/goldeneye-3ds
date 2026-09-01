# Facility mission-flow audit

Facility's stage, briefing, and result handoff are already data-exact. The
frontend maps `SP_LEVEL_FACILITY` to `LEVELID_FACILITY` (34), publishes the
five authored `LARK` objectives, restarts the same mission after failure,
and advances a successful report to Runway. The common stage loader resolves
the exact `UsetuparkZ` setup, normal intro pad 167 (`p1682a1`), STAN tile
`0x69201`, and room 13. The host frontend, mission-result, save-provider, and
all-stage asset suites cover those boundaries.

The authored successful exit is owned by Facility background list `ai_47`,
setup AI ID `0x1006`; it must not be replaced by a platform completion check.
Its relevant exact order is:

1. Wait for Bond to enter the room containing pad `0x3501` or `0x8700`, while
   retaining the distinct branch flag used to choose the cutscene.
2. Set objective bitfield `0x00800000`, disable damage and pickups, hide the
   HUD, lock controls, pause mission time, fade to black, and wait for the
   canonical fade completion state.
3. If any enabled objective is incomplete, fall through to global AI list
   `0x0f00` (`GAILIST_END_LEVEL`), whose unchanged result owner records the
   failed mission. If every objective is complete, branch over that jump,
   hide characters, arm `stop_time_flag` through
   `trigger_fade_and_exit_level_on_button_press`, select the authored
   weapon/camera branch, and run cutscene list `ai_37` (`0x0426`) or `ai_38`
   (`0x0427`).
4. Both cutscene lists wait for their authored animation/timer, fade to black,
   wait for fade completion, issue `exit_level`, then go dead. The unchanged
   `AI_EndLevel` body owns `bossReturnTitleStage`, which applies the mission
   result only when the objective runtime reports complete.

The interpreter compile boundary is now closed without widening to the full
command graph. `platform/3ds/Makefile` keeps the bounded
`GE_PORT_DAM_MISSION_FLOW_SLICE` base and adds
`GE_PORT_FACILITY_MISSION_FLOW_SLICE` only for `chrai.o`. Under that expansion,
`ailistFindById` routes global IDs through the extracted exact
`ge_original_global_ai_find` table, and the following exact canonical bodies
reached by `ai_47`, `ai_37`, and `ai_38` are retained:

- list/animation/wait: `AI_SetChrAiList`, `AI_PlayAnimation`,
  `AI_IFImOnPatrolOrStopped`, `AI_MyTimerStart`, and
  `AI_IFMyTimerGreaterThanTicks`;
- exit trigger/state: `AI_IFBondInRoomWithPad`, `AI_IFBondIsDead`,
  `AI_IFObjectiveAllCompleted`, `AI_EndLevel`, `AI_SetMyFlags2`,
  `AI_UnsetMyFlags2`, and `AI_IFMyFlags2Has`;
- player presentation/control: `AI_IFBondHasItemEquipped`,
  `AI_BondEquipItemCinema`, `AI_BondHideWeapons`,
  `AI_BondDisableDamageAndPickups`, `AI_BondDisableControl`,
  `AI_TriggerFadeAndExitLevelOnButtonPress`, `AI_ScreenFadeToBlack`,
  `AI_ScreenFadeFromBlack`, and `AI_IFScreenFadeCompleted`;
- cutscene scene services: `AI_CameraSwitch`,
  `AI_TRYTeleportingChrToPad`, `AI_HideAllChrs`, `AI_DoorOpenInstant`, and
  `AI_ChrRemoveItemInHand`.

That is exactly a 26-case expansion plus global-list resolution. `AI_GotoFirst`,
`AI_Label`, `AI_Yield`, and `AI_SetObjectiveBitfield` are already retained.
The extracted `ge_original_global_ai_find` table already contains the exact 18
global lists, so the slice can use that table for global IDs without taking on
a second owner or guessed bytecode.

`scripts/test_facility_mission_flow.sh` compiles the real `UsetuparkZ` lists,
the focused unchanged interpreter cases, and the extracted 18-list global AI
owner under ASan/UBSan. It sustains real `0x1006` across multiple yields, takes
both authored room branches, runs `ai_37` and `ai_38` through their
teleport/animation/timer/fade/end sequence, and verifies the incomplete-objective
fallthrough through global list 15 and unchanged `AI_EndLevel`. The test keeps
absent camera/door tags as explicit service frontiers; the canonical cases
retain their normal null-tag behavior rather than inventing test objects.

The remaining decomp-first activation sequence is therefore:

1. Bind the exact camera-switch tag/cutscene definitions, all-character hide,
   HUD/control/timer lock, equipment selection/hide, fade, and objective-all-
   complete services already named by those bodies.
2. Publish Facility's tagged doors/camera commands through the generic stage
   materializer and prove the real active-prop scheduler ticks the `0x1006`
   background actor and the assigned Bond-cinema list once per simulation
   frame without a parallel owner.
3. Connect the resulting `bossReturnTitleStage` call to the same generic
   title/report/save boundary used by Dam, then validate both failure and
   successful-report transitions.
4. Only then validate Facility completion in Azahar. Until that
   proof exists, Facility is an authored navigable scene with a loaded mission
   graph, not a completed playable mission.

`scripts/tests/test_facility_mission_exit_contract.py` pins the exact source
ordering and the focused linked-interpreter boundary.
