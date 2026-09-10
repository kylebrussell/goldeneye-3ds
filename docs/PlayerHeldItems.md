# Canonical player held-item pass

`ge_original_player_held_items_tick()` runs the original right-then-left
`chraiCheckUseHeldItems` pass. For every selected simulation player, call it
after `propsTick`, `chraiUpdateOnscreenPropCount`, and
`chrpropUpdateAutoaimTarget`, before interaction/reload and `propsTickPlayer`.
`gunUpdateAndFireBothHands` occurs later in `maybe_mp_interface`. This order
intentionally consumes the hand firing state published by the preceding gun
update; local display rendering must not invoke another held-item pass.

Before MoveBond, `ge_original_player_apply_sight_option_exact()` performs the
unchanged viewport source call: solo reads its persistent options table;
multiplayer reads the selected canonical `g_playerPerm->sight`. The caller
must bind the same selected player, camera, ordered onscreen props, and model
hit lists used by the active-prop pass. The code creates no player, prop, hit
list, or background state of its own.

The build adds `port/src/ge_original_player_held_items.c` and the output of
`scripts/extract_player_held_items_slice.py REPO OUTPUT`. Existing owners supply
the native visibility room table/count, background geometry/STAN queries,
canonical bullet path, character/object hit and damage dispatch, item stats,
camera, objective photograph service, tank-shell/projectile service, remote
mine state, and effects. Three tiny accessor bodies are locally namespaced to
avoid duplicating existing retained native symbols. The source uses the same
`ge_bg_visibility_room_info` and `ge_bg_visibility_max_rooms` as full props;
retaining the query does not itself populate native room geometry.

At match initialization, after gameplay services reset, enable
`ge_original_gameplay_services_set_exact_held_item_dispatch(1)`. This suppresses
the old PP7-only and modem dispatches in the once-per-frame audio service.
Disable it at teardown. The independent exact gun-dispatch flag retains its
existing solo meaning. Audio mixing continues while held-item ownership is
enabled.

The following native boundaries are explicit:

- `ge_original_player_held_items_bind_lights(context, callback)` supplies the
  native owner of the original `lightFixtureBreak` operation. Its triangle
  pointer is local process data, never a network identifier. Missing or failed
  light mutation records `GE_ORIGINAL_PLAYER_HELD_ITEMS_LIGHT_FRONTIER`.
- The source's STAN-only impact path sets `besttexture=-1`, then reads bytes
  before `g_Textures`. Those unrelated N64 memory bytes have no valid native
  texture-table equivalent. The retained body reports
  `GE_ORIGINAL_PLAYER_HELD_ITEMS_STAN_TEXTURE_FRONTIER` and returns before that
  invalid read. No default texture category is invented.
- Any non-OK result requires the runtime to abort the current match. Earlier
  shot mutations may already have occurred; this API does not roll them back.
  A status is not evidence that every room's geometry or light mutation is
  implemented. The runtime must establish those providers before activation.

The extractor otherwise preserves the original expressions and branch order.
It removes a matching-only read/increment/decrement of an uninitialized float,
initializes three flag-correlated temporary values for native GCC analysis,
and rewrites reverse loops to avoid forming a pointer before the array while
preserving order. The original big-endian texture byte's low nibble is read
through its named `hitTexture` bitfield on native builds. Source hashes and a
mechanical source audit make these adaptations reviewable.

`bash scripts/test_player_held_items.sh` uses ASan/UBSan and the existing
primitive-model combat fixture. For every distinct attacker/target pair in
two- and four-player rosters it runs the original randomized screen-center ray,
held-item dispatcher, model BBOX intersection, hit registration, damage,
armor, death, and score bodies. It also checks real melee extents/LOS/damage,
selected-slot remote-mine ownership, magnet state, sight options, no-fire RNG
stability, and native frontier results. Empty-room/STAN fixtures and peripheral
presentation/projectile/photo sinks are explicit; this is combat acceptance,
not a complete world or UDS match. A separate test calls the actual gameplay
audio service and verifies that ownership suppresses duplicate fire while its
PCM ring continues advancing.
