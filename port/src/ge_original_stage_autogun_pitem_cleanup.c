#include "ge_original_pitem_models.h"
#include "ge_original_stage_autogun_lifecycle.h"

static int release_pitem_model(void *context, void *model_instance)
{
    return ge_original_pitem_model_release_instance(
        (GeOriginalPitemModelProvider *)context, model_instance);
}

GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_cleanup_pitem_exact(
    GeOriginalStageSecurityInstance *instance,
    GeOriginalPitemModelProvider *models)
{
    const GeOriginalStageAutogunCleanupProviders providers = {
        models, release_pitem_model
    };
    if (models == NULL)
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_INVALID_ARGUMENT;
    return ge_original_stage_autogun_lifecycle_cleanup_owned_exact(
        instance, &providers);
}
