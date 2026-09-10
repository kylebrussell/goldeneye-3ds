# Canonical inventory selection boundary — 2026-09-10

`ge_original_inventory_snapshot` and `ge_original_inventory_select` provide
the touchscreen with the original watch's inventory rows and a checked
selection operation. The public interface is
[`ge_original_inventory.h`](../port/include/ge_original_inventory.h).

The snapshot copies up to 64 rows with original watch indexes, mapped weapon
IDs, authored short names, selection flags and opaque identity values. It
uses `bondinvCountTotalItemsInInv`, `bondinvGetItemByIndex`,
`bondinvGetTextbyInvIndex` and original text override precedence. Consequently
hidden mission props stay hidden, weapon props require the original `0x400`
runtime flag, and all-guns cheat rows follow the original virtual indexes.
Inventory props without an equip mapping remain read-only. Dual pairs are
absent from the original watch list and remain in the original weapon cycle.
The count cap is explicit through `total_count`; snapshots allocate no memory
and retain no dereferenceable borrowed pointers.

Selection rechecks the current canonical player and its input-provider owner,
inventory storage, mission epoch, original row index, item and prop identity,
weapon mapping and text ID. The lower-screen caller must keep the displayed
snapshot fixed from touch press to release and call
`ge_original_inventory_invalidate` at mission initialization/cleanup. This
rejects a captured touch across a restart even if player/storage addresses
are reused. A removed, hidden, reordered or remapped row cannot silently
select the new occupant of that row. Rejections make no gameplay changes.

Direct selection is allowed only in single-player gameplay with unlocked
controls, enabled player actions, living Bond, normal camera mode and idle
hands. It blocks watch transitions, forced disarm, stop/game-over states,
tank mode, firing, reloading, throwing and pending weapon animations. This
prevents a touch from interrupting active gun state or bypassing disarm. A
snapshot taken while blocked cannot become actionable just because the state
later changes; a new press on refreshed information is required.

From that idle state, the mutation order follows
`options.c:sub_GAME_7F0A8378`: call the unchanged
`currentPlayerUnEquipWeaponWrapper` for the selected right-hand weapon,
call it for left-hand unarmed, then assign the original equipped watch index.
The wrapper owns loaded-ammo return, held-rocket cleanup and previous-weapon
bookkeeping. Selecting an already single-equipped item returns `NO_CHANGE`
without unloading it again. A touch selection does not pause gameplay, play
the watch animation or consume its sound/selection timers.

## Executed verification

Run `bash scripts/test_original_inventory.sh`; it is also part of
`scripts/test_port.sh`. The ASan/UBSan test extracts the current canonical
inventory, gun and original watch selection function bodies directly from
source, then compiles them with the live boundary. It verifies:

- Byte-identical complete canonical player state after selecting Camera from
  loaded dual pistols through the new boundary versus the original watch.
  Both magazines return to reserve ammo, the held rocket is released and
  advancing or backstepping out of Camera requests the previous dual pair.
- Read-only snapshots, canonical short-name/override precedence, bounded
  copied text, virtual all-guns rows, hidden props, weapon-prop flags, no
  selectable row for a mission item without an equip mapping, and 64-row
  truncation with the full count preserved.
- Rejection of missing/out-of-range rows, negative/changed indexes, changed
  prop identity, removed/hidden/remapped items, expired mission epochs,
  mismatched player/provider ownership and multiplayer state.
- Every nonzero watch transition, every active canonical hand action state,
  pending animations, death, cinematic camera, control locks, disabled
  actions, disarm, stopped/game-over state and tank mode. Rejected selections
  leave the canonical player byte-identical and perform no model work.
- Null and malformed inventory ring transitions fail closed before the
  original unbounded walkers run. No claim is made that arbitrary corrupt
  pointers to prop objects can be made safe by this boundary.

The boundary also compiles for ARM with devkitPro `arm-none-eabi-gcc`,
`-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft` and `-Werror`.
The native Homebrew compiler lacked newlib headers; the successful ARM check
used the project's existing devkitPro Docker image and a read-only source
mount.

The regression uses fixture-owned player/prop/weapon-stat/language data.
Canonical gun model loads, sound playback and object deallocation end at
observed test sinks. It establishes selection and inventory semantics, not
complete model rendering, all campaign gadget usability, physical C-Stick or
touch ergonomics, or console performance. Those remain acceptance checks
through the integrated game and physical hardware.
