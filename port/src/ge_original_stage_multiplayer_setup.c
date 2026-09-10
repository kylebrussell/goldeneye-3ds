#include "ge_original_stage_supplies.h"
#include "ge_original_multiplayer_rules.h"
#include <ultra64.h>
#include <bondconstants.h>
#include "game/mp_weapon.h"
#include <stdlib.h>
#include <string.h>

extern struct s_mp_weapon_set_text mp_weapon_set_text_table[];

void ge_original_stage_multiplayer_setup_close(GeOriginalStageMultiplayerSetup *p)
{
    if (!p) return;
    free(p->records);
    free(p->words);
    memset(p, 0, sizeof(*p));
}

static GeOriginalStagePropRecord *selected_relation(
    const GeOriginalStageSetupRuntime *source, GeOriginalStageMultiplayerSetup *next,
    const GeOriginalStagePropRecord *record)
{
    uintptr_t start=(uintptr_t)source->prop_records, value=(uintptr_t)record;
    if (!record) return NULL;
    if (value<start || (value-start)%sizeof(*record)
            || (value-start)/sizeof(*record)>=source->prop_record_count) return NULL;
    return &next->records[(value-start)/sizeof(*record)];
}

GeOriginalStageSupplyStatus ge_original_stage_multiplayer_setup_prepare(
    const GeOriginalStageSetupRuntime *source, int32_t player_count,
    int32_t scenario, int32_t weapon_set,
    GeOriginalStageMultiplayerSetup *prepared)
{
    GeOriginalStageMultiplayerSetup next={0};
    GeOriginalMultiplayerConfig config={player_count,scenario,weapon_set,0,{0,0,1,1}};
    const struct s_mp_weapon_set *weapons;
    size_t i, j;
    int last_slot=-1;
    if (!source || !prepared || !source->loaded || !source->descriptor
            || !source->descriptor->setup_key
            || !source->prop_records || !source->propdefs_storage
            || strncmp(source->descriptor->setup_key,"Ump_",4)!=0
            || source->prop_record_count>SIZE_MAX/sizeof(*next.records)
            || source->prop_word_count>SIZE_MAX/sizeof(*next.words))
        return GE_ORIGINAL_STAGE_SUPPLY_INVALID_ARGUMENT;
    config.level_id=source->descriptor->level_id;
    if (scenario==SCENARIO_3v1) config.teams[2]=0;
    if (scenario==SCENARIO_2v1) config.teams[2]=1;
    if (ge_original_multiplayer_validate(&config)!=GE_ORIGINAL_MP_OK)
        return GE_ORIGINAL_STAGE_SUPPLY_INVALID_ARGUMENT;
    weapons=mp_weapon_set_text_table[weapon_set].weapon_set;
    next.records=malloc(source->prop_record_count*sizeof(*next.records));
    next.words=malloc(source->prop_word_count*sizeof(*next.words));
    if (!next.records || !next.words) goto invalid;
    memcpy(next.records,source->prop_records,source->prop_record_count*sizeof(*next.records));
    memcpy(next.words,source->propdefs_storage,source->prop_word_count*sizeof(*next.words));
    next.view=*source;
    next.view.prop_records=next.records;
    next.view.propdefs_storage=next.words;
    next.player_count=player_count;next.scenario=scenario;next.weapon_set=weapon_set;
    for(i=0;i<source->prop_record_count;++i){
        const GeOriginalStagePropRecord *original=&source->prop_records[i];
        GeOriginalStagePropRecord *record=&next.records[i];
        uintptr_t start=(uintptr_t)source->propdefs_storage, value=(uintptr_t)original->words;
        size_t word;
        if(original->multiplayer_prepared || value<start || (value-start)%sizeof(uint32_t)) goto invalid;
        word=(value-start)/sizeof(uint32_t);
        if(word>=source->prop_word_count || original->word_count>source->prop_word_count-word) goto invalid;
        record->words=next.words+word;
        record->objective=selected_relation(source,&next,original->objective);
        if(original->objective && !record->objective) goto invalid;
        for(j=0;j<3;++j){
            record->relations[j]=selected_relation(source,&next,original->relations[j]);
            if(original->relations[j] && !record->relations[j]) goto invalid;
        }
        record->multiplayer_weapon_slot=-1;
        if(record->type==PROPDEF_COLLECTABLE && record->word_count==34
                && !(record->words[2]&PROPFLAG_ASSIGNEDTOCHR)){
            int item=(int)(record->words[32]>>24);
            int giveweapon=1;
            last_slot=-1; /* original weaponAssignToHome: reset for EVERY weapon */
            record->multiplayer_prepared=1;
            if(item>=0xF0 && item<=0xF7){
                const struct s_mp_weapon_set *weapon=&weapons[item-0xF0];
                last_slot=item-0xF0;
                item=weapon->itemID;
                record->model_id=weapon->propID;
                record->words[1]=((uint32_t)weapon->propID<<16)|(record->words[1]&0xffffU);
                record->words[0]=((uint32_t)(weapon->size*256.0f)<<16)|(record->words[0]&0xffffU);
                record->words[32]=((uint32_t)item<<24)|(record->words[32]&0xffffffU);
                giveweapon=weapon->allowpickup;
                record->multiplayer_weapon_slot=(int8_t)last_slot;
                ++next.weapon_slots;
            }else if(item==ITEM_TOKEN && scenario!=SCENARIO_TLD){
                giveweapon=0;
            }
            if(item==ITEM_UNARMED || !giveweapon) record->multiplayer_omitted=1;
        }else if(record->type==PROPDEF_AMMO && record->word_count==45){
            const struct s_mp_weapon_set *weapon;
            if(last_slot<0 || last_slot>=8) goto invalid;
            weapon=&weapons[last_slot];
            record->multiplayer_prepared=1;
            record->multiplayer_weapon_slot=(int8_t)last_slot;
            record->multiplayer_ammo_type=(int16_t)weapon->ammotype;
            record->multiplayer_ammo_quantity=(int16_t)weapon->ammoamount;
            if(weapon->ammoamount<0 || weapon->ammoamount>INT16_MAX) goto invalid;
            if(weapon->ammoamount==0){
                record->multiplayer_omitted=1;
            }else{
                if(weapon->ammotype<=AMMO_NONE || weapon->ammotype>AMMOTYPE_GLOBAL_MAX) goto invalid;
                word=32+(size_t)weapon->ammotype-1;
                record->words[word]=(record->words[word]&0xffff0000U)|(uint32_t)weapon->ammoamount;
            }
            ++next.ammo_crates;
        }
        if(record->multiplayer_omitted) ++next.omitted;
    }
    *prepared=next;
    return GE_ORIGINAL_STAGE_SUPPLY_OK;
invalid:
    ge_original_stage_multiplayer_setup_close(&next);
    return GE_ORIGINAL_STAGE_SUPPLY_INVALID_DEFINITION;
}
