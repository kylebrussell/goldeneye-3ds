#ifndef _BOSS_H_
#define _BOSS_H_
#include <ultra64.h>
#ifdef GE_PORT_BOSS_STAGE_SLICE
typedef s32 LEVELID;
enum {
  LEVELID_NONE = -1,
  LEVELID_DAM = 33,
  LEVELID_TITLE = 90
};
#else
#include <bondgame.h>
#endif

struct memallocstring
{
  s32 id;
  void *string;
};

LEVELID bossGetStageNum(void);
void bossSetLoadedStage(LEVELID stage);
void bossInit(void);
void bossEnableShowMemUseFlag(void);
void bossMemBarsFlagToggle(void);
void bossRunTitleStage(void);
void bossReturnTitleStage(void);
s32 bossGetDebugParseFlag(void);

#endif
