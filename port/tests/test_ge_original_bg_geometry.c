#include "ge_original_bg_geometry_internal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t shard_count;
void glassCreateShard(coord3d *position, float minimum, float maximum)
{
    for (size_t axis = 0; axis < 3; ++axis) assert(isfinite(position->f[axis]));
    assert(minimum == 0.0f && maximum == 10.0f);
    ++shard_count;
}

static void be16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void be32(uint8_t *p, uint32_t v) { be16(p, (uint16_t)(v >> 16)); be16(p + 2, (uint16_t)v); }
static void command(uint8_t *p, uint32_t a, uint32_t b) { be32(p,a); be32(p+4,b); }
static uint8_t *read_asset(GeAssetPack *pack, const char *path, size_t *size)
{
    const GeAssetPackEntry *entry = ge_asset_pack_find(pack, path);
    uint8_t *bytes;
    size_t read = 0;
    assert(entry != NULL);
    *size = (size_t)entry->data_size;
    bytes = malloc(*size);
    assert(bytes != NULL && ge_asset_pack_read(pack,path,bytes,*size,&read) == GE_ASSET_PACK_OK);
    assert(read == *size);
    return bytes;
}

static void fixture(void)
{
    uint8_t points[48] = {0}, gdl[32] = {0};
    const int16_t coordinates[3][3] = {{0,0,0},{40,0,0},{0,40,0}};
    GeOriginalBgRoomBounds bounds[2] = {0};
    GeDamRoomBlobDescriptor room = {.room_id = 1, .origin = {100,200,300},
        .point_table = points, .point_table_size = sizeof(points),
        .primary_gdl = gdl, .primary_gdl_size = sizeof(gdl)};
    GeOriginalBgGeometryReport report;
    GeOriginalBgGeometryHit hit;
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) be16(points+i*16+j*2,(uint16_t)coordinates[i][j]);
        memset(points+i*16+12, 240, 4);
    }
    for (size_t a = 0; a < 3; ++a) { bounds[1].minimum[a]=room.origin[a]; bounds[1].maximum[a]=room.origin[a]+40; }
    int32_t light = 0;
    while (light < 2698 && !ge_bg_is_light_exact(light)) ++light;
    assert(light < 2698);
    command(gdl,0xc0000000,(uint32_t)light);
    command(gdl+8,0x04200030,0x0e000000);
    command(gdl+16,0xbf000000,0x00000a14);
    command(gdl+24,0xb8000000,0);
    GeOriginalBgGeometry *owner = ge_original_bg_geometry_open_rooms(&room,1,bounds,2,0.5f,&report);
    assert(owner != NULL && report.status == GE_ORIGINAL_BG_GEOMETRY_OK);
    assert(report.vertices == 3 && report.batches == 1 && report.fixtures == 1);
    ge_original_bg_geometry_bind(owner);
    const coord3d *origin=ge_original_bg_geometry_room_origin(1);
    assert(origin == &owner->origins[1] && origin->x == 100 && origin->y == 200 && origin->z == 300);
    assert(ge_original_bg_geometry_room_origin(0) == &owner->origins[0]);
    assert(!ge_original_bg_geometry_room_origin(-1) && !ge_original_bg_geometry_room_origin(2)
        && !ge_original_bg_geometry_room_origin(INT32_MAX));
    owner->status=GE_ORIGINAL_BG_GEOMETRY_INVALID_ASSET;
    assert(!ge_original_bg_geometry_room_origin(1));
    owner->status=GE_ORIGINAL_BG_GEOMETRY_OK;
    owner->origins[1].y=NAN;
    assert(!ge_original_bg_geometry_room_origin(1) && owner->status==GE_ORIGINAL_BG_GEOMETRY_OK);
    owner->origins[1].y=200;
    GeDamRoomBlobDescriptor shifted=room;shifted.origin[0]=1000;
    GeOriginalBgRoomBounds shifted_bounds[2];memcpy(shifted_bounds,bounds,sizeof(bounds));
    shifted_bounds[1].minimum[0]=1000;shifted_bounds[1].maximum[0]=1040;
    GeOriginalBgGeometry *other=ge_original_bg_geometry_open_rooms(&shifted,1,shifted_bounds,2,0.5f,&report);
    assert(other && ge_original_bg_geometry_room_origin(1)==origin);
    ge_original_bg_geometry_bind(other);
    assert(ge_original_bg_geometry_room_origin(1)==&other->origins[1]
        && ge_original_bg_geometry_room_origin(1)->x==1000);
    ge_original_bg_geometry_bind(NULL);assert(!ge_original_bg_geometry_room_origin(0));
    ge_original_bg_geometry_bind(owner);ge_original_bg_geometry_close(other);
    assert(ge_original_bg_geometry_room_origin(1)==origin);
    const float from[3] = {220,420,650}, to[3] = {220,420,550};
    assert(ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    assert(hit.position[0] == 110 && hit.position[1] == 210 && hit.position[2] == 300);
    assert(hit.texture == light && hit.triangle_type == 0);
    assert(hit.triangle != gdl+16 && (uintptr_t)hit.triangle > UINT32_MAX);
    GeDamRoomWorldVertex vertices[3]; GeDamRoomDrawBatch batches[1];
    GeDamRoomSceneStorage storage = {vertices,3,batches,1}; GeDamRoomScene scene;
    assert(ge_dam_rooms_build(&room,1,NULL,&storage,&scene) == GE_DAM_ROOM_OK);
    size_t changed;
    assert(ge_original_bg_geometry_apply_scene(owner,vertices,3,batches,1,&changed) && changed == 0);
    assert(ge_original_bg_geometry_light_break(owner,hit.triangle,hit.triangle_type,1));
    assert(shard_count > 0);
    size_t first_shards = shard_count;
    assert(ge_original_bg_geometry_apply_scene(owner,vertices,3,batches,1,&changed) && changed == 3);
    for (size_t i=0;i<3;++i) assert(vertices[i].source.red == 60 && vertices[i].source.alpha == 60);
    assert(ge_original_bg_geometry_light_break(owner,hit.triangle,0,1) && shard_count == first_shards);
    assert(ge_dam_rooms_build(&room,1,NULL,&storage,&scene) == GE_DAM_ROOM_OK);
    assert(ge_original_bg_geometry_apply_scene(owner,vertices,3,batches,1,&changed) && changed == 3);
    assert(ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    /* Each failed construction must preserve an already bound world. */
    const uint8_t original_gdl[32] = {
        0xc0,0,0,0,0,0,0,201, 4,0x20,0,0x30,0x0e,0,0,0,
        0xbf,0,0,0,0,0,10,20, 0xb8,0,0,0,0,0,0,0};
    for (unsigned mutation=0; mutation<14; ++mutation) {
        GeDamRoomBlobDescriptor invalid = room;
        memcpy(gdl,original_gdl,sizeof(gdl));
        switch (mutation) {
        case 0: invalid.room_id=0; break;
        case 1: invalid.point_table_size=47; break;
        case 2: invalid.primary_gdl_size=31; break;
        case 3: gdl[24]=0; break; /* missing terminator */
        case 4: gdl[12]=15; break; /* wrong vertex segment */
        case 5: gdl[15]=1; break; /* unaligned vertex address */
        case 6: gdl[9]=0x30; break; /* fourth vertex outside point table */
        case 7: gdl[16]=6; break; /* called list */
        case 8: gdl[16]=1; break; /* matrix */
        case 9: gdl[16]=0xfd; break; /* unknown image pointer provenance */
        case 10: gdl[22]=250; break; /* invalid cache index */
        case 11: gdl[6]=15; gdl[7]=255; break; /* invalid Rare ID */
        case 12: invalid.origin[0]=NAN; break;
        case 13: invalid.point_table=NULL; break;
        }
        assert(ge_original_bg_geometry_open_rooms(&invalid,1,bounds,2,0.5f,&report) == NULL);
        assert(report.status != GE_ORIGINAL_BG_GEOMETRY_OK);
        assert(ge_original_bg_geometry_room_count() == 2);
        assert(ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    }
    memcpy(gdl,original_gdl,sizeof(gdl));
    /* Stale/external triangle tokens and malformed scene provenance cannot
     * mutate canonical colours or a caller's retained scene. */
    assert(!ge_original_bg_geometry_light_break(owner,gdl+16,0,1));
    assert(!ge_original_bg_geometry_light_break(owner,hit.triangle,4,1));
    GeDamRoomWorldVertex saved[3]; memcpy(saved,vertices,sizeof(saved));
    ++batches[0].command_address.offset;
    assert(!ge_original_bg_geometry_apply_scene(owner,vertices,3,batches,1,&changed));
    assert(memcmp(saved,vertices,sizeof(saved)) == 0);
    ge_original_bg_geometry_close(owner);
    assert(ge_original_bg_geometry_native_rooms() == NULL);
    assert(ge_original_bg_geometry_room_origin(0) == NULL);
    puts("BG fixture: native pointer hit, original light/shard boundary, idempotence and scene reload pass");
}

static void nearest_and_filter(void)
{
    uint8_t points[96] = {0}, gdl[56] = {0};
    GeOriginalBgRoomBounds bounds[2] = {0};
    bounds[1].maximum[0]=bounds[1].maximum[1]=40; bounds[1].maximum[2]=1;
    const int16_t xy[3][2]={{0,0},{40,0},{0,40}};
    for (size_t v=0;v<6;++v) {
        be16(points+v*16,(uint16_t)xy[v%3][0]);
        be16(points+v*16+2,(uint16_t)xy[v%3][1]);
        be16(points+v*16+4,v<3?1:0);
    }
    /* Front ordinary surface is one unit away. Original dist-4 preference
     * selects the light two units away, retaining TRI4 subtriangle 3. */
    command(gdl,0xc0000000,949);
    command(gdl+8,0x04200030,0x0e000000);
    command(gdl+16,0xbf000000,0x00000a14);
    command(gdl+24,0xc0000000,201);
    command(gdl+32,0x04200030,0x0e000030);
    command(gdl+40,0xb1000200,0x00100000);
    command(gdl+48,0xb8000000,0);
    GeDamRoomBlobDescriptor room={.room_id=1,.point_table=points,.point_table_size=96,.primary_gdl=gdl,.primary_gdl_size=56};
    GeOriginalBgGeometryReport report; GeOriginalBgGeometryHit hit;
    const float from[3]={10,10,2},to[3]={10,10,-2};
    GeOriginalBgGeometry *owner=ge_original_bg_geometry_open_rooms(&room,1,bounds,2,1,&report);
    assert(owner != NULL); ge_original_bg_geometry_bind(owner);
    assert(ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    assert(hit.texture==201 && hit.triangle_type==3 && hit.position[2]==0);
    ge_original_bg_geometry_close(owner);
    command(gdl+24,0xc0000000,0x4fd);
    owner=ge_original_bg_geometry_open_rooms(&room,1,bounds,2,1,&report);
    assert(owner != NULL); ge_original_bg_geometry_bind(owner);
    assert(ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    assert(hit.texture==949 && hit.position[2]==1);
    ge_original_bg_geometry_close(owner);
    command(gdl,0xc0000000,0x4fd);
    owner=ge_original_bg_geometry_open_rooms(&room,1,bounds,2,1,&report);
    assert(owner != NULL); ge_original_bg_geometry_bind(owner);
    memset(&hit,0x5a,sizeof(hit)); GeOriginalBgGeometryHit saved=hit;
    assert(!ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    assert(memcmp(&hit,&saved,sizeof(hit))==0);
    ge_original_bg_geometry_close(owner);
    puts("BG ordering: exact light distance bias, TRI4 slot provenance and Archives filter pass");
}

static void texture_emission(void)
{
    int8_t format=-1, depth=-1;
    ge_bg_texture_emission_begin();
    /* Original WALL_LAMP: CI8, implicit six LODs, 32x32, 191 colours.
     * Repeat emits the palette-load tile while cached render tiles remain
     * unchanged. Reading just the base image format would fail this case. */
    assert(ge_bg_texture_emission_command(0xc0000000,201,&format,&depth));
    assert(format == G_IM_FMT_CI && depth == G_IM_SIZ_8b);
    assert(ge_bg_texture_emission_command(0xc0000000,201,&format,&depth));
    assert(format == G_IM_FMT_RGBA && depth == G_IM_SIZ_4b);
    ge_bg_texture_emission_begin();
    assert(ge_bg_texture_emission_command(0xc0000000,201,&format,&depth));
    assert(format == G_IM_FMT_CI && depth == G_IM_SIZ_8b);
    assert(!ge_bg_texture_emission_command(0xc0000005,201,&format,&depth));
    puts("BG texture provenance: original emitted palette tile wins when render tiles are cached");
}

static size_t exercise_room(GeOriginalBgGeometry *owner, uint32_t id)
{
    const GeOriginalBgRoomGeometry *room = &owner->room_geometry[id];
    size_t rays = 0;
    for (size_t command_index = 0; command_index < room->command_counts[0]; ++command_index) {
        const GeOriginalBgCommandInfo *info = &room->commands[0][command_index];
        for (unsigned triangle = 0; triangle < info->triangle_count; ++triangle) {
            const Vtx *a = &owner->rooms[id].vertices[info->point_indices[triangle*3]];
            const Vtx *b = &owner->rooms[id].vertices[info->point_indices[triangle*3+1]];
            const Vtx *c = &owner->rooms[id].vertices[info->point_indices[triangle*3+2]];
            float ab[3], ac[3], normal[3], from[3], to[3];
            for (size_t axis = 0; axis < 3; ++axis) {
                ab[axis] = (float)b->v.ob[axis]-a->v.ob[axis];
                ac[axis] = (float)c->v.ob[axis]-a->v.ob[axis];
            }
            normal[0]=ab[1]*ac[2]-ab[2]*ac[1];
            normal[1]=ab[2]*ac[0]-ab[0]*ac[2];
            normal[2]=ab[0]*ac[1]-ab[1]*ac[0];
            float length = sqrtf(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
            if (length == 0) continue;
            for (size_t axis = 0; axis < 3; ++axis) {
                float center = ((float)a->v.ob[axis]+b->v.ob[axis]+c->v.ob[axis])/3 + room->blobs.origin[axis];
                from[axis] = (center+normal[axis]/length*2)*owner->inverse_scale;
                to[axis] = (center-normal[axis]/length*2)*owner->inverse_scale;
            }
            GeOriginalBgGeometryHit hit;
            if (ge_original_bg_geometry_hit(owner,from,to,(int32_t)id,&hit)) {
                const GeOriginalBgCommandInfo *actual = ge_bg_command_info((int32_t)id,hit.triangle);
                assert(actual != NULL && actual->texture == hit.texture);
                assert(actual->tile_format == hit.tile_format && actual->tile_size == hit.tile_size);
                for (size_t axis=0;axis<3;++axis) assert(isfinite(hit.position[axis]) && isfinite(hit.normal[axis]));
                ++rays;
            } else if (info->texture != 0x4fd) {
                fprintf(stderr,"eligible centroid missed room %u command %zu triangle %u texture %d\n",
                    id,command_index,triangle,info->texture);
                assert(!"eligible original triangle centroid missed");
            }
            if (ge_bg_is_light_exact(info->texture)) {
                Gfx *token = ge_bg_gfx_at(room->blobs.primary_gdl,(ptrdiff_t)command_index);
                uint32_t type = ((uint8_t *)token)[0] == 0xbf ? 0 : triangle+1;
                assert(ge_original_bg_geometry_light_break(owner,token,type,(int32_t)id));
            }
        }
    }
    GeDamRoomScene scene;
    GeDamRoomSceneStorage storage = {0};
    GeDamRoomBuildLimits limits = {8,32768};
    GeDamRoomStatus status = ge_dam_rooms_build(&room->blobs,1,&limits,&storage,&scene);
    assert(status == GE_DAM_ROOM_OK || status == GE_DAM_ROOM_CAPACITY_EXCEEDED);
    storage.vertex_capacity = scene.required_vertex_count;
    storage.batch_capacity = scene.required_batch_count;
    storage.vertices = calloc(storage.vertex_capacity,sizeof(*storage.vertices));
    storage.batches = calloc(storage.batch_capacity,sizeof(*storage.batches));
    assert(ge_dam_rooms_build(&room->blobs,1,&limits,&storage,&scene) == GE_DAM_ROOM_OK);
    size_t changed = 123;
    assert(ge_original_bg_geometry_apply_scene(owner,storage.vertices,scene.vertex_count,
        storage.batches,scene.batch_count,&changed) && changed == 0);
    free(storage.vertices); free(storage.batches);
    return rays;
}

static void actual_assets(const char *path)
{
    GeAssetPack pack;
    assert(ge_asset_pack_open(&pack,path) == GE_ASSET_PACK_OK);
    for (size_t index = 0; index < GE_STAGE_COUNT + GE_MULTIPLAYER_STAGE_COUNT; ++index) {
        const GeStageAssetDescriptor *stage = index < GE_STAGE_COUNT
            ? ge_stage_asset_descriptor((GeStageId)index)
            : ge_stage_asset_multiplayer(index-GE_STAGE_COUNT);
        size_t size;
        uint8_t *bytes = read_asset(&pack,stage->background_path,&size);
        GeDamWorld world;
        assert(ge_dam_world_parse(bytes,size,&world) == GE_DAM_WORLD_OK);
        free(bytes);
        bytes = read_asset(&pack,stage->room_bounds_path,&size);
        assert(size == 80 + world.room_count*24);
        GeOriginalBgRoomBounds bounds[GE_BG_GEOMETRY_MAX_ROOMS];
        /* Converted asset contract is little-endian; host tests run native LE. */
        memcpy(bounds,bytes+80,world.room_count*24); free(bytes);
        GeOriginalBgGeometryReport report;
        GeOriginalBgGeometry *owner = ge_original_bg_geometry_load(&pack,stage,&world,bounds,&report);
        printf("%s: status %d failed_room %u rooms %zu vertices %zu commands %zu fixtures %zu\n",
            stage->key,report.status,report.failed_room,report.rooms,report.vertices,report.commands,report.fixtures);
        fflush(stdout);
        assert(owner != NULL);
        ge_original_bg_geometry_bind(owner);
        for (size_t room=0;room<world.room_count;++room) {
            const coord3d *origin=ge_original_bg_geometry_room_origin((int32_t)room);
            assert(origin==&owner->origins[room]);
            if (room != 0)
                assert(memcmp(origin->f,world.rooms[room].origin,sizeof(origin->f))==0);
        }
        assert(!ge_original_bg_geometry_room_origin((int32_t)world.room_count));
        size_t rays = 0, shards_before = shard_count;
        for (uint32_t room=1;room<world.room_count;++room) rays += exercise_room(owner,room);
        assert(rays > 0);
        printf("  %zu actual triangle rays hit; %zu original shard calls; %zu owned bytes\n",
            rays,shard_count-shards_before,owner->allocated_bytes);
        ge_original_bg_geometry_close(owner);
    }
    ge_asset_pack_close(&pack);
}

static void validation_boundaries(void)
{
    uint8_t points[144] = {0}, gdl[48] = {0};
    GeDamRoomBlobDescriptor room = {.room_id=1, .point_table=points,
        .point_table_size=96, .primary_gdl=gdl, .primary_gdl_size=48};
    GeOriginalBgRoomBounds bounds[2] = {0};
    GeOriginalBgGeometryReport report;
    GeDamRoomWorldVertex vertices[6], before[6];
    GeDamRoomDrawBatch batches[2];
    GeDamRoomSceneStorage storage = {vertices,6,batches,2};
    GeDamRoomScene scene;
    const int16_t xy[3][2] = {{0,0},{40,0},{0,40}};
    for (size_t i=0; i<9; ++i) {
        be16(points+i*16,(uint16_t)xy[i%3][0]);
        be16(points+i*16+2,(uint16_t)xy[i%3][1]);
        memset(points+i*16+12,i<3 ? 240 : 120,4);
    }
    bounds[1].maximum[0]=bounds[1].maximum[1]=200;
    bounds[1].minimum[2]=-1; bounds[1].maximum[2]=1;
    command(gdl,0xc0000000,201);
    command(gdl+8,0x04200030,0x0e000000);
    command(gdl+16,0xbf000000,0x00000a14);
    command(gdl+24,0x04200030,0x0e000030);
    command(gdl+32,0xbf000000,0x00000a14);
    command(gdl+40,0xb8000000,0);
    GeOriginalBgGeometry *owner=ge_original_bg_geometry_open_rooms(&room,1,bounds,2,1,&report);
    assert(owner && report.status==GE_ORIGINAL_BG_GEOMETRY_OK);
    ge_original_bg_geometry_bind(owner);
    float from[3]={10,10,2}, to[3]={10,10,-2};
    GeOriginalBgGeometryHit hit, saved_hit;
    assert(ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    saved_hit=hit;
    const float invalid[] = {NAN, INFINITY, -INFINITY, 1000001, -1000001};
    for (size_t i=0; i<sizeof(invalid)/sizeof(invalid[0]); ++i) {
        from[0]=invalid[i];
        assert(!ge_original_bg_geometry_hit(owner,from,to,1,&hit));
        assert(memcmp(&hit,&saved_hit,sizeof(hit))==0);
        assert(ge_original_bg_geometry_status(owner)==GE_ORIGINAL_BG_GEOMETRY_OK);
        from[0]=10;
        assert(ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    }
    assert(ge_dam_rooms_build(&room,1,NULL,&storage,&scene)==GE_DAM_ROOM_OK);
    size_t changed=999;
    assert(ge_original_bg_geometry_apply_scene(owner,vertices,6,batches,2,&changed) && !changed);
    assert(vertices[0].source.red==240 && vertices[3].source.red==120);
    memcpy(before,vertices,sizeof(before));
    batches[1].first_vertex=0;
    assert(!ge_original_bg_geometry_apply_scene(owner,vertices,6,batches,2,&changed));
    assert(!changed && memcmp(before,vertices,sizeof(before))==0);
    batches[1].first_vertex=3;
    assert(ge_original_bg_geometry_apply_scene(owner,vertices,6,batches,2,&changed) && !changed);
    /* Descending disjoint ranges also violate the original builder order. */
    GeDamRoomDrawBatch swapped=batches[0]; batches[0]=batches[1]; batches[1]=swapped;
    assert(!ge_original_bg_geometry_apply_scene(owner,vertices,6,batches,2,&changed));
    assert(!changed && memcmp(before,vertices,sizeof(before))==0);
    swapped=batches[0]; batches[0]=batches[1]; batches[1]=swapped;
    assert(ge_original_bg_geometry_light_break(owner,saved_hit.triangle,saved_hit.triangle_type,1));
    assert(ge_original_bg_geometry_apply_scene(owner,vertices,6,batches,2,&changed));
    assert(changed && ge_original_bg_geometry_status(owner)==GE_ORIGINAL_BG_GEOMETRY_OK);
    ge_original_bg_geometry_close(owner);

    /* Source point identity is insufficient when a new owner has moved the
     * authored room. Reject the entire stale scene before its first write. */
    assert(ge_dam_rooms_build(&room,1,NULL,&storage,&scene)==GE_DAM_ROOM_OK);
    memcpy(before,vertices,sizeof(before));
    room.origin[0]=100; bounds[1].minimum[0]=100;
    owner=ge_original_bg_geometry_open_rooms(&room,1,bounds,2,1,&report);
    assert(owner); ge_original_bg_geometry_bind(owner);
    assert(!ge_original_bg_geometry_apply_scene(owner,vertices,6,batches,2,&changed));
    assert(!changed && memcmp(before,vertices,sizeof(before))==0);
    assert(ge_dam_rooms_build(&room,1,NULL,&storage,&scene)==GE_DAM_ROOM_OK);
    assert(ge_original_bg_geometry_apply_scene(owner,vertices,6,batches,2,&changed) && !changed);

    /* A partial VTX reload preserves cache0..2 for rendering but would make
     * the original last-VTX collision algorithm interpret different points.
     * Reject that unsupported stream and preserve the live prior owner. */
    room.origin[0]=0; bounds[1].minimum[0]=0;
    room.point_table_size=144; room.primary_gdl_size=40;
    for (size_t i=3; i<9; ++i)
        be16(points+i*16,(uint16_t)(100+(i%3==1 ? 40 : 0)));
    command(gdl,0xc0000000,949);
    command(gdl+16,0x04230030,0x0e000060);
    command(gdl+24,0xbf000000,0x00000a14);
    command(gdl+32,0xb8000000,0);
    assert(ge_original_bg_geometry_open_rooms(&room,1,bounds,2,1,&report)==NULL);
    assert(report.status==GE_ORIGINAL_BG_GEOMETRY_INVALID_ASSET);
    assert(ge_bg_geometry_current==owner);
    from[0]=to[0]=110;
    assert(ge_original_bg_geometry_hit(owner,from,to,1,&hit));
    assert(ge_original_bg_geometry_apply_scene(owner,vertices,6,batches,2,&changed) && !changed);
    ge_original_bg_geometry_close(owner);
    puts("BG validation: query isolation, ordered independent scene ranges, room origin and native vertex identity passed");
}

int main(int argc, char **argv)
{
    fixture();
    nearest_and_filter();
    texture_emission();
    validation_boundaries();
    if (argc > 1) actual_assets(argv[1]);
    return 0;
}
