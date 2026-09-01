#include "ge_stan_collision.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    unsigned char *data;
    long length;

    assert(stream != NULL);
    assert(fseek(stream, 0L, SEEK_END) == 0);
    length = ftell(stream);
    assert(length > 0L && fseek(stream, 0L, SEEK_SET) == 0);
    *size = (size_t)length;
    data = malloc(*size);
    assert(data != NULL);
    assert(fread(data, 1U, *size, stream) == *size);
    assert(fclose(stream) == 0);
    return data;
}

int main(int argc, char **argv)
{
    unsigned char *bytes;
    size_t size;
    GeStanCollisionSurface surface;
    GeStanCollisionTile spawn;
    GeStanCollisionHit floor;

    assert(argc == 2);
    bytes = read_file(argv[1], &size);
    assert(ge_stan_collision_open(bytes, size, &surface)
        == GE_STAN_COLLISION_OK);
    assert(surface.tile_count == 2599U);
    assert(surface.point_count == 7908U);
    assert(surface.spawn_tile == 2325U);
    assert(surface.spawn_room == 13U);
    assert(ge_stan_collision_get_tile(&surface, surface.spawn_tile, &spawn)
        == GE_STAN_COLLISION_OK);
    assert(spawn.source_tile_index == 2325U);
    assert(spawn.tile_id == 0x00069201U);
    assert(spawn.room == 13U);
    assert(ge_stan_collision_ground(&surface, 137.0f, 562.0f, -1154.0f,
        32.0f, 512.0f, 0.65f, &floor) == GE_STAN_COLLISION_OK);
    assert(floor.source_tile_index == 2325U);
    assert(floor.room == 13U);
    assert(floor.y == 272.0f);
    free(bytes);
    puts("Facility exact STAN/spawn host validation passed");
    return 0;
}
