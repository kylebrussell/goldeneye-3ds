# Deterministic match protocol and simulation contract

`ge_multiplayer_protocol` supplies a portable, fixed-membership, two-to-four
player lockstep protocol. It is independently tested transport infrastructure.
It does not make the current single-player runtime into a multiplayer game,
construct characters, hash the game's state, or produce compatibility manifests.

## Required integration

1. Agree on the ordered UDS node roster, host in slot 0, and a fresh nonzero
   session identifier before initializing the protocol. Each device owns one
   player slot; both original N64 pad samples in `GeMpInput` belong to that slot.
   Use transport-reported source node IDs when calling `receive`.
2. Load the same complete canonical setup/rules and initialize the simulation
   and its PRNG identically. Supply `GeMpConfig` with SHA256 values for the actual
   build/simulation manifest, **all** packed and sidecar content, complete rules,
   and initialized canonical simulation state, plus its 64-bit PRNG seed.
   The protocol compares all 128 digest bytes, seed, tick rate, and ordered
   roster. It rejects zero digests, but cannot know whether callers hashed the
   full inputs. Existing lobby build/asset hints do not fulfill this contract.
3. Call `ge_mp_protocol_init(protocol, config, local_slot, send, context, now)`.
   `send` must copy each packet before returning: 1 queued, 0 temporarily full,
   -1 fatal. It must not reenter the protocol. Use monotonic milliseconds.
4. Poll at least once per frontend/game iteration and feed received datagrams
   through `ge_mp_protocol_receive`. The caller owns transport draining limits.
   Do not run the old lobby receiver over the same queue during matches.
5. Sample and submit both pads once for `protocol.frame.tick` via `submit_input`
   when state is START or RUNNING. Hold that exact input throughout retries.
   `take_frame` returns one immutable input bundle exactly once when execution
   is permitted. Execute one complete canonical 60 Hz simulation step, in stable
   player/actor order, and call `finish_tick` with its canonical SHA256.
6. Include every future-affecting field and full PRNG state in the state hash:
   players, inventory, health/armor, character bodies/generation, props/pickups,
   projectiles, timers, AI, scenario scores and respawn queues. Explicitly encode
   values in stable order; never hash native structs, pointers, padding, GPU or
   audio presentation state, system clocks, or uninitialized bytes. Rendering
   must select the local camera without changing the shared simulation state.
7. Treat ABORTED as terminal. Stop simulation, release the session at a safe
   frontend boundary, and show `failure`. Call `disconnect` promptly on UDS
   peer/host loss; joining replacements and host migration are unsupported.

The caller is responsible for applying the seed to the real canonical PRNG,
including saving/restoring any presentation-only random streams. A seed field
on a packet cannot prove determinism or remove shared single-player globals.

## Authority, reliability, and bounds

The host controls membership, start, input delivery and tick advancement. Every
participant runs the simulation; there is no authoritative state correction.
The window holds **one tick** (32 bytes of input at four players). It makes no
prediction and never repeats a missing player's previous input.

The host retransmits PREPARE until all peers acknowledge compatible config,
COMMIT until all acknowledge committed initialization, then START until all
acknowledge start. Peers may submit tick 0 input during START, but nobody receives
an executable frame until the host has all START acknowledgments.

For each numbered tick, the host gathers all inputs, transmits the complete
FRAME, and waits for every delivery ACK before EXECUTE. Each simulation runs
once and returns a state hash. All hashes must equal the host's before ADVANCE;
clients acknowledge ADVANCE, and the host waits for every ACK before collecting
the next tick. The previous ADVANCE/hash remains available for replay recovery.
Inputs submitted one tick ahead while the host finishes that barrier are retried.
Same-tick conflicting input/hash or any state disagreement freezes the match.

Packets retry every 40 ms, or immediately after genuine progress. No progress
for 3 seconds aborts; duplicate/replayed traffic does not extend that deadline.
Clock reversal, transport errors, explicit membership loss and tick exhaustion
also abort. Ticks never wrap into a new match. A terminal abort is retransmitted
while the caller continues polling. This is bounded stall-and-abort recovery,
not rollback or state resynchronization. The sequential barriers cost multiple
round trips per tick. They establish a correctness baseline, **not a measured
60 fps radio solution**; input pipelining would require a separately verified
protocol version and measured radio timing.

A partition can leave one participant one executed tick behind another. No node
can advance beyond the missing participant's hash barrier. Start or simulation
execution cannot be made simultaneous over a lossy channel; the contract is
agreed inputs and monotonic bounded progress, not synchronized wall-clock time.

## Wire and identity

All fields are explicitly big-endian. No C structs or pointers cross the wire.
Header: `GEMP` magic (4), version (1), type (1), sender slot (1), player count (1),
session (4), tick (4). CRC32/IEEE (4) follows the payload. Packet sizes are exact:
PREPARE 166 bytes, INPUT 28, FRAME/HASH/ADVANCE 52, ABORT 21, all ACK/control 20.
PREPARE payload: build/content/rules/initial-state SHA256 (32 bytes each), seed
(8), tick rate (2), four ordered node IDs (2 each; unused IDs zero). FRAME always
has four eight-byte dual-pad inputs; unused player inputs must be zero. Each
pad contains buttons (2), signed stick X (1), signed stick Y (1).

Kinds in order starting at 1: PREPARE, PREPARE_ACK, COMMIT, COMMIT_ACK, START,
START_ACK, INPUT, FRAME, FRAME_ACK, EXECUTE, HASH, ADVANCE, ADVANCE_ACK, ABORT.
Protocol version is 1. CRC detects accidental corruption; neither CRC nor these
unkeyed digests authenticates a participant. The caller must avoid session ID
reuse so datagrams from a previous match cannot belong to the new match.

`ge_mp_actor_id(kind, owner, index, generation)` provides the stable registry ID
format: kind:8, owner:8, index:16, generation:32. Kind and generation are nonzero;
owner is a player slot 0–3 or 255 for world-owned actors. The simulation assigns
indices in deterministic order or from authored setup indices, increments
generation on reuse, and never wraps/reuses an ID. No actor serialization is
implemented: all nodes deterministically construct matching registries.

## Verification and remaining acceptance

Run `sh scripts/test_multiplayer_protocol.sh`. ASan/UBSan tests cover full config
byte mismatches, exact wire bytes and endian order, every one-byte corruption,
short/oversized packets, source/session validation, invalid IDs, start and frame
barrier loss, duplicate input submission, state disagreement, timeout, explicit
host/peer loss, clock reversal, transport failure and one-shot tick consumption.
Forty-eight seeded sessions (24 each at two and four players) execute 48 ticks
with deterministic drop, duplication, reorder, corruption and queue-full
injection. Independent expected transcripts must agree on every participant.
The test's state accumulator and 32-byte checksum are explicitly a transport
model, not canonical GoldenEye gameplay or a game-state SHA256 implementation.

Still required: integrate canonical multiplayer simulation and real manifests,
prove its cross-device determinism with gameplay fixtures, wire frontend setup
and camera ownership, then perform coordinator-managed emulator/radio matches.
Physical radio loss, lid suspend, peer/host exit, repeated matches, incompatibility,
sustained frame cost, and subjective playability require actual consoles.
