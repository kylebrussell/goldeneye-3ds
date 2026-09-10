# Canonical multiplayer result snapshot

`ge_original_multiplayer_results_snapshot(request_id, end_reason, result)`
produces the existing `GeOriginalMultiplayerFrontendResult` after the root's
canonical match-end decision completes. Root supplies the nonzero start
request ID and the actual time-limit, point-limit, YOLT-elimination or abort
cause; this module does not infer the cause from the final scores. Elimination
is accepted only for YOLT. Failure leaves the destination unchanged.

The snapshot calls `ge_original_multiplayer_score` for each active slot. This
preserves negative scores, the original Golden Gun owner bonus, raw token
counter scores, YOLT's `4 - order_out_in_yolt`, and team scores that subtract
both suicides and friendly kills.

The original source has no `mpCalculateRankings` function. The authoritative
ranking is `write_playerrank_to_buffer` in `mpmenu.c`: it sorts scores descending
and finds the first equal score. The retained result slice preserves that
function's sorting, tie detection and scenario rank mapping; only localized
rank-label writes become numeric returns. A completed match's winner mask
contains every slot whose original displayed rank is first. Top ties therefore
share the mask, and both/all members of a winning team receive a bit. Aborted
frontend sessions publish zero winner bits while retaining their current stats.

Kills and deaths come from the exact loop in `mpCalculateAwards`: kills sum
all off-diagonal entries of the player's kill-count row, including friendly
kills; deaths sum its column, including the diagonal suicide entry. These
counts are independent of scenario points. The native roster count supplies
that loop's original selected-player-count provider. Slots outside the active
roster remain zero in the output.

The wrapper requires a valid 2..4-player scenario roster with distinct nonnull
player pointers and nonnegative kill counts. It bounds the total kill matrix
and applicable Golden Gun bonus to signed 32-bit range before entering the
unchanged original accumulators. This deliberately rejects unrepresentable or
corrupt huge counters rather than overflowing; valid negative scores remain
signed. Invalid YOLT order and negative token counters are rejected.

Cosmetic awards are a separate original lifecycle. `mpCalculateAwards` changes
menu/music/control state, gathers additional accuracy/movement/favorite-weapon
metrics, and consumes RNG to choose awards and resolve award ties. None of
those actions is required to calculate the original score ranking or the
requested snapshot fields, so this module neither executes nor fabricates
awards. Snapshot leaves current player, permanent data, match state and RNG
unchanged.

Generate `ge_original_multiplayer_results_slice.c` using
`scripts/extract_multiplayer_results_slice.py ROOT OUTPUT`; compile it and
`ge_original_multiplayer_results.c` with the existing rules' original-type,
setup, AIPARSE and US ABI flags.

`bash scripts/test_multiplayer_results.sh` passes ASan/UBSan. It compares each
numeric rank with the unmodified original rank-label function and covers normal
2/4-player matches, tied first ranks, all-negative scores, suicides, 2v2/3v1/2v1
teams, friendly-fire penalties, Golden Gun 2/4-player bonuses, TLD and YOLT 2/4,
simultaneous YOLT elimination, LTK, explicit causes and abort. It also verifies
inactive output slots, no state/RNG mutation and rejection without output
mutation for malformed roster/counters/causes. Both runtime adapter and
extracted slice pass focused ARM compilation.
