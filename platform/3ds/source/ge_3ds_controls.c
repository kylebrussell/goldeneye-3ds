#include "ge_3ds_controls.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    TOUCH_NONE = -1, TOUCH_USE, TOUCH_RELOAD, TOUCH_PREV, TOUCH_NEXT,
    TOUCH_WATCH, TOUCH_AIM, TOUCH_INVERT, TOUCH_SLOWER, TOUCH_FASTER,
    TOUCH_SHOULDERS, TOUCH_PAGE_TOGGLE, TOUCH_PAGE_PREV, TOUCH_PAGE_NEXT
};

void ge_3ds_controls_init(Ge3dsControls *controls)
{
    if (controls == NULL) return;
    memset(controls, 0, sizeof(*controls));
    controls->settings.dual_analog = true;
    controls->settings.move_deadzone = 0.12f;
    controls->settings.look_deadzone = 0.12f;
    controls->settings.look_sensitivity = 1.0f;
    controls->touch_target = TOUCH_NONE;
    controls->panel_dirty = true;
}

static bool valid_settings(const Ge3dsControlSettings *settings)
{
    return isfinite(settings->move_deadzone)
        && settings->move_deadzone >= 0.0f && settings->move_deadzone <= 0.45f
        && isfinite(settings->look_deadzone)
        && settings->look_deadzone >= 0.0f && settings->look_deadzone <= 0.45f
        && isfinite(settings->look_sensitivity)
        && settings->look_sensitivity >= 0.25f
        && settings->look_sensitivity <= 2.0f;
}

bool ge_3ds_controls_load(Ge3dsControls *controls, const char *path)
{
    Ge3dsControlSettings settings;
    char line[256];
    FILE *stream;
    bool ok = true;
    if (controls == NULL || path == NULL) return false;
    stream = fopen(path, "rb");
    if (stream == NULL) return false;
    settings = controls->settings;
    while (fgets(line, sizeof(line), stream) != NULL) {
        char key[64], value[64], extra;
        char *comment = strchr(line, '#');
        int count;
        if (strchr(line, '\n') == NULL && !feof(stream)) {
            ok = false; break;
        }
        if (comment != NULL) *comment = '\0';
        count = sscanf(line, " %63s %63s %c", key, value, &extra);
        if (count <= 0) continue;
        if (strcmp(key, "dual_analog") == 0
                || strcmp(key, "invert_look_y") == 0
                || strcmp(key, "swap_shoulders") == 0) {
            bool enabled;
            if (count != 2 || (strcmp(value, "0") != 0
                    && strcmp(value, "1") != 0)) { ok = false; break; }
            enabled = value[0] == '1';
            if (strcmp(key, "dual_analog") == 0) settings.dual_analog = enabled;
            else if (strcmp(key, "invert_look_y") == 0)
                settings.invert_look_y = enabled;
            else settings.swap_shoulders = enabled;
        } else if (strcmp(key, "move_deadzone") == 0
                || strcmp(key, "look_deadzone") == 0
                || strcmp(key, "look_sensitivity") == 0) {
            char *end;
            float number;
            if (count != 2) { ok = false; break; }
            number = strtof(value, &end);
            if (*end != '\0' || end == value || !isfinite(number)) {
                ok = false; break;
            }
            if (strcmp(key, "move_deadzone") == 0) settings.move_deadzone = number;
            else if (strcmp(key, "look_deadzone") == 0) settings.look_deadzone = number;
            else settings.look_sensitivity = number;
        }
    }
    if (ferror(stream)) ok = false;
    if (fclose(stream) != 0) ok = false;
    if (!ok || !valid_settings(&settings)) return false;
    controls->settings = settings;
    controls->settings_dirty = false;
    controls->panel_dirty = true;
    return true;
}

bool ge_3ds_controls_save(Ge3dsControls *controls, const char *path)
{
    char *temporary;
    FILE *stream;
    bool ok;
    size_t length;
    if (controls == NULL || path == NULL || !valid_settings(&controls->settings))
        return false;
    length = strlen(path);
    if (length > 4096U) return false;
    temporary = malloc(length + sizeof(".tmp"));
    if (temporary == NULL) return false;
    memcpy(temporary, path, length);
    memcpy(temporary + length, ".tmp", sizeof(".tmp"));
    stream = fopen(temporary, "wb");
    if (stream == NULL) { free(temporary); return false; }
    ok = fprintf(stream,
        "# GoldenEye 3DS controls; deadzones 0..0.45, sensitivity 0.25..2\n"
        "dual_analog %d\ninvert_look_y %d\nswap_shoulders %d\n"
        "move_deadzone %.6f\nlook_deadzone %.6f\nlook_sensitivity %.6f\n",
        controls->settings.dual_analog, controls->settings.invert_look_y,
        controls->settings.swap_shoulders, controls->settings.move_deadzone,
        controls->settings.look_deadzone, controls->settings.look_sensitivity) > 0;
    if (fclose(stream) != 0) ok = false;
    if (ok) ok = rename(temporary, path) == 0;
    if (!ok) (void)remove(temporary);
    free(temporary);
    if (ok) controls->settings_dirty = false;
    return ok;
}

