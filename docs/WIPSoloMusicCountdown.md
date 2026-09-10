# WIP: solo music countdown closure

Stopped on the user's usage-limit priority change. This commit preserves
unfinished work; it is **not a completed runtime acceptance change**.

Branch: `codex/solo-music-slot-tick` in the isolated `/1ce8` worktree.
Base: `3b8d905cdef7f10e9246e8383b0256d063727139`.
Required shared helper: original Multiplayer commit `774028e8`, imported here
as `6a0c7e9d` (equivalent cherry-pick; do not apply both). The helper depends
on the native session/music prerequisites already integrated in the base;
Multiplayer confirmed it does not require its local `0ad`/`fde` ancestry.

The main edit calls the retained `reset_all_music_slots()` after the solo
background-AI boundary, before shuffle/fade/sync, with explicit title and
multiplayer exclusion. It stays outside the new-AI/pending condition so each
selected pass uses the original clock, including zero-clock pauses. The
original countdown is common non-title work: Multiplayer's separate world
owner positively performs it after AI and regeneration. Solo exclusion only
prevents duplicate ownership; it does not mean Multiplayer omits countdown.

The existing source frame-order check now requires AI -> countdown -> shuffle
-> movement. It passed locally, and `git diff --check` passed. No ARM build,
emulator run, audio listening or physical acceptance was performed for this
WIP. Multiplayer separately reported both-ABI ASan/UBSan positive world tests
and missing/before-AI/before-regeneration mutant rejection for `774028e8`;
those results do not certify this new solo hook.

`port/tests/test_ge_original_solo_music_slots.c` is an **uncompiled fixture
draft**. Its generated `fixture_frame`/native-countdown harness and test runner
have not been created, and it is not registered in the aggregate. The intended
harness must compile the actual production main block, actual active-prop
pre-tick and native music bodies, with explicit world/audio observers. The
draft covers same-tick AI requests, pending AI, watch/clock0/globaldelta0,
retained inactive timers, expiry, Dam/Surface2 transitions, title and exclusion
of this solo call from Multiplayer. Missing/before-AI, title-guard and duplicate
MP-owner mutants have not been implemented or run.

Remaining before this work can be treated as ready: finish that executable
harness, validate the draft and mutants, register the focused test, and perform
the appropriate integration build. Do not count the static order check as
behavioral closure. No further work should start from these notes without a
new user instruction; the current instruction is to preserve work and stop.
