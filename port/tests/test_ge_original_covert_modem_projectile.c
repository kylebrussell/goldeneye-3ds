#include "ge_original_covert_modem_object.h"
#include "ge_original_covert_modem_projectile.h"
#include "ge_original_default_object.h"
#include "ge_original_default_object_internal.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_prop_state.h"
#include "ge_original_bond_input_provider.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bg.h"
#include "game/bondview.h"
#include "game/player.h"

bg_portal_data_entry *g_BgPortals;
extern f32 room_data_float1;
s32 D_80048380 = 42;

/* The exact portal polygon routine must not be reached for a terminator-only
 * world. This test provider makes that invariant executable. */
s32 sub_GAME_7F0B9F14(s32 portal, coord3d *from, coord3d *to)
{
    (void)portal;
    (void)from;
    (void)to;
    assert(!"portal test reached in terminator-only world");
    return 0;
}

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;
    unsigned char *bytes;
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

static int tile_rgb(void *context, void *stan, float x, float z,
                    uint8_t rgb[3])
{
    (void)context;
    (void)stan;
    assert(isfinite(x) && isfinite(z));
    rgb[0] = 160U;
    rgb[1] = 144U;
    rgb[2] = 128U;
    return 1;
}

int main(int argc, char **argv)
{
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
    GeOriginalPropState prop_state;
    GeOriginalDefaultObjectPrepared prepared;
    GeOriginalDefaultObjectProviders object_providers = {0};
    GeOriginalCovertModemProjectileStats projectile_stats;
    GeOriginalGameplayServiceStats service_stats;
    GeOriginalCovertModemProjectileStatus launch_status;
    struct player player;
    struct player_data permissions;
    PropRecord *player_prop;
    ObjectRecord *object;
    Projectile *projectile;
    bg_portal_data_entry terminator = {0};
    coord3d target;
    coord3d velocity = {{1.25f, 5.0f, -2.5f}};
    Mtxf launch_matrix;
    Mtxf orientation;
    unsigned char *collision_bytes;
    unsigned char *native_bytes;
    size_t collision_size;
    size_t native_size;
    float x = 0.0f;
    float z = 0.0f;
    uint16_t point;

    assert(argc == 2);
    collision_bytes = read_file(argv[1], &collision_size);
    assert(ge_stan_collision_open(collision_bytes, collision_size, &surface)
           == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface, &native_size)
           == GE_STAN_COLLISION_OK);
    native_bytes = malloc(native_size);
    assert(native_bytes != NULL);
    assert(ge_stan_native_materialize(&surface, 0.23363999f,
               native_bytes, native_size, &native) == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_bind_original(&native) == GE_STAN_COLLISION_OK);
    for (point = 0U; point < ge_stan_native_point_count(native.spawn_tile);
            point++) {
        x += native.spawn_tile->points[point].x;
        z += native.spawn_tile->points[point].z;
    }
    x /= ge_stan_native_point_count(native.spawn_tile) * native.level_scale;
    z /= ge_stan_native_point_count(native.spawn_tile) * native.level_scale;

    memset(&player, 0, sizeof(player));
    memset(&permissions, 0, sizeof(permissions));
    assert(ge_original_prop_state_reset(&prop_state, 137U));
    player_prop = ge_original_prop_state_allocate_player(&prop_state);
    assert(player_prop != NULL);
    player_prop->type = PROP_TYPE_PLAYER;
    player_prop->stan = (StandTile *)native.spawn_tile;
    player_prop->pos.x = x;
    player_prop->pos.y = ge_original_stan_get_position_y(
        &native, native.spawn_tile, x, z) + 175.0f;
    player_prop->pos.z = z;
    player.prop = player_prop;
    player.stanHeight = 175.0f;
    player.cameramode = 1;
    player.cameratile = (StandTile *)native.spawn_tile;
    player.pos3 = player_prop->pos;
    player.field_488.current_tile_ptr_for_portals =
        (StandTile *)native.spawn_tile;
    ge_original_bond_input_bind_player(&player, &permissions);
    ge_original_bond_input_provider_reset_normal_dam();

    object_providers.get_tile_rgb = tile_rgb;
    ge_original_default_object_bind(&object_providers, &prepared);
    ge_original_gameplay_services_reset();
    room_data_float1 = 0.23363999f;
    g_BgPortals = &terminator;
    object = ge_original_covert_modem_object_create(PROP_CHRBUG, ITEM_BUG);
    assert(object != NULL);
    assert(ge_original_covert_modem_object_prepare_throw(object, 0U));

    target = player_prop->pos;
    memset(&launch_matrix, 0, sizeof(launch_matrix));
    memset(&orientation, 0, sizeof(orientation));
    launch_matrix.m[0][0] = launch_matrix.m[1][1] =
        launch_matrix.m[2][2] = launch_matrix.m[3][3] = 1.0f;
    orientation.m[0][0] = orientation.m[1][1] =
        orientation.m[2][2] = orientation.m[3][3] = 1.0f;
    launch_status = ge_original_covert_modem_projectile_launch(
        object, &target, &launch_matrix, &velocity, &orientation);
    if (launch_status != GE_ORIGINAL_COVERT_MODEM_PROJECTILE_OK)
        fprintf(stderr, "covert-modem launch status: %d\n", launch_status);
    assert(launch_status == GE_ORIGINAL_COVERT_MODEM_PROJECTILE_OK);
    assert(chrobjGetBboxFromObjectRecord(object) != NULL);
    projectile = object->projectile;
    assert(projectile != NULL);
    assert(object->runtime_bitflags & RUNTIMEBITFLAG_00000080);
    assert(object->prop->stan == (StandTile *)native.spawn_tile);
    assert(object->prop->pos.x == target.x);
    assert(object->prop->pos.z == target.z);
    assert(ge_original_prop_state_room_contains(
        (int16_t)native.spawn_tile->room, object->prop));
    assert(projectile->ownerprop == player_prop);
    assert(projectile->obj == object);
    assert(projectile->speed.x == velocity.x);
    assert(projectile->speed.y == velocity.y);
    assert(projectile->speed.z == velocity.z);
    assert(projectile->flags & PROJECTILEFLAG_STICKY);
    assert(projectile->flags & 2U);
    assert(projectile->unk8C == 0.1f);
    assert(projectile->refreshrate == 60);
    assert(projectile->unkE8 == D_80048380);
    assert(projectile->unkCC[0] == native.spawn_tile->room);
    assert(projectile->unkCC[1] == UINT8_MAX);
    assert(ge_original_prop_state_is_active(object->prop));
    assert(ge_original_prop_state_is_enabled(object->prop));
    ge_original_covert_modem_projectile_snapshot(&projectile_stats);
    assert(projectile_stats.launch_calls == 1U);
    assert(projectile_stats.successful_launches == 1U);
    assert(projectile_stats.pool_allocations == 1U);
    assert(projectile_stats.pool_exhaustions == 0U);
    assert(projectile_stats.sound_events == 1U);
    ge_original_gameplay_services_snapshot(&service_stats);
    assert(service_stats.sound_play_calls == 1U);
    assert(service_stats.sound_parameter_events == 1U);
    puts("original covert-modem projectile/STAN/BBOX-room launch: ok");

    free(native_bytes);
    free(collision_bytes);
    return 0;
}
