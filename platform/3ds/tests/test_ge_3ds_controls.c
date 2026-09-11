#include "ge_3ds_controls.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static GePortInput touch(Ge3dsControls *controls, int x, int y)
{
    Ge3dsControlSample sample = {0};
    sample.touching = x >= 0;
    sample.touch_x = (uint16_t)x;
    sample.touch_y = (uint16_t)y;
    return ge_3ds_controls_sample(controls, &sample, true);
}

static void test_axes(void)
{
    Ge3dsControls controls;
    Ge3dsControlSample sample = {0};
    GePortInput result;
    int raw;
    float previous = 0.0f;
    ge_3ds_controls_init(&controls);
    for (raw = 0; raw <= 32767; raw++) {
        sample.look_x = (int16_t)raw;
        result = ge_3ds_controls_sample(&controls, &sample, true);
        assert(isfinite(result.look_x));
        assert(result.look_x >= previous && result.look_x <= 1.0f);
        previous = result.look_x;
    }
    sample.look_x = -32768;
    assert(ge_3ds_controls_sample(&controls, &sample, true).look_x == -1.0f);
    sample.look_x = 18;
    assert(ge_3ds_controls_sample(&controls, &sample, true).look_x == 0.0f);
    sample.look_x = 19;
    assert(ge_3ds_controls_sample(&controls, &sample, true).look_x < 0.01f);
    controls.settings.invert_look_y = true;
    controls.settings.look_sensitivity = 0.5f;
    sample.look_y = 156;
    result = ge_3ds_controls_sample(&controls, &sample, true);
    assert(result.look_y == -0.5f);
    result = ge_3ds_controls_sample(&controls, &sample, false);
    assert(result.look_y == 1.0f); /* Menu shaping ignores gameplay settings. */
    sample.dpad_x = -1;
    sample.dpad_y = 1;
    result = ge_3ds_controls_sample(&controls, &sample, false);
    assert(result.move_x == -1.0f && result.move_y == 1.0f);
    sample.move_x = 156;
    assert(ge_3ds_controls_sample(&controls, &sample, false).move_x == 1.0f);
}

static void test_actions(void)
{
    Ge3dsControls controls;
    Ge3dsControlSample sample = {0};
    GePortInput result;
    const uint32_t actions[] = { GE_PORT_ACTION_USE, GE_PORT_ACTION_RELOAD,
        GE_PORT_ACTION_PREV_WEAPON, GE_PORT_ACTION_NEXT_WEAPON,
        GE_PORT_ACTION_PAUSE, GE_PORT_ACTION_AIM };
    const int ys[] = {60, 60, 100, 100, 150, 150};
    size_t index;
    ge_3ds_controls_init(&controls);
    for (index = 0; index < sizeof(actions)/sizeof(actions[0]); ++index) {
        (void)touch(&controls, -1, 0);
        result = touch(&controls, index % 2 == 0 ? 60 : 220, ys[index]);
        assert(result.held == actions[index] && result.pressed == actions[index]);
        result = touch(&controls, index % 2 == 0 ? 60 : 220, ys[index]);
        assert(result.held == actions[index] && result.pressed == 0);
    }
    /* Sliding to a new button cancels, and sliding back cannot rearm it. */
    assert(touch(&controls, 60, 60).held == 0);
    assert(touch(&controls, 220, 150).held == 0);
    (void)touch(&controls, -1, 0);
    assert(touch(&controls, 320, 60).held == 0);
    assert(touch(&controls, 60, 60).held == 0);
    (void)touch(&controls, -1, 0);
    /* Touch + physical duplicate use is a single rising edge. */
    sample.held = GE_PORT_ACTION_USE;
    result = ge_3ds_controls_sample(&controls, &sample, true);
    assert(result.pressed == GE_PORT_ACTION_USE);
    sample.touching = true; sample.touch_x = 60; sample.touch_y = 60;
    result = ge_3ds_controls_sample(&controls, &sample, true);
    assert(result.pressed == 0 && result.held == GE_PORT_ACTION_USE);
    sample.held = 0;
    assert(ge_3ds_controls_sample(&controls, &sample, true).held == GE_PORT_ACTION_USE);
    sample.touching = false;
    assert(ge_3ds_controls_sample(&controls, &sample, true).held == 0);
    controls.settings.swap_shoulders = true;
    sample.held = GE_PORT_ACTION_FIRE;
    assert(ge_3ds_controls_sample(&controls, &sample, true).held == GE_PORT_ACTION_AIM);
    assert(ge_3ds_controls_sample(&controls, &sample, false).held == GE_PORT_ACTION_FIRE);
}

