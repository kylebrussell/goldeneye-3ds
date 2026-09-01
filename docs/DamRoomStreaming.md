# Dam room streaming frontier

The original global-visibility interpreter now exposes every
`VISOP_PRELOAD_ROOM` request through `GeDamPreloadQueue`. The ten-room component
loaded at startup is registered as the initial resident set; it remains a cache
budget and is not treated as visibility policy. The live camera/visibility path
uses this provider and reports the oldest exact pending room on the 3DS status
screen.

`GeDamDynamicScene` now performs the missing CPU-side installation. It reloads
the authored blobs for the resident IDs plus the exact queue head, rebuilds into
private vertex/batch storage, and exposes prepare/commit phases. A failed asset,
capacity, or display-list check preserves the previous pointers and generation;
only commit marks the queue room resident. It does not choose eviction order.

The live 3DS camera path now publishes those transactions. It loads candidate
textures into separate slots and projects into a temporary bounded vertex
buffer. Only a successful projection commits CPU residency, replaces the GPU
vertices and render batches, and transfers the candidate texture slots. An
allocation, asset, capacity, texture, or projection failure aborts without
overwriting the last valid frame; the exact room request remains retryable when
publication itself fails.

The same transaction now owns a persistent decomp-object overlay. Authentic
model 62, 104, and 178 Fast3D output is stored with overlay-local indices; each
room rebuild appends it, rebases its batches after the room vertices, and
publishes both together. Replacing the overlay is itself atomic: invalid ranges,
capacity failure, allocation failure, or a failed authored-room rebuild leaves
the previous scene and overlay intact. This keeps active props present as
visibility preloads add rooms without making the room cache responsible for
object placement or lifecycle.

The renderer consumes the generation published by the original player-state
adapter. Each new original spawn or movement publication reruns
`bondviewUpdateCameraMatrices`, portal visibility, pending room installation,
and projection before the frame is drawn. Static frames do not rebuild the
scene, and the adapter does not derive camera movement from 3DS input itself.

Room retirement now uses the original `bgRoomsTickUnload` lifetime rather than
a platform-authored cache heuristic. `GeDamDynamicScene.room_age` mirrors
`model_bin_loaded`: a rendered resident room resets to 1, while an unrendered
room advances 1 to 2 to 3 to 4 and is deleted on the following visibility tick.
`ge_dam_dynamic_scene_tick_visibility` accepts the exact rendered-room set for
that tick. Nonresident visible rooms are ignored because their original preload
requests remain owned by `GeDamPreloadQueue`.

The GPU render loop uses the split form of that operation.
`ge_dam_dynamic_scene_age_visibility` only updates those exact room ages and
pending-delete flags: it never allocates, rebuilds a scene, changes residency,
advances the published generation, or changes the published vertex/batch
pointers. The loop can therefore age after visibility is known and let its
existing GPU-validated `ge_dam_dynamic_scene_prepare_next` transaction include
all due deletions with the next pending preload. `tick_visibility` remains the
CPU-only convenience path and delegates its aging step to the same function.

Deletion is a normal scene transaction. The candidate records the complete
surviving room order and exact rooms being dropped; the old vertices, batches,
residency bits, ages, and queue states remain published until commit. If a room
preload is queued on the delete tick, the candidate includes it and publishes
the install plus all due deletions atomically. A missing asset or failed scene
build leaves the previous resident scene intact and leaves the aged deletion
pending for a later tick. Queue resident/unloaded states change only with the
same successful commit.

`GeDamPreloadQueue` preserves the original request cadence: a resident room
returns zero, while the first missing room is queued and returns nonzero, which
stops further preload attempts in that interpreter pass. The render loop can pop
one request, attempt installation, and complete it as resident or failed. Queue
capacity, duplicate requests, overflow, and failure retries are all explicit and
bounded. `ge_dam_preload_queue_evict_resident` validates an entire resident set
before changing any state, so a partial queue eviction cannot diverge from the
scene transaction. Door behavior remains outside this adapter.
