# Dam controller route triage

This is a source audit of the existing **160-target, 55,000-frame**
`GE_INPUT_PROBE 7` candidate, not evidence that a mission completed.
The integration coordinator owns all emulator runs and their private inputs.
No route inputs or gameplay state were changed by this audit.

The reviewed `dam-end-to-end.cfg` SHA-256 is
`dfd2f2e32ebc9ceabe08ee99ec4cd413eca7f7c37e21732d1750b4a086c82720`.
Its adjacent `dam-end-to-end-authored.json` SHA-256 is
`e36fd572dd73a62c2b7f10a00dc6aa308832c56084e621e3f595ca08a4359e73`.
The JSON's 86 views are geometry provenance; they are not the controller
route's target count or its measured outcome. Recheck hashes before applying
this guide to another candidate.

## First recorded 00 Agent result

Integration's `dam00-correctness` run against pinned source`952af929` ended
with death at target13/160: 5,895 displayed frames, 5,896 simulation ticks.
Both difficulty fields were2 and the final actor status was
`1,0,0,1,1,0,0,0`, with5,896 actor ticks. There were55 PP7 shots,55 background
STAN resolutions and7 damaging guard hits. Chr6/7 were dead and unlinked;
chr4/5 remained alive with zero accumulated damage. The last registered hit
was still on chr7. No objective bits were set. This establishes an opening
combat failure, not a mission completion or a universal bullet-path failure.

The run exposed a concrete diagnostic aiming mismatch: probes select the
canonical HONEY1.1 controls, but `input_probe_apply_live_aim` sent yaw through
`look_x`. Under1.1, `ge_port_n64_buttons` maps that channel to C-button strafe;
the analog `move_x` channel owns yaw. This can move Bond sideways during a
guard dwell while leaving his facing near the preceding navigation direction.
The correction belongs in the probe's controller translation. Normal gameplay,
guard hit tests and objective producers retain their canonical behavior.

The final camera was already in the death sequence; its upward direction
cannot diagnose live shooting pitch. Pre-death checkpoints are the relevant
camera evidence. The run also reported NDSP initialization failure
`d880a7fa`; sound dispatch/decode counts do not establish audible output.

## Target map

Indices are zero-based. `route_targets=k,160` means target `k` remains
unreached; `target_trace` records completed indices and simulation frames.

| Targets | Intended action |
|---|---|
| 0–13 | Opening chr6/7 and road chr5/4. Target 3 aims at chr6 for at most 300 ticks; 10 aims at chr7 for 900; 12/13 allow 300 each. |
| 14–26 | Tunnel combat and armour. Target 19 approaches full Armour prop318/pad113 at XZ `(16795,12519)`, radius60. |
| 27–34 | Gates267/268. USE at29/32; 180-tick waits at30/33. |
| 35–48 | Approach guards40/46/12/13/10/11. |
| 49–53 | Modem tag5/prop290 near `(14617,1746)`. NEXT at50; 90-tick wait51; FIRE150 ticks with pitch command−0.65 at52; settle240 at53. |
| 54–60 | Terminal props262/264, tags6/7. USE55/58; 720-tick countdown wait60. |
| 61–66 | Alarm tag3/prop316; FIRE65. |
| 67–102 | Tower1 alarm tag0/prop310; FIRE101. |
| 103–124 | Tower2 alarm tag1/prop312; FIRE123. |
| 125–144 | Tower3 alarm tag2/prop314; FIRE143. |
| 145–159 | Return to exit pad330 near `(-4.28,-3034.58)` by157; wait420 at158; one fresh FIRE at159. |

Each alarm dwell allows 180 ticks with pulse period45, followed by45 settle
ticks. Guard dwells can finish early after canonical death-complete, but their
300/900-tick timeouts also advance when a guard remains alive.

## Read the actual failure first

Inspect `player_combat` (dead is field5), `actor_status`, `route_targets`,
`target_trace`, final position/room and `movement_blocker` before interpreting
the verifier's first message. Death or an early exit with `k<160` yields a
failed route; the verifier then short-circuits its objective and exit checks.

- PP7 shots with STAN-hit plus clear-path counts both zero isolate the
  player-to-muzzle/STAN boundary.
- Shots reaching background resolution with `guard_hit_test` field0 zero
  isolate guard hit-list population. Inspect `combat_aim` and guard hit
  gate/room/matrix evidence.