static void test_touch_boundaries(void)
{
    static const struct { int x, y; uint32_t action; } cases[] = {
        {0, 39, 0}, {0, 40, GE_PORT_ACTION_USE},
        {159, 87, GE_PORT_ACTION_USE}, {160, 87, GE_PORT_ACTION_RELOAD},
        {319, 87, GE_PORT_ACTION_RELOAD}, {320, 87, 0},
        {0, 88, GE_PORT_ACTION_PREV_WEAPON},
        {159, 135, GE_PORT_ACTION_PREV_WEAPON},
        {160, 88, GE_PORT_ACTION_NEXT_WEAPON},
        {319, 135, GE_PORT_ACTION_NEXT_WEAPON},
        {0, 136, GE_PORT_ACTION_PAUSE}, {159, 175, GE_PORT_ACTION_PAUSE},
        {160, 136, GE_PORT_ACTION_AIM}, {319, 175, GE_PORT_ACTION_AIM},
        {0, 176, 0}, {319, 183, 0}, {0, 240, 0}, {65535, 60, 0}
    };
    size_t index;
    for (index = 0; index < sizeof(cases)/sizeof(cases[0]); index++) {
        Ge3dsControls controls;
        ge_3ds_controls_init(&controls);
        assert(touch(&controls, cases[index].x, cases[index].y).held == cases[index].action);
    }
    {
        Ge3dsControls controls;
        Ge3dsControlSample sample = {0};
        ge_3ds_controls_init(&controls);
        sample.held = GE_PORT_ACTION_AIM;
        (void)ge_3ds_controls_sample(&controls, &sample, true);
        sample.touching = true; sample.touch_x = 280; sample.touch_y = 210;
        assert(ge_3ds_controls_sample(&controls, &sample, true).held == 0);
        assert(ge_3ds_controls_sample(&controls, &sample, true).held == 0);
        sample.touching = false;
        assert(ge_3ds_controls_sample(&controls, &sample, true).held == 0);
        sample.held = 0;
        (void)ge_3ds_controls_sample(&controls, &sample, true);
        sample.held = GE_PORT_ACTION_AIM;
        assert(ge_3ds_controls_sample(&controls, &sample, true).pressed == GE_PORT_ACTION_FIRE);
    }
}

static void test_touch_settings_and_context(void)
{
    Ge3dsControls controls;
    Ge3dsControlSample sample = {0};
    int n;
    ge_3ds_controls_init(&controls);
    touch(&controls, 40, 210);
    assert(controls.settings.invert_look_y && controls.settings_dirty);
    touch(&controls, 40, 210);
    assert(controls.settings.invert_look_y);
    touch(&controls, -1, 0);
    touch(&controls, 40, 210);
    assert(!controls.settings.invert_look_y);
    for (n = 0; n < 20; n++) {
        touch(&controls, -1, 0); touch(&controls, 120, 210);
    }
    assert(controls.settings.look_sensitivity == 0.25f);
    for (n = 0; n < 20; n++) {
        touch(&controls, -1, 0); touch(&controls, 200, 210);
    }
    assert(controls.settings.look_sensitivity == 2.0f);
    touch(&controls, -1, 0); touch(&controls, 280, 210);
    assert(controls.settings.swap_shoulders);
    touch(&controls, -1, 0);
    sample.touching = true; sample.touch_x = 60; sample.touch_y = 60;
    assert(ge_3ds_controls_sample(&controls, &sample, false).held == 0);
    /* A held stylus carried from frontend must not activate a game button. */
    assert(ge_3ds_controls_sample(&controls, &sample, true).held == 0);
    touch(&controls, -1, 0);
    assert(ge_3ds_controls_sample(&controls, &sample, true).held == GE_PORT_ACTION_USE);
    assert(ge_3ds_controls_sample(&controls, &sample, false).held == 0);
    assert(ge_3ds_controls_sample(&controls, &sample, true).held == 0);
}

