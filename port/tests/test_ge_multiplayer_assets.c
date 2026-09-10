#define GE_MP_FIXTURE_NATIVE_STAN
#define main ge_mp_rules_fixture_main
#include "test_ge_original_multiplayer_rules.c"
#undef main
#include "ge_stage_assets.h"
#include "ge_original_stage_setup.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    GeAssetPack pack;
    size_t arena;
    assert(argc == 2);
    assert(GE_STAGE_COUNT == 21);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    assert(ge_stage_asset_descriptor_by_level_id(34) == ge_stage_asset_facility());
    assert(ge_stage_asset_multiplayer_by_level_id(34) != ge_stage_asset_facility());
    for (arena = 0; arena < GE_MULTIPLAYER_STAGE_COUNT; ++arena) {
        const GeStageAssetDescriptor *descriptor = ge_stage_asset_multiplayer(arena);
        GeStageResolvedAssets world = {0};
        GeOriginalStageSetupRuntime stage = {0};
        GeStanNativeMap native;
        size_t required, i, count;
        void *storage;
        int status;
        assert(ge_stage_asset_descriptor(descriptor->stage) == descriptor);
        assert(ge_stage_asset_descriptor_by_key(descriptor->key) == descriptor);
        assert(ge_stage_asset_multiplayer_by_level_id(descriptor->level_id) == descriptor);
        assert(ge_stage_assets_resolve(&pack, descriptor, &world) == GE_STAGE_ASSET_OK);
        status = ge_original_stage_setup_load(&pack, descriptor, &stage);
        if (status != GE_ORIGINAL_STAGE_SETUP_OK)
            fprintf(stderr, "%s setup: %s\n", descriptor->key, ge_original_stage_setup_status_name(status));
        assert(status == GE_ORIGINAL_STAGE_SETUP_OK);
        assert(ge_stan_native_required_size(&world.stan, &required) == GE_STAN_COLLISION_OK);
        storage = malloc(required); assert(storage);
        assert(ge_stan_native_materialize(&world.stan, descriptor->level_scale, storage, required, &native) == GE_STAN_COLLISION_OK);
        assert(ge_stan_native_bind_original(&native) == GE_STAN_COLLISION_OK);
        status = ge_original_stage_setup_bind_stan(&stage, &native);
        if (status != GE_ORIGINAL_STAGE_SETUP_OK)
            fprintf(stderr, "%s STAN: %s (%zu/%zu pads, %zu/%zu bound)\n", descriptor->key,
                ge_original_stage_setup_status_name(status), stage.pad_stan_count, stage.pad_count, stage.bound_pad_stan_count, stage.boundpad_count);
        assert(status == GE_ORIGINAL_STAGE_SETUP_OK);
        assert(ge_original_stage_setup_publish(&stage));
        fixture_native_stan = &native;
        for (count = 2; count <= ge_stage_asset_multiplayer_max_players(arena); ++count) {
            GeOriginalMultiplayerConfig config = {(int)count,SCENARIO_NORMAL,1,descriptor->level_id,{0}};
            setup((int)count, SCENARIO_NORMAL);
            assert(ge_original_multiplayer_configure(&config) == GE_ORIGINAL_MP_OK);
            assert(ge_original_multiplayer_publish_spawns(stage.intro_storage,stage.intro_word_count,stage.pads_storage,stage.pad_count,0) == GE_ORIGINAL_MP_OK);
            assert((size_t)startpadcount == ge_stage_asset_multiplayer_spawn_count(arena));
            for (i=0;i<count;++i) {
                size_t p;
                players[i].bonddead=1;finish_death((int)i);
                assert(ge_original_multiplayer_respawn((int)i,0x8000) == GE_ORIGINAL_MP_OK);
                assert(players[i].bonddead == 0 && players[i].bondhealth == 1);
                assert(ge_stan_native_contains_tile(&native,props[i].stan));
                assert(isfinite(props[i].pos.y));
                assert(props[i].pos.y == players[i].eyeheight + players[i].stanHeight);
                for (p=0;p<(size_t)startpadcount;++p)
                    if (props[i].pos.x == g_Startpad[p]->pos.x && props[i].pos.z == g_Startpad[p]->pos.z) break;
                assert(p < (size_t)startpadcount);
            }
        }
        printf("%s: %zu pads, %zu props, %zu authored spawns; 2..%zu players respawn on original STAN\n",
               descriptor->key,stage.pad_count,stage.prop_record_count,ge_stage_asset_multiplayer_spawn_count(arena),ge_stage_asset_multiplayer_max_players(arena));
        fixture_native_stan = NULL;
        ge_original_stage_setup_close(&stage);
        ge_stage_assets_close(&world);
        free(storage);
    }
    ge_asset_pack_close(&pack);
    return 0;
}
