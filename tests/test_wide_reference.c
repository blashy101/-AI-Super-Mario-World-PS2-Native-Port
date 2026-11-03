/* Test-only paired scanout and matched-input host benchmark. No SDK model or
 * host timing is used as evidence of a particular PS2 frame rate. */
#define _POSIX_C_SOURCE 200809L
#include "smw_ps2.h"
#include "widescreen.h"
#include "src/snes/ppu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void __real_ppu_runLine(Ppu *,int);
void v006_ppu_runLine(Ppu *,int);
static Ppu reference,old_bench,new_bench;
static uint32_t reference_pixels[SMW_MAX_PITCH*SMW_HEIGHT];
static uint32_t bench_pixels[SMW_MAX_PITCH*SMW_HEIGHT];
static unsigned long long wide_pixels,native_pixels;

void __wrap_ppu_runLine(Ppu *p,int line) {
  reference=*p; reference.renderBuffer=(uint8_t*)reference_pixels;
  v006_ppu_runLine(&reference,line);
  __real_ppu_runLine(p,line);
  assert(p->rangeOver==reference.rangeOver && p->timeOver==reference.timeOver);
  if(!line) return;
  unsigned width=256+2*p->extraLeftRight;
  const uint32_t *actual=(const uint32_t*)(p->renderBuffer+(line-1)*p->renderPitch);
  const uint32_t *expected=(const uint32_t*)(reference.renderBuffer+(line-1)*p->renderPitch);
  /* Pillarboxes are cleared by runtime.c, outside either rasterizer. */
  unsigned first=p->wideTile?0:p->extraLeftRight;
  unsigned end=p->wideTile?width:first+256;
  for(unsigned x=first;x<end;x++) if(actual[x]!=expected[x]) {
    fprintf(stderr,"v006 raster differs line=%d x=%u wide=%u new=%08x old=%08x\n",
      line,x,p->wideTile!=NULL,actual[x],expected[x]); abort();
  }
  if(p->wideTile) wide_pixels+=end-first; else native_pixels+=end-first;
}
static double seconds(void) {
  struct timespec t; clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);
  return t.tv_sec+t.tv_nsec*1e-9;
}
void SmwTestWideBenchmark(const Ppu *p,unsigned level) {
  if(!getenv("SMW_WIDE_BENCH") || !p->wideTile) return;
  old_bench=*p; new_bench=*p;
  old_bench.renderBuffer=(uint8_t*)reference_pixels;
  new_bench.renderBuffer=(uint8_t*)bench_pixels;
  const unsigned frames=240, batch=12;
  double old_time=0,new_time=0;
  for(unsigned part=0;part<frames/batch;part++) {
    /* Alternate order and use process CPU time to reduce scheduling noise. */
    for(unsigned pass=0;pass<2;pass++) {
      bool current=(pass^(part&1))!=0;
      Ppu *state=current?&new_bench:&old_bench;
      double start=seconds();
      for(unsigned f=0;f<batch;f++) {
        SmwWide_PrepareDraw(state);
        for(int y=0;y<=224;y++) {
          if(current) __real_ppu_runLine(state,y);
          else v006_ppu_runLine(state,y);
        }
      }
      if(current) new_time+=seconds()-start; else old_time+=seconds()-start;
    }
  }
  printf("HOST_BENCH {\"level\":\"%03x\",\"v006_ms\":%.6f,\"v007_ms\":%.6f,\"ratio\":%.6f}\n",
    level,old_time*1000/frames,new_time*1000/frames,new_time/old_time);
}
void SmwTestWideReferenceSummary(void) {
  printf("PASS: v006 raster comparison, %llu wide and %llu native pixels\n",wide_pixels,native_pixels);
}
