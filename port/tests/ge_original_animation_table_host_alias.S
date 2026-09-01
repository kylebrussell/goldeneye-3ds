/*
 * animationtable_data.c places ANIM_DATA_bond_watch 0x42c8 bytes after
 * ANIM_DATA_empty.  The original linker exports that byte offset as an
 * absolute symbol, and unchanged bondview2.c obtains it through
 * (s32)&ANIM_DATA_bond_watch.  Native host linkers need the same absolute
 * symbol while the generated table wrapper owns the actual bytes.
 */
#ifdef __APPLE__
.globl _ANIM_DATA_bond_watch
.set _ANIM_DATA_bond_watch, 0x42c8
#else
.globl ANIM_DATA_bond_watch
.set ANIM_DATA_bond_watch, 0x42c8
#endif
