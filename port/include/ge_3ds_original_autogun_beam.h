#ifndef GE_3DS_ORIGINAL_AUTOGUN_BEAM_H
#define GE_3DS_ORIGINAL_AUTOGUN_BEAM_H

#include "ge_original_stage_autogun_lifecycle.h"

#include <stddef.h>

/* The complete solo campaign contains 41 authored autoguns.  A stage uses at
 * most seven, but retaining the authored total makes the publication boundary
 * usable by an all-stage renderer audit without another allocation policy. */
#define GE_3DS_ORIGINAL_AUTOGUN_BEAM_CAPACITY 41U
#define GE_3DS_ORIGINAL_AUTOGUN_BEAM_VERTICES 6U
#define GE_3DS_ORIGINAL_AUTOGUN_BEAM_TEXTURE_SOURCE \
    "FLAREORANGELINE.bin"

typedef struct Ge3dsOriginalAutogunBeamVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
    float red;
    float green;
    float blue;
    float alpha;
} Ge3dsOriginalAutogunBeamVertex;

typedef struct Ge3dsOriginalAutogunBeamTextureUv {
    float top_left[2];
    float top_right[2];
    float bottom_left[2];
    float bottom_right[2];
} Ge3dsOriginalAutogunBeamTextureUv;

typedef struct Ge3dsOriginalAutogunBeamDrawList {
    Ge3dsOriginalAutogunBeamVertex vertices[
        GE_3DS_ORIGINAL_AUTOGUN_BEAM_CAPACITY
            * GE_3DS_ORIGINAL_AUTOGUN_BEAM_VERTICES];
    size_t source_count;
    size_t active_count;
    size_t vertex_count;
} Ge3dsOriginalAutogunBeamDrawList;

/* Converts the read-only canonical BeamRecord publication into the
 * world-space quad emitted by sub_GAME_7F061E18 for ITEM_FNP90.  It performs
 * no object tick, random call, timer advance, damage or sound operation. */
int ge_3ds_original_autogun_beams_build_draw_list(
    const GeOriginalStageAutogunBeamSnapshot *snapshots,
    size_t snapshot_count, const float viewer_position[3],
    const Ge3dsOriginalAutogunBeamTextureUv *texture_uv,
    Ge3dsOriginalAutogunBeamDrawList *draw_list);

#endif
