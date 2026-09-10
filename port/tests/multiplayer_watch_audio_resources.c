/* Audio output observations only. Native set_missionstate, stage music tables,
 * random track choice, menu beeps and full award computation are linked. */
#include <ultra64.h>
#include <bondconstants.h>
#include <music.h>
#include <assert.h>
s32 g_musicXTrack1Fade,g_musicXTrack2Fade,g_musicXTrack3Fade;
unsigned fixture_music_intro_plays,fixture_music_stops[3],fixture_music_plays[3];
s32 fixture_music_last_track[3];
u16 get_mTrack2Vol(void) {return 0x4321;}
u16 call_sndGetSfxSlotFirstNaturalVolume(void) {return 0x3210;}
#define TRACK(N) \
void musicTrack##N##Play(s32 id) {assert(id>=0);++fixture_music_plays[N-1];fixture_music_last_track[N-1]=id;if(N==1&&id==M_INTROSWOOSH)++fixture_music_intro_plays;} \
void musicTrack##N##Stop(void) {++fixture_music_stops[N-1];} \
void musicTrack##N##ApplySeqpVol(u16 v) {assert(v==0x4321||v==0x3210);} \
void musicTrack##N##FadeOut(f32 t) {assert(t>0);} \
void musicTrack##N##FadeIn(f32 t,u16 v) {assert(t>0&&(v==0x4321||v==0x3210));}
TRACK(1) TRACK(2) TRACK(3)
#ifndef GE_MP_WATCH_REAL_SFX
/* Older rule-only fixtures lack a sound-slot owner. The full native watch
 * fixture uses the production service and verifies actual flag filtering. */
void sndDeactivateAllSfxByFlag_1(void) {}
#endif
