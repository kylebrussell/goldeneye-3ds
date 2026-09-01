#include "ge_original_gunbarrel.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <stddef.h>
#include <string.h>

#include "ge_original_gunbarrel_contract.inc"

extern signed short sins(unsigned short x);
extern void createGunbarrelRenderHole(void *vertices, int32_t count);

_Static_assert(sizeof(GeOriginalGunbarrelHoleVertex) == 16U,
    "gunbarrel vertex ABI");

static void ge_original_gunbarrel_emit_bond(
    GeOriginalGunbarrelState *state, GeOriginalGunbarrelFrame *frame)
{
    int tick;
    frame->layers |= GE_ORIGINAL_GUNBARREL_LAYER_BOND;
    frame->bond_animation_ticks =
        GE_ORIGINAL_GUNBARREL_BOND_TICKS_PER_FRAME;
    for (tick = 0; tick < GE_ORIGINAL_GUNBARREL_BOND_TICKS_PER_FRAME;
            ++tick) {
        if (state->animation_tick >= 0) {
            ++state->animation_tick;
            if (state->animation_tick
                    == GE_ORIGINAL_GUNBARREL_ANIM_START_TICK)
                frame->animation_start = 1U;
            if (state->animation_tick
                    == GE_ORIGINAL_GUNBARREL_ANIM_SPEEDUP_TICK)
                frame->animation_speedup = 1U;
        }
        if (state->animation_tick == GE_ORIGINAL_GUNBARREL_FIRE_TICK)
            frame->fire_shot = 1U;
    }
}

const char *ge_original_gunbarrel_contract_sha256(void)
{
    return GE_ORIGINAL_GUNBARREL_CONTRACT_SHA256;
}

void ge_original_gunbarrel_assets(GeOriginalGunbarrelAssets *assets)
{
    if (assets == NULL)
        return;
    memset(assets, 0, sizeof(*assets));
    assets->body_model = BODY_Brosnan_Tuxedo;
    assets->head_model = BODY_Male_Pierce_Bond_Tuxedo;
    assets->gun_model = PROP_CHRWPPK;
    assets->model_scale = GE_ORIGINAL_GUNBARREL_MODEL_SCALE;
    assets->animation_play_speed = 0.5f;
    assets->animation_translation_scale = 1.0f;
    assets->walk_animation_frame_backstep =
        GE_ORIGINAL_GUNBARREL_WALK_FRAME_BACKSTEP;
    assets->camera_position[0] = GE_ORIGINAL_GUNBARREL_CAMERA_X;
    assets->camera_position[1] = GE_ORIGINAL_GUNBARREL_CAMERA_Y;
    assets->camera_position[2] = GE_ORIGINAL_GUNBARREL_CAMERA_Z;
    assets->camera_direction[0] = GE_ORIGINAL_GUNBARREL_CAMERA_DIR_X;
    assets->camera_direction[1] = GE_ORIGINAL_GUNBARREL_CAMERA_DIR_Y;
    assets->camera_direction[2] = GE_ORIGINAL_GUNBARREL_CAMERA_DIR_Z;
    assets->camera_up[0] = GE_ORIGINAL_GUNBARREL_CAMERA_UP_X;
    assets->camera_up[1] = GE_ORIGINAL_GUNBARREL_CAMERA_UP_Y;
    assets->camera_up[2] = GE_ORIGINAL_GUNBARREL_CAMERA_UP_Z;
    assets->field_of_view_degrees = GE_ORIGINAL_GUNBARREL_FIELD_OF_VIEW;
    assets->perspective_aspect = GE_ORIGINAL_GUNBARREL_PERSPECTIVE_ASPECT;
    assets->perspective_near = GE_ORIGINAL_GUNBARREL_PERSPECTIVE_NEAR;
    assets->perspective_far = GE_ORIGINAL_GUNBARREL_PERSPECTIVE_FAR;
    assets->logical_width = GE_ORIGINAL_GUNBARREL_LOGICAL_WIDTH;
    assets->logical_height = GE_ORIGINAL_GUNBARREL_LOGICAL_HEIGHT;
    assets->native_width = GE_ORIGINAL_GUNBARREL_NATIVE_WIDTH;
    assets->native_height = GE_ORIGINAL_GUNBARREL_NATIVE_HEIGHT;
    assets->sight_width = GE_ORIGINAL_GUNBARREL_SIGHT_WIDTH;
    assets->sight_height = GE_ORIGINAL_GUNBARREL_SIGHT_HEIGHT;
    assets->sight_y = GE_ORIGINAL_GUNBARREL_SIGHT_Y;
    assets->backdrop_offset_x = GE_ORIGINAL_GUNBARREL_BACKDROP_OFFSET_X;
    assets->backdrop_offset_y = GE_ORIGINAL_GUNBARREL_BACKDROP_OFFSET_Y;
    assets->backdrop_scale_x = GE_ORIGINAL_GUNBARREL_BACKDROP_SCALE_X;
    assets->backdrop_scale_y = GE_ORIGINAL_GUNBARREL_BACKDROP_SCALE_Y;
    assets->blood_width = GE_ORIGINAL_GUNBARREL_BLOOD_WIDTH;
    assets->blood_height = GE_ORIGINAL_GUNBARREL_BLOOD_HEIGHT;
    assets->blood_red = 150U;
    assets->blood_green = 0U;
    assets->blood_blue = 0U;
    assets->blood_alpha = 180U;
}

