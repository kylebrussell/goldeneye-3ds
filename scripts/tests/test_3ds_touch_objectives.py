#!/usr/bin/env python3
"""Execute the actual 3DS objective presentation helper with live-provider fixtures."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()
start = source.index("static void update_touch_objectives(void)")
brace = source.index("{", start)
depth = 0
for end in range(brace, len(source)):
    depth += (source[end] == "{") - (source[end] == "}")
    if depth == 0:
        helper = source[start:end + 1]
        break
else:
    raise AssertionError("unclosed objective helper")

harness = r'''
#include "ge_3ds_controls.h"
#include "ge_original_stage_objective_live.h"
#include "ge_original_language_text.h"
#include <assert.h>
#include <string.h>
static Ge3dsControls controls;
static int count, selected_difficulty;
static unsigned evaluations;
static struct Fixture {
    int8_t difficulty;
    int present, has_id;
    const char *text;
    GeOriginalStageObjectiveRuntimeStatus status;
    uint8_t value;
} fixture[GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
int ge_original_stage_objective_live_count(void) { return count; }
static int lvlGetSelectedDifficulty(void) { return selected_difficulty; }
int ge_original_stage_objective_live_difficulty(uint8_t menu, int8_t *difficulty)
{
    assert(menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX);
    *difficulty=fixture[menu].difficulty;
    return fixture[menu].present;
}
int ge_original_stage_objective_live_text_id(uint8_t menu, uint16_t *id)
{
    assert(menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX);
    *id=menu;
    return fixture[menu].has_id;
}
const char *ge_original_language_text(uint16_t id)
{
    assert(id < GE_ORIGINAL_STAGE_OBJECTIVE_MAX);
    return fixture[id].text;
}
GeOriginalStageObjectiveRuntimeStatus ge_original_stage_objective_live_evaluate(
    uint8_t menu, GeOriginalStageObjectiveEvaluation *evaluation)
{
    assert(menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX);
    evaluations++;
    evaluation->value=fixture[menu].value;
    return fixture[menu].status;
}
/* No mutating HUD status-change/cleanup functions are defined: accidental
 * calls from this helper must fail to link. */
''' + helper + r'''
int main(void)
{
    ge_3ds_controls_init(&controls);
    count=100; /* Malformed count cannot overrun bounded provider menu IDs. */
    for(int i=0;i<GE_ORIGINAL_STAGE_OBJECTIVE_MAX;++i) {
        fixture[i].present=fixture[i].has_id=1;
        fixture[i].text="Objective";
        fixture[i].status=GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
    }
    fixture[0].text="Agent objective";
    fixture[0].value=GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE;
    fixture[1].difficulty=1;
    fixture[1].text="Secret Agent objective";
    fixture[1].value=GE_ORIGINAL_STAGE_OBJECTIVE_FAILED;
    fixture[2].difficulty=2;
    fixture[2].text="00 Agent objective";
    fixture[3].status=GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED;
    fixture[3].value=GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE;
    fixture[4].value=255;
    fixture[5].present=0;
    fixture[6].has_id=0;
    fixture[7].text=NULL;
    selected_difficulty=0;
    update_touch_objectives();
    assert(controls.objective_count==5 && evaluations==5);
    assert(strcmp(controls.objectives[0].text,"Agent objective")==0);
    assert(controls.objectives[0].status==GE_3DS_TOUCH_OBJECTIVE_COMPLETE);
    assert(controls.objectives[1].status==GE_3DS_TOUCH_OBJECTIVE_UNAVAILABLE);
    assert(controls.objectives[2].status==GE_3DS_TOUCH_OBJECTIVE_UNAVAILABLE);
    selected_difficulty=1; evaluations=0;
    update_touch_objectives();
    assert(controls.objective_count==6 && evaluations==6);
    assert(strcmp(controls.objectives[1].text,"Secret Agent objective")==0);
    assert(controls.objectives[1].status==GE_3DS_TOUCH_OBJECTIVE_FAILED);
    selected_difficulty=2; evaluations=0;
    update_touch_objectives();
    assert(controls.objective_count==7 && evaluations==7);
    assert(strcmp(controls.objectives[2].text,"00 Agent objective")==0);
    assert(controls.objectives[2].status==GE_3DS_TOUCH_OBJECTIVE_INCOMPLETE);
    count=0; evaluations=0;
    update_touch_objectives();
    assert(controls.objective_count==0 && evaluations==0);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="ge-touch-objectives-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(harness)
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-I" + str(repo / "port/include"),
        "-I" + str(repo / "platform/3ds/include"),
        str(path / "test.c"), str(repo / "platform/3ds/source/ge_3ds_controls.c"),
        "-lm", "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True, env={
        **os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
        "UBSAN_OPTIONS": "halt_on_error=1",
    })
print("3DS objective helper: difficulty, original statuses, blockers, unavailable data and bounds passed")