- Nonzero guard hits followed by death may instead reflect insufficient
  00 Agent combat cadence, ammunition or cover. A dwell timeout does not
  establish that its guard was killed.

Before the moving-button repair, `input_probe_sample` discarded a moving
target's held buttons whenever `aim_chr<0`. This affects twelve ordinary
walking FIRE targets in this candidate. Scripted guard dwell shots still
execute. Results from a pinned executable with that behavior must preserve
that interpretation; a later helper repair does not change an earlier run.

## Gates, modem and objectives

At a gate stall around27 or31, compare the movement blocker with `gate`,
`gate_route` and `door_interaction`. Target27 is150 units past gate267's
center before the first USE29, with radius100. Reaching it depends on actual
gate state; the authored waypoint graph does not prove passability. Both
gates must record controller activation, a generation change, full-open
maximum fraction and portal−1. `gate_both_open_frames` must remain zero.

After53, expect modem flag`0x100` plus a successful modem throw. A throw alone
does not prove attachment: ai20 requires item`0x2f` attached to tag5.
Flag`0x2000` means a stationary misplaced modem; `0x800|0x200` indicate the
destroyed-link/backup failure path. NEXT50 assumes PP7→modem inventory order
despite earlier pickups. The current result has no per-target selected-item
or ammunition trace.

After60, expect backup flag`0x400`. Ai21 accepts activation of either terminal
tag6 or7 after the modem flag; both USE actions are not required by the AI.
Before modem attachment it consumes the activation and emits the warning
without beginning the countdown.

Alarm dwells do not resolve object aim. They retain navigation yaw, and
`look_y=0` does not reset residual pitch. If an alarm survives, establish
weapon, ammunition and aim before diagnosing a bullet-runtime failure.
Final flags must include`0x1500`, and objective menus0–3 must all have value1
and blocker0. Menu0 additionally requires all four exact alarm tags destroyed.

## Authored exit and persistence

Ai24/`0x1004` detects the **room** of pad330, not its XZ radius. It sets
`0x1000`, locks controls/time, enables authored damage suppression, and moves
Bond's world Z by4 per tick. The Y threshold or a timer greater than350 leads
to a60-tick black fade, objective evaluation, armed button exit, three yields,
and POSEND camera tag`0x0a` with cinema ai17/`0x412`. Missing objectives instead
select failure list15. In `mission_exit`, fade ticks, POSEND requests and title
requests (fields6/7/8) should be positive.

Every dwell rechecks its XZ radius. Forced exit movement may leave target158's
radius135 before its420-tick wait ends, preventing target159's sole fresh FIRE
even after the exit is armed. If flags`0x1500` and POSEND are present but the
route remains at158/159 with no title request, compare final XZ against
`(-4.28,-3034.58)` and inspect control locks/camera. The wait also assumes room
detection occurred no later than reaching the pad target. These are route
hypotheses to test against the actual run, not reasons to bypass exit logic.
The expected zero cheat-invincibility field remains compatible with authored
exit damage suppression, which uses a separate global.

`mission_result` contains title requests, briefing frontiers and briefing
commits. Values≥1,0,≥1 prove result API dispatch. Void persistence callbacks
cannot report a disk-write failure through that commit counter. Preserve the
private96-byte save and inspect a retained copy:

```sh
bash scripts/inspect_campaign_save.sh /path/to/retained/goldeneye.sav 0
python3 scripts/verify_dam_end_to_end_result.py /path/to/retained/result
```

A successful Dam00 save records the best Dam00 time (ticks/60, capped to the
original10-bit representation; faster existing time is preserved), marks
lower difficulties complete and makes Facility00 accessible. No Dam cheat is
expected from ordinary00 completion. Use an isolated known starting save to
attribute progress to this run. A host reload does not establish a physical
console reboot test.

## Evidence limits and future route changes

Current results omit per-target weapon/ammo/pitch, prop-specific shot and
activation health, ai21 countdown/offset, actor failure history, and per-target
Y/room. Steering traces stop after128 samples rather than retaining a ring.
The result also needs its externally retained complete input/config hash.
Final healthy actor status alone cannot prove uninterrupted actor health.
Sound counters prove dispatch/decode, not listening; the existing verifier's
average≤50ms gate does not establish60Hz performance.

Preserve the current pinned run before changing its executable or inputs.
Any subsequent remediation should alter ordinary controller navigation,
aiming, firing, weapon selection or timing based on the observed frontier.
Objective flags, object health, player position and terminal AI conditions
must continue to be produced by canonical gameplay.
