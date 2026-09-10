# Local wireless lobby and remaining multiplayer work

The port now includes a real libctru UDS host/join/ready/disconnect lobby. It is
a **connection test**, not a playable multiplayer mode. It never starts a match,
changes the gameplay player count, or treats network readiness as proof of
simulation compatibility.

## Use on New Nintendo 3DS

1. Install the same build and asset pack on two to four systems and enable
   wireless. Hold **SELECT while launching** GoldenEye to enter the diagnostic
   lobby before the renderer starts. The asset pack must be available.
2. On one system press **X** to host. On the others press **Y** to find nearby
   lobbies, choose one with **Up/Down**, and press **A** to join.
3. Press **A** to toggle readiness. Check that each system lists the other
   compatible peers and their readiness. Joining, leaving, or a heartbeat
   timeout clears local readiness.
4. **B** leaves the current lobby; another **B** returns to the game.
   **START** returns directly to the game. UDS is released before solo startup.

The menu deliberately says “Connection test / lobby only” and “Network matches
are not available yet.” No router, Internet service, or external account is
used. Discovery runs only on request, before gameplay; it may block while the
UDS service scans. Match-time networking would need a separate scheduling design.

## Implemented boundaries

- `port/src/ge_local_wireless.c`: portable 32-byte big-endian presence protocol,
  strict magic/version/type/length/reserved-field checks, CRC32, nonzero session
  ID, build/asset contract comparison, wrap-safe sequence ordering, transport
  source-node validation, timeout and membership readiness handling.
- `platform/3ds/source/ge_3ds_wireless.c`: lazy UDS initialization, four-device
  hosting with spectators disabled, bounded discovery results, joining,
  250 ms heartbeat sends, at most eight received datagrams per poll, queue-full
  retry throttling, disconnect detection and cleanup. Full-size UDS receive
  buffers consume and reject oversized application packets.
- `platform/3ds/source/ge_3ds_wireless_lobby.c`: the console entry point.
  `ge_3ds_wireless_lobby_run(build_id, asset_id)` requires an initialized
  framebuffer/console and must be called **before `C3D_Init`**. It owns and
  releases its UDS session. Do not call it inside an active C3D frame.

The UDS communication ID is private homebrew value `0x47453037`; protocol
version 1 is also the discovery subtype. The static passphrase provides
application separation only; this lobby is not a security or authentication
boundary. CRC32 checks accidental corruption, not malicious alterations.

Current startup uses the dated lobby contract `0x20260909` as `build_id` and
folds the pack's source ROM SHA-1, entry count and file size into `asset_id`.
**These are compatibility hints for the lobby only.** The build value is not a
binary fingerprint; the asset value is not a digest of all packed bytes or
sidecar textures. Same-size modified content can pass. Before matches can be
enabled, exchange complete build/simulation and content manifests, including
sidecars, and reject differences. Session IDs currently derive from a system
tick and provide stale-session separation, not a cryptographic nonce.

## Verification

Run `sh scripts/test_local_wireless.sh`. Both test executables use ASan and UBSan:

- Protocol round trips and byte order, every single-byte corruption, all short
  packet lengths, oversized packets, invalid identities, incompatible session/
  build/assets, replay and sequence wraparound, peer timeouts, clock reversal,
  disconnect/node reuse, host absence, and the four-player readiness limit.
- UDS service doubles exercise initialization failure, advertisement failure
  cleanup, bounded packet draining, heartbeat send rate, transient queue-full
  behavior, fatal send cleanup, discovery capacity and compatibility filtering,
  join bounds, host loss, and idempotent shutdown.

The adapter and console module also compile against the actual libctru headers
in the installed `devkitpro/devkitarm:latest` toolchain with `-Wall -Wextra
-Werror`. Mocks and ARM compilation do not validate radio behavior. The remaining
hardware acceptance test is a two-to-four-console run, including peer leave,
host exit, wireless disable, lid suspend/resume, repeated host/join cycles,
incompatible builds, and packet-loss conditions. Measure frame cost separately
once a match runtime exists.

## Required before playable wireless matches

The current platform publishes a single player in `main.c`; the original
player-body adapter returns `GE_ORIGINAL_PLAYER_BODY_MULTIPLAYER_UNAVAILABLE`
when the multi-player construction branch is needed. The local network layer
does not close those gameplay boundaries.

1. Restore canonical two-to-four-player allocation and per-player state,
   switching `g_CurrentPlayer` safely through each simulation tick. Restore
   multiplayer body/head selection, actor construction, death/respawn and held
   items; ensure each console renders its local player's camera without split
   screen.
2. Load and validate multiplayer arenas and original multiplayer setup/weapon
   sets. Close ammo, respawn and scenario/scoring dependencies currently
   represented only by conditional adapters. Verify character and pickup
   ownership rather than reusing single-player globals.
3. Choose and implement a synchronization model. Lockstep needs deterministic
   fixed-step simulation and identical PRNG state on every device; authoritative
   host replication needs explicit actor IDs, ownership and state replication.
   Neither is supplied by a lobby or a stream of raw controller samples.
4. Add player-slot admission, rules/seed agreement, reliable acknowledged start,
   input tick numbering/buffering, bounded packet-loss recovery, state hashes,
   disconnect policy, and explicit desynchronization handling. Do not begin
   simulation on the current all-ready helper: each peer sees its own transient
   membership/readiness view and there is no distributed start barrier.
5. Validate gameplay parity and measure stereo 60 fps on real hardware with
   four active players, weapons, effects and radio traffic. Integrate the lobby
   into the normal frontend and touchscreen only once match setup can be
   completed honestly.
