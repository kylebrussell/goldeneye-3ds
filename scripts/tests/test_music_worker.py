#!/usr/bin/env python3
"""Exercise the production single-job worker with host synchronization primitives."""
from pathlib import Path
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
header=r'''
#ifndef TEST_3DS_H
#define TEST_3DS_H
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#include <time.h>
#include <assert.h>
typedef uint64_t u64;
#define U64_MAX UINT64_MAX
#define R_FAILED(x) ((x)<0)
#define RESET_ONESHOT 0
typedef struct { atomic_flag flag; } LightLock;
typedef struct { atomic_int signaled; } LightEvent;
typedef struct {pthread_t thread;void (*fn)(void *);void *arg;} *Thread;
extern int test_new, test_fail, test_query_fail;
extern pthread_mutex_t events_lock;
extern pthread_cond_t events_cond;
static inline int APT_CheckNew3DS(bool *b){*b=test_new;return test_query_fail ? -1 : 0;}
static inline void LightLock_Init(LightLock *l){atomic_flag_clear(&l->flag);}
static inline void LightLock_Lock(LightLock *l){while(atomic_flag_test_and_set_explicit(&l->flag,memory_order_acquire))sched_yield();}
static inline void LightLock_Unlock(LightLock *l){atomic_flag_clear_explicit(&l->flag,memory_order_release);}
static inline void LightEvent_Init(LightEvent *e,int type){(void)type;atomic_init(&e->signaled,0);}
static inline void LightEvent_Clear(LightEvent *e){atomic_store(&e->signaled,0);}
static inline void LightEvent_Signal(LightEvent *e){pthread_mutex_lock(&events_lock);atomic_store(&e->signaled,1);pthread_cond_broadcast(&events_cond);pthread_mutex_unlock(&events_lock);}
static inline void LightEvent_Wait(LightEvent *e){pthread_mutex_lock(&events_lock);while(!atomic_exchange(&e->signaled,0))pthread_cond_wait(&events_cond,&events_lock);pthread_mutex_unlock(&events_lock);}
static inline uint64_t svcGetSystemTick(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000U+t.tv_nsec;}
static void *test_entry(void *p){Thread t=p;t->fn(t->arg);return NULL;}
static inline Thread threadCreate(void (*fn)(void *),void *arg,size_t stack,int prio,int core,bool detached){assert(stack==32768 && prio==0x30 && core==2 && !detached);if(test_fail)return NULL;Thread t=malloc(sizeof(*t));assert(t);t->fn=fn;t->arg=arg;assert(!pthread_create(&t->thread,NULL,test_entry,t));return t;}
static inline int threadJoin(Thread t,u64 timeout){assert(timeout==U64_MAX);return pthread_join(t->thread,NULL);}
static inline void threadFree(Thread t){free(t);}
#endif
'''
test=r'''
#include "3ds.h"
#include "ge_3ds_music_worker.h"
#include <stdio.h>
int test_new=1,test_fail=0,test_query_fail=0;
pthread_mutex_t events_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t events_cond=PTHREAD_COND_INITIALIZER;
typedef struct {uint32_t seed, output[257];int calls;} Job;
static void execute(void *ctx){Job *j=ctx;for(unsigned i=0;i<257;++i){j->output[i]=j->seed*1664525U+i;if(i%31==0)sched_yield();}++j->calls;}
int main(void){
 for(unsigned cycle=0;cycle<12;++cycle){
  Job j={0};
  for(unsigned i=0;i<1000;++i){j.seed=i+cycle*1000;int calls=j.calls;assert(ge_3ds_music_worker_submit(execute,&j));ge_3ds_music_worker_wait();assert(j.calls==calls+1);for(unsigned k=0;k<257;++k)assert(j.output[k]==j.seed*1664525U+k);}
  Ge3dsMusicWorkerStats s;ge_3ds_music_worker_stats(&s);assert(s.enabled && s.submissions==1000);
  assert(ge_3ds_music_worker_submit(execute,&j));ge_3ds_music_worker_close();assert(j.calls==1001);
  ge_3ds_music_worker_stats(&s);assert(!s.enabled && !s.submissions);ge_3ds_music_worker_close();
 }
 for(unsigned mode=0;mode<3;++mode){test_new=mode!=0;test_fail=mode==1;test_query_fail=mode==2;Job j={.seed=123};assert(!ge_3ds_music_worker_submit(execute,&j));assert(!j.calls);execute(&j);assert(j.calls==1);ge_3ds_music_worker_close();}
 puts("Music worker: 12000 handoffs, 12 in-flight shutdowns, old-hardware, query-failure and creation-failure fallback pass");
}
'''
with tempfile.TemporaryDirectory(prefix='ge-music-worker-') as tmp:
 p=Path(tmp);(p/'3ds.h').write_text(header);(p/'test.c').write_text(test)
 for mode in ['address,undefined','thread']:
  binary=p/mode.replace(',','-')
  subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pthread','-fsanitize='+mode,'-I'+str(p),'-I'+str(repo/'platform/3ds/include'),str(p/'test.c'),str(repo/'platform/3ds/source/ge_3ds_music_worker.c'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
