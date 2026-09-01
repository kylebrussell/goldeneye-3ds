#include "ge_stan_collision.h"
#include "ge_stan_native.h"
#include "ge_original_stan_slice.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(GE_PORT_STAN_DYNAMIC_PROP_COLLISION)
extern struct PropRecord *stanSavedColl_posData;
static s16 dynamic_prop_indices[2] = { -1, -1 };
s16 *ptr_list_object_lookup_indices = dynamic_prop_indices;
static unsigned char dynamic_prop_storage;
static int dynamic_prop_enabled;
static float dynamic_line_start_x;
static float dynamic_line_start_z;
static float dynamic_line_end_x;
static float dynamic_line_end_z;

void roomGetProps(s32 *rooms)
{
    assert(rooms != NULL && rooms[0] >= 0);
    dynamic_prop_indices[0] = dynamic_prop_enabled ? 0 : -1;
    dynamic_prop_indices[1] = -1;
}

s32 propIsOfCdType(struct PropRecord *prop, s32 cdtypes)
{
    assert(prop == (struct PropRecord *)&dynamic_prop_storage);
    assert(cdtypes != 0);
    return TRUE;
}

void chraiGetCollisionBounds(struct PropRecord *prop, struct rect4f **polygon,
        s32 *edges, f32 *top, f32 *bottom)
{
    static struct rect4f bounds;
    const float x = (dynamic_line_start_x + dynamic_line_end_x) * 0.5f;
    const float z = (dynamic_line_start_z + dynamic_line_end_z) * 0.5f;
    assert(prop == (struct PropRecord *)&dynamic_prop_storage);
    bounds.points[0].x = x - 1.0f;
    bounds.points[0].y = z - 1.0f;
    bounds.points[1].x = x + 1.0f;
    bounds.points[1].y = z - 1.0f;
    bounds.points[2].x = x + 1.0f;
    bounds.points[2].y = z + 1.0f;
    bounds.points[3].x = x - 1.0f;
    bounds.points[3].y = z + 1.0f;
    *polygon = &bounds;
    *edges = 4;
    *top = 1000.0f;
    *bottom = -1000.0f;
}

struct PropRecord *ge_port_stan_prop_at_index(s16 index)
{
    assert(index == 0);
    return (struct PropRecord *)&dynamic_prop_storage;
}
#endif

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;
    uint8_t *bytes;

    assert(file != NULL);
    assert(fseek(file, 0L, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0L);
    assert(fseek(file, 0L, SEEK_SET) == 0);
    *size = (size_t)length;
    bytes = malloc(*size);
    assert(bytes != NULL);
    assert(fread(bytes, 1U, *size, file) == *size);
    assert(fclose(file) == 0);
    return bytes;
}

