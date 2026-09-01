#include "ge_asset_pack.h"
#include "ge_dam_dynamic_scene.h"
#include "ge_dam_preload_queue.h"
#include "ge_dam_world.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
                    != GE_PICA_TEXTURE_SOURCE_RARE_ID) {
            continue;
        }
        for (prior_index = 0U; prior_index < batch_index; ++prior_index) {
            const GeDamRoomDrawBatch *prior = &scene->batches[prior_index];

            if (prior->texture_valid != 0U
                    && prior->material.texture_enabled != 0U
                    && prior->material.texture_source
                        == GE_PICA_TEXTURE_SOURCE_RARE_ID
                    && prior->texture.texture_id
                        == batch->texture.texture_id) {
                break;
            }
        }
        if (prior_index == batch_index) {
            ++texture_count;
        }
    }
    return texture_count;
}

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

static FileData read_file(const char *path)
{
    FileData result = {NULL, 0U};
    FILE *stream = fopen(path, "rb");
    long length;

    if (stream == NULL || fseek(stream, 0L, SEEK_END) != 0
            || (length = ftell(stream)) <= 0L
            || fseek(stream, 0L, SEEK_SET) != 0) {
        if (stream != NULL) fclose(stream);
        return result;
    }
    result.size = (size_t)length;
    result.bytes = malloc(result.size);
    if (result.bytes == NULL
            || fread(result.bytes, 1U, result.size, stream) != result.size) {
        free(result.bytes);
        result.bytes = NULL;
        result.size = 0U;
    }
    fclose(stream);
    return result;
}

int main(int argc, char **argv)
{
    static const uint8_t initial[] = {
        135U, 133U, 134U, 132U, 136U, 124U, 125U, 126U, 127U, 128U,
    };
    static const GeDamDynamicSceneLimits limits = {
        GE_DAM_WORLD_MAX_ROOMS, 65536U, 65536U,
    };
    FileData background;
    GeAssetPack pack;
    GeDamWorld world;
    GeDamPreloadQueue queue;
    GeDamDynamicScene scene;
    int result = 0;
    int argument;

    if (argc < 4) {
        fprintf(stderr, "usage: %s BACKGROUND GEPACK ROOM...\n", argv[0]);
        return 2;
    }
    background = read_file(argv[1]);
    if (background.bytes == NULL
            || ge_dam_world_parse(background.bytes, background.size, &world)
                != GE_DAM_WORLD_OK
            || ge_asset_pack_open(&pack, argv[2]) != GE_ASSET_PACK_OK) {
        fprintf(stderr, "could not load Dam background/asset pack\n");
        free(background.bytes);
        return 2;
    }
    if (ge_dam_preload_queue_init(&queue, world.room_count,
            GE_DAM_PRELOAD_MAX_ROOMS, initial, sizeof(initial))
                != GE_DAM_PRELOAD_OK
            || ge_dam_dynamic_scene_init(&scene, &pack, &world,
                initial, sizeof(initial), &limits)
                != GE_DAM_DYNAMIC_SCENE_OK) {
        fprintf(stderr, "could not build the initial Dam resident scene\n");
        ge_asset_pack_close(&pack);
        free(background.bytes);
        return 2;
    }
    printf("initial: %lu room, %lu vertex, %lu batch, %lu texture\n",
           (unsigned long)scene.room_count,
           (unsigned long)scene.scene.vertex_count,
           (unsigned long)scene.scene.batch_count,
           (unsigned long)count_unique_rare_textures(&scene));
    for (argument = 3; argument < argc; ++argument) {
        char *end = NULL;
        long parsed;
        GeDamDynamicSceneStatus status;

        errno = 0;
        parsed = strtol(argv[argument], &end, 0);
        if (errno != 0 || end == argv[argument] || *end != '\0'
                || parsed < 0L || (size_t)parsed >= world.room_count) {
            fprintf(stderr, "invalid room: %s\n", argv[argument]);
            result = 2;
            break;
        }
        if (ge_dam_preload_queue_room_state(&queue, (uint8_t)parsed)
                == GE_DAM_PRELOAD_ROOM_RESIDENT) {
            printf("room %ld: already resident\n", parsed);
            continue;
        }
        (void)ge_dam_preload_queue_request(&queue, (uint8_t)parsed);
        status = ge_dam_dynamic_scene_install_next(&scene, &queue);
        printf("room %ld: %s -> %lu room, %lu vertex, %lu batch, "
               "%lu texture, "
               "%llu generation/%llu failure\n",
               parsed, ge_dam_dynamic_scene_status_name(status),
               (unsigned long)scene.room_count,
               (unsigned long)scene.scene.vertex_count,
               (unsigned long)scene.scene.batch_count,
               (unsigned long)count_unique_rare_textures(&scene),
               (unsigned long long)scene.generation,
               (unsigned long long)scene.install_failures);
        if (status != GE_DAM_DYNAMIC_SCENE_OK) {
            result = 1;
            break;
        }
    }
    ge_dam_dynamic_scene_close(&scene);
    ge_asset_pack_close(&pack);
    free(background.bytes);
    return result;
}
