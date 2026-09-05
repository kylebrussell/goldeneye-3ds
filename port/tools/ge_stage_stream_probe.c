#include "ge_asset_pack.h"
#include "ge_dam_dynamic_scene.h"
#include "ge_dam_preload_queue.h"
#include "ge_stage_assets.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_ROOM_CAPACITY 10U

static size_t count_unique_rare_textures(const GeDamDynamicScene *scene)
{
    size_t texture_count = 0U;
    size_t batch_index;

    for (batch_index = 0U; batch_index < scene->scene.batch_count;
            ++batch_index) {
        const GeDamRoomDrawBatch *batch = &scene->batches[batch_index];
        size_t prior_index;

        if (batch->texture_valid == 0U
                || batch->material.texture_enabled == 0U
                || batch->material.texture_source
                    != GE_PICA_TEXTURE_SOURCE_RARE_ID) continue;
        for (prior_index = 0U; prior_index < batch_index; ++prior_index) {
            const GeDamRoomDrawBatch *prior = &scene->batches[prior_index];
            if (prior->texture_valid != 0U
                    && prior->material.texture_enabled != 0U
                    && prior->material.texture_source
                        == GE_PICA_TEXTURE_SOURCE_RARE_ID
                    && prior->texture.texture_id
                        == batch->texture.texture_id) break;
        }
        if (prior_index == batch_index) ++texture_count;
    }
    return texture_count;
}

static int install_room(GeDamDynamicScene *scene,
                        GeDamPreloadQueue *queue,
                        uint8_t room)
{
    GeDamDynamicSceneStatus status;

    if (ge_dam_preload_queue_room_state(queue, room)
            == GE_DAM_PRELOAD_ROOM_RESIDENT) return 0;
    (void)ge_dam_preload_queue_request(queue, room);
    status = ge_dam_dynamic_scene_install_next(scene, queue);
    printf("room %u: %s -> %lu room, %lu vertex, %lu batch, "
           "%lu texture, %llu generation/%llu failure\n",
           (unsigned)room, ge_dam_dynamic_scene_status_name(status),
           (unsigned long)scene->room_count,
           (unsigned long)scene->scene.vertex_count,
           (unsigned long)scene->scene.batch_count,
           (unsigned long)count_unique_rare_textures(scene),
           (unsigned long long)scene->generation,
           (unsigned long long)scene->install_failures);
    return status == GE_DAM_DYNAMIC_SCENE_OK ? 0 : 1;
}

static int audit_materials(GeAssetPack *pack, const GeStageAssetDescriptor *stage,
                           const GeDamWorld *world)
{
    const GeDamDynamicSceneLimits limits = {1U, 65536U, 65536U};
    int failures = 0;
    puts("stage,room,batch,texture,triangles,pass,depth,depth_test,depth_write,blend,alpha_test,alpha_threshold,fog,fallback,texture_enabled,texture_source,color_combine,alpha_combine");
    for (size_t room = 1U; room < world->room_count; ++room) {
        GeDamDynamicScene scene = {0};
        uint8_t id = (uint8_t)room;
        GeDamDynamicSceneStatus status = ge_dam_dynamic_scene_init_for_stage(
            &scene, pack, stage, world, &id, 1U, &limits);
        if (status != GE_DAM_DYNAMIC_SCENE_OK) {
            fprintf(stderr, "%s room %zu: %s\n", stage->key, room,
                ge_dam_dynamic_scene_status_name(status));
            failures++;
        } else for (size_t batch = 0U; batch < scene.scene.batch_count; ++batch) {
            const GeDamRoomDrawBatch *b = &scene.batches[batch];
            const GePicaMaterial *m = &b->material;
            printf("%s,%zu,%zu,%u,%zu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                stage->key, room, batch, m->texture_id, b->triangle_count,
                b->list_kind, m->depth_mode, m->depth_test_enabled,
                m->depth_write_enabled, m->blend_enabled, m->alpha_test,
                m->alpha_threshold, m->fog_enabled, m->fallback_flags,
                m->texture_enabled, m->texture_source, m->color_combine,
                m->alpha_combine);
        }
        ge_dam_dynamic_scene_close(&scene);
    }
    return failures != 0;
}

