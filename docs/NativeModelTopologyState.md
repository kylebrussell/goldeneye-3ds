# Native Model topology state (ED)

`ge_mp_state_model_topology` owns typed physical `ModelNode` and
`ModelFileHeader` records under kind `0xED`. It is the topology dependency for
the separate Model RW owner (`0xEE`). EC continues to own Model instances.
The aggregate schema remains incomplete: node RO data, RW allocations,
skeletons, switch/texture content and animation caches require other owners.

## Source boundary

`modelGetNodeRwData` selects a node-specific RW index, walks `Parent`, and
redirects through a HEAD ancestor's RW buffer. The graph used for this lookup
is mutable. Original `modelInitRwData`, LOD/switch relation functions and
`modelApplyReorderRelationsByArg` change `Child`, `Next` and `Prev`.
`modelAttachPart` and `modelApplyHeadRelations` attach the head root and update
its `Parent`; the former also changes the body header's `numRecords`.
Saving RW values without owning these relationships would miss future lookup
and traversal behavior.

Pitem instances share cached resource headers/nodes. Character resources have
cached graphs and each instantiated body/head pair owns separate copied
headers/nodes. Those graphs must have distinct physical identities. The four
embedded player-gait nodes and their header are static shared objects.
ED enumerates physical containers, including hidden/detached nodes. It does
not derive population or identity from traversal/render order.

## Lifetime and registration

After E3 begins, call `ge_mp_state_model_topology_begin` on a zeroed owner.
`GeMpModelTopologyDefinition` supplies an exact native pointer, exact
`ge_mp_state_model_topology_native_size(component)`, creation namespace,
physical slot and component (`NODE=1`, `HEADER=2`). Descriptors must be key
sorted with unique addresses. Native `sizeof` adapts to pointer width and
regional header layout; N64/ARM nodes are 0x18 bytes, non-debug US headers
are 0x20. No serialized N64 allocation may be passed as a host-native object.
The bounded capacity is 2048 physical records; registry and aggregate limits
also apply. Allocate the large owner/snapshot outside the 3DS stack.

Namespaces/slots come from an agreed containing-owner/content manifest, not
pointer bits, first-seen traversal or exporter registration order. Suggested
identity inputs are the canonical Pitem/character model ID and authored node
offset, the physical character instance slot plus body/head part, or the
four explicit gait-node roles. ED does not invent a numeric namespace policy.

`ge_mp_state_model_topology_add` registers newly loaded stage-lived resources
while preserving all existing IDs/generations. Its input is sorted, contains
only new keys/addresses, and cannot alias owner storage. It preflights native
lifetime, every current ID, all descriptors and registry capacity before
mutating anything. Key insertion between earlier records is supported; the
owner remains sorted. A successful append observes new native storage; it
does not manufacture a native allocation. Integration must register every
newly loaded/copy-constructed topology before producing an aggregate snapshot.

End ED after the final consumer/snapshot and before freeing any containing
provider, player/gait storage, native arena or replacing the registry. Cached
Pitem topology survives instance release; character copied topology survives
until provider destruction. The captured E3 pool identity and actual stage
epoch protect against a reset/rebound pool. They cannot detect an early free
of separately borrowed topology within the same stage: keeping that storage
readable until ED ends is the caller's explicit obligation.

End validates the full count, registry shape, descriptor component/extent and
every exact ED/namespace/slot ID before release. Malformed owners leave owner
and registry unchanged. Native lifetime expiry still allows legitimate ID
retirement and reports the failure. A stale generation cannot release its
replacement. Keep generation history within the match; session registry
ownership remains the caller's policy.

`actor(owner,native,&id)` resolves either physical component.
`check(owner,native,component)` verifies the stage lease, exact known object
and live ID before a later reader dereferences it. This is provenance and
extent validation, not a proof that arbitrary links are acyclic or that a
node's external RO data is readable. The RW reader must bound its own parent
walk and validate the actual RO-data extent/index and buffer span.

## Typed map

