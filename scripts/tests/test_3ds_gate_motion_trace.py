#!/usr/bin/env python3
"""Execute source-extracted bounded gate tracing and native motion reads."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from extract_guard_bullet_hit_slice import find_function

class GateTrace(unittest.TestCase):
    def test_bounded_observation(self):
        main = (ROOT / 'platform/3ds/source/main.c').read_text()
        native = (ROOT / 'port/src/ge_original_door.c').read_text()
        code = r'''
#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>
#include "ge_original_door_runtime.h"
#include "ge_original_door_collision.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef struct {
 uint32_t gate_trace_remaining,gate_trace_age;
 int32_t gate_trace_state[2];uint8_t gate_trace_valid[2];
 unsigned actor_tick_status,active_prop_status;
 GeOriginalDoorCollisionState door_collision;
 GeOriginalDoorRuntimeState door_runtime;
} RuntimeStageOrdinaryObjects;
static DoorRecord doors[2];
static FILE *sink;static unsigned opens,closes;static bool fail_open;
static DoorRecord *find_native_door(void *definition) {
 return definition==&doors[0]||definition==&doors[1]?definition:NULL;
}
static void *stage_definition_by_command(RuntimeStageOrdinaryObjects *o,size_t i){
 (void)o;assert(i==267||i==268);return &doors[i-267];
}
static unsigned long long svcGetSystemTick(void){return 123;}
static FILE *capture_open(const char *path,const char *mode){
 assert(strcmp(path,"sdmc:/3ds/goldeneye-3ds/use-audit.log")==0);
 assert(strcmp(mode,"a")==0);++opens;return fail_open?NULL:sink;
}
static int capture_close(FILE *stream){assert(stream==sink);++closes;return fflush(stream);}
#define fopen capture_open
#define fclose capture_close
'''
        code += '\nstatic PropRecord g_Props[4];\nstatic GeOriginalDoorCollisionState *collision_state;\n'
        collision = (ROOT / 'port/src/ge_original_door_collision.c').read_text()
        code += find_function(collision, 'ge_port_door_collision_note_blocker')
        code += find_function(native, 'ge_original_door_runtime_motion_snapshot')
        code += '\n' + find_function(main, 'trace_stage_gate_motion')
        code += r'''
#undef fopen
#undef fclose
int main(void){
 RuntimeStageOrdinaryObjects objects={0};
 GeOriginalDoorMotionSnapshot snap;
 doors[0].openstate=3;doors[0].maxFrac=.95f;doors[0].speed=.01f;
 doors[0].lastcalc60i=45;doors[0].runtime_bitflags=0x4010;
 doors[0].linkedDoor=&doors[1];
 DoorRecord before[2];memcpy(before,doors,sizeof(doors));
 assert(!ge_original_door_runtime_motion_snapshot(NULL,&snap));
 assert(!ge_original_door_runtime_motion_snapshot(&doors[0],NULL));
 assert(ge_original_door_runtime_motion_snapshot(&doors[0],&snap));
 assert(snap.last_calc_tick==45 && snap.runtime_flags==0x4010);
 assert(snap.linked_definition==(uintptr_t)&doors[1]);
 collision_state=&objects.door_collision;
 g_Props[0].pos.x=17075;g_Props[2].pos.z=9161;g_Props[2].type=PROP_TYPE_OBJ;
 coord2d polygon[8];for(int i=0;i<8;i++){polygon[i].x=100+i;polygon[i].y=200+i;}
 ge_port_door_collision_note_blocker(&g_Props[0],&g_Props[2],(rect4f*)polygon,8,300,-100,(rect4f*)polygon,8,250,-50,0);
 GeOriginalDoorCollisionWitness *w=&objects.door_collision.witness;
 assert(w->sequence==1 && w->door_index==0 && w->blocker_index==2);
 assert(w->door_polygon[7][0]==107 && w->blocker_polygon[7][1]==207);
 assert(w->door_position[0]==17075 && w->blocker_position[2]==9161);
 assert(w->blocker_type==PROP_TYPE_OBJ && w->blocker_top==250);
 ge_port_door_collision_note_blocker(&g_Props[0],&g_Props[2],(rect4f*)polygon,8,300,-100,NULL,0,180,20,30);
 assert(w->sequence==2 && w->blocker_edges==0 && w->blocker_radius==30);
 assert(w->blocker_polygon[7][1]==0); /* Previous object hull is cleared. */
 assert(objects.door_collision.tests==0 && objects.door_collision.blocked_results==0);
 for(int i=0;i<8;i++)assert(polygon[i].x==100+i && polygon[i].y==200+i);
 assert(g_Props[0].pos.x==17075 && g_Props[2].pos.z==9161);
 sink=tmpfile();assert(sink);
 trace_stage_gate_motion(&objects,"after_props");assert(opens==0);
 objects.gate_trace_remaining=120;
 trace_stage_gate_motion(&objects,"activation");assert(opens==1);
 assert(objects.gate_trace_remaining==120 && objects.gate_trace_age==0);
 for(unsigned i=0;i<120;++i)trace_stage_gate_motion(&objects,"after_props");
 assert(opens==10 && closes==10); /* activation, first result, eight periodic */
 assert(objects.gate_trace_remaining==0 && objects.gate_trace_age==120);
 trace_stage_gate_motion(&objects,"after_props");assert(opens==10);
 assert(memcmp(before,doors,sizeof(doors))==0); /* Diagnostics never tick doors. */
 objects.gate_trace_remaining=120;objects.gate_trace_age=0;
 trace_stage_gate_motion(&objects,"activation");
 trace_stage_gate_motion(&objects,"props_skipped");
 doors[0].openstate=1;doors[0].openPosition=.1f;doors[0].lastcalc60i=46;
 objects.door_collision.tests=4;objects.door_collision.blocked_results=3;
 trace_stage_gate_motion(&objects,"after_props");assert(opens==13);
 doors[0].openstate=0;trace_stage_gate_motion(&objects,"after_props");assert(opens==14);
 fail_open=true;doors[0].openstate=1;
 trace_stage_gate_motion(&objects,"after_props");assert(opens==15 && closes==14);
 for(unsigned i=0;i<120;++i)trace_stage_gate_motion(&objects,"props_skipped");
 assert(objects.gate_trace_remaining==0); /* IO failure cannot extend tracing. */
 rewind(sink);char text[32768];size_t n=fread(text,1,sizeof(text)-1,sink);text[n]=0;
 assert(strstr(text,"phase=activation age=0 remaining=120"));
 assert(strstr(text,"phase=after_props age=120 remaining=0"));
 assert(strstr(text,"phase=props_skipped"));
 assert(strstr(text,"collision_tests=4 clear=0 blocked=3"));
 assert(strstr(text,"lastcalc=45 runtime=00004010"));
 assert(strstr(text,"gate_blocker sequence=2"));
 assert(strstr(text,"blocker_pos=0,0,9161"));
 assert(strstr(text,"radius=30"));
 assert(strstr(text,"[107,207] blocker_edges=0"));
 fclose(sink);return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='ge-gate-trace-') as folder:
            src=Path(folder)/'test.c';src.write_text(code)
            exe=Path(folder)/'test'
            flags=['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-comment',
                   '-fms-extensions','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                   '-D_LANGUAGE_C','-DGE_PORT_USE_ORIGINAL_TYPES','-DAIPARSE',
                   '-DGE_PORT_SETUP_DATA','-DGE_PORT_MS_INHERITS',
                   '-I'+str(ROOT/'port/include'),'-idirafter',str(ROOT/'include'),
                   '-idirafter',str(ROOT/'include/PR'),'-idirafter',str(ROOT/'src')]
            subprocess.run(flags+[str(src),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
        self.assertIn('trace_stage_gate_motion(objects, "activation")',main)
        self.assertIn('stage_props_ticked ? "after_props" : "props_skipped"',main)

if __name__=='__main__': unittest.main()
