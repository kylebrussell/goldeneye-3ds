#define GE_MP_WATCH_NO_MAIN
#include "test_ge_original_multiplayer_watch.c"
#include "ge_original_multiplayer_watch_render.h"
#include "ge_3ds_original_hud.h"
#include <math.h>
#include "game/gun.h"
#include "ge_original_language_text.h"
extern GunModelFileRecord gitem_structs[];
static Ge3dsOriginalHudAtlas font;
static Ge3dsOriginalHudDrawList drawing;
static GeOriginalMultiplayerWatchRenderSnapshot rendered;
s32 j_text_trigger;
static unsigned captured_pages;
static void capture_page(unsigned slot)
{
    GeOriginalMultiplayerWatchViewport v={320,240,active_count==2?0:(int)(slot%2)*160,
        active_count==2?(int)slot*120:(int)(slot/2)*120,active_count==2?320:160,120,0,-1};
    assert(ge_original_multiplayer_select_player(slot));
    struct player *copy=malloc(active_count*sizeof(*copy));assert(copy);
    struct player_data permanent[4];GeOriginalMultiplayerWatchState before,after;
    for(unsigned i=0;i<active_count;++i)copy[i]=*g_playerPointers[i];
    memcpy(permanent,g_playerPlayerData,sizeof(permanent));assert(ge_original_multiplayer_watch_snapshot(&before));
    uint64_t rng=g_randomSeed,chr_rng=g_chrObjRandomSeed;struct player *selected=g_CurrentPlayer;
    assert(ge_original_multiplayer_watch_render_capture(slot,&v,&font,&rendered)==GE_ORIGINAL_MP_WATCH_RENDER_OK);
    assert(ge_original_multiplayer_watch_snapshot(&after));assert(!memcmp(&before,&after,sizeof(before)));
    for(unsigned i=0;i<active_count;++i)assert(!memcmp(&copy[i],g_playerPointers[i],sizeof(copy[i])));
    assert(!memcmp(permanent,g_playerPlayerData,sizeof(permanent))&&g_CurrentPlayer==selected
        &&rng==g_randomSeed&&chr_rng==g_chrObjRandomSeed);free(copy);
    GeOriginalMultiplayerWatchRenderSnapshot saved=rendered;
    for(int eye=0;eye<3;++eye) {
        int drawn=ge_3ds_original_multiplayer_watch_build_draw_list(&font,&rendered,(400.0f-v.width)/2-v.left,(240.0f-v.height)/2-v.top,&drawing);
        if(!drawn){fprintf(stderr,"mode%d slot%u viewport%d,%d %dx%d screen%dx%d space%d overlap%d count%u\n",g_CurrentPlayer->mpmenumode,slot,rendered.viewport.left,rendered.viewport.top,rendered.viewport.width,rendered.viewport.height,rendered.viewport.screen_width,rendered.viewport.screen_height,rendered.viewport.text_spacing,rendered.viewport.overlap_correction,rendered.count);
        for(unsigned n=0;n<rendered.count;++n){const GeOriginalMultiplayerWatchTextDraw *d=&rendered.draws[n];fprintf(stderr,"draw%u xy%d,%d size%d,%d line%d off%d outline%u %s\n",n,d->x,d->y,d->width,d->height,d->lineheight,d->y_offset,d->outlined,d->text);}}
        assert(drawn);
        assert(!memcmp(&saved,&rendered,sizeof(saved))&&rng==g_randomSeed);
        for(size_t i=0;i<drawing.glyph_vertex_count;++i) {
            const Ge3dsOriginalHudVertex *p=&drawing.vertices[i];
            assert(isfinite(p->x)&&isfinite(p->y)&&p->r>=0&&p->r<=1&&p->a>=0&&p->a<=1);
        }
    }
    ++captured_pages;
}
/* Optional owned CPU preview inputs: no native pointers or font substitutes. */
static void dump_page(const char *name)
{
    const char *dir=getenv("GE_MP_WATCH_RENDER_DUMP_DIR");
    if(!dir || !*dir)return;
    char path[1024];int length=snprintf(path,sizeof(path),"%s/%up-%s.json",dir,active_count,name);
    assert(length>0 && (size_t)length<sizeof(path));FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"{\"players\":%u,\"mode\":%d,\"scenario\":%d,\"gameover\":%d,\"vertices\":[",
        active_count,g_CurrentPlayer->mpmenumode,scenario,g_gameOverFlag);
    for(size_t i=0;i<drawing.glyph_vertex_count;++i) {
        const Ge3dsOriginalHudVertex *v=&drawing.vertices[i];
        fprintf(f,"%s[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g]",i?",":"",
            v->x,v->y,v->u,v->v,v->r,v->g,v->b,v->a);
    }
    fprintf(f,"],\"texts\":[");
    for(unsigned i=0;i<rendered.count;++i) {
        fprintf(f,"%s\"",i?",":"");
        for(const char *p=rendered.draws[i].text;*p;++p) {
            if(*p=='\n')fputs("\\n",f);
            else {if(*p=='\"'||*p=='\\')fputc('\\',f);fputc(*p,f);}
        }
        fputc('"',f);
    }
    fputs("]}\n",f);assert(!fclose(f));
    length=snprintf(path,sizeof(path),"%s/bank-gothic.pgm",dir);
    assert(length>0 && (size_t)length<sizeof(path));f=fopen(path,"wb");assert(f);
    fprintf(f,"P5\n128 128\n255\n");
    for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;++x)
        fputc(ge_3ds_original_hud_atlas_alpha(&font,x,y),f);
    assert(!fclose(f));
}
static const GeOriginalMultiplayerWatchTextDraw *find_text(const char *text)
{
    for(unsigned i=0;i<rendered.count;++i)if(!strcmp(rendered.draws[i].text,text))return &rendered.draws[i];
    return NULL;
}
static void pages(unsigned count)
{
    assert(ge_3ds_original_hud_build_bank_gothic_atlas(&font));
    assert(font.glyphs['['].baseline == -1 && font.glyphs['['].height == 14);
    assert(ge_original_multiplayer_watch_awards(0)==GE_ORIGINAL_MP_WATCH_OK);
    for(int over=0;over<2;++over)for(unsigned slot=0;slot<count;++slot)for(int mode=MENU_GOWOC;mode<=MENU_FINISHED;++mode) {
        g_gameOverFlag=over;g_playerPointers[slot]->mpmenuon=1;g_playerPointers[slot]->mpmenumode=mode;
        g_playerPointers[slot]->mpquitconfirm=slot%2;capture_page(slot);
        assert(rendered.constructed&&rendered.restored);
        for(unsigned n=0;n<rendered.count;++n)assert(rendered.draws[n].lineheight==13);
        if(!slot && !over) {
            if(mode==MENU_GOWOC)dump_page("play");
            if(mode==MENU_KILLS)dump_page("kills");
            if(mode==MENU_SCORES)dump_page("scores");
            if(mode==MENU_EXIT_CONFIRM)dump_page("confirm");
        }
        if(!slot && over && mode==MENU_GOWOC)dump_page("awards");
        if(mode==MENU_FINISHED)assert(!rendered.visible&&!drawing.visible);
        if(mode==MENU_KILLS){char label[32];snprintf(label,sizeof(label),"P%u KILLS",slot+1);assert(find_text(label));}
        if(mode==MENU_LOSSES){char label[32];snprintf(label,sizeof(label),"P%u LOSSES",slot+1);assert(find_text(label));}
        if(mode==MENU_GOWOC){assert(find_text("Weapon of choice:"));
            assert(find_text(ge_original_language_text(gitem_structs[ITEM_WPPK+slot].weapon_of_choice_text)));
            if(!slot)assert(find_text("PP7 Special Issue\n"));}
        if(mode==MENU_EXIT_CONFIRM){assert(find_text("cancel")&&find_text("confirm"));
            assert(find_text(slot%2?"confirm":"cancel")->outlined);}
    }
    g_gameOverFlag=0;g_pausedFlag=1;who_paused=count-1;
    g_playerPointers[count-1]->mpmenumode=MENU_PAUSE;capture_page(count-1);assert(find_text("PAUSED")->outlined);
    g_playerPointers[0]->mpmenumode=MENU_PAUSE;capture_page(0);assert(!find_text("PAUSED")->outlined);dump_page("paused-other");
    g_pausedFlag=0;
    /* Exact directed score subtraction retains negative values and highlight. */
    memset(g_playerPlayerData[0].kill_counts,0,sizeof(g_playerPlayerData[0].kill_counts));
    g_playerPlayerData[0].kill_counts[0]=3;g_playerPointers[0]->mpmenumode=MENU_SCORES;
    capture_page(0);assert(find_text("-3")&&find_text("-3")->rgba==0xa0ffa0f0);
    if(count==4) {
        GeOriginalMultiplayerConfig config={4,SCENARIO_2v2,1,LEVELID_TEMPLE,{0,0,1,1}};
        assert(ge_original_multiplayer_configure(&config)==GE_ORIGINAL_MP_OK);
        capture_page(0);assert(find_text("-3")&&find_text("-3")->rgba==0xffa0a0f0);dump_page("team-scores");
    }
    /* Canonical YOLT suppresses score/rank while alive, rank after stop. */
    scenario=SCENARIO_YOLT;g_stopPlayFlag=0;g_gameOverFlag=0;capture_page(0);
    assert(!find_text("SCORES")&&!find_text("Rank: 1st"));
    g_stopPlayFlag=1;capture_page(0);assert(find_text("Rank: 1st")&&!find_text("SCORES"));g_stopPlayFlag=0;
    for(unsigned i=0;i<count;++i)g_playerPlayerData[i].kill_counts[0]=0;
    g_CurrentPlayer->mpmenuon=0;g_CurrentPlayer->bonddead=1;g_CurrentPlayer->deathanimfinished=1;g_CurrentPlayer->redbloodfinished=1;
    capture_page(0);assert(rendered.scissor&&find_text("press start\n"));dump_page("dead-continue");
    g_playerPlayerData[1].kill_counts[0]=2;capture_page(0);assert(!rendered.visible&&!rendered.count);
    g_playerPlayerData[1].kill_counts[0]=1;g_CurrentPlayer->deathanimfinished=0;capture_page(0);assert(!rendered.visible);
    g_CurrentPlayer->deathanimfinished=1;g_gameOverFlag=1;capture_page(0);assert(!rendered.visible);
    /* Snapshot has no native text pointer: changing another native page after
     * capture cannot change its formatted strings or geometry. */
    g_CurrentPlayer->mpmenuon=1;g_CurrentPlayer->mpmenumode=MENU_KILLS;capture_page(0);
    GeOriginalMultiplayerWatchRenderSnapshot saved=rendered;g_CurrentPlayer->mpmenumode=MENU_LOSSES;
    assert(ge_3ds_original_multiplayer_watch_build_draw_list(&font,&rendered,40,0,&drawing));assert(!memcmp(&saved,&rendered,sizeof(saved)));
}
static void boundaries(void)
{
    GeOriginalMultiplayerWatchRenderSnapshot saved=rendered;
    GeOriginalMultiplayerWatchViewport v={320,240,0,0,320,120,0,-1};
    assert(ge_original_multiplayer_watch_render_capture(1,&v,&font,&rendered)==GE_ORIGINAL_MP_WATCH_RENDER_INVALID);
    assert(!rendered.count&&!rendered.visible);
    j_text_trigger=1;
    assert(ge_original_multiplayer_watch_render_capture(0,&v,&font,&rendered)==GE_ORIGINAL_MP_WATCH_RENDER_RESOURCE_FAILED);
    j_text_trigger=0;g_CurrentPlayer->mpmenumode=MENU_GOWOC;
    int32_t old=g_CurrentPlayer->ptr_text_first_mp_award;g_CurrentPlayer->ptr_text_first_mp_award=0x12345678;
    assert(ge_original_multiplayer_watch_render_capture(0,&v,&font,&rendered)==GE_ORIGINAL_MP_WATCH_RENDER_RESOURCE_FAILED);
    assert(!rendered.count&&!rendered.visible);g_CurrentPlayer->ptr_text_first_mp_award=old;
    rendered=saved;rendered.draws[0].width=INT32_MAX;
    assert(!ge_3ds_original_multiplayer_watch_build_draw_list(&font,&rendered,0,0,&drawing)&&!drawing.visible);
    rendered=saved;
}
int main(int argc,char **argv)
{
    unsigned count=argc>1?(unsigned)atoi(argv[1]):2,local=argc>2?(unsigned)atoi(argv[2]):0;
    start_watch(count,local%count);pages(count);
    if(argc>3){FILE *f=fopen(argv[3],"wb");assert(f);for(unsigned i=0;i<rendered.count;++i)
        fprintf(f,"%d %d %08x %u %s\n",rendered.draws[i].x,rendered.draws[i].y,rendered.draws[i].rgba,rendered.draws[i].outlined,rendered.draws[i].text);fclose(f);}
    boundaries();ge_original_multiplayer_players_destroy();
    printf("native watch render %up: %u source pages, real language/fonts, signed/team scores, rank/YOLT/continue, deep copy and draw-only repeated eyes passed\n",count,captured_pages);
    return 0;
}
