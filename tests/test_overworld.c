/* Real map loaders, native header comparison and frozen v010 raster oracle.
 * Matched-input host timings do not measure a PS2 frame rate. */
#define _POSIX_C_SOURCE 200809L
#define main v010_fixture_main
#include "test_v010.c"
#undef main

void __real_ppu_runLine(Ppu *,int);
void v010_ppu_runLine(Ppu *,int);
static Ppu reference,old_bench,new_bench;
static uint32_t reference_pixels[SMW_MAX_PITCH*224],native_pixels[256*224];
static uint32_t bench_pixels[SMW_MAX_PITCH*224];
static bool compare;
static uint64_t compared,portraits;

void __wrap_ppu_runLine(Ppu *p,int line) {
  if(compare) {
    reference=*p;reference.renderBuffer=(uint8_t*)reference_pixels;
    v010_ppu_runLine(&reference,line);
  }
  __real_ppu_runLine(p,line);
  if(!compare || !line) return;
  const uint32_t *actual=(const uint32_t*)(p->renderBuffer+(line-1)*p->renderPitch);
  const uint32_t *expected=(const uint32_t*)((uint8_t*)reference_pixels+(line-1)*p->renderPitch);
  for(unsigned x=0;x<342;x++) if(actual[x]!=expected[x]) {
    fprintf(stderr,"map raster differs line=%d x=%u new=%08x v010=%08x math=%02x\n",line,x,actual[x],expected[x],p->cgadsub);
    abort();
  }
  compared+=342;
}
static double seconds(void) {
  struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);
  return t.tv_sec+t.tv_nsec*1e-9;
}
static void benchmark(unsigned map,bool japanese) {
  if(!getenv("SMW_MAP_BENCH")) return;
  old_bench=new_bench=*g_ppu;
  old_bench.renderBuffer=(uint8_t*)reference_pixels;
  new_bench.renderBuffer=(uint8_t*)bench_pixels;
  const unsigned frames=120,batch=6;
  double elapsed[2]={0};
  for(unsigned part=0;part<frames/batch;part++) for(unsigned pass=0;pass<2;pass++) {
    unsigned current=(part+pass)&1;
    Ppu *p=current?&new_bench:&old_bench;
    double start=seconds();
    for(unsigned f=0;f<batch;f++) for(int y=0;y<=224;y++) {
      if(current) __real_ppu_runLine(p,y); else v010_ppu_runLine(p,y);
    }
    elapsed[current]+=seconds()-start;
  }
  for(unsigned y=0;y<224;y++) for(unsigned x=0;x<342;x++)
    assert(reference_pixels[y*384+x]==bench_pixels[y*384+x]);
  printf("HOST_MAP_BENCH {\"map\":%u,\"japanese\":%s,\"v010_ms\":%.6f,\"v011_ms\":%.6f,\"ratio\":%.6f}\n",
      map,japanese?"true":"false",elapsed[0]*1000/frames,elapsed[1]*1000/frames,elapsed[1]/elapsed[0]);
}
static void portrait(void) {
  reference=*g_ppu;
  reference.wideTile=NULL;reference.wideSpan=NULL;reference.wideMapTile=NULL;
  reference.extraLeftCur=reference.extraRightCur=reference.extraLeftRight=0;
  reference.wideBackdropMath=false;reference.renderPitch=256*4;
  reference.renderBuffer=(uint8_t*)native_pixels;
  for(int y=0;y<=40;y++) v010_ppu_runLine(&reference,y);
  for(unsigned y=4;y<40;y++) for(unsigned x=24;x<80;x++) {
    if(native_pixels[y*256+x]!=image.pixels[y*384+x+43]) {
      fprintf(stderr,"native header differs x=%u y=%u native=%08x wide=%08x\n",x,y,native_pixels[y*256+x],image.pixels[y*384+x+43]);abort();
    }
    portraits++;
  }
  for(unsigned i=0;i<16;i++) {
    int x=(g_ppu->oam[i*2]&255)-(((g_ppu->highOam[i>>2]>>((i&3)*2))&1)<<8);
    assert(g_ppu->wideOamX[i]==x && x>=0);
  }
}
int main(int argc,char **argv) {
  assert(argc>=3);guards();bootstrap(argv[1]);load_japanese(argv[2]);
  for(unsigned language=0;language<2;language++) {
    compare=false;assert(SmwLocale_Select(language));bootstrap(argv[1]);
    for(unsigned map=0;map<7;map++) {
      compare=true;map_fixture(map,NULL);portrait();benchmark(map,language);
      if(argc>3) {char name[64];snprintf(name,sizeof(name),"v011-map-%s-%u",language?"jp":"en",map);ppm(argv[3],name);}
      Ppu saved=*g_ppu;
      for(unsigned variant=0;variant<24;variant++) {
        *g_ppu=saved;
        if(variant<8) {
          static const uint8_t math[]={0x30,0x70,0xb0,0xf0,0x10,0x20,0x3f,0xff};
          g_ppu->cgadsub=math[variant];
        } else if(variant<12) {
          g_ppu->cgwsel=3|((variant-8)<<6)|((variant&1)<<4);
          g_ppu->windowsel=0xaaaaaa;g_ppu->window1left=16;g_ppu->window1right=239;
          g_ppu->screenWindowed[0]=g_ppu->screenEnabled[0];g_ppu->screenWindowed[1]=2;
        } else if(variant<16) {
          g_ppu->hScroll[0]+=variant;g_ppu->hScroll[1]+=variant;
          g_ppu->bgmode^=(variant&1)<<3;
        } else if(variant<20) g_ppu->mosaic=((variant-15)<<4)|7;
        else {
          g_ppu->cgram[0]=(variant*1361)&32767;
          g_ppu->cgram[0x8f]=(variant*2341)&32767;
          g_ppu->fixedColor=(variant*3451)&32767;g_ppu->paletteValid=false;
        }
        draw();
      }
      *g_ppu=saved;
      /* Stale actor coordinates must never relocate native header masks. */
      for(unsigned i=0;i<16;i++) SmwWide_RecordMapPiece(i,-200);
      SmwWide_Latch();draw();portrait();
      for(unsigned f=0;f<48;f++) {
        smw_step(0,0);if(f&1) smw_skip_draw();else {draw();portrait();}
        smw_audio(pcm,534);
      }
    }
  }
  printf("PASS: %llu map pixels match v010 raster; %llu portrait/lives pixels match native 4:3; both languages, seven maps, windows, math, mosaic, palette changes and OAM reuse\n",(unsigned long long)compared,(unsigned long long)portraits);
  return 0;
}
