#!/usr/bin/env python3
"""Execute the production Use dispatcher against deterministic native seams."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'platform/3ds/source/main.c').read_text()
start = source.index('static void tick_stage_interaction(')
brace = source.index('{', start)
depth = 0
for end in range(brace, len(source)):
    depth += (source[end] == '{') - (source[end] == '}')
    if depth == 0:
        dispatcher = source[start:end + 1]
        break
harness = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ge_3ds_controls.h"
#include "ge_original_door_interaction.h"
#include "ge_original_stage_alarm_interaction.h"
#include "ge_original_stage_special_objects.h"
#define RUNTIME_STAGE_ACTOR_TICK_READY 1
/* Local runtime storage is opaque to the native seams under test. */
typedef struct { bool root_active, live; void *prop, *definition;
 size_t command_index, definition_size; } RuntimeStageOrdinaryEntry;
typedef struct { void *prop; size_t command_index; } GeOriginalStageInteractiveEntry;
typedef struct { size_t entry_count; GeOriginalStageInteractiveEntry *entries; } Interactive;
typedef struct { int actor_tick_status; unsigned alarm_interaction_ticks,
 alarm_interaction_activations; size_t entry_count;
 RuntimeStageOrdinaryEntry *entries; Interactive interactive;
 GeOriginalDoorInteractionState door_interaction;
 GeOriginalStageAlarmInteractionResult alarm_interaction; } RuntimeStageOrdinaryObjects;
typedef struct { float original_camera_position[3], original_camera_look[3];
 unsigned original_camera_room; } RuntimeDamPreview;
static RuntimeStageOrdinaryObjects objects;
static GeOriginalDoorInteractionResult native_result;
static bool native_edge, allocation_failure, no_selection;
static void *candidate;
static unsigned selected_calls, activation_calls, feedback_calls;
static int activation_status;
static Ge3dsUseFeedback last_feedback;
static void publish_stage_use_feedback(Ge3dsUseFeedback feedback) {
 ++feedback_calls; last_feedback = feedback;
}
static void stage_alarm_play_sfx(void *context, uint32_t id) { (void)context; (void)id; }
GeOriginalDoorInteractionResult ge_original_door_interaction_tick(void) {
 objects.door_interaction.activate_edges += native_edge;
 return native_result;
}
static void *ge_original_gameplay_services_find_interactable(void *const *props, size_t count) {
 ++selected_calls; assert(count == 1 && props[0] == candidate); return no_selection ? NULL : candidate;
}
GeOriginalStageAlarmInteractionStatus ge_original_stage_object_interact_exact(
 void *prop, const GeOriginalStageAlarmInteractionProviders *providers,
 GeOriginalStageAlarmInteractionResult *result) {
 assert(prop == candidate && providers->play_sfx == stage_alarm_play_sfx);
 ++activation_calls; result->status = activation_status; return activation_status;
}
static const GeOriginalStageInteractiveEntry *ge_original_stage_interactive_entry(
 const Interactive *interactive, size_t index) { return &interactive->entries[index]; }
int ge_original_stage_monitor_snapshot(const void *definition, size_t size,
 GeOriginalStageMonitorSnapshot *snapshot) { (void)definition; (void)size; (void)snapshot; return 0; }
const char *ge_original_door_interaction_result_name(GeOriginalDoorInteractionResult result) {
 (void)result; return "stub";
}
static unsigned long long svcGetSystemTick(void) { return 0; }
static FILE *audit_open(const char *path, const char *mode) {
 assert(strstr(path, "use-audit.log")); (void)mode; return NULL;
}
static void *test_calloc(size_t n, size_t size) { return allocation_failure ? NULL : calloc(n, size); }
#define fopen audit_open
#define calloc test_calloc
'''
tests = r'''
int main(void) {
 RuntimeDamPreview preview = {0};
 RuntimeStageOrdinaryEntry entries[3] = {{0}};
 objects.actor_tick_status = RUNTIME_STAGE_ACTOR_TICK_READY;
 objects.entries = entries;
 native_result = GE_ORIGINAL_DOOR_INTERACTION_IDLE;
 tick_stage_interaction(&objects, &preview, true);
 assert(feedback_calls == 0 && objects.alarm_interaction_ticks == 1);
 native_edge = true; native_result = GE_ORIGINAL_DOOR_INTERACTION_RELOAD_REQUESTED;
 tick_stage_interaction(&objects, &preview, true);
 assert(last_feedback == GE_3DS_USE_NO_TARGET && selected_calls == 0);
 native_result = GE_ORIGINAL_DOOR_INTERACTION_ACTIVATED;
 tick_stage_interaction(&objects, &preview, true);
 assert(last_feedback == GE_3DS_USE_ACTIVATED && activation_calls == 0);
 native_result = GE_ORIGINAL_DOOR_INTERACTION_LOCKED;
 tick_stage_interaction(&objects, &preview, true);
 assert(last_feedback == GE_3DS_USE_LOCKED);
 for (int error = GE_ORIGINAL_DOOR_INTERACTION_INVALID_STATE;
      error <= GE_ORIGINAL_DOOR_INTERACTION_MISSING_SWITCH_PROVIDER; ++error) {
  native_result = error; tick_stage_interaction(&objects, &preview, true);
  assert(last_feedback == GE_3DS_USE_UNAVAILABLE);
 }
 native_result = GE_ORIGINAL_DOOR_INTERACTION_RELOAD_REQUESTED;
 objects.entry_count = 3;
 entries[0].prop = &entries[0]; /* inactive must be omitted */
 entries[1].root_active = true; /* null prop must be omitted */
 entries[2].root_active = true; entries[2].prop = &entries[2];
 candidate = entries[2].prop;
 activation_status = GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK;
 tick_stage_interaction(&objects, &preview, true);
 assert(last_feedback == GE_3DS_USE_ACTIVATED && activation_calls == 1);
 assert(selected_calls == 1 && objects.alarm_interaction_activations == 1);
 activation_status = GE_ORIGINAL_STAGE_ALARM_INTERACTION_INVALID_ARGUMENT;
 tick_stage_interaction(&objects, &preview, true);
 assert(last_feedback == GE_3DS_USE_UNAVAILABLE && activation_calls == 2);
 assert(objects.alarm_interaction_activations == 1);
 allocation_failure = true;
 tick_stage_interaction(&objects, &preview, true);
 assert(last_feedback == GE_3DS_USE_UNAVAILABLE && selected_calls == 2);
 allocation_failure = false;
 unsigned before = feedback_calls;
 activation_status = GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK;
 tick_stage_interaction(&objects, &preview, false);
 assert(feedback_calls == before && activation_calls == 3);
 assert(objects.alarm_interaction_activations == 2); /* reload preserves native action */
 native_edge = false;
 tick_stage_interaction(&objects, &preview, true);
 assert(feedback_calls == before && activation_calls == 3);
 native_edge = true; no_selection = true;
 tick_stage_interaction(&objects, &preview, true);
 assert(last_feedback == GE_3DS_USE_NO_TARGET && activation_calls == 3);
 puts("Use dispatcher: idle, doors, ordinary props, errors and reload feedback passed");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='ge-use-feedback-') as directory:
    root = Path(directory)
    (root / 'test.c').write_text(harness + dispatcher + tests)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
        '-D_POSIX_C_SOURCE=200809L', '-fsanitize=address,undefined',
        '-I', str(repo / 'platform/3ds/include'), '-I', str(repo / 'port/include'),
        str(root / 'test.c'), '-o', str(root / 'test')], check=True)
    subprocess.run([str(root / 'test')], check=True)
