#include "ge_asset_pack.h"
#include "ge_original_dam_mission_stage_storage.h"
#include "ge_original_stage_mission_runtime.h"
#include "ge_original_stage_setup.h"
#include "ge_stage_assets.h"

#include "bondtypes.h"

#include <assert.h>
#include <stdio.h>

stagesetup g_CurrentSetup;
s32 objectiveregisters1;
f32 g_AiAccuracyModifier;
f32 g_AiDamageModifier;
f32 g_AiHealthModifier;
s32 g_SeenBondRecentlyGuardCount;

extern ChrRecord *g_ActiveChrs;
extern s32 g_ActiveChrsCount;

static size_t expected_background_lists(const stagesetup *setup)
{
    size_t count = 0U;
    size_t index;
    for (index = 0U; setup->ailists[index].ailist != NULL; ++index) {
        if (setup->ailists[index].ID >= 0x1000) ++count;
    }
    return count;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    size_t stage_index;
    size_t campaign_background_lists = 0U;
    size_t campaign_live_actors = 0U;

    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup = {0};
        GeOriginalStageMissionRuntime mission = {0};
        size_t expected;
        size_t actor_index;
        int32_t first_background_id = -1;
        uint16_t first_background_offset = UINT16_MAX;

        assert(descriptor != NULL);
        assert(ge_original_stage_setup_load(&pack, descriptor, &setup)
            == GE_ORIGINAL_STAGE_SETUP_OK);
        g_CurrentSetup = *setup.setup;
        expected = expected_background_lists(setup.setup);
        ge_original_dam_stage_storage_reset();
        ge_original_stage_mission_runtime_reset_globals(&mission);
        assert(mission.globals_reset != 0U);
        assert(ge_original_stage_mission_runtime_begin(&mission, &setup)
            == GE_ORIGINAL_STAGE_MISSION_RUNTIME_OK);
        assert(mission.objective_registers == 0U);
        assert(mission.authored_background_list_count == expected);
        assert(mission.live_background_actor_count == expected);
        assert((size_t)g_ActiveChrsCount == expected);
        for (actor_index = 0U; actor_index < expected; ++actor_index) {
            assert(g_ActiveChrs[actor_index].chrnum == 0xfe);
            assert(g_ActiveChrs[actor_index].ailist != NULL);
            assert(g_ActiveChrs[actor_index].aioffset == 0);
            assert(g_ActiveChrs[actor_index].aireturnlist == -1);
            assert(g_ActiveChrs[actor_index].actiontype == ACT_NULL);
        }
        for (actor_index = 0U;
                setup.setup->ailists[actor_index].ailist != NULL;
                ++actor_index) {
            if (setup.setup->ailists[actor_index].ID >= 0x1000) {
                first_background_id = setup.setup->ailists[actor_index].ID;
                break;
            }
        }
        if (expected != 0U) {
            assert(first_background_id >= 0x1000);
            assert(ge_original_stage_mission_runtime_actor_offset(
                &mission, first_background_id, &first_background_offset));
            assert(first_background_offset == 0U);
        }
        objectiveregisters1 = (s32)(UINT32_C(0x01000000) | stage_index);
        assert(ge_original_stage_mission_runtime_observe_tick(&mission)
            == GE_ORIGINAL_STAGE_MISSION_RUNTIME_OK);
        assert(mission.observed_ticks == 1U);
        assert(mission.objective_registers
            == (UINT32_C(0x01000000) | stage_index));
        campaign_background_lists += expected;
        campaign_live_actors += mission.live_background_actor_count;
        printf("%s: %lu background AI actors\n", descriptor->key,
            (unsigned long)expected);
        ge_original_stage_mission_runtime_close(&mission);
        ge_original_stage_setup_close(&setup);
    }
    assert(campaign_background_lists == campaign_live_actors);
    assert(campaign_background_lists == 183U);
    ge_asset_pack_close(&pack);
    puts("Stage mission runtime: exact background AI allocation across all stages");
    return 0;
}
