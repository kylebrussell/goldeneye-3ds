#!/usr/bin/env python3
"""Exact SFX PCM against the original wide, per-block predictor algorithm."""
from pathlib import Path
import re
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
source=(repo/'port/src/ge_original_sfx_bank.c').read_text()
def bounds(text,marker):
 a=text.index(marker);b=text.index('{',a)+1;depth=1
 while depth:
  depth+=(text[b]=='{')-(text[b]=='}');b+=1
 return a,b
reference=source
a,b=bounds(reference,'static int64_t floor_divide_2048(')
reference=reference[:a]+'''static int64_t floor_divide_2048(int64_t value) {
 if (value >= 0) return value / 2048;
 return -(((-value) + 2047) / 2048);
}'''+reference[b:]
a,b=bounds(reference,'static int narrow_predictor(');reference=reference[:a]+reference[b:]
reference=reference.replace('narrow = narrow_predictor(book);','narrow = 0;')
assert 'if (!book_reusable || predictor != last_predictor)' in reference
reference=reference.replace('if (!book_reusable || predictor != last_predictor)','if ((void)book_reusable, (void)last_predictor, 1)')
public=re.findall(r'\b(ge_original_sfx_bank_\w+)\s*\(', (repo/'port/include/ge_original_sfx_bank.h').read_text())
test=r'''
#include "ge_original_sfx_bank.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
GeOriginalSfxBankStatus reference_ge_original_sfx_bank_decode(const GeOriginalSfxBank*,int32_t,int16_t*,size_t,size_t*,GeOriginalSfxInfo*);
static uint32_t rng=741;
static uint32_t next(void){rng=rng*1664525U+1013904223U;return rng;}
static void be16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void be32(uint8_t *p,uint32_t v){be16(p,(uint16_t)(v>>16));be16(p+2,(uint16_t)v);}
static uint8_t *read_file(const char *path,size_t *size){
 FILE *f=fopen(path,"rb");assert(f);assert(fseek(f,0,SEEK_END)==0);long n=ftell(f);assert(n>0);rewind(f);
 uint8_t *p=malloc((size_t)n);assert(p);assert(fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*size=(size_t)n;return p;
}
static uint64_t hash=UINT64_C(14695981039346656037),samples_checked;
static void assets(const char *ctl,const char *tbl){
 size_t cn,tn;uint8_t *c=read_file(ctl,&cn),*t=read_file(tbl,&tn);GeOriginalSfxBank bank;
 assert(ge_original_sfx_bank_init(&bank,c,cn,t,tn)==GE_ORIGINAL_SFX_BANK_OK);
 size_t valid=0;
 for(int32_t id=0;id<bank.sound_count;++id){
  size_t na,nb;GeOriginalSfxInfo ia,ib;
  int sa=ge_original_sfx_bank_decode(&bank,id,NULL,0,&na,&ia);
  int sb=reference_ge_original_sfx_bank_decode(&bank,id,NULL,0,&nb,&ib);
  assert(sa==sb && na==nb && memcmp(&ia,&ib,sizeof(ia))==0);
  if(sa!=GE_ORIGINAL_SFX_BANK_OUTPUT_TOO_SMALL)continue;
  int16_t *a=calloc(na+16,sizeof(*a)),*b=calloc(na+16,sizeof(*b));assert(a&&b);
  sa=ge_original_sfx_bank_decode(&bank,id,a,na,&na,&ia);
  sb=reference_ge_original_sfx_bank_decode(&bank,id,b,nb,&nb,&ib);
  assert(sa==sb && na==nb && memcmp(&ia,&ib,sizeof(ia))==0);
  assert(memcmp(a,b,(na+16)*sizeof(*a))==0);
  for(size_t i=0;i<na;++i){hash^=(uint16_t)a[i];hash*=UINT64_C(1099511628211);}
  samples_checked+=na;++valid;free(a);free(b);
 }
 printf("Original bank: %zu/%u sounds, %llu exact PCM samples, digest %016llx\n",valid,bank.sound_count,(unsigned long long)samples_checked,(unsigned long long)hash);
 free(c);free(t);
}
static void synthetic(void){
 size_t narrow_books=0,wide_books=0;
 for(unsigned n=0;n<6000;++n){
  uint8_t *ca=calloc(1024,1),*cb=calloc(1024,1),*ta=calloc(512,1),*tb=calloc(512,1);
  assert(ca&&cb&&ta&&tb);
  unsigned predictors=1+n%16,frames=1+n%12;
  be16(ca,0x4231);be16(ca+2,1);be32(ca+4,8);be16(ca+8,1);be32(ca+12,22050);be32(ca+20,24);
  be16(ca+38,1);be32(ca+40,44);be32(ca+48,60);be32(ca+52,66);ca[56]=64;ca[57]=100;ca[64]=60;
  be32(ca+70,frames*9);be32(ca+82,86);be32(ca+86,2);be32(ca+90,predictors);
  for(unsigned p=0;p<predictors;++p){
   int16_t book[16];
   for(unsigned k=0;k<16;++k){book[k]=(int16_t)next();if(n%2==0)book[k]=(int16_t)(book[k]/8);}
   if(n%7==0){memset(book,0,sizeof(book));book[0]=32767;book[8]=(int16_t)(30719+n%3);}
   if(narrow_predictor(book))++narrow_books;else ++wide_books;
   for(unsigned k=0;k<16;++k)be16(ca+94+p*32+k*2,(uint16_t)book[k]);
  }
  for(unsigned f=0;f<frames;++f){
   ta[f*9]=(uint8_t)(((n+f)%16)<<4 | (n%3==0 ? 0 : next()%predictors));
   for(unsigned k=1;k<9;++k)ta[f*9+k]=(uint8_t)next();
  }
  if(n%13==0)ta[(frames-1)*9]=(uint8_t)0xff; /* Invalid when fewer than 16 predictors. */
  memcpy(cb,ca,1024);memcpy(tb,ta,512);
  GeOriginalSfxBank a,b;assert(ge_original_sfx_bank_init(&a,ca,1024,ta,512)==0);assert(ge_original_sfx_bank_init(&b,cb,1024,tb,512)==0);
  int16_t *pa=calloc(208,sizeof(*pa)),*pb=calloc(208,sizeof(*pb));assert(pa&&pb);
  int16_t *oa=n%5==0?(int16_t*)(ca+96):n%5==1?(int16_t*)ta:pa;
  int16_t *ob=n%5==0?(int16_t*)(cb+96):n%5==1?(int16_t*)tb:pb;
  size_t na,nb,capacity=frames*16-(n%11==0?1:0);GeOriginalSfxInfo ia,ib;
  int sa=ge_original_sfx_bank_decode(&a,0,oa,capacity,&na,&ia),sb=reference_ge_original_sfx_bank_decode(&b,0,ob,capacity,&nb,&ib);
  assert(sa==sb&&na==nb&&memcmp(&ia,&ib,sizeof(ia))==0);
  assert(memcmp(ca,cb,1024)==0&&memcmp(ta,tb,512)==0&&memcmp(pa,pb,208*sizeof(*pa))==0);
  free(ca);free(cb);free(ta);free(tb);free(pa);free(pb);
 }
 assert(narrow_books&&wide_books);
 printf("6000 synthetic decodes: %zu bounded/%zu wide books, all scales, history/saturation, changing predictors, aliased control/samples and partial/range errors exact\n",narrow_books,wide_books);
}
int main(int argc,char **argv){assert(argc==3);synthetic();assets(argv[1],argv[2]);}
'''
with tempfile.TemporaryDirectory(prefix='ge-sfx-exact-') as tmp:
 p=Path(tmp);(p/'reference.c').write_text(reference)
 (p/'test.c').write_text('#include "'+str(repo/'port/src/ge_original_sfx_bank.c')+'"\n'+test)
 for optimization in ('-O2', '-O3'):
  print('Checking', optimization, flush=True)
  flags=['-std=c11',optimization,'-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(repo/'port/include')]
  subprocess.run(['cc',*flags,*['-D'+n+'=reference_'+n for n in public],'-c',str(p/'reference.c'),'-o',str(p/'reference.o')],check=True)
  subprocess.run(['cc',*flags,str(p/'test.c'),str(p/'reference.o'),'-lm','-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test'),str(repo/'assets/music/sfx.ctl'),str(repo/'assets/music/sfx.tbl')],check=True)