static void test_objectives(void)
{
    Ge3dsControls controls;
    Ge3dsTouchObjective items[GE_3DS_TOUCH_OBJECTIVE_MAX + 1];
    FILE *stream;
    char rendered[4096] = {0};
    size_t index;
    long bytes;
    ge_3ds_controls_init(&controls);
    assert(!ge_3ds_controls_objectives_due(&controls, 1000));
    assert(touch(&controls, 160, 12).held == 0);
    assert(controls.objectives_visible && !controls.settings_dirty);
    assert(ge_3ds_controls_objectives_due(&controls, 1000));
    assert(!ge_3ds_controls_objectives_due(&controls, 1000));
    assert(!ge_3ds_controls_objectives_due(&controls, 1249));
    assert(ge_3ds_controls_objectives_due(&controls, 1250));
    assert(ge_3ds_controls_objectives_due(&controls, 500)); /* Clock reset. */
    for (index = 0; index < sizeof(items)/sizeof(items[0]); index++) {
        items[index].text = "Install covert modem";
        items[index].status = GE_3DS_TOUCH_OBJECTIVE_INCOMPLETE;
    }
    items[0].text = "  Neutralize\nall\talarms  ";
    items[0].status = GE_3DS_TOUCH_OBJECTIVE_COMPLETE;
    items[1].text = NULL;
    items[1].status = (Ge3dsTouchObjectiveStatus)999;
    ge_3ds_controls_set_objectives(&controls, items, GE_3DS_TOUCH_OBJECTIVE_MAX + 1);
    assert(controls.objective_count == GE_3DS_TOUCH_OBJECTIVE_MAX);
    assert(strcmp(controls.objectives[0].text, "Neutralize all alarms") == 0);
    assert(controls.objectives[1].status == GE_3DS_TOUCH_OBJECTIVE_UNAVAILABLE);
    stream = tmpfile(); assert(stream != NULL);
    ge_3ds_controls_draw_panel(&controls, stream);
    bytes = ftell(stream); assert(bytes > 0);
    ge_3ds_controls_set_objectives(&controls, items, GE_3DS_TOUCH_OBJECTIVE_MAX);
    assert(!controls.panel_dirty);
    ge_3ds_controls_draw_panel(&controls, stream);
    assert(ftell(stream) == bytes);
    rewind(stream);
    assert(fread(rendered, 1, sizeof(rendered) - 1, stream) > 0);
    assert(strstr(rendered, "Neutralize all alarms") != NULL);
    assert(strstr(rendered, "[COMPLETE]") != NULL);
    assert(strstr(rendered, "[UNAVAILABLE]") != NULL);
    fclose(stream);
    /* Body touches cannot invoke the gameplay controls beneath this page. */
    touch(&controls, -1, 0);
    assert(touch(&controls, 60, 60).held == 0);
    touch(&controls, -1, 0);
    assert(touch(&controls, 280, 220).held == 0);
    assert(controls.objective_page == 1);
    touch(&controls, -1, 0); touch(&controls, 280, 220);
    assert(controls.objective_page == 1); /* Last page stays bounded. */
    touch(&controls, -1, 0); touch(&controls, 160, 220);
    assert(controls.objective_page == 0);
    touch(&controls, -1, 0);
    assert(touch(&controls, 50, 220).held == GE_PORT_ACTION_PAUSE);
    items[0].status = GE_3DS_TOUCH_OBJECTIVE_FAILED;
    controls.panel_dirty = false;
    ge_3ds_controls_set_objectives(&controls, items, 1);
    assert(controls.panel_dirty);
    touch(&controls, -1, 0); touch(&controls, 40, 12);
    assert(!controls.objectives_visible);
    assert(!ge_3ds_controls_objectives_due(&controls, 2000));
    touch(&controls, -1, 0); touch(&controls, 160, 12);
    assert(ge_3ds_controls_objectives_due(&controls, 2001));
    ge_3ds_controls_set_objectives(&controls, NULL, 1000);
    assert(controls.objective_count == 0 && controls.objective_page == 0);
    /* Long authored text is bounded and printable, with visible truncation. */
    items[0].text = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
    ge_3ds_controls_set_objectives(&controls, items, 1);
    assert(strlen(controls.objectives[0].text) == GE_3DS_TOUCH_OBJECTIVE_TEXT_SIZE - 1U);
    stream = tmpfile(); assert(stream != NULL);
    ge_3ds_controls_draw_panel(&controls, stream);
    rewind(stream); memset(rendered, 0, sizeof(rendered));
    assert(fread(rendered, 1, sizeof(rendered) - 1, stream) > 0);
    assert(strstr(rendered, "...") != NULL);
    fclose(stream);
}

