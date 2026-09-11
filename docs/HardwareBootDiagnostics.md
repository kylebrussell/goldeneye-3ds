# Hardware black-screen investigation

The user reports both screens remain black with no audio when launching the
9c064e0a build through Homebrew Launcher on New Nintendo 3DS XL. The installed
3DSX, SMDH and asset pack were read back and hash verified. Existing DSP support
was present. The cause is not yet established.

Startup now enters through a small function before allocating the runtime's
large stack frame. It initializes a bottom-screen console and presents named
milestones during GPU, pack, collision, setup, sound, renderer and frontend
initialization. The console clears after the first frontend submission. There
is no per-frame log. Normal gameplay and asset formats are unchanged.

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