All optional sequences have zero count for the other component. No native
padding, pointer bytes or struct bytes are serialized.

| Field | Type | Native source |
|---|---|---|
| 256 | U8 | descriptor component |
| 257 | U16, node only | full `Opcode`, including upper flag bits |
| 258 | nullable actor refs | node/header order below |
| 259 | I16 × 4, header only | `numSwitches`, `numMatrices`, `numRecords`, `numtextures` |
| 260 | F32, header only | `BoundingVolumeRadius` |
| 261 | I32, non-EU header only | `isLoaded` |
| 262 | U8 | descriptor namespace |
| 263 | U16 | descriptor physical slot |

Node references are `Data, Parent, Next, Prev, Child`. Header references are
`RootNode, Skeleton, Switches, Textures`, plus `SwitchNames` when compiling the
native DEBUG header. Node links and roots resolve against ED first; nonnull
external references require a typed provider and a live registered ID.
Unknown/stale/zero resolutions return `INCOMPLETE`, zero output records and
an `unresolved_kinds` bitmask. The aggregate still requires referent records.

Header counts and `isLoaded` retain their declared signed values. Radius must
be finite and retains finite binary32 bits, including negative zero. Export
does not repair graph links, normalize flags, traverse hidden nodes, or invoke
relation functions.

## Read-only native views

`ge_original_model_topology.h` exposes `GeOriginalModelTopologyView`: native
header and extent, full node-array base/count/stride, authored node offsets,
RO-data base/stride and canonical model ID. These are borrowed resources;
none of the getters loads assets, changes status, ticks animation or uses RNG.

- `ge_original_pitem_model_resource_topology(provider,model_id,&view)` returns
  only an already cached resource, shared by its instances.
- `ge_original_character_model_resource_topology` returns the cached graph.
- `ge_original_character_model_instance_topology(provider,slot,part,&view)`
  returns copied body (`part=0`) or attached head (`part=1`) storage. Inactive
  or absent parts fail. Source offsets still refer to the original resource.
- `ge_original_player_gait_topology_header`, `*_node_count` and `*_node`
  expose the exact static header and four authored nodes. They do not pretend
  that the separately declared nodes form a native array.

## Remaining dependency work

EE must own native RW buffers with their actual allocation/free generations,
the node-specific variants and HEAD buffer/header references. Pitem release
frees RW storage even while ED's resource nodes remain alive, so ED's stage
lease cannot be reused as an RW allocation lease.

RO data also needs typed ownership: `RwDataIndex` is assigned by model setup,
and RO node links, origins and other fields can be changed by native consumers.
Giving `ModelNode.Data` only an immutable source-content ID would not account
for those mutations. Switch tables, skeletons, texture tables and their
contents need corresponding typed/verified identities. Matrices, frame caches
and callback roles remain the explicit EC/provider dependencies. ED alone
does not attest complete model or animation state.

## Validation and build

`bash scripts/test_mp_state_model_topology.sh [staged-model-pack]` reads the
existing multiplayer-bodies `stages.gepack` by default; shared assets are not
rewritten. Both enum ABIs run under ASan/UBSan. Fixtures retain the actual
model index/init/relation bodies, front switch setters and head attachment.
Two/four models exercise LOD changes, switch toggles, stable BSP sibling
order, header record-count mutation and native HEAD RW-base redirection.
Actual packaged constructors verify shared Pitem, copied character body/head,
cached character and static gait physical views. Additional tests cover
insertion with preserved IDs, transactional failures, stale references,
unknown mappings, non-finite radius, malformed teardown and native expiry.
This is a bounded topology/lookup test, not world/combat acceptance or a
complete aggregate-state fixture.

`ge_mp_state_model_topology.c` uses the same native type/setup/region/enum
flags as EC and links E3/registry state. Its getters use their existing
Pitem/character/gait-data source owners. No new production extractor, main
hook or gameplay implementation is introduced; integration owns descriptor
collection, schema/provider registration and lifetime ordering.