static void write_text(const char *path, const char *text)
{
    FILE *stream = fopen(path, "wb");
    assert(stream != NULL);
    assert(fputs(text, stream) >= 0);
    assert(fclose(stream) == 0);
}

static void test_inventory(void)
{
    Ge3dsControls controls;
    Ge3dsTouchInventoryItem items[GE_3DS_TOUCH_INVENTORY_MAX + 1];
    FILE *stream;
    char rendered[8192] = {0};
    long bytes;
    size_t index;
    ge_3ds_controls_init(&controls);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == -1);
    assert(!ge_3ds_controls_inventory_due(&controls, 1000));
    touch(&controls, 280, 12);
    assert(controls.inventory_visible && !controls.objectives_visible);
    assert(!ge_3ds_controls_inventory_due(&controls, 1000)); /* Held tab. */
    touch(&controls, -1, 0);
    assert(ge_3ds_controls_inventory_due(&controls, 1000));
    assert(!ge_3ds_controls_inventory_due(&controls, 1249));
    assert(ge_3ds_controls_inventory_due(&controls, 1250));
    for (index = 0; index < sizeof(items)/sizeof(items[0]); ++index)
        items[index] = (Ge3dsTouchInventoryItem){"PP7 (silenced)", false, true};
    items[0].selected = true;
    items[1].selectable = false;
    items[2].text = "  Covert\nmodem  ";
    ge_3ds_controls_set_inventory(&controls, items, GE_3DS_TOUCH_INVENTORY_MAX + 1);
    assert(controls.inventory_count == GE_3DS_TOUCH_INVENTORY_MAX);
    assert(strcmp(controls.inventory[2].text, "Covert modem") == 0);
    stream = tmpfile(); assert(stream != NULL);
    ge_3ds_controls_draw_panel(&controls, stream);
    bytes = ftell(stream); assert(bytes > 0);
    ge_3ds_controls_set_inventory(&controls, items, GE_3DS_TOUCH_INVENTORY_MAX);
    assert(!controls.panel_dirty);
    ge_3ds_controls_draw_panel(&controls, stream);
    assert(ftell(stream) == bytes);
    rewind(stream);
    assert(fread(rendered, 1, sizeof(rendered)-1, stream) > 0);
    assert(strstr(rendered, "EQUIPPED") && strstr(rendered, "UNAVAILABLE"));
    assert(strstr(rendered, "Covert modem"));
    fclose(stream);
    assert(touch(&controls, 80, 40).held == 0);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == -1);
    assert(!ge_3ds_controls_inventory_due(&controls, 9000)); /* Snapshot frozen. */
    touch(&controls, -1, 0);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == 0);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == -1);
    assert(ge_3ds_controls_inventory_due(&controls, 9001));
    touch(&controls, 80, 72); touch(&controls, -1, 0);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == -1); /* Disabled. */
    touch(&controls, 80, 104); touch(&controls, 80, 136);
    touch(&controls, 80, 104); touch(&controls, -1, 0);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == -1); /* Drag cancels. */
    touch(&controls, 80, 104);
    ge_3ds_controls_set_inventory(&controls, items, 10);
    touch(&controls, -1, 0);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == -1); /* Replaced snapshot. */
    touch(&controls, 280, 220); touch(&controls, -1, 0);
    assert(controls.inventory_page == 1);
    touch(&controls, 80, 199); touch(&controls, -1, 0);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == 9);
    touch(&controls, 280, 220); touch(&controls, -1, 0);
    assert(controls.inventory_page == 1);
    touch(&controls, 160, 220); touch(&controls, -1, 0);
    assert(controls.inventory_page == 0);
    assert(touch(&controls, 40, 220).held == GE_PORT_ACTION_PAUSE);
    touch(&controls, -1, 0);
    touch(&controls, 160, 12); touch(&controls, -1, 0);
    assert(controls.objectives_visible && !controls.inventory_visible);
    touch(&controls, 40, 12); touch(&controls, -1, 0);
    assert(!controls.objectives_visible && !controls.inventory_visible);
    ge_3ds_controls_set_inventory(&controls, NULL, 100);
    assert(controls.inventory_count == 0 && controls.inventory_page == 0);
}

