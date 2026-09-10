#!/usr/bin/env python3
"""Exercise renderer-only guard eligibility, culling and RNG ownership.

The production runtime is included directly so no alternative implementation
of its private room and sphere decisions can accidentally pass this test.
"""
from pathlib import Path
import subprocess
import re
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / "scripts"))
from extract_dam_guard_chr_scheduler_slice import function_text

source = (repo / "port/src/ge_original_stage_guard_runtime.c").read_text()
extract_source = re.sub(r"\bGeOriginalStageGuardRuntimeStatus\s*\n", "GeOriginalStageGuardRuntimeStatus ", source)
for name, count in (("ge_original_stage_guard_runtime_update_matrices", 1),
                    ("collect_scene_inputs", 2)):
    body = function_text(extract_source, name)
    assert body.count("runtime_guard_room_draw_visible(") == count, name
    assert "!slot->state.visible" not in body, name
uncached = function_text(extract_source, "build_scene_internal")
assert "collect_scene_inputs(" in uncached
assert "!slot->state.visible" not in uncached

main_source = (repo / "platform/3ds/source/main.c").read_text()
for name in ("update_stage_guard_visibility", "publish_stage_ordinary_visibility"):
    body = function_text(main_source, name)
    assert "dam_visibility_contains_room(" in body
    assert "dam_renderer_visibility_contains_room(" not in body
for name in ("refresh_stage_glass_shading", "refresh_stage_windowed_door_shading"):
    assert "dam_renderer_visibility_contains_room(" in function_text(main_source, name)
stream = function_text(main_source, "stream_dam_renderer_camera")
guard_visibility = function_text(main_source, "prepare_dam_guard_renderer_visibility")
assert "ge_original_stage_guard_runtime_set_renderer_visibility(" in guard_visibility
assert "stereo_rooms ? preview->renderer_visibility.membership : NULL" in guard_visibility
assert "ge_3ds_stereo_max_world_shift_px(" in guard_visibility
assert stream.index("prepare_dam_guard_renderer_visibility(") < stream.index("prepare_dam_gpu_world_projection(")
multiplayer_visibility = function_text(main_source, "runtime_multiplayer_view_visibility")
assert multiplayer_visibility.index("memcpy(preview->canonical_preload_requests,") < multiplayer_visibility.index("update_dam_renderer_visibility(")
assert multiplayer_visibility.index("update_dam_renderer_visibility(") < multiplayer_visibility.index("prepare_dam_guard_renderer_visibility(")
assert "dam_renderer_visibility_contains_room(" in stream
preload_request = re.search(r"const bool requested\s*=.*?;", stream, re.S).group()
articulated = function_text(main_source, "refresh_stage_articulated_objects")
assert "renderer_visibility.stereo" in articulated
assert "ge_original_stage_model_publication_resident_input(" in articulated
assert "ge_original_stage_model_publication_input(" in articulated