int main(int argc, char **argv)
{
    static const GeDamDynamicSceneLimits limits = {
        GE_DAM_WORLD_MAX_ROOMS, 65536U, 65536U,
    };
    const GeStageAssetDescriptor *stage;
    GeAssetPack pack;
    GeStageResolvedAssets assets;
    GeDamPreloadQueue queue;
    GeDamDynamicScene scene;
    uint8_t initial[INITIAL_ROOM_CAPACITY];
    size_t initial_count = 0U;
    int result = 0;
    int argument;

    if (argc < 3) {
        fprintf(stderr, "usage: %s STAGE GEPACK [ROOM...]\n", argv[0]);
        return 2;
    }
    stage = ge_stage_asset_descriptor_by_key(argv[1]);
    if (stage == NULL || ge_asset_pack_open(&pack, argv[2])
            != GE_ASSET_PACK_OK) {
        fprintf(stderr, "unknown stage or unreadable asset pack\n");
        return 2;
    }
    if (argc == 4 && strcmp(argv[3], "--materials") == 0) {
        if (ge_stage_assets_resolve(&pack, stage, &assets) != GE_STAGE_ASSET_OK) {
            ge_asset_pack_close(&pack);
            return 2;
        }
        result = audit_materials(&pack, stage, &assets.world);
        ge_stage_assets_close(&assets);
        ge_asset_pack_close(&pack);
        return result;
    }
    if (ge_stage_assets_resolve(&pack, stage, &assets)
            != GE_STAGE_ASSET_OK
            || ge_dam_world_collect_connected(
                &assets.world, stage->expected_spawn_room, initial,
                sizeof(initial), &initial_count) != GE_DAM_WORLD_OK
            || initial_count == 0U
            || ge_dam_preload_queue_init(
                &queue, assets.world.room_count, GE_DAM_PRELOAD_MAX_ROOMS,
                initial, initial_count) != GE_DAM_PRELOAD_OK
            || ge_dam_dynamic_scene_init_for_stage(
                &scene, &pack, stage, &assets.world, initial, initial_count,
                &limits) != GE_DAM_DYNAMIC_SCENE_OK) {
        fprintf(stderr, "could not resolve or stream stage %s\n", stage->key);
        ge_stage_assets_close(&assets);
        ge_asset_pack_close(&pack);
        return 2;
    }
    printf("%s initial:", stage->key);
    for (argument = 0; (size_t)argument < initial_count; ++argument)
        printf(" %u", (unsigned)initial[argument]);
    printf("\n%lu room, %lu vertex, %lu batch, %lu texture\n",
           (unsigned long)scene.room_count,
           (unsigned long)scene.scene.vertex_count,
           (unsigned long)scene.scene.batch_count,
           (unsigned long)count_unique_rare_textures(&scene));

    argument = 3;
    if (argument < argc && strcmp(argv[argument], "--all-connected") == 0) {
        uint8_t connected[GE_DAM_WORLD_MAX_ROOMS];
        size_t connected_count = 0U;
        size_t index;

        if (ge_dam_world_collect_connected(
                &assets.world, stage->expected_spawn_room, connected,
                sizeof(connected), &connected_count) != GE_DAM_WORLD_OK) {
            result = 2;
        } else {
            for (index = 0U; index < connected_count; ++index) {
                if (install_room(&scene, &queue, connected[index]) != 0) {
                    result = 1;
                    break;
                }
            }
        }
        ++argument;
    }
    for (; result == 0 && argument < argc; ++argument) {
        char *end = NULL;
        long parsed;

        errno = 0;
        parsed = strtol(argv[argument], &end, 0);
        if (errno != 0 || end == argv[argument] || *end != '\0'
                || parsed < 0L
                || (size_t)parsed >= assets.world.room_count) {
            fprintf(stderr, "invalid room: %s\n", argv[argument]);
            result = 2;
            break;
        }
        if (install_room(&scene, &queue, (uint8_t)parsed) != 0) {
            result = 1;
            break;
        }
    }
    ge_dam_dynamic_scene_close(&scene);
    ge_stage_assets_close(&assets);
    ge_asset_pack_close(&pack);
    return result;
}