int main(int argc, char **argv)
{
    uint8_t *bytes;
    size_t size;
    GeStanCollisionSurface surface;
    GeStanCollisionTile tile;
    GeStanCollisionPoint point;
    GeStanCollisionHit hit;
    GeStanNativeMap native;
    GeStanNativeTile *spawn;
    GeStanNativeTile *walk_tile;
    GeStanNativeTile *radius_tile;
    GeStanNativeTile *linked_tile;
    GeStanNativeTile *guard_sight_tile;
    GeStanNativeTile *bond_sight_tile;
    GeStanNativeTile *special_tile = NULL;
    GeStanNativeTile *ladder_source = NULL;
    GeOriginalStanLocusResult locus;
    uint8_t *native_bytes;
    size_t native_size;
    unsigned int linked_edge = 0U;
    unsigned int unlinked_edge = 0U;
    unsigned int ladder_edge = 0U;
    float spawn_x = 0.0f;
    float spawn_z = 0.0f;
    float linked_x = 0.0f;
    float linked_z = 0.0f;
    uint16_t index;

    assert(argc == 2);
    bytes = read_file(argv[1], &size);
    assert(ge_stan_collision_open(bytes, size, &surface)
            == GE_STAN_COLLISION_OK);
    assert(surface.tile_count == 2755U);
    assert(surface.point_count == 8366U);
    assert(surface.spawn_tile == 171U);
    assert(surface.spawn_room == 135U);

    assert(ge_stan_collision_get_tile(&surface, surface.spawn_tile, &tile)
            == GE_STAN_COLLISION_OK);
    assert(tile.source_tile_index == 171U);
    assert(tile.tile_id == UINT32_C(0x000631));
    assert(tile.room == 135U && tile.point_count == 3U);
    assert(tile.mid == UINT16_C(0x0fff));
    assert(tile.tail == UINT16_C(0x3012));
    assert(ge_stan_collision_get_point(&surface, tile.first_point, &point)
            == GE_STAN_COLLISION_OK);
    assert(point.x == 4685 && point.y == -25 && point.z == 4054);

    /* Setup pad 33 (p6g1) is seven source units over authored tile 171. */
    assert(ge_stan_collision_ground(&surface, 4719.0f, -18.0f, 3949.0f,
            16.0f, 16.0f, 0.65f, &hit) == GE_STAN_COLLISION_OK);
    assert(hit.source_tile_index == 171U);
    assert(hit.tile_id == UINT32_C(0x000631));
    assert(hit.room == 135U);
    assert(fabsf(hit.y - -25.0f) < 0.001f);
    assert(hit.normal[1] > 0.999f);

    /*
     * Materialize the complete STAN at the exact original link base.  The
     * resulting arena is what the decompiled traversal routines consume.
     */
    assert(ge_stan_native_required_size(&surface, &native_size)
            == GE_STAN_COLLISION_OK);
    assert(native_size == (size_t)UINT32_C(0x15c10));
    native_bytes = malloc(native_size);
    assert(native_bytes != NULL);
    assert(ge_stan_native_materialize(&surface, 0.23363999f, native_bytes,
            native_size, &native) == GE_STAN_COLLISION_OK);
    assert(native.tile_count == surface.tile_count);
    assert((uint8_t *)native.first_tile - native.base
            == GE_STAN_NATIVE_PREFIX_SIZE);
    assert((uint8_t *)native.spawn_tile - native.base == 0x1618);
    assert((uint8_t *)native.terminator - native.base == 0x15c08);
    assert(ge_stan_native_tile_id(native.spawn_tile) == UINT32_C(0x000631));
    assert(native.spawn_tile->room == 135U);
    assert(ge_stan_native_point_count(native.spawn_tile) == 3U);
    assert(native.spawn_tile->points[0].x == 4685);
    assert(native.spawn_tile->points[0].y == -25);
    assert(native.spawn_tile->points[0].z == 4054);
    assert(ge_stan_native_bind_original(&native) == GE_STAN_COLLISION_OK);
    assert(ge_original_stan_match_tile_name(&native, "p6g1")
            == native.spawn_tile);
    assert(ge_original_stan_match_tile_name(&native, "") == NULL);

    /*
     * Dam's authored pad 11 guard (p156g) can see the normal-path point used
     * by the emulator encounter probe.  Keep this exact multi-tile ray as a
     * regression for guard perception through the native STAN arena.
     */
    guard_sight_tile = ge_original_stan_match_tile_name(&native, "p156g");
    assert(guard_sight_tile != NULL);
    assert(ge_stan_collision_ground(&surface, 3777.11f, -9.27f, 4723.39f,
            32.0f, 32.0f, 0.65f, &hit) == GE_STAN_COLLISION_OK);
    bond_sight_tile = ge_stan_native_tile_at(&native, hit.tile_index);
    assert(bond_sight_tile != NULL);
    walk_tile = guard_sight_tile;
    assert(ge_original_stan_test_line_static(&native, &walk_tile,
            3790.0f / native.level_scale, 4745.0f / native.level_scale,
            3777.11f / native.level_scale, 4723.39f / native.level_scale,
            0.0f, 0.0f, 0.0f, 1.0f));
    assert(walk_tile == bond_sight_tile);

    /* Original stanGetPositionYValue works in original runtime coordinates. */
    assert(fabsf(ge_original_stan_get_position_y(&native, native.spawn_tile,
            4719.0f / native.level_scale, 3949.0f / native.level_scale)
            - (-25.0f / native.level_scale)) < 0.01f);
    assert(ge_original_stan_test_point_within_bounds(&native,
            native.spawn_tile, 4719.0f / native.level_scale,
            3949.0f / native.level_scale));

    /* Exercise original linked-tile traversal across an authored edge. */
    spawn = native.spawn_tile;
    for (index = 0U; index < ge_stan_native_point_count(spawn); ++index) {
        spawn_x += (float)spawn->points[index].x;
        spawn_z += (float)spawn->points[index].z;
        if (spawn->points[index].link != 0U) {
            linked_edge = index;
        } else {
            unlinked_edge = index;
        }
    }
    spawn_x = spawn_x / 3.0f / native.level_scale;
    spawn_z = spawn_z / 3.0f / native.level_scale;
    linked_tile = (GeStanNativeTile *)(native.base
        + (size_t)spawn->points[linked_edge].link * GE_STAN_NATIVE_LINK_UNIT);
    for (index = 0U; index < ge_stan_native_point_count(linked_tile); ++index) {
        linked_x += (float)linked_tile->points[index].x;
        linked_z += (float)linked_tile->points[index].z;
    }
    linked_x /= (float)ge_stan_native_point_count(linked_tile)
        * native.level_scale;
    linked_z /= (float)ge_stan_native_point_count(linked_tile)
        * native.level_scale;
    walk_tile = spawn;
    assert(ge_original_stan_walk_tiles_between_points(&native, &walk_tile,
            spawn_x, spawn_z, linked_x, linked_z));
    assert(walk_tile == linked_tile);
    walk_tile = spawn;
    assert(ge_original_stan_test_line_static(&native, &walk_tile,
            spawn_x, spawn_z, linked_x, linked_z, 30.0f, 30.0f, 0.0f,
            1.0f));
    assert(walk_tile == linked_tile);

#if defined(GE_PORT_STAN_DYNAMIC_PROP_COLLISION)
    /* The production guard LOS call supplies nonzero dynamic collision
     * types.  Retain the unchanged stanTestLineUnobstructed branch: an empty
     * room-prop list preserves authored portal traversal, while an authentic
     * collision polygon clips the same otherwise-clear line. */
    dynamic_line_start_x = spawn_x;
    dynamic_line_start_z = spawn_z;
    dynamic_line_end_x = linked_x;
    dynamic_line_end_z = linked_z;
    dynamic_prop_enabled = FALSE;
    walk_tile = spawn;
    assert(stanTestLineUnobstructed((StandTile **)&walk_tile,
            spawn_x, spawn_z, linked_x, linked_z, 1,
            30.0f, 30.0f, 0.0f, 1.0f));
    assert(walk_tile == linked_tile);
    dynamic_prop_enabled = TRUE;
    walk_tile = spawn;
    assert(!stanTestLineUnobstructed((StandTile **)&walk_tile,
            spawn_x, spawn_z, linked_x, linked_z, 1,
            30.0f, 30.0f, 0.0f, 1.0f));

    /* MoveBond follows the line test with this unchanged volume test. The
     * production geometry slice must preserve its dynamic-prop branch too:
     * an empty room list accepts the authored radius, while the same exact
     * polygon is saved as the blocker when present. */
    dynamic_prop_enabled = FALSE;
    walk_tile = spawn;
    assert(stanTestVolume((StandTile **)&walk_tile,
            (spawn_x + linked_x) * 0.5f,
            (spawn_z + linked_z) * 0.5f,
            2.0f, 1, 30.0f, 30.0f) < 0);
    dynamic_prop_enabled = TRUE;
    walk_tile = spawn;
    assert(stanTestVolume((StandTile **)&walk_tile,
            (spawn_x + linked_x) * 0.5f,
            (spawn_z + linked_z) * 0.5f,
            2.0f, 1, 30.0f, 30.0f) == STAN_COLLISION_FOUND);
    assert(stanSavedColl_posData
           == (struct PropRecord *)&dynamic_prop_storage);
    dynamic_prop_enabled = FALSE;
#endif

    /* The original radius traversal accepts the authored tile interior. */
    radius_tile = spawn;
    assert(ge_original_stan_test_radius(&native, &radius_tile, spawn_x,
            spawn_z, 1.0f) == GE_ORIGINAL_STAN_COLLISION_NONE);
    assert(radius_tile == spawn);
    radius_tile = spawn;
    assert(ge_original_stan_test_volume_static(&native, &radius_tile,
            spawn_x, spawn_z, 1.0f, 30.0f, 30.0f)
        == GE_ORIGINAL_STAN_COLLISION_NONE);

    /* Original locus record stays contained and reports no spawn flags. */
    radius_tile = spawn;
    assert(ge_original_stan_test_locus(&native, &radius_tile, spawn_x,
            spawn_z, 1.0f, &locus));
    assert(locus.collision_result == GE_ORIGINAL_STAN_COLLISION_NONE);
    assert(locus.force_crouch == 0);
    assert(locus.linked_ladder == 0);

    /* An authored special-1 tile exercises the original force-crouch bit. */
    for (index = 0U; index < native.tile_count; ++index) {
        GeStanNativeTile *candidate = ge_stan_native_tile_at(&native, index);
        if ((((uint16_t)candidate->mid >> 12U) & UINT16_C(0xf)) == 1U) {
            special_tile = candidate;
            break;
        }
    }
    assert(special_tile != NULL);
    radius_tile = special_tile;
    assert(ge_original_stan_test_locus(&native, &radius_tile,
            (float)special_tile->points[0].x / native.level_scale,
            (float)special_tile->points[0].z / native.level_scale,
            0.0f, &locus));
    assert(locus.collision_result == GE_ORIGINAL_STAN_COLLISION_NONE);
    assert(locus.force_crouch == 1);

    /* A link targeting an authored special-3 tile reports the ladder flag. */
    for (index = 0U; index < native.tile_count && ladder_source == NULL;
            ++index) {
        GeStanNativeTile *candidate = ge_stan_native_tile_at(&native, index);
        unsigned int point_index;

        if ((((uint16_t)candidate->mid >> 12U) & UINT16_C(0xf)) == 1U) {
            continue;
        }
        for (point_index = 0U;
                point_index < ge_stan_native_point_count(candidate);
                ++point_index) {
            const uint16_t link = candidate->points[point_index].link;
            GeStanNativeTile *target;

            if (link == 0U) {
                continue;
            }
            target = (GeStanNativeTile *)(native.base
                + (size_t)link * GE_STAN_NATIVE_LINK_UNIT);
            if ((((uint16_t)target->mid >> 12U) & UINT16_C(0xf)) == 3U) {
                ladder_source = candidate;
                ladder_edge = point_index;
                break;
            }
        }
    }
    assert(ladder_source != NULL);
    {
        const GeStanNativePoint *a = &ladder_source->points[ladder_edge];
        const GeStanNativePoint *b = &ladder_source->points[
            (ladder_edge + 1U) % ge_stan_native_point_count(ladder_source)];

        radius_tile = ladder_source;
        assert(ge_original_stan_test_locus(&native, &radius_tile,
                ((float)a->x + (float)b->x) * 0.5f / native.level_scale,
                ((float)a->z + (float)b->z) * 0.5f / native.level_scale,
                1.0f, &locus));
        assert(locus.linked_ladder == 1);
    }

    /* A radius touching an authored linked edge traverses its neighbour. */
    {
        const GeStanNativePoint *a = &spawn->points[linked_edge];
        const GeStanNativePoint *b =
            &spawn->points[(linked_edge + 1U) % 3U];
        const float edge_mid_x = ((float)a->x + (float)b->x) * 0.5f
            / native.level_scale;
        const float edge_mid_z = ((float)a->z + (float)b->z) * 0.5f
            / native.level_scale;

        radius_tile = spawn;
        assert(ge_original_stan_test_radius(&native, &radius_tile,
                edge_mid_x, edge_mid_z, 1.0f)
            == GE_ORIGINAL_STAN_COLLISION_NONE);

        /* The original height callback can selectively block this link. */
        radius_tile = spawn;
        assert(ge_original_stan_test_locus_edge_above_y(&native,
                &radius_tile, edge_mid_x, edge_mid_z, 1.0f,
                ((float)((a->y < b->y) ? a->y : b->y) - 1.0f)
                    / native.level_scale)
            == GE_ORIGINAL_STAN_COLLISION_FOUND);
        radius_tile = spawn;
        assert(ge_original_stan_test_locus_edge_above_y(&native,
                &radius_tile, edge_mid_x, edge_mid_z, 1.0f,
                ((float)((a->y > b->y) ? a->y : b->y) + 1.0f)
                    / native.level_scale)
            == GE_ORIGINAL_STAN_COLLISION_NONE);
    }

    /* Crossing the spawn triangle's authored zero-link edge is obstructed. */
    {
        const GeStanNativePoint *a = &spawn->points[unlinked_edge];
        const GeStanNativePoint *b = &spawn->points[(unlinked_edge + 1U) % 3U];
        const float edge_mid_x = ((float)a->x + (float)b->x) * 0.5f
            / native.level_scale;
        const float edge_mid_z = ((float)a->z + (float)b->z) * 0.5f
            / native.level_scale;
        const float outside_x = edge_mid_x * 2.0f - spawn_x;
        const float outside_z = edge_mid_z * 2.0f - spawn_z;

        walk_tile = spawn;
        assert(!ge_original_stan_walk_tiles_between_points(&native, &walk_tile,
                spawn_x, spawn_z, outside_x, outside_z));
        assert(walk_tile == spawn);

        walk_tile = spawn;
        assert(!ge_original_stan_test_line_static(&native, &walk_tile,
                spawn_x, spawn_z, outside_x, outside_z, 30.0f, 30.0f,
                0.0f, 1.0f));
        assert(walk_tile == spawn);

        /* The same zero-link edge is a blocking circle collision. */
        radius_tile = spawn;
        assert(ge_original_stan_test_radius(&native, &radius_tile,
                edge_mid_x, edge_mid_z, 1.0f)
            == GE_ORIGINAL_STAN_COLLISION_FOUND);
        assert(radius_tile == spawn);
        radius_tile = spawn;
        assert(ge_original_stan_test_volume_static(&native, &radius_tile,
                edge_mid_x, edge_mid_z, 1.0f, 30.0f, 30.0f)
            == GE_ORIGINAL_STAN_COLLISION_FOUND);
    }

    assert(ge_stan_collision_ground(&surface, 6000.0f, -18.0f, 6000.0f,
            16.0f, 16.0f, 0.65f, &hit) == GE_STAN_COLLISION_NO_GROUND);
    assert(ge_stan_collision_get_tile(&surface, surface.tile_count, &tile)
            == GE_STAN_COLLISION_INVALID_BLOB);
    bytes[0] = 0U;
    assert(ge_stan_collision_open(bytes, size, &surface)
            == GE_STAN_COLLISION_INVALID_BLOB);

    free(native_bytes);
    free(bytes);
    puts("Dam native STAN/original geometry traversal test passed");
    return 0;
}
