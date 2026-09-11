#include "ge_3ds_controls.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    TOUCH_NONE = -1, TOUCH_USE, TOUCH_RELOAD, TOUCH_PREV, TOUCH_NEXT,
    TOUCH_WATCH, TOUCH_AIM, TOUCH_INVERT, TOUCH_SLOWER, TOUCH_FASTER,
    TOUCH_SHOULDERS, TOUCH_TAB_CONTROLS, TOUCH_TAB_OBJECTIVES,
    TOUCH_TAB_INVENTORY, TOUCH_PAGE_PREV, TOUCH_PAGE_NEXT, TOUCH_ITEM_FIRST
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
    controls->inventory_request = -1;
    controls->mission_difficulty = -1;
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

void ge_3ds_controls_resume(Ge3dsControls *controls,
    uint32_t held_physical, bool touch_held)
{
    if (controls == NULL) return;
    controls->suppressed_actions = held_physical;
    controls->previous_held = 0U;
    controls->touch_target = TOUCH_NONE;
    controls->was_touching = touch_held;
    controls->inventory_request = -1;
    controls->objectives_refresh_pending = true;
    controls->inventory_refresh_pending = true;
    controls->panel_dirty = true;
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

static int touch_target(uint16_t x, uint16_t y, const Ge3dsControls *controls)
{
    if (x >= 320U || y >= 240U) return TOUCH_NONE;
    if (y < 24U) return x < 106U ? TOUCH_TAB_CONTROLS
        : x < 213U ? TOUCH_TAB_OBJECTIVES : TOUCH_TAB_INVENTORY;
    if (controls->inventory_visible && y >= 40U && y < 200U) {
        size_t row = (size_t)controls->inventory_page * 5U + (y - 40U) / 32U;
        return row < controls->inventory_count && controls->inventory[row].selectable
            ? TOUCH_ITEM_FIRST + (int)row : TOUCH_NONE;
    }
    if (controls->objectives_visible || controls->inventory_visible) {
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
    controls->suppressed_actions &= sample->held;
    held = sample->held & ~(controls->suppressed_shoulders | controls->suppressed_actions);
    if (gameplay && controls->settings.swap_shoulders) {
        uint32_t shoulders = held & (GE_PORT_ACTION_FIRE | GE_PORT_ACTION_AIM);
        held &= ~(GE_PORT_ACTION_FIRE | GE_PORT_ACTION_AIM);
        if (shoulders & GE_PORT_ACTION_FIRE) held |= GE_PORT_ACTION_AIM;
        if (shoulders & GE_PORT_ACTION_AIM) held |= GE_PORT_ACTION_FIRE;
    }
    target = sample->touching ? touch_target(sample->touch_x, sample->touch_y, controls) : TOUCH_NONE;
    if (gameplay && !sample->touching && controls->was_touching
            && controls->touch_target >= TOUCH_ITEM_FIRST) {
        controls->inventory_request = controls->touch_target - TOUCH_ITEM_FIRST;
    }
    if (!gameplay) controls->inventory_request = -1;
    if (!gameplay || !sample->touching) controls->touch_target = TOUCH_NONE;
    else if (!controls->was_touching) {
        controls->touch_target = target;
        switch (target) {
        case TOUCH_TAB_CONTROLS:
        case TOUCH_TAB_OBJECTIVES:
        case TOUCH_TAB_INVENTORY:
            controls->objectives_visible = target == TOUCH_TAB_OBJECTIVES;
            controls->inventory_visible = target == TOUCH_TAB_INVENTORY;
            controls->objectives_refresh_pending = controls->objectives_visible;
            controls->inventory_refresh_pending = controls->inventory_visible;
            controls->panel_dirty = true;
            break;
        case TOUCH_PAGE_PREV:
            if (controls->inventory_visible) {
                if (controls->inventory_page > 0U) controls->inventory_page--;
            } else if (controls->objective_page > 0U) controls->objective_page--;
            controls->panel_dirty = true;
            break;
        case TOUCH_PAGE_NEXT:
            if (controls->inventory_visible) {
                if ((size_t)(controls->inventory_page + 1U) * 5U < controls->inventory_count)
                    controls->inventory_page++;
            } else if ((size_t)(controls->objective_page + 1U) * 5U < controls->objective_count)
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

bool ge_3ds_controls_inventory_due(Ge3dsControls *controls, uint64_t now_ms)
{
    if (controls == NULL || !controls->inventory_visible || controls->was_touching)
        return false;
    if (!controls->inventory_refresh_pending
            && now_ms >= controls->inventory_refresh_ms
            && now_ms - controls->inventory_refresh_ms < 250U) return false;
    controls->inventory_refresh_pending = false;
    controls->inventory_refresh_ms = now_ms;
    return true;
}

void ge_3ds_controls_set_inventory(Ge3dsControls *controls,
    const Ge3dsTouchInventoryItem *items, size_t count)
{
    bool changed;
    size_t index;
    if (controls == NULL) return;
    if (items == NULL) count = 0U;
    if (count > GE_3DS_TOUCH_INVENTORY_MAX) count = GE_3DS_TOUCH_INVENTORY_MAX;
    changed = count != controls->inventory_count;
    for (index = 0U; index < count; ++index) {
        Ge3dsTouchInventoryCache item = {0};
        objective_copy_text(item.text, items[index].text == NULL ? "Item" : items[index].text);
        item.selected = items[index].selected;
        item.selectable = items[index].selectable;
        if (strcmp(item.text, controls->inventory[index].text) != 0
                || item.selected != controls->inventory[index].selected
                || item.selectable != controls->inventory[index].selectable)
            changed = true;
        controls->inventory[index] = item;
    }
    controls->inventory_count = count;
    if ((size_t)controls->inventory_page * 5U >= count) controls->inventory_page = 0U;
    /* Publishing a snapshot expires every captured/released selection, even
     * when identical text hides changed canonical ownership. */
    controls->inventory_request = -1;
    if (controls->touch_target >= TOUCH_ITEM_FIRST) controls->touch_target = TOUCH_NONE;
    if (changed && controls->inventory_visible) controls->panel_dirty = true;
}

int ge_3ds_controls_take_inventory_selection(Ge3dsControls *controls)
{
    int row;
    if (controls == NULL) return -1;
    row = controls->inventory_request;
    controls->inventory_request = -1;
    if (row >= 0) controls->inventory_refresh_pending = true;
    return row;
}

static void draw_inventory(const Ge3dsControls *controls, FILE *stream)
{
    size_t index, first = (size_t)controls->inventory_page * 5U;
    fprintf(stream, "\x1b[2J\x1b[1;1HCONTROLS  | OBJECTIVES | [INVENTORY]"
        "\x1b[4;1HPage %u/%u  Release on an item to equip",
        (unsigned)controls->inventory_page + 1U,
        (unsigned)(controls->inventory_count == 0U ? 1U : (controls->inventory_count + 4U) / 5U));
    if (controls->inventory_count == 0U)
        fprintf(stream, "\x1b[8;1HInventory unavailable.");
    for (index = first; index < controls->inventory_count && index < first + 5U; ++index) {
        const Ge3dsTouchInventoryCache *item = &controls->inventory[index];
        unsigned row = 6U + (unsigned)(index - first) * 4U;
        fprintf(stream, "\x1b[%u;1H+-------------------------------------+"
            "\x1b[%u;2H%.33s%s\x1b[%u;2H%s",
            row, row + 1U, item->text, strlen(item->text) > 33U ? "..." : "",
            row + 2U, item->selected ? "EQUIPPED"
                : item->selectable ? "READY" : "UNAVAILABLE");
    }
    fprintf(stream, "\x1b[26;1H+------------+------------+------------+"
        "\x1b[28;1H|   WATCH    | PREV PAGE  | NEXT PAGE  |"
        "\x1b[30;1H+------------+------------+------------+");
}

static void draw_objectives(const Ge3dsControls *controls, FILE *stream)
{
    static const char *statuses[] = { "INCOMPLETE", "COMPLETE", "FAILED", "UNAVAILABLE" };
    static const char *difficulties[] = { "Agent", "Secret Agent", "00 Agent", "007" };
    size_t index, first = (size_t)controls->objective_page * 5U;
    fprintf(stream, "\x1b[2J\x1b[1;1HCONTROLS  | [OBJECTIVES] | INVENTORY"
        "\x1b[3;1HPage %u/%u      %s",
        (unsigned)controls->objective_page + 1U,
        (unsigned)(controls->objective_count == 0U ? 1U : (controls->objective_count + 4U) / 5U),
        controls->mission_difficulty >= 0 && controls->mission_difficulty <= 3
            ? difficulties[controls->mission_difficulty] : "Difficulty unavailable");
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

void ge_3ds_controls_set_difficulty(Ge3dsControls *controls, int difficulty)
{
    if (controls == NULL) return;
    if (difficulty < 0 || difficulty > 3) difficulty = -1;
    if (controls->mission_difficulty != difficulty) {
        controls->mission_difficulty = (int8_t)difficulty;
        if (controls->objectives_visible) controls->panel_dirty = true;
    }
}

void ge_3ds_controls_set_use_feedback(Ge3dsControls *controls,
    Ge3dsUseFeedback feedback)
{
    if (controls == NULL) return;
    if (feedback < GE_3DS_USE_NONE || feedback > GE_3DS_USE_UNAVAILABLE)
        feedback = GE_3DS_USE_UNAVAILABLE;
    if (controls->use_feedback != feedback) {
        controls->use_feedback = feedback;
        controls->panel_dirty = true;
    }
}

static void draw_use_feedback(const Ge3dsControls *controls, FILE *stream)
{
    const char *message = "";
    switch (controls->use_feedback) {
    case GE_3DS_USE_NONE: return;
    case GE_3DS_USE_NO_TARGET: message = "Face the object and move closer."; break;
    case GE_3DS_USE_ACTIVATED: message = "Activation sent"; break;
    case GE_3DS_USE_LOCKED: message = "Locked"; break;
    default: message = "Use unavailable"; break;
    }
    /* Row 2 is free on controls, objectives and inventory pages. */
    fprintf(stream, "\x1b[2;1H%s", message);
}

void ge_3ds_controls_draw_panel(Ge3dsControls *controls, FILE *stream)
{
    if (controls == NULL || stream == NULL || !controls->panel_dirty) return;
    if (controls->inventory_visible) {
        draw_inventory(controls, stream);
        draw_use_feedback(controls, stream);
        controls->panel_dirty = false;
        return;
    }
    if (controls->objectives_visible) {
        draw_objectives(controls, stream);
        draw_use_feedback(controls, stream);
        controls->panel_dirty = false;
        return;
    }
    fprintf(stream,
        "\x1b[2J\x1b[1;1H[CONTROLS] | OBJECTIVES | INVENTORY"
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
    draw_use_feedback(controls, stream);
    controls->panel_dirty = false;
}
