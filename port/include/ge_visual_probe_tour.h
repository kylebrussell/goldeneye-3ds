#ifndef GE_VISUAL_PROBE_TOUR_H
#define GE_VISUAL_PROBE_TOUR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_VISUAL_PROBE_LABEL_CAPACITY 40U

typedef struct GeVisualProbeView {
    float position[3];
    float look[3];
    float up[3];
    uint32_t hold_frames;
    uint8_t room;
    char label[GE_VISUAL_PROBE_LABEL_CAPACITY];
} GeVisualProbeView;

typedef struct GeVisualProbeTour {
    GeVisualProbeView *views;
    size_t capacity;
    size_t count;
    uint64_t total_frames;
} GeVisualProbeTour;

typedef enum GeVisualProbeTourStatus {
    GE_VISUAL_PROBE_TOUR_OK = 0,
    GE_VISUAL_PROBE_TOUR_INVALID_ARGUMENT,
    GE_VISUAL_PROBE_TOUR_INVALID_FORMAT,
    GE_VISUAL_PROBE_TOUR_CAPACITY_EXCEEDED
} GeVisualProbeTourStatus;

/* Parses the diagnostic-only GEVIEW1 text format. The camera records are
 * runtime-space inputs to the existing original bondview camera publication;
 * this module never owns or mutates player/gameplay state. */
GeVisualProbeTourStatus ge_visual_probe_tour_parse(
    const char *data,
    size_t data_size,
    GeVisualProbeView *storage,
    size_t storage_capacity,
    GeVisualProbeTour *tour);

/* Resolves one display frame to a held camera. Returns NULL after the finite
 * tour is complete, allowing an automated emulator runner to stop capture. */
const GeVisualProbeView *ge_visual_probe_tour_view_at(
    const GeVisualProbeTour *tour,
    uint64_t display_frame,
    size_t *view_index);

const char *ge_visual_probe_tour_status_name(GeVisualProbeTourStatus status);

#ifdef __cplusplus
}
#endif

#endif
