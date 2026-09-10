#include <ultra64.h>
#include <bondgame.h>
#include <bondtypes.h>
#include "game/chrobjdata.h"

/* Exact embedded decomp assets used by sets_a_bunch_of_BONDdata_values_to_default. */
#undef MODELSKELETON
#define MODELSKELETON(NAME, NUMJOINTS, SKELSIZE) \
    ModelSkeleton SKELETON(NAME) = { \
        NUMJOINTS, 0, JOINTLIST(NAME), SKELSIZE, 0 \
    };
#include <assets/embedded/skeletons/player_gait_object.inc.c>
#include <assets/embedded/player_gait_object/modelnode.inc.c>
#include <assets/embedded/player_gait_object/header.inc.c>

#include "ge_original_model_topology.h"
const void *ge_original_player_gait_topology_header(size_t *bytes)
{
    if (bytes) *bytes=sizeof(player_gait_object_header);
    return &player_gait_object_header;
}
size_t ge_original_player_gait_topology_node_count(void){return 4;}
int ge_original_player_gait_topology_node(size_t index,const void **node,size_t *bytes)
{
    const ModelNode *nodes[]={&player_gait_hdr,&player_gait_pos_hdr_1,
        &player_gait_pos_hdr_2,&player_gait_pos_hdr_3};
    if (index>=4 || !node || !bytes) return 0;
    *node=nodes[index];*bytes=sizeof(ModelNode);return 1;
}
