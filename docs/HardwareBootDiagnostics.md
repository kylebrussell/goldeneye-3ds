# Hardware black-screen investigation

The user reports both screens remain black with no audio when launching the
9c064e0a build through Homebrew Launcher on New Nintendo 3DS XL. The installed
3DSX, SMDH and asset pack were read back and hash verified. Existing DSP support
was present. The cause is not yet established.

Startup now enters through a small function before allocating the runtime's
large stack frame. It initializes a bottom-screen console and presents named
milestones during GPU, pack, collision, setup, sound, renderer and frontend
initialization. A bounded trace of the first three frontend frames separates
GPU-queue, music, DSP and VBlank waits. The console clears only after those
three frames complete; there is no continuing per-frame log. Normal gameplay
and asset formats are unchanged.

`SD:/3ds/goldeneye-3ds/boot.log` is replaced each launch, with each checkpoint
flushed and closed. A missing new log suggests launch failed before application
entry or SD access; it does not distinguish those causes. The last checkpoint
narrows the next investigation. A first-frame submission is not proof that
hardware displayed the frame.

The original entry used about 109 KiB of local stack before graphics setup.
The diagnostic entry compiles to a 16-byte own frame and tail-calls the runtime
only after presenting a checkpoint. This improves observability; it does not
establish a stack overflow as the original cause.

Validation: forced pinned ARM build passed; existing multiplayer music order,
probe replay and probe difficulty wiring checks passed after updating their
runtime entry selector. Hardware rerun and log collection are pending. Use the
same verified asset pack; the microSD must be reconnected to install this build.

The first physical diagnostic run reached `First frontend frame submitted`
after all startup milestones, then both screens became black. Its original log
is retained at `build/hardware-boot-20260911/first-frontend-stall.log`. This
rules out a failure before application entry for that run, but does not identify
which subsequent wait stalled. Optional frontend background, overlay and glyph
draws now omit zero-vertex submissions. Production-gate sanitizer tests verify
empty calls are omitted and nonempty ranges retained; hardware confirmation
of a causal connection is pending. The next diagnostic keeps the bottom console
visible while those first three frames pass through each individual wait.
