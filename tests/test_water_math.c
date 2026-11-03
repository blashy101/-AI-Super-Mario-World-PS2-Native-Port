/* Exact water math and measured CPU cost against the frozen v013 raster. */
#define _POSIX_C_SOURCE 200809L
#include "native_fixture.h"
void __real_ppu_runLine(Ppu *,int);
void v013_ppu_runLine(Ppu *,int);
void v013_ppu_write(Ppu *,uint8_t,uint8_t);
static Ppu reference,old_bench,new_bench;
static uint32_t expected[384*224],bench_new[384*224];
static bool compare;
static unsigned long long compared;
static int16_t pcm[1068];
void __wrap_ppu_runLine(Ppu *p,int line) {
  if(compare) {reference=*p;reference.renderBuffer=(uint8_t*)expected;v013_ppu_runLine(&reference,line);}
  __real_ppu_runLine(p,line);
  if(!compare || !line) return;
  const uint32_t *a=(const uint32_t*)(p->renderBuffer+(line-1)*p->renderPitch);
  const uint32_t *b=(const uint32_t*)((uint8_t*)expected+(line-1)*p->renderPitch);
  for(unsigned x=0;x<smw_video_width();x++) if(a[x]!=b[x]) {
    fprintf(stderr,"water math differs line=%d x=%u native=%08x v013=%08x math=%02x sel=%02x fixed=%04x\n",line,x,a[x],b[x],p->cgadsub,p->cgwsel,p->fixedColor);abort();
  }
  compared+=smw_video_width();
}
static double seconds(void) {
  struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;
}
static void benchmark(unsigned level,bool dirty) {
  if(!getenv("SMW_WATER_BENCH")) return;
  old_bench=new_bench=*g_ppu;
  old_bench.renderBuffer=(uint8_t*)expected;new_bench.renderBuffer=(uint8_t*)bench_new;
  double elapsed[2]={0};unsigned frames=120,batch=6;
  for(unsigned part=0;part<frames/batch;part++) for(unsigned pass=0;pass<2;pass++) {
    unsigned current=(part+pass)&1;Ppu *p=current?&new_bench:&old_bench;
    double start=seconds();
    for(unsigned f=0;f<batch;f++) {
      if(dirty) {p->cgram[0x87]=((part*batch+f)*1013)&32767;p->paletteValid=false;}
      for(unsigned y=0;y<=224;y++) {if(current) __real_ppu_runLine(p,y);else v013_ppu_runLine(p,y);}
    }
    elapsed[current]+=seconds()-start;
  }
  for(unsigned y=0;y<224;y++) for(unsigned x=0;x<smw_video_width();x++) assert(expected[y*smw_video_pitch()+x]==bench_new[y*smw_video_pitch()+x]);
  printf("HOST_WATER_BENCH {\"level\":%u,\"wide\":%s,\"palette_changes\":%s,\"v013_ms\":%.6f,\"v014_ms\":%.6f,\"ratio\":%.6f}\n",level,SmwWide_Enabled()?"true":"false",dirty?"true":"false",elapsed[0]*1000/frames,elapsed[1]*1000/frames,elapsed[1]/elapsed[0]);
}
static void cgram_writes(void) {
  load_level(0x018);smw_set_widescreen(true);compare=true;draw();
  old_bench=*g_ppu;
  /* Replay writes through both actual register handlers, including redundant
   * palette uploads, raw bit 15, byte-latch resets and pointer wrap at 255. */
  for(unsigned pass=0;pass<6;pass++) {
    for(unsigned i=0;i<256;i++) {
      unsigned color=g_ppu->cgram[i];
      if(pass==2) color^=0x8000;
      if(pass==3) color^=1;
      if(pass==4) color^=0x100;
      if(pass==5) color=(i*1009)&65535;
      ppu_write(g_ppu,0x21,i);v013_ppu_write(&old_bench,0x21,i);
      if(i&1) { /* Discard an incomplete first write with CGADD. */
        ppu_write(g_ppu,0x22,0x5a);v013_ppu_write(&old_bench,0x22,0x5a);
        ppu_write(g_ppu,0x21,i);v013_ppu_write(&old_bench,0x21,i);
      }
      ppu_write(g_ppu,0x22,color);v013_ppu_write(&old_bench,0x22,color);
      ppu_write(g_ppu,0x22,color>>8);v013_ppu_write(&old_bench,0x22,color>>8);
      assert(!memcmp(g_ppu->cgram,old_bench.cgram,sizeof(g_ppu->cgram)));
      assert(g_ppu->cgramPointer==old_bench.cgramPointer);
      assert(g_ppu->cgramSecondWrite==old_bench.cgramSecondWrite);
      assert(g_ppu->cgramBuffer==old_bench.cgramBuffer);
    }
    assert(g_ppu->cgramPointer==0);draw();
  }
  compare=false;
}
int main(int argc,char **argv) {
  assert(argc>=2);guards();bootstrap(argv[1]);
  const unsigned levels[]={0x018,0x102,0x10a,0x0c2};
  for(unsigned wide=0;wide<2;wide++) for(unsigned scene=0;scene<4;scene++) {
    load_level(levels[scene]);smw_set_widescreen(wide);compare=true;draw();
    /* All four operations, clipping, subscreen/fixed fallback, brightness,
     * live palette writes, scrolling and water mosaics use the same oracle. */
    Ppu saved=*g_ppu;
    for(unsigned v=0;v<48;v++) {
      *g_ppu=saved;g_ppu->cgadsub=0x24|((v&3)<<6);
      if(v>=4 && v<12) {
        g_ppu->cgwsel=(v&1?2:0)|((v&2)<<3)|((v&4)<<4);
        g_ppu->windowsel=0xaaaaaa;g_ppu->window1left=40;g_ppu->window1right=210;
      } else if(v<20) {
        g_ppu->cgram[(v*7)&31]=(v*1013)&32767;g_ppu->cgram[0x87]=(v*331)&32767;g_ppu->paletteValid=false;
        g_ppu->fixedColor=(v*751)&32767;
      } else if(v<28) {
        g_ppu->mosaic=((v-19)<<4)|4;g_ppu->vScroll[2]+=v;g_ppu->hScroll[1]+=v;
      } else if(v<44) {
        g_ppu->inidisp=v-28;g_ppu->paletteValid=false;
      } else {
        g_ppu->screenEnabled[0]=4;g_ppu->screenEnabled[1]=v&1?0:0x13;
      }
      draw();
    }
    *g_ppu=saved;
    for(unsigned f=0;f<48;f++) {
      smw_step(SMW_RIGHT|((f&8)?SMW_B:0),0);draw();smw_audio(pcm,534);
    }
    if(argc>2) {char name[48];snprintf(name,sizeof(name),"water-%03x-%u",levels[scene],wide);ppm(argv[2],name);}
    compare=false;
    if(scene==0) {benchmark(levels[scene],false);benchmark(levels[scene],true);}
  }
  cgram_writes();
  printf("PASS: %llu water/terrain pixels match frozen v013, 4:3/16:9, add/subtract/half, clipping, palette/fixed-colour writes, fade and mosaic\n",compared);
  return 0;
}
