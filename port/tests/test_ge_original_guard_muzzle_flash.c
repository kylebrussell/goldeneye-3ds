#include "ge_original_guard_muzzle_flash.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int closef(float a, float b) { return fabsf(a - b) < 0.001f; }

int main(void)
{
    GeOriginalPitemModelGunfire gunfire;
    GeOriginalGuardMuzzleFlashPublication publication;
    float matrix[4][4] = {
        {1.0f,0.0f,0.0f,0.0f}, {0.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,1.0f,0.0f}, {0.0f,0.0f,-1000.0f,1.0f},
    };
    memset(&gunfire, 0, sizeof(gunfire));
    gunfire.visible=1U;gunfire.offset[0]=-407.0f;
    gunfire.offset[1]=-0.5f;gunfire.offset[2]=2.5f;
    gunfire.size[0]=533.0f;gunfire.size[1]=213.0f;gunfire.size[2]=217.0f;
    gunfire.image_id=2119U;gunfire.image_width=32U;gunfire.image_height=32U;
    assert(ge_original_guard_muzzle_flash_build(&gunfire,matrix,1.0f,
        0U,32767,0,&publication));
    assert(publication.gunfire.image_id==2119U);
    assert(publication.vertices[0].texture_s==-211);
    assert(publication.vertices[0].texture_t==512);
    assert(publication.vertices[1].texture_s==512);
    assert(publication.vertices[1].texture_t==-211);
    assert(publication.vertices[2].texture_s==1235);
    assert(publication.vertices[2].texture_t==512);
    assert(memcmp(&publication.vertices[0],&publication.vertices[3],
                  sizeof(publication.vertices[0]))==0);
    assert(memcmp(&publication.vertices[2],&publication.vertices[4],
                  sizeof(publication.vertices[2]))==0);
    /* Translation also changes the canonical camera-facing solution; the
     * resulting attached quad must remain finite and nondegenerate. */
    matrix[3][0]=12.0f;matrix[3][1]=-7.0f;
    {
        GeOriginalGuardMuzzleFlashPublication translated;
        assert(ge_original_guard_muzzle_flash_build(&gunfire,matrix,1.0f,
            0U,32767,0,&translated));
        assert(isfinite(translated.vertices[0].position[0]));
        assert(isfinite(translated.vertices[0].position[1]));
        assert(!closef(translated.vertices[0].position[0],
                       translated.vertices[2].position[0])
            || !closef(translated.vertices[0].position[1],
                       translated.vertices[2].position[1]));
    }
    gunfire.visible=0U;
    assert(!ge_original_guard_muzzle_flash_build(&gunfire,matrix,1.0f,
        0U,32767,0,&publication));
    puts("canonical dogfnegx muzzle quad publication passed");
    return 0;
}