static void test_resume(void)
{
    Ge3dsControls controls;
    Ge3dsControlSample pad = {0};
    GePortInput result;
    ge_3ds_controls_init(&controls);
    controls.settings.swap_shoulders = true;
    controls.inventory_visible = true;
    controls.inventory_page = 3;
    pad.held = GE_PORT_ACTION_FIRE | GE_PORT_ACTION_USE | GE_PORT_ACTION_PAUSE;
    pad.touching = true; pad.touch_x = 40; pad.touch_y = 12;
    ge_3ds_controls_resume(&controls, pad.held, true);
    assert(controls.inventory_visible && controls.inventory_page == 3);
    assert(controls.settings.swap_shoulders);
    result = ge_3ds_controls_sample(&controls, &pad, true);
    assert(result.held == 0 && result.pressed == 0);
    assert(controls.inventory_visible); /* Held tab cannot change page. */
    pad.held &= ~GE_PORT_ACTION_USE;
    (void)ge_3ds_controls_sample(&controls, &pad, true);
    pad.held |= GE_PORT_ACTION_USE;
    result = ge_3ds_controls_sample(&controls, &pad, true);
    assert(result.pressed == GE_PORT_ACTION_USE && result.held == GE_PORT_ACTION_USE);
    pad.held = 0; pad.touching = false;
    (void)ge_3ds_controls_sample(&controls, &pad, true);
    pad.held = GE_PORT_ACTION_FIRE; pad.touching = true;
    result = ge_3ds_controls_sample(&controls, &pad, true);
    assert(result.pressed == GE_PORT_ACTION_AIM); /* Swap after release. */
    assert(!controls.inventory_visible);
    controls.inventory_request = 5;
    ge_3ds_controls_resume(&controls, 0, false);
    assert(ge_3ds_controls_take_inventory_selection(&controls) == -1);
}

static void test_persistence(const char *path)
{
    Ge3dsControls controls, restored;
    const char *invalid[] = { "look_sensitivity nan\n", "move_deadzone 0.9\n",
        "dual_analog 2\n", "invert_look_y\n", "look_deadzone 0.1 trailing\n",
        "look_sensitivity 1.0x\n", "swap_shoulders 1\nlook_sensitivity inf\n" };
    size_t index;
    ge_3ds_controls_init(&controls);
    assert(!ge_3ds_controls_load(&controls, "/missing/ge-controls.cfg"));
    write_text(path, "# legacy config\ndual_analog 0\nunknown_future_key 7\n");
    assert(ge_3ds_controls_load(&controls, path));
    assert(!controls.settings.dual_analog);
    assert(controls.settings.look_sensitivity == 1.0f);
    touch(&controls, 40, 210);
    touch(&controls, -1, 0);
    touch(&controls, 120, 210);
    assert(ge_3ds_controls_save(&controls, path));
    assert(!controls.settings_dirty);
    ge_3ds_controls_init(&restored);
    assert(ge_3ds_controls_load(&restored, path));
    assert(!restored.settings.dual_analog && restored.settings.invert_look_y);
    assert(restored.settings.look_sensitivity == 0.75f);
    {
        char temporary[4096];
        assert(snprintf(temporary, sizeof(temporary), "%s.tmp", path) > 0);
        assert(mkdir(temporary, 0700) == 0);
        controls.settings.look_sensitivity = 2.0f;
        controls.settings_dirty = true;
        assert(!ge_3ds_controls_save(&controls, path));
        assert(controls.settings_dirty);
        /* Failure opening the temporary preserves the previous real file. */
        assert(ge_3ds_controls_load(&restored, path));
        assert(restored.settings.look_sensitivity == 0.75f);
        assert(remove(temporary) == 0);
    }
    for (index = 0; index < sizeof(invalid)/sizeof(invalid[0]); index++) {
        Ge3dsControlSettings previous = restored.settings;
        write_text(path, invalid[index]);
        assert(!ge_3ds_controls_load(&restored, path));
        assert(memcmp(&previous, &restored.settings, sizeof(previous)) == 0);
    }
    restored.settings_dirty = true;
    assert(!ge_3ds_controls_save(&restored, "/missing/ge-controls.cfg"));
    assert(restored.settings_dirty);
    {
        FILE *stream = tmpfile();
        long bytes;
        assert(stream != NULL);
        ge_3ds_controls_draw_panel(&restored, stream);
        bytes = ftell(stream);
        assert(bytes > 0);
        ge_3ds_controls_draw_panel(&restored, stream);
        assert(ftell(stream) == bytes);
        fclose(stream);
    }
}

