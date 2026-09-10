# Explicit multiplayer input and timer boundary

`ge_port_input_sample_pads(input, pads)` samples one outgoing controller pair.
It uses the same `ge_port_n64_buttons` action mapping as the existing solo tick
and `ge_original_input_sample_pads` for configured stick calibration. Fire uses
primary Z; use/reload uses B; inventory uses A; previous weapon uses A+Z; dual
analog aim uses secondary Z. Single-stick C-button synthesis is unchanged.
Sampling does not consume input. The caller retains pending logical press edges
until the corresponding outgoing frame is committed.

`ge_port_advance_multiplayer_tick(state, raw_pairs, count)` accepts a complete
two/four-player input frame (three is also valid). It validates canonical roster
pointers, provider identity, configured input count, and selected slot before
publishing all already-quantized pads once. The original common port tick then
dispatches one retrace through `updateFrameCounters` and the original level
timer/boss path. It does not accumulate elapsed wall time, reset a player/input
owner, rescale remote sticks or add a random draw. `simulation_ticks` advances
once; a second accepted call is the next simulation tick, even if held input is
identical. Protocol duplicate suppression belongs before this API.

Select the same canonical input slot on every node before this once-per-world
boundary. The common idle-button provider reads that slot; selecting a different
local display player could otherwise change idle/stage accounting. Subsequent
all-player simulation uses the original shuffled order.

Explicit level pause, malformed/count/selection/roster mismatch, active RAMROM
input, and exhausted tick counter reject without changing the input ring or
port/timer/RNG state. Controls-locked frames still consume and dispatch. The
original `lv.c` controls-lock branch sets gameplay clock/delta to zero while
the retrace and simulation dispatch counters advance; the adapter preserves
that distinction. Transport-paused or uncommitted frames must not call the API.

`ge_original_input_configured_player_count` and
`ge_original_input_selected_player` are read-only configuration queries.
`get_nth_player_from_shuffled` is now retained by the existing canonical guard
scheduler support extractor, next to `shuffle_player_ids` and its inverse.

`scripts/test_multiplayer_port_tick.sh` runs two/four-player ASan/UBSan fixtures
across 64 ticks (multiple wraps of the original 20-sample ring), checking exact
button edges, both pads, raw +/-80 while local calibration is +/-60, one input
consume and timer update per call, controls-lock dispatch, and transactional
rejection. The original stage subsystem callbacks are observed boundaries with
no world implementation in this fixture. Movement, full-roster props, gun,
death, rules, hashes and transport acceptance remain owned by the main runtime;
this test is not a simulated authored match.