static float axis(int16_t raw, float deadzone, float sensitivity, bool gameplay)
{
    float value = (float)raw / 156.0f;
    float magnitude = fabsf(value);
    if (magnitude <= deadzone) return 0.0f;
    if (gameplay) magnitude = (magnitude - deadzone) / (1.0f - deadzone);
    magnitude *= sensitivity;
    if (magnitude > 1.0f) magnitude = 1.0f;
    return value < 0.0f ? -magnitude : magnitude;
}

static int touch_target(uint16_t x, uint16_t y, bool objectives_visible)
{
    if (x >= 320U || y >= 240U) return TOUCH_NONE;
    if (y < 24U) return TOUCH_PAGE_TOGGLE;
    if (objectives_visible) {
        if (y < 200U) return TOUCH_NONE;
        return x < 106U ? TOUCH_WATCH
            : x < 213U ? TOUCH_PAGE_PREV : TOUCH_PAGE_NEXT;
    }
    if (y >= 40U && y < 88U) return x < 160U ? TOUCH_USE : TOUCH_RELOAD;
    if (y >= 88U && y < 136U) return x < 160U ? TOUCH_PREV : TOUCH_NEXT;
    if (y >= 136U && y < 176U) return x < 160U ? TOUCH_WATCH : TOUCH_AIM;
    if (y >= 184U) return TOUCH_INVERT + x / 80U;
    return TOUCH_NONE;
}

static uint32_t touch_action(int target)
{
    static const uint32_t actions[] = {
        GE_PORT_ACTION_USE, GE_PORT_ACTION_RELOAD, GE_PORT_ACTION_PREV_WEAPON,
        GE_PORT_ACTION_NEXT_WEAPON, GE_PORT_ACTION_PAUSE, GE_PORT_ACTION_AIM
    };
    return target >= TOUCH_USE && target <= TOUCH_AIM ? actions[target] : 0U;
}

