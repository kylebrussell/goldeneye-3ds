# Native multiplayer watch presentation

`ge_original_multiplayer_watch_render_capture(slot, viewport, bank_gothic, out)`
runs the retained `mp_watch_menu_display` and its source rank/score/menu helpers
against the currently selected physical player. Capture never selects a player,
publishes VI state, advances the watch, calls awards, or consumes RNG. It requires
the live native session/watch owners and the real ROM Bank Gothic atlas. Every
text command owns its string; later page changes and teardown cannot invalidate
a successful snapshot. Invalid resources clear the published command count and
visibility and return an explicit error.

Generate the slice with
`scripts/extract_multiplayer_watch_render_slice.py ROOT OUTPUT`. Its generated
source and `ge_original_multiplayer_watch_render.c` require the existing original
types, setup, US, MS inheritance, `PLAYERFLAG=int` ABI flags. The portable builder
in `ge_3ds_original_hud.c` requires no native player layout. The retained watch
slice, real language provider/tables, and regenerated gun-data slice are link
dependencies. The latter now preserves all twelve named native watch metadata
fields in `gitem_structs`, including `weapon_of_choice_text`; model/file/stat
ownership is unchanged.

The source display functions retain the original play/favorite/award, kills,
losses, scores, pause, exit, confirmation and finished pages. This includes
directed negative scores, team colors, YOLT/token rank and score admission,
initiating-player pause highlight, native chevrons/glow, result labels and
eligible dead-player continue prompts. Award strings resolve through the live
watch owner's legacy-word sidecar. Language IDs resolve through the real typed
`LmpmenuE`/`LgunE` tables, never the unbound packed `g_LangBanks` loader.

Layout uses the retained `textMeasure` with the actual font's kerning, widths,
heights and signed baselines. Native `chars['[']` intentionally indexes physical
glyph 91 directly; its Bank Gothic baseline is -1 and height is 14, giving the
native default line height of 13. The shared atlas now retains that signed byte
without changing the six-byte glyph layout. Tests explicitly cover this source
indexing behavior and run the existing HUD regression.

Host ABI repairs replace decompiled undersized/overlaid formatting buffers with
bounded character arrays, and pointer-in-s32 temporary labels with typed string
locals. Award text uses its typed lifetime resolver. The unused uninitialized
`if (colour);` register-liveness expression is removed from the source slice.
These changes do not change page selection, source formatting or score logic.

`ge_3ds_original_multiplayer_watch_build_draw_list(atlas, snapshot, dx, dy, out)`
realizes the captured ASCII commands using the existing font atlas and vertex
format. Kerning, space/newline advancement, text colors, eight per-glyph outline
neighbors, clipping and viewport scissor are preserved for these authored pages.
This is a draw-only translation, with no scaling. Repeated eye passes consume
the same owned output. The capture supports the US horizontal font resource
boundary; Japanese glyph mode fails explicitly. Native stage text defaults are
spacing 0 and overlap correction -1, supplied in the viewport descriptor.

Use the authored 320×240 screen with 320×120 two-player viewports at `(0,slot*120)`
or 160×120 four-player viewports at `((slot%2)*160,(slot/2)*120)`. To center a
single local viewport on a 400×240 display, supply
`dx=(400-view_width)/2-view_left`, `dy=(240-view_height)/2-view_top`. Capture while
that local player is selected and native owners remain alive; build/upload once
before eye presentation. This preserves the source layout without rewriting
canonical VI or gameplay camera state.

`bash scripts/test_multiplayer_watch_render.sh` runs 42 two-player and 75
four-player source pages per peer with native/short enum ABIs and ASan/UBSan.
It uses actual players, awards, language, favorite metadata and ROM font data.
It checks score/team/confirmation/continuation predicates, command line height,
full player/permanent/watch/RNG immutability, deep string ownership, repeated
draw-only passes, missing resource rejection and peer/ABI output equality.
Adapter, generated slice and shared builder also compile under strict ARM flags.

Set `GE_MP_WATCH_RENDER_GALLERY=1` for optional CPU previews (Python/Pillow).
The fixture writes the owned vertices and texts to JSON plus the actual atlas
to `build/host-tests/multiplayer-watch-render/gallery`; the script produces
individual PNGs and `watch-gallery.png`. These use nearest atlas sampling and
ordinary alpha compositing to inspect font geometry and page layout. They are
not N64/PICA rasterizer validation, emulator screenshots or hardware/eye-render
evidence. Main-loop continuation, GPU upload and local display are separate
integration responsibilities; the shared gameplay watch owner remains the sole
menu/input/awards tick owner.
