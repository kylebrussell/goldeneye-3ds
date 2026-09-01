#include "ge_visual_probe_tour.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int ge_visual_probe_parse_float(const char **cursor,
                                       const char *end,
                                       float *value)
{
    char *number_end;

    while (*cursor < end && isspace((unsigned char)**cursor)) ++*cursor;
    if (*cursor == end) return 0;
    errno = 0;
    *value = strtof(*cursor, &number_end);
    if (number_end == *cursor || number_end > end || errno == ERANGE
            || !isfinite(*value)) return 0;
    *cursor = number_end;
    return 1;
}

static int ge_visual_probe_parse_unsigned(const char **cursor,
                                          const char *end,
                                          unsigned long *value)
{
    char *number_end;

    while (*cursor < end && isspace((unsigned char)**cursor)) ++*cursor;
    if (*cursor == end || **cursor == '-') return 0;
    errno = 0;
    *value = strtoul(*cursor, &number_end, 10);
    if (number_end == *cursor || number_end > end || errno == ERANGE) return 0;
    *cursor = number_end;
    return 1;
}

static int ge_visual_probe_basis_valid(const float look[3], const float up[3])
{
    const float look_length = look[0]*look[0] + look[1]*look[1]
        + look[2]*look[2];
    const float up_length = up[0]*up[0] + up[1]*up[1] + up[2]*up[2];
    const float cross_x = up[1]*look[2] - up[2]*look[1];
    const float cross_y = up[2]*look[0] - up[0]*look[2];
    const float cross_z = up[0]*look[1] - up[1]*look[0];
    const float cross_length = cross_x*cross_x + cross_y*cross_y
        + cross_z*cross_z;

    return look_length > 0.000001f && up_length > 0.000001f
        && cross_length > 0.000001f;
}

static GeVisualProbeTourStatus ge_visual_probe_parse_line(
    const char *line,
    const char *end,
    GeVisualProbeView *view)
{
    const char *cursor = line;
    unsigned long frames;
    unsigned long room;
    size_t axis;
    size_t label_length;

    if (!ge_visual_probe_parse_unsigned(&cursor, end, &frames)
            || !ge_visual_probe_parse_unsigned(&cursor, end, &room)
            || frames == 0UL || frames > UINT32_MAX || room > UINT8_MAX)
        return GE_VISUAL_PROBE_TOUR_INVALID_FORMAT;
    for (axis = 0U; axis < 3U; ++axis)
        if (!ge_visual_probe_parse_float(&cursor, end, &view->position[axis]))
            return GE_VISUAL_PROBE_TOUR_INVALID_FORMAT;
    for (axis = 0U; axis < 3U; ++axis)
        if (!ge_visual_probe_parse_float(&cursor, end, &view->look[axis]))
            return GE_VISUAL_PROBE_TOUR_INVALID_FORMAT;
    for (axis = 0U; axis < 3U; ++axis)
        if (!ge_visual_probe_parse_float(&cursor, end, &view->up[axis]))
            return GE_VISUAL_PROBE_TOUR_INVALID_FORMAT;
    while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
    label_length = (size_t)(end - cursor);
    while (label_length != 0U
            && isspace((unsigned char)cursor[label_length - 1U]))
        --label_length;
    if (label_length == 0U || label_length >= sizeof(view->label)
            || !ge_visual_probe_basis_valid(view->look, view->up))
        return GE_VISUAL_PROBE_TOUR_INVALID_FORMAT;
    memcpy(view->label, cursor, label_length);
    view->label[label_length] = '\0';
    view->hold_frames = (uint32_t)frames;
    view->room = (uint8_t)room;
    return GE_VISUAL_PROBE_TOUR_OK;
}

GeVisualProbeTourStatus ge_visual_probe_tour_parse(
    const char *data,
    size_t data_size,
    GeVisualProbeView *storage,
    size_t storage_capacity,
    GeVisualProbeTour *tour)
{
    static const char signature[] = "GEVIEW1";
    const char *cursor;
    const char *end;
    GeVisualProbeTour candidate = {storage, storage_capacity, 0U, 0U};

    if (data == NULL || data_size == 0U || storage == NULL
            || storage_capacity == 0U || tour == NULL)
        return GE_VISUAL_PROBE_TOUR_INVALID_ARGUMENT;
    memset(tour, 0, sizeof(*tour));
    if (data_size < sizeof(signature)
            || memcmp(data, signature, sizeof(signature) - 1U) != 0
            || (data[sizeof(signature) - 1U] != '\n'
                && data[sizeof(signature) - 1U] != '\r'))
        return GE_VISUAL_PROBE_TOUR_INVALID_FORMAT;
    cursor = data + sizeof(signature);
    end = data + data_size;
    while (cursor < end) {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        const char *trimmed;
        GeVisualProbeTourStatus status;

        if (line_end == NULL) line_end = end;
        trimmed = cursor;
        while (trimmed < line_end
                && isspace((unsigned char)*trimmed)) ++trimmed;
        if (trimmed < line_end && *trimmed != '#') {
            if (candidate.count == candidate.capacity)
                return GE_VISUAL_PROBE_TOUR_CAPACITY_EXCEEDED;
            status = ge_visual_probe_parse_line(
                trimmed, line_end, &candidate.views[candidate.count]);
            if (status != GE_VISUAL_PROBE_TOUR_OK) return status;
            if (UINT64_MAX - candidate.total_frames
                    < candidate.views[candidate.count].hold_frames)
                return GE_VISUAL_PROBE_TOUR_INVALID_FORMAT;
            candidate.total_frames +=
                candidate.views[candidate.count].hold_frames;
            ++candidate.count;
        }
        cursor = line_end < end ? line_end + 1 : end;
    }
    if (candidate.count == 0U) return GE_VISUAL_PROBE_TOUR_INVALID_FORMAT;
    *tour = candidate;
    return GE_VISUAL_PROBE_TOUR_OK;
}

const GeVisualProbeView *ge_visual_probe_tour_view_at(
    const GeVisualProbeTour *tour,
    uint64_t display_frame,
    size_t *view_index)
{
    uint64_t end_frame = 0U;
    size_t index;

    if (tour == NULL || tour->views == NULL || display_frame >= tour->total_frames)
        return NULL;
    for (index = 0U; index < tour->count; ++index) {
        end_frame += tour->views[index].hold_frames;
        if (display_frame < end_frame) {
            if (view_index != NULL) *view_index = index;
            return &tour->views[index];
        }
    }
    return NULL;
}

const char *ge_visual_probe_tour_status_name(GeVisualProbeTourStatus status)
{
    switch (status) {
    case GE_VISUAL_PROBE_TOUR_OK: return "ok";
    case GE_VISUAL_PROBE_TOUR_INVALID_ARGUMENT: return "invalid argument";
    case GE_VISUAL_PROBE_TOUR_INVALID_FORMAT: return "invalid format";
    case GE_VISUAL_PROBE_TOUR_CAPACITY_EXCEEDED: return "capacity exceeded";
    default: return "unknown";
    }
}