uint32_t ge_original_gunbarrel_build_hole(
    GeOriginalGunbarrelHoleVertex *vertices, uint32_t capacity)
{
    if (vertices == NULL
            || capacity < GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT)
        return 0U;
    createGunbarrelRenderHole(vertices, 0x1e);
    return GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT;
}

static void ge_original_gunbarrel_transform_hole(
    const GeOriginalGunbarrelHoleVertex *source,
    GeOriginalGunbarrelLayerHoleVertex *destination,
    float center_x, float center_y, float scale_x, float scale_y,
    int authored_shade)
{
    uint32_t index;
    for (index = 0U; index < GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT;
            ++index) {
        destination[index].x = center_x + (float)source[index].x * scale_x;
        destination[index].y = center_y + (float)source[index].y * scale_y;
        destination[index].red = authored_shade
            ? source[index].red : UINT8_C(0xe6);
        destination[index].green = authored_shade
            ? source[index].green : UINT8_C(0xe6);
        destination[index].blue = authored_shade
            ? source[index].blue : UINT8_C(0xe6);
        /* Both original paths render opaque: the generated hole vertex alpha
         * is unused by G_CC_PRIMITIVE and G_CC_SHADE respectively. */
        destination[index].alpha = UINT8_MAX;
    }
}

uint32_t ge_original_gunbarrel_build_frame_holes(
    const GeOriginalGunbarrelFrame *frame,
    GeOriginalGunbarrelLayerHoleVertex *vertices, uint32_t capacity)
{
    GeOriginalGunbarrelHoleVertex authored[
        GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT];
    uint32_t required = 0U;
    if (frame == NULL) return 0U;
    if ((frame->layers & GE_ORIGINAL_GUNBARREL_LAYER_MOVING_HOLE) != 0U)
        required = GE_ORIGINAL_GUNBARREL_MAX_LAYER_HOLE_VERTICES;
    else if ((frame->layers
                & GE_ORIGINAL_GUNBARREL_LAYER_SIGHT_BACKDROP) != 0U)
        required = GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT;
    if (required == 0U) return 0U;
    if (vertices == NULL || capacity < required
            || ge_original_gunbarrel_build_hole(authored,
                GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT) == 0U)
        return 0U;
    if ((frame->layers & GE_ORIGINAL_GUNBARREL_LAYER_MOVING_HOLE) != 0U) {
        ge_original_gunbarrel_transform_hole(authored, vertices,
            frame->title_x, frame->title_y, 1.0f, 1.0f, 0);
        ge_original_gunbarrel_transform_hole(authored,
            vertices + GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT,
            frame->transition_x, frame->transition_y,
            1.0f, 1.0f, 0);
    } else {
        ge_original_gunbarrel_transform_hole(authored, vertices,
            frame->title_x + GE_ORIGINAL_GUNBARREL_BACKDROP_OFFSET_X,
            frame->title_y + GE_ORIGINAL_GUNBARREL_BACKDROP_OFFSET_Y,
            GE_ORIGINAL_GUNBARREL_BACKDROP_SCALE_X,
            GE_ORIGINAL_GUNBARREL_BACKDROP_SCALE_Y, 1);
    }
    return required;
}

