#!/usr/bin/env python3
"""Exercise the generated collision adapter against authored-address geometry."""
from pathlib import Path
import importlib.util
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('hit', repo/'scripts/extract_guard_bullet_hit_slice.py')
hit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hit)
prop = (repo/'src/game/propobj.c').read_text()
body = hit.apply_native_model_hit_abi(hit.find_function(prop, 'bgTestHitOnObj'), 'bgTestHitOnObj')
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ge_native_model_hit_vertices.h"
typedef int8_t s8; typedef uint8_t u8; typedef int16_t s16; typedef uint16_t u16;
typedef int32_t s32; typedef uint32_t u32; typedef float f32; typedef double f64;
typedef union coord3d { struct { float x,y,z; }; float f[3]; } coord3d;
typedef struct { int32_t x,y,z; } BoundVec;
typedef struct Vertex { struct { int16_t x,y,z; } coord; int16_t index;
    uintptr_t link; uint32_t rgba; } Vertex;
typedef struct { uint8_t bytes[8]; } Gfx;
typedef struct HitThing { coord3d hitpos,normal; Vertex *vtx0,*vtx1,*vtx2;
    Gfx *tricmd; int16_t unk28,texturenum,tileformat,tilesize; } HitThing;
#define TRUE 1
#define FALSE 0
#define G_VTX 4
#define G_ENDDL 0xb8
#define G_TRI1 0xbf
#define G_TRI4 0xb1
#define G_SETTIMG 0xfd
#define M_U32_MAX_VALUE_F 4294967295.0f
#define GE_PORT_MODEL_HIT_NATIVE_ABI 1
static GeNativeModelHitVertices ge_port_model_hit_vertices;
static Vertex *ge_port_model_hit_vertex(const GeNativeModelHitVertices *r,Vertex *v,unsigned s) {
    size_t i; return v && ge_native_model_hit_vertex_index(r,s,&i) ? v+i : NULL;
}
static uint32_t be32(const void *ptr) { const uint8_t *p=ptr;
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
static uint16_t be16(const void *ptr) { const uint8_t *p=ptr;return (uint16_t)(p[0]<<8|p[1]); }
#define GE_MODEL_HIT_U32(p,i) be32((const uint8_t *)(p)+4*(i))
#define GE_MODEL_HIT_U16(p,i) be16((const uint8_t *)(p)+2*(i))
#define GE_MODEL_HIT_GDL_NEXT(p) ((p)+1)
#define GE_MODEL_HIT_GDL_PREV(p) ((p)-1)
static int ge_port_model_hit_texture_number(const Gfx *p) { (void)p; return -1; }
static BoundVec D_8003204C={32767,32767,32767}, D_80032058={-32768,-32768,-32768};
static BoundVec D_80032070={32767,32767,32767}, D_8003207C={-32768,-32768,-32768};
static coord3d D_80032064, D_80032088;
'''
program = prefix + hit.find_function((repo/'src/game/bg.c').read_text(), 'bgTestRayIntersectsBbox') + '\n'
program += hit.find_function((repo/'src/game/line_tri_intersect.c').read_text(), 'intersectRayTriangle') + '\n' + body
program += r'''
static void put32(void *ptr,uint32_t value) { uint8_t *p=ptr;
    p[0]=value>>24;p[1]=value>>16;p[2]=value>>8;p[3]=value; }
static void command(Gfx *g,uint32_t a,uint32_t b) {put32(g->bytes,a);put32(g->bytes+4,b);}
static void compare_ray(Vertex *vertices,Gfx *commands,coord3d start,coord3d dir) {
    coord3d end={{start.x+dir.x*32767,start.y+dir.y*32767,start.z+dir.z*32767}}, zero={{0,0,0}};
    HitThing actual={0}, expected={0};
    int want=intersectRayTriangle(vertices,vertices+1,vertices+2,&zero,&start,&end,&dir,&expected);
    int got=bgTestHitOnObj(&start,&end,&dir,commands,NULL,vertices,&actual);
    assert(got==want);
    if(got) {assert(memcmp(&actual.hitpos,&expected.hitpos,sizeof(coord3d))==0);
        assert(memcmp(&actual.normal,&expected.normal,sizeof(coord3d))==0);
        assert(actual.vtx0==vertices && actual.vtx1==vertices+1 && actual.vtx2==vertices+2);}
}
static int reference_mesh(Vertex *vertices,const uint8_t *blob,coord3d *start,
    coord3d *end,coord3d *dir, HitThing *best) {
    int found=0, base=0;
    float distance=M_U32_MAX_VALUE_F;
    coord3d zero={{0,0,0}};
    for(unsigned list=0;list<2;++list) {
        unsigned cursor=list?0x8d8:0x780;
        for(;blob[cursor]!=G_ENDDL;cursor+=8) {
            const uint8_t *c=blob+cursor; unsigned opcode=c[0];
            if(opcode==G_VTX) {
                base=(int)((be32(c+4)&0xffffff)-0x100)/16-(c[1]&15);
                continue;
            }
            if(opcode!=G_TRI1 && opcode!=G_TRI4) continue;
            /* Independently decode Rare's four packed triangle slots. */
            unsigned indices[4][3]={{c[7]&15,c[7]>>4,c[3]&15},
                {c[6]&15,c[6]>>4,c[3]>>4}, {c[5]&15,c[5]>>4,c[2]&15},
                {c[4]&15,c[4]>>4,c[2]>>4}};
            if(opcode==G_TRI1)for(unsigned j=0;j<3;++j)indices[0][j]=c[5+j]/10;
            for(unsigned t=0;t<(opcode==G_TRI1?1U:4U);++t) {
                Vertex *v[3]; HitThing h={0};
                for(unsigned j=0;j<3;++j) {
                    int i=base+(int)indices[t][j];assert(i>=0&&i<100);v[j]=vertices+i;
                }
                if(!intersectRayTriangle(v[0],v[1],v[2],&zero,start,end,dir,&h))continue;
                float dx=(int)h.hitpos.x-(int)start->x,dy=(int)h.hitpos.y-(int)start->y;
                float dz=(int)h.hitpos.z-(int)start->z, d=dx*dx+dy*dy+dz*dz;
                if(d<distance) {distance=d;*best=h;best->vtx0=v[0];best->vtx1=v[1];best->vtx2=v[2];found=1;}
            }
        }
    }
    return found;
}
static void authored_weapon(const char *path) {
    FILE *f=fopen(path,"rb");assert(f); uint8_t blob[2352];
    assert(fread(blob,1,sizeof(blob),f)==sizeof(blob));fclose(f);
    assert(be32(blob+0x740)==0x05000780 && be32(blob+0x744)==0x050008d8);
    assert(be32(blob+0x74c)==0x05000100 && be16(blob+0x750)==100);
    Vertex *v=calloc(100,sizeof(*v)); assert(v);
    for(unsigned i=0;i<100;++i) {
        const uint8_t *p=blob+0x100+i*16;
        v[i].coord.x=(int16_t)be16(p);v[i].coord.y=(int16_t)be16(p+2);v[i].coord.z=(int16_t)be16(p+4);
    }
    uint32_t random=7139;
    for(unsigned i=0;i<10001;++i) {
        coord3d start,dir,end;HitThing a={0},b={0};
        for(unsigned j=0;j<3;++j) {
            random=random*1664525U+1013904223U;start.f[j]=(int)(random%1001)-500;
            random=random*1664525U+1013904223U;dir.f[j]=((int)(random%2001)-1000)/1000.0f;
        }
        if(i==0) { /* Captured first divergent shot, before any gameplay difference. */
            start=(coord3d){{-189.98919677734375f,3729.6630859375f,1801.111083984375f}};
            dir=(coord3d){{0.9975886344909668f,-8.9271240234375f,-4.394455909729004f}};
        }
        for(unsigned j=0;j<3;++j)end.f[j]=start.f[j]+dir.f[j]*32767.0f;
        ge_port_model_hit_vertices=(GeNativeModelHitVertices){.count=100,.blob_offset=256,.blob_offset_known=1};
        int want=reference_mesh(v,blob,&start,&end,&dir,&b);
        int got=bgTestHitOnObj(&start,&end,&dir,(Gfx *)(blob+0x780),(Gfx *)(blob+0x8d8),v,&a);
        assert(want==got);if(i==0)assert(!got);
        if(got) {assert(memcmp(&a.hitpos,&b.hitpos,sizeof(coord3d))==0);
            assert(a.vtx0==b.vtx0 && a.vtx1==b.vtx1 && a.vtx2==b.vtx2);}
    }
    free(v);puts("Authored KF7: captured divergent shot is a weapon miss; 10000 independent mesh comparisons exact");
}
int main(int argc,char **argv) {
    unsigned trials=0;
    /* Only three allocated vertices: the old double offset immediately trips
       ASan, while correct hits retain pointers to durable native vertices. */
    Vertex *vertices=calloc(3,sizeof(*vertices)); assert(vertices);
    vertices[0].coord.x=-20;vertices[0].coord.y=-20;vertices[0].coord.z=100;
    vertices[1].coord.x=20;vertices[1].coord.y=-20;vertices[1].coord.z=100;
    vertices[2].coord.x=0;vertices[2].coord.y=20;vertices[2].coord.z=100;
    for(unsigned segment=4;segment<=5;++segment) for(unsigned origin=0;origin<=4096;origin+=256)
    for(unsigned first=0;first<=13;++first) {
        Gfx g[3]; command(g,0x04200030U|(first<<16),(segment<<24)|(segment==5?origin:0));
        command(g+1,0xbf000000U,(first*10U<<16)|((first+1U)*10U<<8)|((first+2U)*10U));
        command(g+2,0xb8000000U,0);
        ge_port_model_hit_vertices=(GeNativeModelHitVertices){.count=3,.blob_offset=origin,.blob_offset_known=1};
        for(int y=-30;y<=30;y+=3)for(int x=-30;x<=30;x+=3) {
            compare_ray(vertices,g,(coord3d){{x,y,0}},(coord3d){{0,0,1}});++trials;
        }
        /* TRI4 has three degenerate padding triangles after the real one. */
        command(g+1,0xb1000000U|(first+2U),((first+1U)<<4)|first);
        for(int x=-30;x<=30;++x) {
            compare_ray(vertices,g,(coord3d){{x,0,0}},(coord3d){{0,0,1}});++trials;
        }
    }
    GeNativeModelHitVertices r={.count=3,.blob_offset=256,.blob_offset_known=1}; size_t index=999;
    assert(!ge_native_model_hit_vertices_load(&r,0x04200030,0x050000f0));
    assert(!ge_native_model_hit_vertices_load(&r,0x04200030,0x05000110));
    assert(!ge_native_model_hit_vertices_load(&r,0x04200030,0x05000101));
    assert(!ge_native_model_hit_vertices_load(&r,0x042e0030,0x05000100));
    assert(!ge_native_model_hit_vertices_load(&r,0x04200020,0x05000100));
    assert(!ge_native_model_hit_vertices_load(&r,0x04200030,0x06000100));
    assert(!ge_native_model_hit_vertex_index(&r,0,&index));
    assert(ge_native_model_hit_vertices_load(&r,0x042d0030,0x05000100));
    assert(!ge_native_model_hit_vertex_index(&r,0,&index));
    assert(ge_native_model_hit_vertex_index(&r,15,&index)&&index==2);
    assert(!ge_native_model_hit_vertex_index(&r,16,&index));
    r.blob_offset_known=0;
    assert(!ge_native_model_hit_vertices_load(&r,0x04200030,0x05000100));
    assert(ge_native_model_hit_vertices_load(&r,0x04200030,0x04000000));
    free(vertices);
    if(argc>1)authored_weapon(argv[1]);
    printf("Native collision vertices: %u exact ray cases, segment origins, partial slots, TRI1/TRI4, bounds; sizeof(Vertex)=%zu\n",trials,sizeof(Vertex));
}
'''
with tempfile.TemporaryDirectory(prefix='ge-native-hit-') as tmp:
    directory=Path(tmp); source=directory/'hit.c'; source.write_text(program)
    for name, text in [('wide',program), ('n64_stride',program.replace('uintptr_t link;', 'uint32_t link;'))]:
        source.write_text(text)
        binary=directory/name
        subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',
            '-Wno-unused-variable','-Wno-unused-but-set-variable','-Wno-empty-body',
            '-fsanitize=address,undefined','-I',str(repo/'port/include'),str(source),'-o',str(binary)],check=True)
        weapon=repo/'build/u/assets/obseg/prop/PchrkalashZ.bin'
        subprocess.run([str(binary), *([str(weapon)] if weapon.exists() else [])],check=True)
