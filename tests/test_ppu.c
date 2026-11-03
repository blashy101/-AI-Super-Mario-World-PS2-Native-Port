/* Compare optimized raster output against the prototype's original PPU. */
#include "ppu.h"
#include <assert.h>
#include <stdio.h>
#include <stddef.h>

bool g_new_ppu=true;
Ppu *ref_ppu_init(void);
void ref_ppu_reset(Ppu *ppu);
void ref_PpuBeginDrawing(Ppu *ppu,uint8_t *pixels,size_t pitch,uint32_t flags);
void ref_ppu_runLine(Ppu *ppu,int line);
void ref_ppu_write(Ppu *ppu,uint8_t address,uint8_t value);
static uint32_t fast_pixels[256*224] __attribute__((aligned(64)));
static uint32_t ref_pixels[256*224] __attribute__((aligned(64)));
static uint32_t rng=0xb61f85;
static uint32_t random32(void) { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static uint32_t rgb(uint32_t value) {
#ifdef SMW_PS2_PIXEL_FORMAT
  return (value&255)<<16|(value&0xff00)|(value>>16&255);
#else
  return value;
#endif
}
int main(void) {
  Ppu *a=ppu_init(),*b=ref_ppu_init(); assert(a&&b);
  assert((offsetof(Ppu,bgBuffers)&15)==0);
  assert((offsetof(Ppu,objBuffer)&15)==0);
  for(unsigned fixture=0;fixture<240;fixture++) {
    ppu_reset(a); ref_ppu_reset(b);
    for(unsigned i=0;i<0x8000;i++) a->vram[i]=random32();
    for(unsigned i=0;i<256;i++) a->cgram[i]=random32()&0x7fff;
    for(unsigned i=0;i<256;i++) a->oam[i]=random32();
    for(unsigned i=0;i<32;i++) a->highOam[i]=random32();
    a->bgmode=fixture%8==0?7:1|((fixture&1)<<3);
    a->mosaic=fixture%3==0?0:(fixture%5<<4)|(fixture&7);
    a->obsel=(fixture%6)<<5;
    a->inidisp=fixture%16;
    a->screenEnabled[0]=31;
    a->screenEnabled[1]=random32()&31;
    a->screenWindowed[0]=fixture&31;
    a->screenWindowed[1]=(fixture>>1)&31;
    a->cgadsub=random32(); a->cgwsel=random32();
    a->fixedColor=random32()&0x7fff;
    a->windowsel=random32()&0xffffff;
    a->window1left=fixture; a->window1right=175+fixture%80;
    a->window2left=80+fixture%100; a->window2right=220;
    a->bgTileAdr=random32();
    for(unsigned i=0;i<4;i++) {
      a->bgXsc[i]=random32();
      a->hScroll[i]=random32()&511; a->vScroll[i]=random32()&511;
    }
    a->m7matrix[0]=256; a->m7matrix[3]=256;
    if(fixture>=96) {
      a->bgmode=1|(fixture&1?8:0); a->mosaic=0;
      a->cgadsub=0x20|((fixture&3)<<6);
      a->cgwsel=2|((fixture>>2&3)<<4)|((fixture>>4&3)<<6);
      a->screenEnabled[0]=fixture&64?0:21;
      a->screenEnabled[1]=fixture&32?2:0;
    }
    if(fixture>=224) {
      a->screenEnabled[0]=16; a->screenEnabled[1]=0;
      a->screenWindowed[0]=0; a->cgadsub=0;
      a->obsel=fixture&1?2<<5:0;
      for(unsigned i=0;i<128;i++) {
        a->oam[i*2]=((fixture&2?250:32)<<8)|(fixture&4?249:8);
        a->oam[i*2+1]=random32();
      }
      memset(a->highOam,fixture&1?0xaa:fixture&4?0x55:0,32);
    }
    *b=*a;
    PpuBeginDrawing(a,(uint8_t*)fast_pixels,256*4,1);
    ref_PpuBeginDrawing(b,(uint8_t*)ref_pixels,256*4,1);
    for(unsigned line=0;line<=224;line++) {
      /* Direct native writes and scanline brightness changes must invalidate the cache. */
      if(line==63 || line==137) {
        a->inidisp=b->inidisp=(fixture+line)&15;
        a->cgram[line]=b->cgram[line]=random32()&0x7fff;
        a->paletteValid=false; /* The native palette upload hook does this. */
      }
      if(line==100) {
        ppu_write(a,0x21,15); ref_ppu_write(b,0x21,15);
        ppu_write(a,0x22,0x28); ref_ppu_write(b,0x22,0x28);
        ppu_write(a,0x22,0x1f); ref_ppu_write(b,0x22,0x1f);
      }
      /* Changes during a frame must rebuild the sprite candidate lists. */
      if(line==48 || line==148) {
        uint8_t value=((fixture+line)%8)<<5;
        ppu_write(a,0x01,value); ref_ppu_write(b,0x01,value);
      }
      if(line==72) {
        uint8_t value=fixture*2;
        ppu_write(a,0x02,value); ref_ppu_write(b,0x02,value);
        ppu_write(a,0x03,0x80); ref_ppu_write(b,0x03,0x80);
      }
      if(line==81) {
        ppu_write(a,0x04,0x25); ref_ppu_write(b,0x04,0x25);
        ppu_write(a,0x04,0x5d); ref_ppu_write(b,0x04,0x5d);
      }
      if(line==112) {
        ppu_write(a,0x33,2); ref_ppu_write(b,0x33,2);
      }
      if(line==120 || line==160) {
        uint8_t value=0xe0|((line+fixture)&31);
        ppu_write(a,0x32,value); ref_ppu_write(b,0x32,value);
      }
      if(line==180) a->inidisp=b->inidisp=0x80;
      if(line==190) a->inidisp=b->inidisp=15;
      ppu_runLine(a,line); ref_ppu_runLine(b,line);
      assert(a->rangeOver==b->rangeOver && a->timeOver==b->timeOver);
    }
    for(unsigned i=0;i<256*224;i++) {
      if(rgb(fast_pixels[i])!=ref_pixels[i]) {
        fprintf(stderr,"fixture=%u pixel=%u,%u fast=%08x ref=%08x\n",fixture,i%256,i/256,
            (unsigned)rgb(fast_pixels[i]),(unsigned)ref_pixels[i]);
        return 1;
      }
    }
    assert(a->rangeOver==b->rangeOver && a->timeOver==b->timeOver);
  }
  puts("PASS: 240 reference raster fixtures, including backdrop blends, sprite limits/rotation/interlace/OAM writes, flips, clipping, mosaic, palette writes and Mode 7");
  ppu_free(a); ppu_free(b); return 0;
}
