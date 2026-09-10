#ifndef GE_3DS_CONTROLS_H
#define GE_3DS_CONTROLS_H

#include <stddef.h>
#include <stdio.h>
#include "ge_port.h"

typedef struct Ge3dsControlSettings {
    bool dual_analog;
    bool invert_look_y;
    bool swap_shoulders;
    float move_deadzone;
    float look_deadzone;
    float look_sensitivity;
} Ge3dsControlSettings;

typedef struct Ge3dsControlSample {
    int16_t move_x, move_y, look_x, look_y;
    /* Original mapping: right shoulder fire, left shoulder aim. */
    uint32_t held;
    int8_t dpad_x, dpad_y;
    bool touching;
    uint16_t touch_x, touch_y;
} Ge3dsControlSample;

enum { GE_3DS_TOUCH_OBJECTIVE_MAX = 10, GE_3DS_TOUCH_OBJECTIVE_TEXT_SIZE = 128 };
typedef enum Ge3dsTouchObjectiveStatus {
    GE_3DS_TOUCH_OBJECTIVE_INCOMPLETE,
    GE_3DS_TOUCH_OBJECTIVE_COMPLETE,
    GE_3DS_TOUCH_OBJECTIVE_FAILED,
    GE_3DS_TOUCH_OBJECTIVE_UNAVAILABLE
} Ge3dsTouchObjectiveStatus;
typedef struct Ge3dsTouchObjective {
    const char *text;
    Ge3dsTouchObjectiveStatus status;
} Ge3dsTouchObjective;
typedef struct Ge3dsTouchObjectiveCache {
    char text[GE_3DS_TOUCH_OBJECTIVE_TEXT_SIZE];
    Ge3dsTouchObjectiveStatus status;
} Ge3dsTouchObjectiveCache;

typedef struct Ge3dsControls {
    Ge3dsControlSettings settings;
    uint32_t previous_held;
    uint32_t suppressed_shoulders;
    int touch_target;
    bool was_touching;
    bool settings_dirty;
    bool panel_dirty;
    bool objectives_visible;
    bool objectives_refresh_pending;
    uint8_t objective_page;
    size_t objective_count;
    uint64_t objective_refresh_ms;
    Ge3dsTouchObjectiveCache objectives[GE_3DS_TOUCH_OBJECTIVE_MAX];
} Ge3dsControls;

void ge_3ds_controls_init(Ge3dsControls *controls);
/* Missing files preserve defaults. Malformed known values reject the entire
 * file; unknown keys are ignored for forward compatibility. */
bool ge_3ds_controls_load(Ge3dsControls *controls, const char *path);
/* Same-directory temporary file + rename preserves the existing config if a
 * write fails. Call outside the render loop; successful save clears dirty. */
bool ge_3ds_controls_save(Ge3dsControls *controls, const char *path);
/* Frontend mode keeps original axis shaping, ignores touch/settings and
 * supports D-pad menu navigation. Probes/replays should bypass this result
 * exactly as before and pass gameplay=false to suppress touch interaction. */
GePortInput ge_3ds_controls_sample(Ge3dsControls *controls,
    const Ge3dsControlSample *sample, bool gameplay);
/* Poll the original providers only while visible, at most 4 Hz. Opening the
 * page requests an immediate refresh. Stored copies outlive stage language data. */
bool ge_3ds_controls_objectives_due(Ge3dsControls *controls, uint64_t now_ms);
void ge_3ds_controls_set_objectives(Ge3dsControls *controls,
    const Ge3dsTouchObjective *objectives, size_t count);
/* 320x240 / 40x30 console panel; updates only when panel_dirty is set. */
void ge_3ds_controls_draw_panel(Ge3dsControls *controls, FILE *stream);

#endif
