#include "ge_original_multiplayer_watch_render.h"
#include "ge_original_multiplayer_watch.h"
#include "ge_original_multiplayer_session.h"
#include "ge_original_language_text.h"
#include "ge_3ds_original_hud.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/gun.h"
#include "game/mpmenu.h"
#include "assets/obseg/text/LgunE.h"
#include <string.h>
extern GunModelFileRecord gitem_structs[];
extern s32 array_favweapon[4][2],j_text_trigger;
void ge_original_mp_watch_text_measure_exact(s32*,s32*,char*,struct fontchar*,struct font*,s32);
Gfx *ge_original_mp_watch_menu_display_exact(Gfx*);
static struct {
    struct font font;
    GeOriginalMultiplayerWatchRenderSnapshot *out;
    GeOriginalMultiplayerWatchRenderStatus status;
} capture;
static int text_valid(const char *text)
{
    if(!text)return 0;
    for(unsigned i=0;i<GE_ORIGINAL_MP_WATCH_TEXT_CAPACITY;++i) {
        unsigned char c=(unsigned char)text[i];
        if(!c)return 1;
        if(c!='\n' && (c<0x20 || c>0x7e))return 0;
    }
    return 0;
}
static char *checked_text(const char *text)
{
    if(text_valid(text))return (char *)text;
    capture.status=GE_ORIGINAL_MP_WATCH_RENDER_RESOURCE_FAILED;
    /* Prevent unsafe continuation through original formatting after failure;
     * no commands are published when this resource frontier occurs. */
    return "";
}
char *ge_original_mp_watch_render_language(s32 id)
{
    if(id<0 || id>UINT16_MAX || (!((id>>10)==LMPMENU && (id&1023)<=0x23)
        && !((id>>10)==LGUN && (id&1023)<GUN_STR_DF)))return checked_text(NULL);
    return checked_text(ge_original_language_text((uint16_t)id));
}
char *ge_original_mp_watch_render_award(unsigned which)
{
    const char *text=NULL;
    if(!ge_original_multiplayer_watch_award_text(capture.out->player,which,&text))return checked_text(NULL);
    return checked_text(text);
}
char *ge_original_mp_watch_render_favorite(int slot,int hand)
{
    if(slot<0 || slot>=getPlayerCount() || hand<0 || hand>1)return checked_text(NULL);
    int item=array_favweapon[slot][hand];
    if(item<0 || item>ITEM_JOYPAD)return checked_text(NULL);
    return ge_original_mp_watch_render_language(gitem_structs[item].weapon_of_choice_text);
}
struct font *ge_original_mp_watch_render_font(void) {return &capture.font;}
s32 ge_original_mp_watch_render_spacing(void) {return capture.out->viewport.text_spacing;}
s16 ge_original_mp_watch_render_vi(unsigned index)
{
    const GeOriginalMultiplayerWatchViewport *v=&capture.out->viewport;
    switch(index) {
    case 0:return (s16)v->screen_width;case 1:return (s16)v->screen_height;
    case 2:return (s16)v->left;case 3:return (s16)v->top;
    case 4:return (s16)v->width;default:return (s16)v->height;
    }
}
void ge_original_mp_watch_render_measure(s32 *height,s32 *width,char *text,
    struct fontchar *chars,struct font *font,s32 lineheight)
{
    text=checked_text(text);
    ge_original_mp_watch_text_measure_exact(height,width,text,chars,font,lineheight);
}
Gfx *ge_original_mp_watch_render_state(Gfx *gdl,unsigned op)
{
    if(op==0)capture.out->constructed=1;
    else if(op==1)capture.out->restored=1;
    else capture.out->scissor=1;
    return gdl;
}
static Gfx *draw(Gfx *gdl,s32 *x,s32 *y,char *text,struct fontchar *chars,struct font *font,
    u32 rgba,u32 outline,s32 width,s32 height,s32 offset,s32 lineheight,int outlined)
{
    text=checked_text(text);
    if(capture.status!=GE_ORIGINAL_MP_WATCH_RENDER_OK)return gdl;
    if(capture.out->count==GE_ORIGINAL_MP_WATCH_DRAW_CAPACITY) {
        capture.status=GE_ORIGINAL_MP_WATCH_RENDER_CAPACITY;return gdl;
    }
    if(!lineheight)lineheight=chars['['].baseline+chars['['].height;
    GeOriginalMultiplayerWatchTextDraw *d=&capture.out->draws[capture.out->count++];
    d->x=*x;d->y=*y;d->width=width;d->height=height;d->y_offset=offset;
    d->lineheight=lineheight;d->rgba=rgba;d->outline_rgba=outline;d->outlined=(uint8_t)outlined;
    memcpy(d->text,text,strlen(text)+1);
    /* Exact ASCII cursor advancement from textRender/Outlined. Page code is
     * retained, including any later reads of the advanced native x/y. */
    int start=*x;unsigned previous='H';
    for(const unsigned char *p=(const unsigned char *)text;*p;++p) {
        if(*p==' ') {*x+=5;previous='H';}
        else if(*p=='\n') {
            previous='H';
            if(!outlined && capture.out->viewport.overlap_correction>=0 && start==*x)
                *y+=capture.out->viewport.overlap_correction;
            else *y+=lineheight;
            *x=start;
        } else {
            const struct fontchar *c=&chars[*p-0x21],*prev=&chars[previous-0x21];
            *x-=font->kerning[prev->kerningindex*13+c->kerningindex]+capture.out->viewport.text_spacing-1;
            *x+=c->width;previous=*p;
        }
    }
    return gdl;
}
Gfx *ge_original_mp_watch_render_text(Gfx *gdl,s32 *x,s32 *y,char *text,
    struct fontchar *chars,struct font *font,u32 rgba,s32 w,s32 h,u32 offset,s32 lineheight)
{return draw(gdl,x,y,text,chars,font,rgba,0,w,h,(s32)offset,lineheight,0);}
Gfx *ge_original_mp_watch_render_outlined(Gfx *gdl,s32 *x,s32 *y,char *text,
    struct fontchar *chars,struct font *font,u32 rgba,u32 outline,s32 w,s32 h,s32 offset,s32 lineheight)
{return draw(gdl,x,y,text,chars,font,rgba,outline,w,h,offset,lineheight,1);}
GeOriginalMultiplayerWatchRenderStatus ge_original_multiplayer_watch_render_capture(
    uint32_t slot,const GeOriginalMultiplayerWatchViewport *v,const Ge3dsOriginalHudAtlas *atlas,
    GeOriginalMultiplayerWatchRenderSnapshot *out)
{
    GeOriginalMultiplayerSessionNativeView session;GeOriginalMultiplayerWatchState watch;
    if(!out || !v || !atlas || !atlas->ready || capture.out || slot>=4
        || get_cur_playernum()!=(int)slot || ge_original_multiplayer_session_native_view(&session)!=GE_ORIGINAL_MP_SESSION_OK
        || !ge_original_multiplayer_watch_snapshot(&watch) || slot>=session.count
        || v->screen_width<=0 || v->screen_width>4096 || v->screen_height<=0 || v->screen_height>4096
        || v->left<0 || v->top<0 || v->width<=0 || v->height<=0
        || v->width>v->screen_width-v->left || v->height>v->screen_height-v->top
        || v->text_spacing<-16 || v->text_spacing>16 || v->overlap_correction<-1 || v->overlap_correction>128)
        {if(out){out->count=0;out->visible=0;}return GE_ORIGINAL_MP_WATCH_RENDER_INVALID;}
    GeOriginalMultiplayerWatchViewport viewport=*v;
    memset(out,0,sizeof(*out));
    if(j_text_trigger)return GE_ORIGINAL_MP_WATCH_RENDER_RESOURCE_FAILED;
    if(g_CurrentPlayer->mpmenuon && (g_CurrentPlayer->mpmenumode<MENU_GOWOC
        || g_CurrentPlayer->mpmenumode>MENU_FINISHED))return GE_ORIGINAL_MP_WATCH_RENDER_INVALID;
    for(unsigned i=0;i<169;++i)capture.font.kerning[i]=atlas->kerning[i];
    for(unsigned i=0;i<94;++i) {
        const Ge3dsOriginalHudGlyph *g=&atlas->glyphs[i];
        if(g->width>15 || !g->width || g->height>15 || !g->height || g->kerning_index>=13)
            return GE_ORIGINAL_MP_WATCH_RENDER_RESOURCE_FAILED;
        capture.font.chars[i]=(struct fontchar){i+0x21,g->baseline,g->height,g->width,g->kerning_index,NULL};
    }
    out->viewport=viewport;out->player=slot;capture.out=out;capture.status=GE_ORIGINAL_MP_WATCH_RENDER_OK;
    (void)ge_original_mp_watch_menu_display_exact(NULL);
    GeOriginalMultiplayerWatchRenderStatus status=capture.status;capture.out=NULL;
    if(status!=GE_ORIGINAL_MP_WATCH_RENDER_OK){out->count=0;out->visible=0;return status;}
    for(unsigned i=0;i<out->count;++i)if(out->draws[i].text[0])out->visible=1;
    return status;
}
