#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_dam_objective_models.h"
#include "ge_original_default_object_internal.h"
#include "ge_original_dam_monitor.h"
#include "ge_original_dam_monitor_render.h"

s32 g_ClockTimer;
f32 g_GlobalTimerDelta;

u32 randomGetNext(void)
{
    return 0U;
}

static u8 test_state;

u8 ge_port_default_object_state(ObjectRecord *object)
{
    assert(object != NULL);
    return test_state;
}

void ge_port_default_object_set_state(ObjectRecord *object, u8 state)
{
    assert(object != NULL);
    test_state = state;
}

static void read_exact(const char *path, uint8_t *bytes, size_t size)
{
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fread(bytes, 1, size, file) == size);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
}

static void exercise_object_init(ModelFileHeader *header, Model *model,
                                 int16_t model_id)
{
    ObjectRecord object;
    PropRecord prop;
    unsigned char collision[0x50];

    memset(&object, 0, sizeof(object));
    memset(&prop, 0xa5, sizeof(prop));
    memset(collision, 0x5a, sizeof(collision));
    object.obj = model_id;
    object.flags = PROPFLAG_00000100;
    test_state = 0;
    assert(ge_original_objInitPreallocatedSlice(
               &object, header, &prop, model,
               GE_ORIGINAL_DAM_OBJECTIVE_PITEM_SCALE, collision) == &prop);
    assert(object.model == model && object.prop == &prop);
    assert((void *)object.ptr_allocated_collisiondata_block == collision);
    assert((test_state & PROPSTATE_EXT_COLISION_BLOCK) != 0U);
    assert(object.projectile == NULL && object.maxdamage == 0.0f);
    assert(object.shadecol.r == 0U && object.shadecol.g == 0U
           && object.shadecol.b == 0U && object.shadecol.a == 0U);
    assert(object.nextcol.r == 0U && object.nextcol.g == 0U
           && object.nextcol.b == 0U && object.nextcol.a == 0U);
    assert(model->unk00 == -1 && model->chr == NULL);
    assert(fabsf(model->scale - 0.1f) < 0.0001f);
    assert(prop.type == PROP_TYPE_OBJ && prop.obj == &object);
    assert(prop.pos.x == 0.0f && prop.pos.y == 0.0f && prop.pos.z == 0.0f);
    assert(prop.stan == NULL);
}

