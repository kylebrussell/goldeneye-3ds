#!/usr/bin/env python3
"""Check the captured coplanar road arrow against the RDP minimum window.

This uses captured float32 projection and completed hardware depth, not a claim
of exact PICA rasterization. No texture sampling or final color is simulated.
"""
from pathlib import Path
import math
import re
import struct
import sys
repo = Path(__file__).resolve().parents[2]
base = Path(sys.argv[1]) if len(sys.argv) > 1 else repo/'build/hardware-captures/gate-decals-20260912-122714/3ds/goldeneye-3ds'
tick = '57251717148'
block = (base/'render-audit.log').read_text().split('gpu_snapshot=1 tick='+tick)[1].split('gpu_snapshot_end=')[0]
runs = [dict(re.findall(r'(\w+)=([^ ]+)', line)) for line in block.splitlines() if line.startswith('gpu_run=')]
arrow = runs[6]
assert (arrow['texture'], arrow['room'], arrow['zmode'], arrow['ztest'], arrow['zwrite']) == ('56','111','3','1','0')
assert 'context=00000f71' in next(line for line in block.splitlines() if line.startswith('gpu_depth_draw=6 '))
vertices = list(struct.iter_unpack('<9f', (base/f'render-{tick}.vertices.bin').read_bytes()))
depth = struct.unpack('<96000I', (base/f'render-{tick}.left.depth.bin').read_bytes())
matrix = [list(map(float,re.search(rf'^gpu_matrix=0,{r},(.*)$',block,re.M)[1].split(','))) for r in range(4)]
header = (repo/'platform/3ds/include/ge_3ds_material.h').read_text()
assert '#define GE_3DS_DECAL_MIN_DEPTH_TOLERANCE (8.0f / (1022.0f * 256.0f))' in header
tolerance = 8/(1022*256)
def morton3(n):
    return (n&1)|((n&2)<<1)|((n&4)<<2)
def actual(x,y):
    u,v=239-y,x
    index=((v//8)*30+u//8)*64+morton3(u%8)+2*morton3(v%8)
    return (depth[index]&0xffffff)/0xffffff
start,count=int(arrow['first']),int(arrow['count'])
assert all(v[1] == -25 for v in vertices[start:start+count])
road=runs[2]
assert road['texture']=='22'
assert all(v[1] == -25 for v in vertices[int(road['first']):int(road['first'])+int(road['count'])])
pixels=accepted=occluded=0
minimum=0.0
receiver_minimum=0.0
maximum=-1.0
for i in range(start,start+count,3):
    clip=[[sum(matrix[r][k]*value for k,value in enumerate((*v[:3],1))) for r in range(4)] for v in vertices[i:i+3]]
    assert all(p[3]>0 and -p[3]<=p[2]<=0 for p in clip)
    points=[(200-200*p[1]/p[3],120-120*p[0]/p[3]) for p in clip]
    a,b,c=points
    den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
    for y in range(max(0,math.floor(min(p[1] for p in points))),min(239,math.ceil(max(p[1] for p in points)))+1):
        for x in range(max(40,math.floor(min(p[0] for p in points))),min(359,math.ceil(max(p[0] for p in points)))+1):
            u=((b[1]-c[1])*(x+.5-c[0])+(c[0]-b[0])*(y+.5-c[1]))/den
            v=((c[1]-a[1])*(x+.5-c[0])+(a[0]-c[0])*(y+.5-c[1]))/den
            w=1-u-v
            if min(u,v,w)<0: continue
            z=-sum(weight*p[2]/p[3] for weight,p in zip((u,v,w),clip))
            receiver=actual(x,y)
            delta=z-receiver
            minimum=min(minimum,delta); maximum=max(maximum,delta)
            if delta>=-tolerance: receiver_minimum=min(receiver_minimum,delta)
            pixels+=1
            accepted+=z+tolerance>=receiver
            # A foreground depth farther outside the supported receiver window
            # must continue occluding the arrow, with receiver depth unchanged.
            occluded+=z+tolerance<receiver+2*tolerance
assert pixels==4837 and accepted==4834 and occluded==pixels
assert minimum < -2*tolerance  # three actual foreground-covered samples still reject
assert minimum<maximum<0  # exact coplanar receiver is in front after GPU rounding
print(f'Captured road arrow: {accepted}/{pixels} receiver samples accepted, 3 actual occluder rejects; {occluded} synthetic foreground rejects; receiver delta {receiver_minimum:.9g}..{maximum:.9g}')
