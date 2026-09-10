#!/usr/bin/env python3
"""Run the actual diagnostic gate with the original difficulty accessors."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "scripts"))
from extract_dam_guard_chr_scheduler_slice import function_text

source = (root / "platform/3ds/source/main.c").read_text()
main = source[source.index("int main("):]
prepare = function_text(source, "input_probe_prepare_difficulty")
assert main.index("input_probe_prepare_difficulty(") < main.index("load_original_stage_setup(")
assert "!run_start_frontend\n                && !load_visual_probe_tour(" in main
assert "load_input_probe(" not in main  # no late reparse after setup
assert "input_probe.setup_difficulty = lvlGetSelectedDifficulty();" in main
assert '"setup_difficulty=%d\\n", runtime->setup_difficulty' in source
assert '"difficulty=%ld\\n", (long)lvlGetSelectedDifficulty()' in source
assert '"probe_difficulty_override=%u\\n"' in source
assert "lvlSetSelectedDifficulty((DIFFICULTY)difficulty);" in function_text(
    source, "original_frontend_set_difficulty")
lv = (root / "src/game/lv.c").read_text()
accessors = "\n".join(function_text(lv, name) for name in (
    "lvlGetSelectedDifficulty", "lvlSetSelectedDifficulty"))
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "ge_3ds_probe_difficulty.h"
typedef int DIFFICULTY;
static int g_SelectedDifficulty;
#define PROBE_DIFFICULTY_PATH "difficulty.cfg"
typedef struct { int setup_difficulty; bool difficulty_override; bool enabled; } RuntimeInputProbe;
static bool controller_probe_valid;
static unsigned loader_calls;
static bool load_input_probe(RuntimeInputProbe *runtime) {
    ++loader_calls;
    memset(runtime, 0, sizeof(*runtime));
    runtime->enabled = controller_probe_valid;
    return controller_probe_valid;
}
static void config(const char *text) {
    FILE *f = fopen(PROBE_DIFFICULTY_PATH, "wb");
    assert(f); assert(fputs(text, f) >= 0); assert(fclose(f) == 0);
}
'''
suffix = r'''
int main(void) {
    RuntimeInputProbe r = {0};
    /* A stray valid or invalid config never changes normal frontend state. */
    controller_probe_valid = true;
    g_SelectedDifficulty = 1;
    config("2\n");
    assert(input_probe_prepare_difficulty(&r, false));
    assert(g_SelectedDifficulty == 1 && loader_calls == 0);
    config("bad");
    assert(input_probe_prepare_difficulty(&r, false));
    assert(g_SelectedDifficulty == 1 && loader_calls == 0);
    /* A missing/malformed controller probe never enables the override. */
    controller_probe_valid = false;
    assert(input_probe_prepare_difficulty(&r, true));
    assert(g_SelectedDifficulty == 1 && !r.enabled);
    controller_probe_valid = true;
    assert(!input_probe_prepare_difficulty(&r, true));
    assert(g_SelectedDifficulty == 1 && !r.difficulty_override);
    /* Missing optional config preserves the selected canonical difficulty. */
    assert(remove(PROBE_DIFFICULTY_PATH) == 0);
    assert(input_probe_prepare_difficulty(&r, true));
    assert(r.setup_difficulty == 1 && !r.difficulty_override);
    assert(g_SelectedDifficulty == 1);
    for (int d = 0; d < 4; ++d) {
        char text[] = "0\n"; text[0] += d; config(text);
        assert(input_probe_prepare_difficulty(&r, true));
        assert(r.setup_difficulty == d && r.difficulty_override && r.enabled);
        assert(lvlGetSelectedDifficulty() == d);
    }
    puts("probe difficulty canonical gate and pre-setup wiring passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="ge-probe-difficulty-wiring-") as directory:
    temp = Path(directory)
    c = temp / "test.c"
    c.write_text(prefix + accessors + prepare + suffix)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I" + str(root / "platform/3ds/include"), str(c),
                    str(root / "platform/3ds/source/ge_3ds_probe_difficulty.c"),
                    "-o", str(temp / "test")], check=True)
    subprocess.run([str(temp / "test")], cwd=temp, check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                        "UBSAN_OPTIONS": "halt_on_error=1"})
