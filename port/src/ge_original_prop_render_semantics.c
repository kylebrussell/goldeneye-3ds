#include "ge_original_prop_render_semantics.h"
#include "ge_original_effect_runtime.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

extern int ge_original_prop_render_semantic_exact(void *,void *,int,
    void (*)(void *,Model *,ModelNode *,u32,u32));
typedef struct GeRenderContext {
    GeOriginalStageGuardRuntime *guards;
    unsigned viewport;
} GeRenderContext;

static void flash(void *opaque,Model *model,ModelNode *node,u32 scale,u32 angle)
{
    GeRenderContext *context=opaque;
    ge_original_stage_guard_runtime_record_muzzle_flash(context->guards,
        context->viewport,model,node,scale,angle);
}

static int render(void *context,void *prop,int alpha)
{
    return ge_original_prop_render_semantic_exact(context,prop,alpha,flash);
}

int ge_original_prop_render_semantics_prepare(GeOriginalStageGuardRuntime *guards,
    uint64_t tick,unsigned viewport)
{
    GeRenderContext context={guards,viewport};
    int begun=ge_original_stage_guard_runtime_begin_muzzle_flashes(guards,tick,viewport);
    if(!begun)return 0;
    if(begun==2)return 1;
    if(!ge_original_effect_runtime_render_view(&context,render))return 0;
    ge_original_stage_guard_runtime_end_muzzle_flashes(guards,viewport);
    return 1;
}