GePortInput ge_3ds_controls_sample(Ge3dsControls *controls,
    const Ge3dsControlSample *sample, bool gameplay)
{
    GePortInput result = {0};
    uint32_t held;
    int target;
    if (controls == NULL || sample == NULL) return result;
    result.move_x = axis(sample->move_x, gameplay ? controls->settings.move_deadzone : 0.12f, 1.0f, gameplay);
    result.move_y = axis(sample->move_y, gameplay ? controls->settings.move_deadzone : 0.12f, 1.0f, gameplay);
    result.look_x = axis(sample->look_x, gameplay ? controls->settings.look_deadzone : 0.12f,
        gameplay ? controls->settings.look_sensitivity : 1.0f, gameplay);
    result.look_y = axis(sample->look_y, gameplay ? controls->settings.look_deadzone : 0.12f,
        gameplay ? controls->settings.look_sensitivity : 1.0f, gameplay);
    if (gameplay && controls->settings.invert_look_y) result.look_y = -result.look_y;
    if (result.move_x == 0.0f && sample->dpad_x != 0)
        result.move_x = sample->dpad_x < 0 ? -1.0f : 1.0f;
    if (result.move_y == 0.0f && sample->dpad_y != 0)
        result.move_y = sample->dpad_y < 0 ? -1.0f : 1.0f;
    controls->suppressed_shoulders &= sample->held;
    held = sample->held & ~controls->suppressed_shoulders;
    if (gameplay && controls->settings.swap_shoulders) {
        uint32_t shoulders = held & (GE_PORT_ACTION_FIRE | GE_PORT_ACTION_AIM);
        held &= ~(GE_PORT_ACTION_FIRE | GE_PORT_ACTION_AIM);
        if (shoulders & GE_PORT_ACTION_FIRE) held |= GE_PORT_ACTION_AIM;
        if (shoulders & GE_PORT_ACTION_AIM) held |= GE_PORT_ACTION_FIRE;
    }
    target = sample->touching ? touch_target(sample->touch_x, sample->touch_y, controls->objectives_visible) : TOUCH_NONE;
    if (!gameplay || !sample->touching) controls->touch_target = TOUCH_NONE;
    else if (!controls->was_touching) {
        controls->touch_target = target;
        switch (target) {
        case TOUCH_PAGE_TOGGLE:
            controls->objectives_visible = !controls->objectives_visible;
            controls->objectives_refresh_pending = controls->objectives_visible;
            controls->panel_dirty = true;
            break;
        case TOUCH_PAGE_PREV:
            if (controls->objective_page > 0U) controls->objective_page--;
            controls->panel_dirty = true;
            break;
        case TOUCH_PAGE_NEXT:
            if ((size_t)(controls->objective_page + 1U) * 5U < controls->objective_count)
                controls->objective_page++;
            controls->panel_dirty = true;
            break;
        case TOUCH_INVERT:
            controls->settings.invert_look_y = !controls->settings.invert_look_y;
            break;
        case TOUCH_SHOULDERS:
            controls->settings.swap_shoulders = !controls->settings.swap_shoulders;
            /* Changing handedness while aiming must not turn that held
             * shoulder into a shot. Rearm it only after release. */
            controls->suppressed_shoulders = sample->held
                & (GE_PORT_ACTION_FIRE | GE_PORT_ACTION_AIM);
            held &= ~(GE_PORT_ACTION_FIRE | GE_PORT_ACTION_AIM);
            break;
        case TOUCH_SLOWER:
            controls->settings.look_sensitivity = fmaxf(0.25f, controls->settings.look_sensitivity - 0.25f);
            break;
        case TOUCH_FASTER:
            controls->settings.look_sensitivity = fminf(2.0f, controls->settings.look_sensitivity + 0.25f);
            break;
        default: break;
        }
        if (target >= TOUCH_INVERT && target <= TOUCH_SHOULDERS) {
            controls->settings_dirty = true;
            controls->panel_dirty = true;
        }
    } else if (target != controls->touch_target) {
        /* Leaving the captured button cancels it until the stylus lifts. */
        controls->touch_target = TOUCH_NONE;
    }
    held |= touch_action(controls->touch_target);
    result.held = held;
    result.pressed = held & ~controls->previous_held;
    controls->previous_held = held;
    controls->was_touching = sample->touching;
    return result;
}

bool ge_3ds_controls_objectives_due(Ge3dsControls *controls, uint64_t now_ms)
{
    if (controls == NULL || !controls->objectives_visible) return false;
    if (!controls->objectives_refresh_pending
            && now_ms >= controls->objective_refresh_ms
            && now_ms - controls->objective_refresh_ms < 250U) return false;
    controls->objectives_refresh_pending = false;
    controls->objective_refresh_ms = now_ms;
    return true;
}

static void objective_copy_text(char *destination, const char *source)
{
    size_t used = 0U;
    bool space = false;
    if (source == NULL) source = "Objective text unavailable";
    while (*source != '\0' && used + 1U < GE_3DS_TOUCH_OBJECTIVE_TEXT_SIZE) {
        unsigned char ch = (unsigned char)*source++;
        /* Normalize authored line breaks and omit console escape/control
         * bytes. The native console font supports printable ASCII only. */
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
            space = used != 0U;
            continue;
        }
        if (ch < 32U || ch > 126U) continue;
        if (space && used + 2U < GE_3DS_TOUCH_OBJECTIVE_TEXT_SIZE)
            destination[used++] = ' ';
        space = false;
        destination[used++] = (char)ch;
    }
    destination[used] = '\0';
}

void ge_3ds_controls_set_objectives(Ge3dsControls *controls,
    const Ge3dsTouchObjective *objectives, size_t count)
{
    size_t index;
    bool changed;
    if (controls == NULL) return;
    if (objectives == NULL) count = 0U;
    if (count > GE_3DS_TOUCH_OBJECTIVE_MAX) count = GE_3DS_TOUCH_OBJECTIVE_MAX;
    changed = count != controls->objective_count;
    for (index = 0U; index < count; ++index) {
        Ge3dsTouchObjectiveCache entry = {0};
        objective_copy_text(entry.text, objectives[index].text);
        entry.status = objectives[index].status;
        if ((unsigned)entry.status > GE_3DS_TOUCH_OBJECTIVE_UNAVAILABLE)
            entry.status = GE_3DS_TOUCH_OBJECTIVE_UNAVAILABLE;
        if (strcmp(entry.text, controls->objectives[index].text) != 0
                || entry.status != controls->objectives[index].status)
            changed = true;
        controls->objectives[index] = entry;
    }
    controls->objective_count = count;
    if ((size_t)controls->objective_page * 5U >= count) controls->objective_page = 0U;
    if (changed && controls->objectives_visible) controls->panel_dirty = true;
}

