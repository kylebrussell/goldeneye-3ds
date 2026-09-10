# Native multiplayer BG render semantics

The shared runtime now executes the CPU portion of original BG prop rendering
after `propsTickPlayer` and before `gunUpdateAndFireBothHands`. Global effects
advance before the all-character pre-tick, pickup regeneration and shuffle.
The prior frame's final shuffled player remains selected at that boundary.

`extract_prop_render_semantics_slice.py` retains source from `chr.c`,
`propobj.c`, `objecthandler.c` and `model.c`. The existing native effect runtime
provides `bg.c` room-depth order and `chrpropsRenderPass` prop order:

- Signed-depth ascending room passes 0 and 2, then descending alpha pass 1.
- Original fog/fade gates and opaque/alpha model flags.
- Sorted `ModelHitEntry` traversal, node-root pruning, model relations, and
  character onscreen statistics; original alpha-pass hit-list retirement.
- `dogfnegx` scale RNG, and angle RNG only when the authored image exists.
- `dorottex` three RNG calls per authored quad in its original alpha gate.
- Monitor command/controller updates, including conditional random jumps,
  using the existing relocated canonical command and image owners.
- Original held/right/left/hat and object/child impact-bit retirement.

GPU commands, colour/scissor setup, temporary display vertices and N64
float-to-fixed output are presentation boundaries. Native float matrix banks
remain available to the platform renderer. Monitor display-list and temporary
vertex pointers are not written into canonical RW data by this CPU traversal.

The flash cache receives physical model/node identities and already sampled
values. Renderer collection applies its own visibility and uses a private RNG
for supplemental widened-view flashes. It never draws gameplay RNG. Existing
solo collection remains unchanged; multiplayer cannot invoke the former
allocation-order sampler through `prepare_muzzle_flashes`.

`scripts/test_multiplayer_render_semantics.sh` builds the fresh production host
graph using the 3DS extraction recipe, records every compile source/command,
audits strong native owners in the linked executable, and runs ASan/UBSan.
The fixture uses actual 2/4-player storage, fog, RNG, model traversal and
hit-list retirement. Fixture models deliberately put sorted order ahead of
allocation order and include an excluded nested flash, an untextured flash,
and two primary-node quads. These are resource fixtures, not a completed local
match acceptance transcript. The separate body-provider fixture checks
native-node publication, repeated collection and local-renderer independence
with actual canonical body/held-item model ownership.
