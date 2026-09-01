#ifndef GE_BLOTTER_MODEL_H
#define GE_BLOTTER_MODEL_H

#include "ge_gbi_pipeline.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_BLOTTER_MODEL_DISPLAY_LIST_PATH \
    "converted/models/blotter1/display_list.bin"
#define GE_BLOTTER_MODEL_VERTICES_PATH \
    "converted/models/blotter1/vertices.bin"
#define GE_BLOTTER_MODEL_PREVIEW_MATRIX_PATH \
    "converted/models/blotter1/matrix_identity.bin"

#define GE_BLOTTER_MODEL_DISPLAY_LIST_BYTES 80U
#define GE_BLOTTER_MODEL_VERTEX_BYTES 64U
#define GE_BLOTTER_MODEL_MATRIX_BYTES 64U
#define GE_BLOTTER_MODEL_TEXTURE_ID 182U
#define GE_BLOTTER_MODEL_TRIANGLE_COUNT 2U
#define GE_BLOTTER_MODEL_VERTEX_COUNT 6U

typedef struct GeBlotterModelBlobs {
    const uint8_t *display_list;
    size_t display_list_size;
    const uint8_t *vertices;
    size_t vertices_size;
    /* N64 fixed-point model-view matrix supplied at draw time. */
    const uint8_t *matrix;
    size_t matrix_size;
} GeBlotterModelBlobs;

typedef struct GeBlotterModelVertex {
    GeGbiVertex source;
    GeGbiProcessedVertex processed;
} GeBlotterModelVertex;

typedef struct GeBlotterModelTriangle {
    GeBlotterModelVertex vertices[3];
} GeBlotterModelTriangle;

typedef enum GeBlotterModelStatus {
    GE_BLOTTER_MODEL_OK = 0,
    GE_BLOTTER_MODEL_INVALID_ARGUMENT,
    GE_BLOTTER_MODEL_INVALID_BLOB_LAYOUT,
    GE_BLOTTER_MODEL_PIPELINE_ERROR,
    GE_BLOTTER_MODEL_UNEXPECTED_GEOMETRY,
    GE_BLOTTER_MODEL_UNEXPECTED_MATERIAL
} GeBlotterModelStatus;

typedef struct GeBlotterModel {
    GeBlotterModelStatus status;
    GeGbiPipelineResult pipeline;
    GeGbiRareTextureState material;
    size_t triangle_count;
    size_t vertex_count;
    GeBlotterModelTriangle triangles[GE_BLOTTER_MODEL_TRIANGLE_COUNT];
} GeBlotterModel;

/*
 * Runs the preserved model display list using segments 5 (commands), 4
 * (vertices), and 3 (the caller's live model-view matrix). The returned
 * triangles own no pointers into the source blobs and remain valid after the
 * caller releases them.
 */
GeBlotterModelStatus ge_blotter_model_build(const GeBlotterModelBlobs *blobs,
                                             GeBlotterModel *model);

const char *ge_blotter_model_status_name(GeBlotterModelStatus status);

#ifdef __cplusplus
}
#endif

#endif
