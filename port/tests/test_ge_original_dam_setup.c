#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ge_original_dam_setup.h"
#include "bondtypes.h"

extern stagesetup UsetupdamZ;
extern PadRecord padlist[];
extern BoundPadRecord pad3dlist[];
extern int32_t intro[];
extern int32_t propDefs[];
extern waypoint pathwaypoints[];
extern waygroup pathsets[];
extern PathRecord patrolpaths[];
extern AIListRecord ailists[];
extern char *padnames[];
extern char *pad3dnames[];

enum {
    DAM_NORMAL_SPAWN_RECORD_WORD = 63,
    DAM_NORMAL_SPAWN_PAD = 33
};

int main(void)
{
    stagesetup *setup = ge_original_dam_setup_get();
    GeOriginalDamSpawn spawn = {0};

    assert(setup == &UsetupdamZ);
    assert(setup->pathwaypoints == pathwaypoints);
    assert(setup->waypointgroups == pathsets);
    assert(setup->intro == intro);
    assert(setup->propDefs == (PropDefHeaderRecord *)propDefs);
    assert(setup->patrolpaths == patrolpaths);
    assert(setup->ailists == ailists);
    assert(setup->pads == padlist);
    assert(setup->boundpads == pad3dlist);
    assert(setup->padnames == (pname *)padnames);
    assert(setup->boundpadnames == (pname *)pad3dnames);

    /* Six fixed cameras and the watch record precede normal-play spawn. */
    assert(intro[DAM_NORMAL_SPAWN_RECORD_WORD] == INTROTYPE_SPAWN);
    assert(intro[DAM_NORMAL_SPAWN_RECORD_WORD + 1] == DAM_NORMAL_SPAWN_PAD);
    assert(intro[DAM_NORMAL_SPAWN_RECORD_WORD + 2] == 0);

    assert(padlist[DAM_NORMAL_SPAWN_PAD].pos.x == 4719.0f);
    assert(padlist[DAM_NORMAL_SPAWN_PAD].pos.y == -18.0f);
    assert(padlist[DAM_NORMAL_SPAWN_PAD].pos.z == 3949.0f);
    assert(strcmp(padlist[DAM_NORMAL_SPAWN_PAD].plink, "p6g1") == 0);
    assert(ge_original_dam_setup_normal_spawn(&spawn));
    assert(spawn.pad_id == DAM_NORMAL_SPAWN_PAD);
    assert(spawn.position[0] == 4719.0f);
    assert(spawn.position[1] == -18.0f);
    assert(spawn.position[2] == 3949.0f);
    assert(spawn.up[0] == 0.0f);
    assert(spawn.up[1] == 1.0f);
    assert(spawn.up[2] == 0.0f);
    assert(spawn.look[0] == -1.0f);
    assert(spawn.look[1] == 0.0f);
    assert(spawn.look[2] == -0.000643f);
    assert(strcmp(spawn.plink, "p6g1") == 0);
    assert(!ge_original_dam_setup_normal_spawn(NULL));

    assert(pathwaypoints[0].padID == 0x00000149);
    assert(pathsets[0].waypoints != NULL);
    assert(patrolpaths[0].waypoints != NULL);
    assert(ailists[0].ailist != NULL);
    assert(ailists[0].ID == 0x00000401);
    assert(propDefs[0] != (int32_t)PROPDEF_END);

    puts("original Dam setup contract: ok (normal spawn pad 33)");
    return 0;
}