static void draw_objectives(const Ge3dsControls *controls, FILE *stream)
{
    static const char *statuses[] = { "INCOMPLETE", "COMPLETE", "FAILED", "UNAVAILABLE" };
    size_t index, first = (size_t)controls->objective_page * 5U;
    fprintf(stream, "\x1b[2J\x1b[1;1H< CONTROLS  /  MISSION OBJECTIVES"
        "\x1b[3;1HPage %u/%u      Live mission status",
        (unsigned)controls->objective_page + 1U,
        (unsigned)(controls->objective_count == 0U ? 1U : (controls->objective_count + 4U) / 5U));
    if (controls->objective_count == 0U)
        fprintf(stream, "\x1b[7;1HNo objectives available.");
    for (index = first; index < controls->objective_count && index < first + 5U; ++index) {
        const Ge3dsTouchObjectiveCache *entry = &controls->objectives[index];
        char second_line[37] = {0};
        const char *second;
        size_t length = strlen(entry->text), split = 36U;
        unsigned row = 5U + (unsigned)(index - first) * 4U;
        if (length > 36U) {
            while (split > 0U && entry->text[split] != ' ') split--;
            if (split == 0U) split = 36U;
            second = entry->text + split;
            while (*second == ' ') second++;
            length = strlen(second);
            memcpy(second_line, second, length < 36U ? length : 36U);
            if (length > 36U) memcpy(second_line + 33U, "...", 3U);
            fprintf(stream, "\x1b[%u;1H%u. %.*s\x1b[%u;4H%s",
                row, (unsigned)index + 1U, (int)split, entry->text, row + 1U, second_line);
        } else fprintf(stream, "\x1b[%u;1H%u. %s", row, (unsigned)index + 1U, entry->text);
        fprintf(stream, "\x1b[%u;4H[%s]", row + 2U, statuses[entry->status]);
    }
    fprintf(stream, "\x1b[26;1H+------------+------------+------------+"
        "\x1b[28;1H|   WATCH    | PREV PAGE  | NEXT PAGE  |"
        "\x1b[30;1H+------------+------------+------------+");
}

void ge_3ds_controls_draw_panel(Ge3dsControls *controls, FILE *stream)
{
    if (controls == NULL || stream == NULL || !controls->panel_dirty) return;
    if (controls->objectives_visible) {
        draw_objectives(controls, stream);
        controls->panel_dirty = false;
        return;
    }
    fprintf(stream,
        "\x1b[2J\x1b[1;1HFIELD CONTROLS  /  OBJECTIVES >"
        "\x1b[3;1H%s"
        "\x1b[4;1H%s fire / %s aim    B use / Y reload"
        "\x1b[5;1HAim + Circle down: crouch / zoom"
        "\x1b[6;1H+------------------+------------------+"
        "\x1b[9;1H|       USE        |      RELOAD      |"
        "\x1b[12;1H+------------------+------------------+"
        "\x1b[15;1H|   PREV WEAPON     |   NEXT WEAPON    |"
        "\x1b[18;1H+------------------+------------------+"
        "\x1b[20;1H|      WATCH       |    HOLD AIM      |"
        "\x1b[22;1H+------------------+------------------+"
        "\x1b[23;1HLook speed: %.2fx    Pitch: %s"
        "\x1b[24;1H+--------+---------+---------+--------+"
        "\x1b[26;1H| INVERT | SLOWER  | FASTER  | SWAP   |"
        "\x1b[27;1H| PITCH  |  LOOK   |  LOOK   | L / R  |"
        "\x1b[29;1H+--------+---------+---------+--------+",
        controls->settings.dual_analog ? "Circle: move   C-Stick: look"
            : "Circle: move/turn  C: strafe/look",
        controls->settings.swap_shoulders ? "L/ZL" : "R/ZR",
        controls->settings.swap_shoulders ? "R/ZR" : "L/ZL",
        controls->settings.look_sensitivity,
        controls->settings.invert_look_y ? "inverted" : "normal");
    controls->panel_dirty = false;
}