int main(int argc, char **argv)
{
    uint8_t modembox_blob[GE_ORIGINAL_MODEMBOX_BLOB_SIZE];
    uint8_t satdish_blob[GE_ORIGINAL_SATDISH_BLOB_SIZE];
    GeOriginalDamObjectiveModels runtime;
    GeOriginalDamObjectiveModelsStatus status;
    void *header;
    void *model;
    float scale;
    MonitorRecord monitor;
    GeOriginalDamMonitorRenderSnapshot monitor_render;
    unsigned tick;

    assert(argc == 3);
    read_exact(argv[1], modembox_blob, sizeof(modembox_blob));
    read_exact(argv[2], satdish_blob, sizeof(satdish_blob));
    status = ge_original_modembox_model_relocate(
        &runtime.modembox, modembox_blob, sizeof(modembox_blob));
    assert(status == GE_ORIGINAL_DAM_OBJECTIVE_MODELS_OK);
    status = ge_original_satdish_model_relocate(
        &runtime.satdish, satdish_blob, sizeof(satdish_blob));
    assert(status == GE_ORIGINAL_DAM_OBJECTIVE_MODELS_OK);

    assert(runtime.modembox.nodes[0].Opcode == MODELNODE_OPCODE_GROUP);
    assert(runtime.modembox.nodes[1].Opcode == MODELNODE_OPCODE_BBOX);
    assert(runtime.modembox.nodes[2].Opcode == MODELNODE_OPCODE_DLCOLLISION);
    assert(runtime.modembox.nodes[2].Next == &runtime.modembox.nodes[3]);
    assert(runtime.modembox.nodes[3].Opcode == MODELNODE_OPCODE_SWITCH);
    assert(runtime.modembox.nodes[3].Child == &runtime.modembox.nodes[4]);
    assert(runtime.modembox.header.Switches[0] == &runtime.modembox.nodes[4]);
    assert(runtime.modembox.header.numSwitches == 1);
    assert(runtime.modembox.header.numRecords
           == GE_ORIGINAL_MODEMBOX_RW_WORD_COUNT);
    assert(runtime.modembox.switch_data.Switch.RwDataIndex == 2U);
    assert(runtime.modembox.collision_data[1].DisplayListCollisions.RwDataIndex
           == 3U);
    assert(runtime.modembox.rwdata_words[2] == 1U);
    assert(runtime.modembox.collision_data[0].DisplayListCollisions.numVertices
           == 56);
    assert(runtime.modembox.collision_data[1].DisplayListCollisions.numVertices
           == 4);
    assert(runtime.modembox.bbox_data.BoundingBox.Bounds.xmin == -253.0f);
    assert(runtime.modembox.bbox_data.BoundingBox.Bounds.ymax == 217.0f);
    assert(runtime.modembox.vertices[0].coord.x == 253);
    assert(runtime.modembox.screen_vertices[0].coord.x == 200);
    assert((const uint8_t *)runtime.modembox.collision_data[0]
               .DisplayListCollisions.Primary
           == modembox_blob + GE_ORIGINAL_MODEMBOX_PRIMARY_GDL_OFFSET);
    assert((const uint8_t *)runtime.modembox.collision_data[1]
               .DisplayListCollisions.Primary
           == modembox_blob + GE_ORIGINAL_MODEMBOX_SCREEN_GDL_OFFSET);

    /* Execute the unchanged monitor interpreter on the ROM model's authored
     * Switches[0] node. Image 5 selects monitor texture slot 29, performs its
     * exact one-tick green transition and begins the upward scroll/pause. */
    assert(ge_original_dam_monitor_initialize(&monitor, 5)
        == GE_ORIGINAL_DAM_MONITOR_OK);
    g_ClockTimer = 1;
    g_GlobalTimerDelta = 1.0f;
    assert(ge_original_dam_monitor_render_tick(
        &runtime.modembox.model, &monitor, UINT32_C(0x10001002), 0U,
        &monitor_render));
    assert(monitor_render.switch_node == &runtime.modembox.nodes[4]);
    assert(monitor_render.image_slot == 29U);
    assert(monitor_render.texture_id == 2224U);
    assert(monitor_render.width == 32U && monitor_render.height == 32U);
    assert(monitor_render.format == G_IM_FMT_I);
    assert(monitor_render.depth == G_IM_SIZ_8b);
    assert(monitor_render.texture_mode == 8U);
    assert(monitor_render.texture_alpha_mode == 1U);
    assert(monitor_render.command_offset == 8U);
    assert(monitor_render.pause60 == 120);
    assert(monitor_render.vertices[0].x == 200);
    assert(monitor_render.vertices[0].s == 1024);
    assert(monitor_render.vertices[0].t == 1017);
    assert(monitor_render.vertices[2].s == 0);
    assert(monitor_render.vertices[2].t == -6);
    assert(monitor_render.vertices[0].red == 0U);
    assert(monitor_render.vertices[0].green == 128U);
    assert(monitor_render.vertices[0].blue == 0U);
    assert(monitor_render.vertices[0].alpha == 255U);
    for (tick = 0U; tick < 121U; tick++)
        assert(ge_original_dam_monitor_render_tick(
            &runtime.modembox.model, &monitor, UINT32_C(0x10001002), 0U,
            &monitor_render));
    assert(monitor_render.command_offset == 13U);
    assert(monitor_render.pause60 == 120);
    assert(!ge_original_dam_monitor_render_tick(
        &runtime.satdish.model, &monitor, 0U, 0U, &monitor_render));

    assert(runtime.satdish.nodes[0].Opcode == MODELNODE_OPCODE_GROUP);
    assert(runtime.satdish.nodes[1].Opcode == MODELNODE_OPCODE_BBOX);
    assert(runtime.satdish.nodes[2].Opcode == MODELNODE_OPCODE_DLCOLLISION);
    assert(runtime.satdish.header.numRecords
           == GE_ORIGINAL_SATDISH_RW_WORD_COUNT);
    assert(runtime.satdish.collision_data.DisplayListCollisions.numVertices
           == 92);
    assert(runtime.satdish.collision_data.DisplayListCollisions
               .numCollisionVertices == 42);
    assert(runtime.satdish.bbox_data.BoundingBox.Bounds.xmin == -2275.0f);
    assert(runtime.satdish.bbox_data.BoundingBox.Bounds.zmax == 1225.0f);
    assert(runtime.satdish.vertices[0].coord.z == 875);
    assert((const uint8_t *)runtime.satdish.collision_data
               .DisplayListCollisions.Primary
           == satdish_blob + GE_ORIGINAL_SATDISH_PRIMARY_GDL_OFFSET);
    assert((const uint8_t *)runtime.satdish.collision_data
               .DisplayListCollisions.Secondary
           == satdish_blob + GE_ORIGINAL_SATDISH_SECONDARY_GDL_OFFSET);

    assert(ge_original_dam_objective_models_model_load(
        &runtime, GE_ORIGINAL_MODEMBOX_MODEL_ID));
    assert(ge_original_dam_objective_models_resolve_instance(
        &runtime, GE_ORIGINAL_MODEMBOX_MODEL_ID, &header, &model, &scale));
    assert(header == &runtime.modembox.header
           && model == &runtime.modembox.model && scale == 0.1f);
    exercise_object_init(header, model, GE_ORIGINAL_MODEMBOX_MODEL_ID);
    assert(ge_original_dam_objective_models_resolve_instance(
        &runtime, GE_ORIGINAL_SATDISH_MODEL_ID, &header, &model, &scale));
    assert(header == &runtime.satdish.header
           && model == &runtime.satdish.model && scale == 0.1f);
    exercise_object_init(header, model, GE_ORIGINAL_SATDISH_MODEL_ID);

    modembox_blob[0x34U] = 0xffU;
    assert(ge_original_modembox_model_relocate(
               &runtime.modembox, modembox_blob, sizeof(modembox_blob))
           == GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_LAYOUT);
    satdish_blob[0x18U] = 0xffU;
    assert(ge_original_satdish_model_relocate(
               &runtime.satdish, satdish_blob, sizeof(satdish_blob))
           == GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_LAYOUT);
    puts("Dam objective models relocated; exact Switches[0] monitor animation "
         "and objInit lifecycle passed");
    return 0;
}