int ge_original_gunbarrel_sight_rect(
    const GeOriginalGunbarrelFrame *frame,
    GeOriginalGunbarrelSightRect *rect)
{
    int32_t x_offset;
    if (frame == NULL || rect == NULL
            || (frame->layers
                & GE_ORIGINAL_GUNBARREL_LAYER_SNIPER_SIGHT) == 0U)
        return 0;
    /* insert_sniper_sight_eye_intro uses floorFloat(viGetX()*g_TitleX/1280)
     * while the title VI is in its authored 440x330 high-resolution mode. */
    x_offset = (int32_t)floorf(
        (float)GE_ORIGINAL_GUNBARREL_NATIVE_WIDTH * frame->title_x
            / (float)GE_ORIGINAL_GUNBARREL_LOGICAL_WIDTH);
    rect->destination_left = (int16_t)(x_offset < 0 ? 0 : x_offset);
    rect->destination_top = (int16_t)GE_ORIGINAL_GUNBARREL_SIGHT_Y;
    rect->destination_right =
        (int16_t)GE_ORIGINAL_GUNBARREL_NATIVE_WIDTH;
    rect->destination_bottom = (int16_t)(GE_ORIGINAL_GUNBARREL_SIGHT_Y
        + GE_ORIGINAL_GUNBARREL_SIGHT_HEIGHT);
    rect->source_left = (int16_t)(x_offset < 0 ? -x_offset : 0);
    rect->source_top = 0;
    rect->source_right = (int16_t)(rect->source_left
        + rect->destination_right - rect->destination_left);
    rect->source_bottom =
        (int16_t)GE_ORIGINAL_GUNBARREL_SIGHT_HEIGHT;
    return rect->destination_left < rect->destination_right;
}

void ge_original_gunbarrel_reset(GeOriginalGunbarrelState *state)
{
    if (state == NULL)
        return;
    memset(state, 0, sizeof(*state));
    state->mode = 2U;
    state->title_x = GE_ORIGINAL_GUNBARREL_X_INITIAL;
    state->title_y = GE_ORIGINAL_GUNBARREL_Y_INITIAL;
    state->transition_x = GE_ORIGINAL_GUNBARREL_TRANSITION_X_INITIAL;
    state->transition_y = GE_ORIGINAL_GUNBARREL_TRANSITION_Y_INITIAL;
    state->transition_delay = GE_ORIGINAL_GUNBARREL_WORD_INITIAL;
}

