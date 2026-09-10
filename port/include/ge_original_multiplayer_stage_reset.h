#ifndef GE_ORIGINAL_MULTIPLAYER_STAGE_RESET_H
#define GE_ORIGINAL_MULTIPLAYER_STAGE_RESET_H

/* Retained statement subset from bondviewLoadSetupIntroSection: force-disarm,
 * native camera mode, mission timer, then watch timer, in original order.
 * Call once before the stage's first multiplayer spawn. This is not a reset
 * of the rest of that original intro function or any per-player state. */
void ge_original_multiplayer_stage_intro_shared_reset_exact(void);

#endif
