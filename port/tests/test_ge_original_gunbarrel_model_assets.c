#include "ge_asset_pack.h"
#include "ge_original_character_models.h"
#include "ge_original_pitem_models.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <stdio.h>

void modelCalculateRwDataLen(ModelFileHeader *header)
{
    (void)header;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalCharacterModelProvider *characters;
    GeOriginalPitemModelProvider *pitems;
    GeOriginalCharacterModelStatus character_status;
    GeOriginalPitemModelStatus pitem_status;
    GeOriginalCharacterModelPair pair;
    void *gun_header = NULL;
    void *gun_model = NULL;
    float gun_scale = 0.0f;

    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    characters = ge_original_character_model_provider_create(
        &pack, 2U, 1U, &character_status);
    assert(characters != NULL
        && character_status == GE_ORIGINAL_CHARACTER_MODEL_OK);
    if (!ge_original_character_model_resolve_pair(
            characters, BODY_Brosnan_Tuxedo,
            BODY_Male_Pierce_Bond_Tuxedo, 0, &pair)) {
        fprintf(stderr, "gunbarrel character pair: %s\n",
            ge_original_character_model_status_name(
                ge_original_character_model_last_status(characters)));
        return 2;
    }
    assert(pair.model_header != NULL && pair.model_instance != NULL
        && pair.matrix_count > 0U);

    pitems = ge_original_pitem_model_provider_create(
        &pack, 1U, 1U, &pitem_status);
    assert(pitems != NULL && pitem_status == GE_ORIGINAL_PITEM_MODEL_OK);
    if (!ge_original_pitem_model_resolve_instance(
            pitems, PROP_CHRWPPK, &gun_header, &gun_model, &gun_scale)) {
        fprintf(stderr, "gunbarrel PP7: %s\n",
            ge_original_pitem_model_status_name(
                ge_original_pitem_model_last_status(pitems)));
        return 3;
    }
    assert(gun_header != NULL && gun_model != NULL && gun_scale > 0.0f);
    printf("gunbarrel authored model assets pass: %lu matrices, PP7 %.6f\n",
        (unsigned long)pair.matrix_count, (double)gun_scale);
    ge_original_character_model_provider_destroy(characters);
    ge_original_pitem_model_provider_destroy(pitems);
    ge_asset_pack_close(&pack);
    return 0;
}