GeOriginalGunbarrelTickResult ge_original_gunbarrel_tick(
    GeOriginalGunbarrelState *state,
    GeOriginalGunbarrelBloodTick blood_tick,
    void *blood_context,
    GeOriginalGunbarrelFrame *frame)
{
    uint8_t mode;
    if (state == NULL || frame == NULL)
        return GE_ORIGINAL_GUNBARREL_TICK_INVALID;
    memset(frame, 0, sizeof(*frame));
    mode = state->mode;
    frame->mode = mode;
    frame->title_x = state->title_x;
    frame->title_y = state->title_y;
    frame->transition_x = state->transition_x;
    frame->transition_y = state->transition_y;
    if (state->complete || mode == 9U) {
        state->complete = 1U;
        frame->mode_after = 9U;
        frame->sequence_complete = 1U;
        return GE_ORIGINAL_GUNBARREL_TICK_COMPLETE;
    }
    /* title.c owns a real 42-frame decoded blood stream in modes 4 and 5.
     * Refuse to advance rather than silently replacing that dependency. */
    if ((mode == 4U || mode == 5U) && blood_tick == NULL) {
        frame->mode_after = mode;
        return GE_ORIGINAL_GUNBARREL_TICK_NEEDS_BLOOD_DECODER;
    }

    switch ((int)mode - 2) {
    case 0:
        frame->layers = GE_ORIGINAL_GUNBARREL_LAYER_CLEAR_BLACK
            | GE_ORIGINAL_GUNBARREL_LAYER_MOVING_HOLE;
        state->title_x += GE_ORIGINAL_GUNBARREL_X_INCREMENT;
        if (state->transition_delay < 0) {
            state->transition_delay = 200;
            state->transition_x = state->title_x - 12.0f;
        } else {
            state->transition_delay -= 6;
        }
        if (state->title_x > GE_ORIGINAL_GUNBARREL_X_LIMIT) {
            ++state->mode;
            state->title_x = GE_ORIGINAL_GUNBARREL_X_RESTART;
        }
        break;
    case 1:
        frame->layers = GE_ORIGINAL_GUNBARREL_LAYER_SNIPER_SIGHT
            | GE_ORIGINAL_GUNBARREL_LAYER_SIGHT_BACKDROP;
        if (state->title_x < 600.0f)
            ge_original_gunbarrel_emit_bond(state, frame);
        state->title_x -= GE_ORIGINAL_GUNBARREL_X_DECREMENT;
        if (state->title_x <= GE_ORIGINAL_GUNBARREL_X_END) {
            ++state->mode;
            state->intro_counter = GE_ORIGINAL_GUNBARREL_HOLD_FRAMES;
        }
        break;
    case 2:
        frame->layers = GE_ORIGINAL_GUNBARREL_LAYER_SNIPER_SIGHT
            | GE_ORIGINAL_GUNBARREL_LAYER_SIGHT_BACKDROP;
        ge_original_gunbarrel_emit_bond(state, frame);
        --state->intro_counter;
        if (state->intro_counter < 0) {
            ++state->mode;
            (void)blood_tick(blood_context, 0);
            frame->blood_reset = 1U;
            state->intro_counter = 1;
        }
        break;
    case 3:
    {
        int blood_complete = 0;
        --state->intro_counter;
        if (state->intro_counter == 0) {
            blood_complete = blood_tick(blood_context, 1);
            frame->blood_advance = 1U;
            state->intro_counter = 2;
        }
        frame->layers = GE_ORIGINAL_GUNBARREL_LAYER_SNIPER_SIGHT
            | GE_ORIGINAL_GUNBARREL_LAYER_SIGHT_BACKDROP
            | GE_ORIGINAL_GUNBARREL_LAYER_BLOOD_IMAGE;
        ge_original_gunbarrel_emit_bond(state, frame);
        if (blood_complete) {
            ++state->mode;
            state->sway_angle = 0U;
            state->transition_x = state->title_x;
            state->intro_counter = 0;
        }
        break;
    }
    case 4:
        state->sway_angle = (uint16_t)(state->sway_angle
            + GE_ORIGINAL_GUNBARREL_SINE_INCREMENT);
        ++state->intro_counter;
        state->title_x = ((float)sins(state->sway_angle) * 64.0f
            / 32768.0f) + state->transition_x;
        frame->title_x = state->title_x;
        frame->layers = GE_ORIGINAL_GUNBARREL_LAYER_SNIPER_SIGHT
            | GE_ORIGINAL_GUNBARREL_LAYER_SIGHT_BACKDROP
            | GE_ORIGINAL_GUNBARREL_LAYER_BLOOD_COLOUR;
        ge_original_gunbarrel_emit_bond(state, frame);
        if (state->intro_counter >= GE_ORIGINAL_GUNBARREL_SWAY_FRAMES) {
            state->intro_counter = 0;
            ++state->mode;
        }
        break;
    case 5:
        state->sway_angle = (uint16_t)(state->sway_angle
            + GE_ORIGINAL_GUNBARREL_SINE_INCREMENT);
        state->title_x = ((float)sins(state->sway_angle) * 64.0f
            / 32768.0f) + state->transition_x;
        frame->title_x = state->title_x;
        frame->layers = GE_ORIGINAL_GUNBARREL_LAYER_SNIPER_SIGHT
            | GE_ORIGINAL_GUNBARREL_LAYER_SIGHT_BACKDROP
            | GE_ORIGINAL_GUNBARREL_LAYER_BLOOD_COLOUR
            | GE_ORIGINAL_GUNBARREL_LAYER_FADE_BLACK;
        ge_original_gunbarrel_emit_bond(state, frame);
        state->intro_counter += GE_ORIGINAL_GUNBARREL_FADE_INCREMENT;
        frame->fade_alpha = (uint8_t)state->intro_counter;
        if (state->intro_counter >= GE_ORIGINAL_GUNBARREL_FADE_LIMIT) {
            state->intro_counter = 0;
            ++state->mode;
        }
        break;
    case 6:
        frame->layers = GE_ORIGINAL_GUNBARREL_LAYER_CLEAR_BLACK;
        if (state->intro_counter++
                >= GE_ORIGINAL_GUNBARREL_BLACK_FRAMES) {
            state->intro_counter = 0;
            ++state->mode;
            state->complete = 1U;
        }
        break;
    default:
        return GE_ORIGINAL_GUNBARREL_TICK_INVALID;
    }
    ++state->rendered_frames;
    frame->mode_after = state->mode;
    frame->sequence_complete = state->complete;
    return state->complete ? GE_ORIGINAL_GUNBARREL_TICK_COMPLETE
        : GE_ORIGINAL_GUNBARREL_TICK_RUNNING;
}