static void test_use_feedback(void)
{
    Ge3dsControls controls;
    const char *messages[] = {"", "Face the object and move closer.",
        "Activation sent", "Locked", "Use unavailable"};
    ge_3ds_controls_init(&controls);
    assert(controls.use_feedback == GE_3DS_USE_NONE);
    ge_3ds_controls_set_use_feedback(NULL, GE_3DS_USE_LOCKED);
    for (unsigned page = 0; page < 3; ++page) {
        controls.objectives_visible = page == 1;
        controls.inventory_visible = page == 2;
        for (unsigned code = 1; code <= GE_3DS_USE_UNAVAILABLE; ++code) {
            FILE *stream = tmpfile();
            char output[4096] = {0};
            Ge3dsControlSettings settings = controls.settings;
            uint32_t held = controls.previous_held;
            assert(stream != NULL && strlen(messages[code]) < 40U);
            controls.panel_dirty = false;
            ge_3ds_controls_set_use_feedback(&controls, (Ge3dsUseFeedback)code);
            assert(controls.panel_dirty);
            assert(memcmp(&settings, &controls.settings, sizeof(settings)) == 0);
            assert(held == controls.previous_held);
            ge_3ds_controls_draw_panel(&controls, stream);
            rewind(stream);
            assert(fread(output, 1, sizeof(output) - 1U, stream) > 0U);
            assert(strstr(output, "\x1b[2;1H") != NULL);
            assert(strstr(output, messages[code]) != NULL);
            assert(!controls.panel_dirty);
            ge_3ds_controls_set_use_feedback(&controls, (Ge3dsUseFeedback)code);
            assert(!controls.panel_dirty);
            fclose(stream);
        }
    }
    ge_3ds_controls_set_use_feedback(&controls, (Ge3dsUseFeedback)99);
    assert(controls.use_feedback == GE_3DS_USE_UNAVAILABLE);
    ge_3ds_controls_set_use_feedback(&controls, GE_3DS_USE_NONE);
    ge_3ds_controls_set_use_feedback(&controls, (Ge3dsUseFeedback)-1);
    assert(controls.use_feedback == GE_3DS_USE_UNAVAILABLE);
    ge_3ds_controls_set_use_feedback(&controls, GE_3DS_USE_NONE);
    assert(controls.use_feedback == GE_3DS_USE_NONE && controls.panel_dirty);
    {
        FILE *stream = tmpfile();
        char output[4096] = {0};
        assert(stream != NULL);
        ge_3ds_controls_draw_panel(&controls, stream);
        rewind(stream);
        assert(fread(output, 1, sizeof(output) - 1U, stream) > 0U);
        assert(strstr(output, "Use unavailable") == NULL);
        assert(strstr(output, "\x1b[2J") != NULL);
        fclose(stream);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    test_use_feedback();
    test_axes();
    test_actions();
    test_touch_boundaries();
    test_touch_settings_and_context();
    test_persistence(argv[1]);
    test_objectives();
    test_inventory();
    test_resume();
    puts("3DS controls: analog, edges, capture, settings, persistence, objective/inventory pages and resume passed");
    return 0;
}