test = r'''
#include "ge_original_stage_guard_runtime.c"
#include "ge_dam_renderer_visibility.h"
#include <assert.h>
#include <stdio.h>
PropRecord *g_ActivePropsHead;
static struct player player;
struct player *g_CurrentPlayer=&player;
struct player *g_playerPointers[4]={&player,NULL,NULL,NULL};
s32 g_GlobalTimer=17;
static unsigned flash_queries;
static uint32_t captured_random;
static int16_t captured_sine;
signed short sins(unsigned short angle) { return (int16_t)angle; }
int ge_original_pitem_model_instance_gunfire(
    const GeOriginalPitemModelProvider *models,const void *model,size_t index,
    GeOriginalPitemModelGunfire *gunfire) {
    (void)models;(void)model;assert(index==0);++flash_queries;
    memset(gunfire,0,sizeof(*gunfire));gunfire->visible=1;return 1;
}
int ge_original_guard_muzzle_flash_build(const GeOriginalPitemModelGunfire *gunfire,
    const float matrix[4][4],float scale,uint32_t random,int16_t cosine,int16_t sine,
    GeOriginalGuardMuzzleFlashPublication *publication) {
    (void)gunfire;(void)matrix;(void)scale;(void)cosine;(void)publication;
    captured_random=random;captured_sine=sine;return 1;
}
static void identity(float matrix[4][4]) {
    memset(matrix,0,16*sizeof(float));for(unsigned i=0;i<4;++i)matrix[i][i]=1;
}
typedef struct {
    bool visibility_ready,renderer_visibility_ready;
    GeOriginalBgVisibilityResult visibility_result;
    GeDamRendererVisibility renderer_visibility;
    uint8_t canonical_preload_requests[GE_DAM_PRELOAD_MAX_ROOMS];
    uint8_t after_preload[2];
} RuntimeDamPreview;
''' + "\n".join(function_text(main_source, name) for name in (
    "dam_visibility_contains_room", "dam_renderer_visibility_contains_room")) + '''
static bool main_preload_requested(const RuntimeDamPreview *preview,size_t candidate) {
''' + preload_request + r'''
    return requested;
}
static void consumer_checks(void) {
    RuntimeDamPreview preview={0};
    preview.visibility_ready=true;preview.visibility_result.room_count=1;
    preview.visibility_result.rooms[0].room=1;
    preview.renderer_visibility_ready=true;
    preview.renderer_visibility.membership[1]=1;
    preview.renderer_visibility.membership[2]=2;
    preview.renderer_visibility.membership[3]=4;
    RuntimeDamPreview before=preview;
    assert(dam_visibility_contains_room(&preview,1));
    assert(!dam_visibility_contains_room(&preview,2));
    assert(!dam_visibility_contains_room(&preview,3));
    assert(dam_renderer_visibility_contains_room(&preview,1));
    assert(dam_renderer_visibility_contains_room(&preview,2));
    assert(dam_renderer_visibility_contains_room(&preview,3));
    assert(!dam_renderer_visibility_contains_room(&preview,256));
    assert(memcmp(&preview,&before,sizeof(preview))==0);
    preview.renderer_visibility_ready=false;
    for(unsigned room=0;room<=256;++room)
        assert(dam_renderer_visibility_contains_room(&preview,room)
            ==dam_visibility_contains_room(&preview,room));
    /* Failure fallback has fewer preload slots than the renderer's maximum
     * room count. Exercise the actual main expression with poisoned adjacent
     * bytes, then retain the renderer's valid larger membership capacity. */
    memset(preview.canonical_preload_requests,1,sizeof(preview.canonical_preload_requests));
    memset(preview.after_preload,0xa5,sizeof(preview.after_preload));
    before=preview;
    for(size_t room=0;room<GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS;++room)
        assert(main_preload_requested(&preview,room)==(room<GE_DAM_PRELOAD_MAX_ROOMS));
    assert(memcmp(&preview,&before,sizeof(preview))==0);
    preview.renderer_visibility_ready=true;
    preview.renderer_visibility.preload[GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS-1]=1;
    assert(main_preload_requested(&preview,GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS-1));
}
static void sphere_checks(void) {
    float view[4][4],center[3]={110,0,-100};identity(view);
    /* At FOV 90/aspect 1 a point at x=110,z=-100 is outside mono, but
     * inside an eye union expanded by 25% of horizontal clip width. */
    assert(!ge_original_stage_guard_draw_sphere_visible(view,90,1,1,center,0));
    assert(runtime_draw_sphere_visible_with_margin(view,90,1,1,center,0,.25f));
    center[0]=-110;
    assert(runtime_draw_sphere_visible_with_margin(view,90,1,1,center,0,.25f));
    center[0]=126;
    assert(!runtime_draw_sphere_visible_with_margin(view,90,1,1,center,0,.25f));
    center[0]=1000000;
    assert(runtime_draw_sphere_visible_with_margin(view,90,1,1,center,0,INFINITY));
    center[1]=110;
    assert(!runtime_draw_sphere_visible_with_margin(view,90,1,1,center,0,INFINITY));
    center[1]=0;center[2]=1;
    assert(!runtime_draw_sphere_visible_with_margin(view,90,1,1,center,0,INFINITY));
    /* Every point moved onto the screen by an actual permitted stereo shift
     * must survive the widened horizontal sphere test. */
    const float constant=1440, coefficient=-8, margin=88.0f/160.0f;
    for(unsigned depth=15;depth<=1024;depth+=7)
        for(int x=-1000;x<=1000;x+=3) {
            const float mono=(float)x/(float)depth;
            center[0]=(float)x;center[1]=0;center[2]=-(float)depth;
            for(int eye=-1;eye<=1;eye+=2) {
                const float displaced=mono+(float)eye*(constant/(float)depth+coefficient)/160;
                if(displaced>=-1 && displaced<=1)
                    assert(runtime_draw_sphere_visible_with_margin(view,90,1,15,center,0,margin));
            }
        }
}
int main(void) {
    consumer_checks();sphere_checks();
    GeOriginalStageGuardRuntime runtime={0},before;
    GeOriginalStageGuardSlot slot={0},slot_before;
    GeOriginalStageGuardWeaponSlot weapon={0};
    PropRecord prop={0},weapon_prop={0};
    PropRecord *props[1]={&prop};
    ChrRecord chr={0},chr_before;
    Model model={0},weapon_model={0};
    ModelFileHeader header={0};RenderPosView matrix={0};
    GeOriginalGuardMuzzleFlashPublication flash;
    uint8_t membership[256]={0};
    identity(runtime.draw_world_to_view);runtime.draw_camera_ready=1;
    player.c_perspfovy=90;player.c_perspaspect=1;player.c_perspnear=1;
    runtime.draw_vertical_fov=90;runtime.draw_aspect=1;runtime.draw_near=1;
    runtime.count=1;runtime.slots=&slot;runtime.props=props;
    runtime.weapon_count=1;runtime.weapons=&weapon;
    runtime.weapon_models=(void*)&runtime;
    slot.state.room_id=3;slot.state.chr_id=12;slot.state.chr_record=&chr;
    slot.state.prop_record=&prop;slot.pair.model_instance=&model;
    chr.chrnum=12;chr.model=&model;chr.prop=&prop;
    prop.chr=&chr;prop.type=PROP_TYPE_CHR;prop.pos.z=-100;
    model.obj=&header;model.scale=1;
    header.numMatrices=1;weapon_model.obj=&header;weapon_model.render_pos=&matrix;
    weapon.state.matrices_ready=1;weapon.model=&weapon_model;
    weapon.prop=&weapon_prop;weapon.owner=&slot;weapon_prop.parent=&prop;
    slot_before=slot;chr_before=chr;
    membership[3]=4;
    assert(ge_original_stage_guard_runtime_set_renderer_visibility(&runtime,membership,4,.25f));
    membership[3]=0; /* The runtime owns a copy. */
    assert(runtime_guard_room_draw_visible(&runtime,&slot));
    assert(memcmp(&slot,&slot_before,sizeof(slot))==0);
    assert(memcmp(&chr,&chr_before,sizeof(chr))==0 && prop.flags==0);
    before=runtime;
    assert(!ge_original_stage_guard_runtime_set_renderer_visibility(&runtime,NULL,4,0));
    assert(!ge_original_stage_guard_runtime_set_renderer_visibility(&runtime,membership,0,0));
    assert(!ge_original_stage_guard_runtime_set_renderer_visibility(&runtime,membership,257,0));
    assert(!ge_original_stage_guard_runtime_set_renderer_visibility(&runtime,membership,4,NAN));
    assert(!ge_original_stage_guard_runtime_set_renderer_visibility(&runtime,membership,4,-1));
    assert(memcmp(&runtime,&before,sizeof(runtime))==0);
    /* Added eye-only flashes must never consume the canonical PRNG, and are
     * stable when the same view is submitted for the second eye. */
    randomSetSeed(123);u64 saved_seed=g_randomSeed;
    assert(ge_original_stage_guard_runtime_muzzle_flash(&runtime,0,&flash));
    uint32_t first_random=captured_random;int16_t first_sine=captured_sine;
    assert(g_randomSeed==saved_seed);
    assert(ge_original_stage_guard_runtime_muzzle_flash(&runtime,0,&flash));
    assert(g_randomSeed==saved_seed && captured_random==first_random && captured_sine==first_sine);
    /* Canonical room visibility does not imply the mono sphere would draw:
     * geometry recovered only by stereo expansion is also an added flash. */
    slot.state.visible=1;prop.pos.x=110;
    assert(runtime_guard_draw_visible(&runtime,0));
    assert(!runtime_guard_draw_visible_with_margin(&runtime,0,0));
    /* A later selected player cannot replace the captured viewport's FOV. */
    player.c_perspfovy=20;
    assert(runtime_guard_draw_visible(&runtime,0));
    assert(!runtime_guard_draw_visible_with_margin(&runtime,0,0));
    player.c_perspfovy=90;
    assert(ge_original_stage_guard_runtime_muzzle_flash(&runtime,0,&flash));
    assert(g_randomSeed==saved_seed);
    prop.pos.x=0;
    u64 expected_seed=saved_seed;
    uint32_t expected_random=randomGetNextFrom(&expected_seed);
    uint32_t expected_angle=randomGetNextFrom(&expected_seed);
    assert(ge_original_stage_guard_runtime_muzzle_flash(&runtime,0,&flash));
    assert(g_randomSeed==expected_seed && captured_random==expected_random);
    assert(captured_sine==(int16_t)(expected_angle*1024U));
    assert(ge_original_stage_guard_runtime_set_renderer_visibility(&runtime,NULL,0,0));
    assert(runtime_guard_room_draw_visible(&runtime,&slot));
    slot.state.visible=0;
    assert(!runtime_guard_room_draw_visible(&runtime,&slot));
    unsigned queries=flash_queries;
    assert(!ge_original_stage_guard_runtime_muzzle_flash(&runtime,0,&flash));
    assert(flash_queries==queries && g_randomSeed==expected_seed);
    /* Disabled stereo uses the original sphere decision. */
    prop.pos.x=110;
    assert(!runtime_guard_draw_visible(&runtime,0));
    puts("renderer guard integration: copied eye rooms, horizontal bounds, mono restore, canonical flags/PRNG passed");
}
'''
with tempfile.TemporaryDirectory(prefix="ge-guard-eye-") as temporary:
    path = Path(temporary)
    (path / "test.c").write_text(test)
    dead_strip = "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections"
    for flags in ([], ["-fshort-enums"]):
        subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-Wno-unused-variable", "-Wno-incompatible-pointer-types",
            "-Wno-missing-braces", "-ffunction-sections", "-fdata-sections", dead_strip,
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-D_LANGUAGE_C",
            "-DGE_PORT_USE_ORIGINAL_TYPES", "-DAIPARSE", "-DGE_PORT_SETUP_DATA",
            "-DGE_PORT_MS_INHERITS", "-DVERSION_US", "-fms-extensions", *flags,
            "-I", str(repo / "port/include"), "-I", str(repo / "port/src"),
            "-I", str(repo / "src/game"), "-idirafter", str(repo / "include"),
            "-idirafter", str(repo / "src"), str(path / "test.c"),
            str(repo / "port/src/random_port.c"), "-lm", "-o", str(path / "test")], check=True)
        subprocess.run([str(path / "test")], check=True)
