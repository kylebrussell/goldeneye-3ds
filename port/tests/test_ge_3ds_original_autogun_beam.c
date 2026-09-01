#include "ge_3ds_original_autogun_beam.h"

#include <bondconstants.h>

#include <assert.h>
#include <math.h>
#include <string.h>

static int closef(float left,float right)
{return fabsf(left-right)<0.0001f;}

static GeOriginalStageAutogunBeamSnapshot beam_at(float start)
{
    GeOriginalStageAutogunBeamSnapshot beam={0};
    beam.origin[0]=0.0f;beam.origin[1]=0.0f;beam.origin[2]=0.0f;
    beam.direction[0]=1.0f;
    beam.maximum_distance=100.0f;
    beam.speed=20.0f;
    beam.minimum_distance=20.0f;
    beam.distance=start;
    beam.weapon_id=ITEM_FNP90;
    beam.age=2;
    beam.active=1U;
    return beam;
}

int main(void)
{
    const Ge3dsOriginalAutogunBeamTextureUv uv={
        {0.1f,0.2f},{0.9f,0.2f},{0.1f,0.8f},{0.9f,0.8f}
    };
    const float viewer[3]={0.0f,0.0f,10.0f};
    GeOriginalStageAutogunBeamSnapshot beams[2];
    GeOriginalStageAutogunBeamSnapshot unchanged[2];
    Ge3dsOriginalAutogunBeamDrawList draw;
    const Ge3dsOriginalAutogunBeamVertex *v;

    assert(strcmp(GE_3DS_ORIGINAL_AUTOGUN_BEAM_TEXTURE_SOURCE,
                  "FLAREORANGELINE.bin")==0);
    assert(ge_3ds_original_autogun_beams_build_draw_list(
        NULL,0U,viewer,&uv,&draw));
    assert(draw.source_count==0U&&draw.active_count==0U
           &&draw.vertex_count==0U);

    beams[0]=beam_at(10.0f);
    beams[1]=beam_at(0.0f);beams[1].active=0U;beams[1].age=-1;
    memcpy(unchanged,beams,sizeof(beams));
    assert(ge_3ds_original_autogun_beams_build_draw_list(
        beams,2U,viewer,&uv,&draw));
    assert(memcmp(beams,unchanged,sizeof(beams))==0);
    assert(draw.source_count==2U&&draw.active_count==1U
           &&draw.vertex_count==6U);
    v=draw.vertices;
    /* Exact 0.1 model scale: origin+10 along X, radius 30 -> 3 units,
     * and the far edge tapers to 2.7 units at X=30. */
    assert(closef(v[0].x,10.0f)&&closef(v[0].y,-3.0f)
           &&closef(v[0].z,0.0f));
    assert(closef(v[1].x,30.0f)&&closef(v[1].y,-2.7f));
    assert(closef(v[2].x,30.0f)&&closef(v[2].y,2.7f));
    assert(closef(v[5].x,10.0f)&&closef(v[5].y,3.0f));
    assert(closef(v[0].u,0.9f)&&closef(v[0].v,0.2f)
           &&closef(v[1].u,0.9f)&&closef(v[1].v,0.8f)
           &&closef(v[2].u,0.1f)&&closef(v[2].v,0.8f)
           &&closef(v[5].u,0.1f)&&closef(v[5].v,0.2f));
    assert(v[0].red==1.0f&&v[0].green==1.0f&&v[0].blue==1.0f
           &&v[0].alpha==1.0f);

    /* Negative initial distance shortens the leading segment exactly; a
     * positive start which reaches maxdist is capped at the impact point. */
    beams[0]=beam_at(-10.0f);
    assert(ge_3ds_original_autogun_beams_build_draw_list(
        beams,1U,viewer,&uv,&draw));
    assert(closef(draw.vertices[0].x,0.0f)
           &&closef(draw.vertices[1].x,10.0f));
    beams[0]=beam_at(90.0f);
    beams[0].minimum_distance=30.0f;
    assert(ge_3ds_original_autogun_beams_build_draw_list(
        beams,1U,viewer,&uv,&draw));
    assert(closef(draw.vertices[0].x,90.0f)
           &&closef(draw.vertices[1].x,100.0f));

    beams[0]=beam_at(0.0f);beams[0].weapon_id=ITEM_LASER;
    assert(!ge_3ds_original_autogun_beams_build_draw_list(
        beams,1U,viewer,&uv,&draw));
    assert(draw.active_count==1U&&draw.vertex_count==0U);
    assert(!ge_3ds_original_autogun_beams_build_draw_list(
        beams,GE_3DS_ORIGINAL_AUTOGUN_BEAM_CAPACITY+1U,
        viewer,&uv,&draw));
    assert(!ge_3ds_original_autogun_beams_build_draw_list(
        beams,1U,NULL,&uv,&draw));
    return 0;
}
