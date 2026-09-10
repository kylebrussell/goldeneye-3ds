/* Test IO only. Records/replays raw joy input and reads explicit native fields.
 * This witness is deliberately not a complete multiplayer state exporter. */
#include "game/chrObjRandom.h"
#include "random.h"
#include "game/mp_music.h"
extern s32 mission_timer,watch_time_0,g_PlayerTickCount;
extern s32 stageMusicID,dword_CODE_bss_8008C604;
extern s32 music_slot_active_0[4],music_slot_minutes_0[4],music_slot_seconds_0[4];
static const char *tape_directory,*witness_path;
static int tape_record;
static FILE *witness;
static unsigned run_number;
typedef struct InputTape {
    FILE *file;
    unsigned count,repeat;
    GeOriginalInputPad pads[4][2];
} InputTape;
static void tape_open(InputTape *t,unsigned count)
{
    if(!tape_directory)return;
    char path[1024];snprintf(path,sizeof(path),"%s/caves-%u.tape",tape_directory,count);
    t->file=fopen(path,tape_record?"w":"r");assert(t->file);t->count=count;
    if(tape_record)fprintf(t->file,"GEMP_INPUT_V1 %u\n",count);
    else {unsigned roster;assert(fscanf(t->file,"GEMP_INPUT_V1 %u",&roster)==1 && roster==count);}
}
static void tape_flush(InputTape *t)
{
    if(!t->repeat)return;
    fprintf(t->file,"%u",t->repeat);
    for(unsigned slot=0;slot<t->count;++slot)for(unsigned hand=0;hand<2;++hand)
        fprintf(t->file," %d %d %04x",t->pads[slot][hand].stick_x,t->pads[slot][hand].stick_y,t->pads[slot][hand].buttons);
    fputc('\n',t->file);t->repeat=0;
}
static void tape_frame(InputTape *t,GeOriginalInputPad pads[4][2])
{
    if(!t->file)return;
    if(tape_record){
        if(t->repeat && memcmp(t->pads,pads,sizeof(t->pads)))tape_flush(t);
        memcpy(t->pads,pads,sizeof(t->pads));++t->repeat;return;
    }
    if(!t->repeat){
        assert(fscanf(t->file,"%u",&t->repeat)==1 && t->repeat && t->repeat<=2400);
        for(unsigned slot=0;slot<t->count;++slot)for(unsigned hand=0;hand<2;++hand){
            int x,y;unsigned buttons;
            assert(fscanf(t->file,"%d %d %x",&x,&y,&buttons)==3);
            assert(x>=-128 && x<=127 && y>=-128 && y<=127 && buttons<=65535);
            t->pads[slot][hand].stick_x=(int8_t)x;t->pads[slot][hand].stick_y=(int8_t)y;
            t->pads[slot][hand].buttons=(uint16_t)buttons;
        }
    }
    memcpy(pads,t->pads,sizeof(t->pads));--t->repeat;
}
static void tape_close(InputTape *t)
{
    if(!t->file)return;
    if(tape_record)tape_flush(t);
    else {unsigned extra;assert(t->repeat==0 && fscanf(t->file,"%u",&extra)==EOF);}
    assert(!fclose(t->file));
}
static uint32_t float_word(float value)
{ uint32_t bits;memcpy(&bits,&value,sizeof(bits));return bits; }
static int prop_index(const GeOriginalPropPoolStorage *pool,const PropRecord *prop)
{
    if(!prop)return -1;
    uintptr_t offset=(uintptr_t)prop-(uintptr_t)pool->props;
    assert(offset<pool->prop_capacity*sizeof(*prop) && offset%sizeof(*prop)==0);
    return (int)(offset/sizeof(*prop));
}
static void text_hex(FILE *out,const char *text)
{
    if(!text){fputs("null",out);return;}
    fputc('"',out);size_t n=0;
    for(;text[n];++n){assert(n<1024);fprintf(out,"%02x",(unsigned char)text[n]);}
    fputc('"',out);
}
static void witness_frame(Match *match,int tick,unsigned last_slot)
{
    if(!witness)return;
    GeOriginalBossState boss;ge_original_boss_snapshot(&boss);
    GeOriginalPropPoolStorage pool;assert(ge_original_prop_state_pool_storage(&pool));
    GeOriginalMultiplayerSessionState session;assert(ge_original_multiplayer_session_snapshot(&session)==GE_ORIGINAL_MP_SESSION_OK);
    fprintf(witness,"{\"run\":%u,\"roster\":%u,\"tick\":%d,\"state\":{\"last_slot\":%u,\"current_slot\":%d,\"boss\":[%d,%d],\"shuffle\":[%d,%d,%d,%d],\"mission\":%d,\"mission_state\":%d,\"watch\":%d,\"player_tick_count\":%d,\"rng\":[\"%016llx\",\"%016llx\"],\"session\":[%d,%d,%d,%d,%u],\"active_count\":%u,\"players\":[",
        run_number,match->count,tick,last_slot,get_cur_playernum(),boss.current_stage,boss.requested_stage,array_PLAYER_IDs[0],array_PLAYER_IDs[1],array_PLAYER_IDs[2],array_PLAYER_IDs[3],mission_timer,get_mission_state(),watch_time_0,g_PlayerTickCount,
        (unsigned long long)g_randomSeed,(unsigned long long)g_chrObjRandomSeed,
        session.stop_play,session.game_over,session.paused,session.controls_locked,session.results_causes,
        ge_original_prop_state_active_count());
    for(unsigned slot=0;slot<match->count;++slot){
        const struct player *p=ge_original_multiplayer_player(slot);
        const struct player_data *d=&g_playerPlayerData[slot];
        uint32_t offset;const void *animation;char animation_offset[32]="null";
        if(p->players_cur_animation){
            assert(ge_original_guard_animation_legacy_reference(p->players_cur_animation,(uintptr_t)ptr_animation_table,&animation,&offset));
            snprintf(animation_offset,sizeof(animation_offset),"%u",offset);
        }
        if(slot)fputc(',',witness);
        fprintf(witness,"{\"pos\":[%u,%u,%u],\"angles\":[%u,%u],\"health\":[%u,%u],\"life\":[%d,%d,%d,%u,%d,%d],\"menu\":[%d,%d],\"body\":[%s,%d,%u],\"hands\":[",
            float_word(p->prop->pos.x),float_word(p->prop->pos.y),float_word(p->prop->pos.z),
            float_word(p->vv_theta),float_word(p->vv_verta),float_word(p->bondhealth),float_word(p->bondarmour),
            p->bonddead,p->deathanimfinished,p->redbloodfinished,float_word(p->colourfadetimemax60),p->lifestarttime60,p->kills_this_life,
            p->mpmenuon,p->mpmenumode,animation_offset,p->prop->chr->actiontype,p->prop->flags);
        for(unsigned hand=0;hand<2;++hand){const struct hand *h=&p->hands[hand];
            if(hand)fputc(',',witness);
            fprintf(witness,"[%d,%d,%d,%d,%d,%d]",h->weaponnum,h->weapon_ammo_in_magazine,h->weapon_firing_status,h->weapon_action_state,h->weapon_current_animation,h->volley);
        }
        fputs("],\"ammo\":[",witness);
        for(unsigned i=0;i<sizeof(p->ammoheldarr)/sizeof(p->ammoheldarr[0]);++i)fprintf(witness,"%s%d",i?",":"",p->ammoheldarr[i]);
        fprintf(witness,"],\"stats\":{\"kills\":%d,\"kill_counts\":[",d->kill_count);
        for(unsigned i=0;i<4;++i)fprintf(witness,"%s%d",i?",":"",d->kill_counts[i]);
        fputs("],\"shots\":[",witness);for(unsigned i=0;i<7;++i)fprintf(witness,"%s%d",i?",":"",d->shot_count[i]);
        fprintf(witness,"],\"distance\":%u,\"onscreen\":%d,\"innings\":[%d,%d,%d,%d],\"multi_kills\":[%d,%d]},\"awards\":[",
            float_word(d->distance_traveled),d->time_other_players_on_screen,d->min_time_between_kills,d->max_time_between_kills,d->longest_inning,d->shortest_inning,d->most_killed_one_life,d->most_killed_one_time);
        for(unsigned award=0;award<2;++award){const char *text=NULL;assert(ge_original_multiplayer_watch_award_text(slot,award,&text));if(award)fputc(',',witness);text_hex(witness,text);}
        fputs("]}",witness);
    }
    fprintf(witness,"],\"music\":{\"stage\":%d,\"saved_mission\":%d,\"slots\":[",stageMusicID,dword_CODE_bss_8008C604);
    for(unsigned i=0;i<4;++i)fprintf(witness,"%s[%d,%d,%d]",i?",":"",music_slot_active_0[i],music_slot_minutes_0[i],music_slot_seconds_0[i]);
    fputs("]},\"objects\":[",witness);
    for(size_t i=0;i<match->object_count;++i){const PropRecord *p=match->objects[i].prop;
        fprintf(witness,"%s[%zu,%d,%d,%u,%d]",i?",":"",match->objects[i].command_index,prop_index(&pool,p),p->type,p->flags,p->timetoregen);}
    fputs("],\"active_order\":[",witness);unsigned n=0;
    for(const PropRecord *p=pool.active_head;p;p=p->next){assert(n++<pool.prop_capacity);fprintf(witness,"%s%d",n>1?",":"",prop_index(&pool,p));}
    fprintf(witness,"]},\"ownership_history\":{\"pool_lifetime\":%llu,\"allocations_at_reset\":%llu,\"allocations_now\":%llu,\"player_lifetimes\":[",
        (unsigned long long)pool.lifetime,(unsigned long long)pool.allocations_at_reset,(unsigned long long)pool.allocations_now);
    for(unsigned slot=0;slot<match->count;++slot){GeOriginalMultiplayerInventoryStorage storage;assert(ge_original_multiplayer_inventory_storage(slot,&storage));
        fprintf(witness,"%s%llu",slot?",":"",(unsigned long long)storage.lifetime);}
    fputs("]}}\n",witness);
}
