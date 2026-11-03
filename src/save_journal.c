#include "save_journal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint32_t crc32(const void *data,size_t size) {
  const uint8_t *p=data; uint32_t crc=~0u;
  for(size_t i=0;i<size;i++) {
    crc^=p[i];
    for(int b=0;b<8;b++) crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);
  }
  return ~crc;
}
static uint32_t get32(const uint8_t *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(uint8_t *p,uint32_t n) { p[0]=n; p[1]=n>>8; p[2]=n>>16; p[3]=n>>24; }
static bool read_slot(const char *base,int slot,void *data,size_t size,uint32_t *sequence) {
  char path[280]; snprintf(path,sizeof(path),"%s.%d",base,slot);
  FILE *f=fopen(path,"rb"); if(!f) return false;
  uint8_t header[20];
  bool ok=fread(header,1,20,f)==20 && !memcmp(header,"SMWSAVE2",8) && get32(header+12)==size;
  if(ok) ok=fread(data,1,size,f)==size && fgetc(f)==EOF && crc32(data,size)==get32(header+16);
  fclose(f);
  if(ok) *sequence=get32(header+8);
  return ok;
}
static bool newer(uint32_t a,uint32_t b) { return (int32_t)(a-b)>0; }
bool smw_journal_load(const char *base,void *data,size_t size) {
  uint8_t *a=malloc(size),*b=malloc(size); uint32_t x=0,y=0;
  if(!a||!b) { free(a); free(b); return false; }
  bool valid_a=read_slot(base,0,a,size,&x),valid_b=read_slot(base,1,b,size,&y);
  if(valid_a||valid_b) memcpy(data,valid_b&&(!valid_a||newer(y,x))?b:a,size);
  free(a); free(b); return valid_a||valid_b;
}
bool smw_journal_save(const char *base,const void *data,size_t size) {
  uint8_t *a=malloc(size),*b=malloc(size); uint32_t x=0,y=0;
  if(!a||!b) { free(a); free(b); return false; }
  bool va=read_slot(base,0,a,size,&x),vb=read_slot(base,1,b,size,&y);
  int latest=vb&&(!va||newer(y,x))?1:0;
  if((va||vb) && !memcmp(data,latest?b:a,size)) { free(a); free(b); return true; }
  unsigned target=(va||vb)?(latest^1):0;
  uint32_t sequence=(va||vb)?(latest?y:x)+1:1;
  free(a); free(b);
  uint8_t header[20]; memcpy(header,"SMWSAVE2",8);
  put32(header+8,sequence); put32(header+12,size); put32(header+16,crc32(data,size));
  char path[280]; snprintf(path,sizeof(path),"%s.%u",base,target);
  FILE *f=fopen(path,"wb"); if(!f) return false;
  bool ok=fwrite(header,1,20,f)==20 && fwrite(data,1,size,f)==size;
  if(fclose(f)) ok=false;
  return ok;
}
