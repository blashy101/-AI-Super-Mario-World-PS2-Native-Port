/* Regressions for the message/HUD boundary and complete map-border glyphs. */
#define main v010_fixture_main
#include "test_v010.c"
#undef main
static Ppu native_layer,wide_layer;
static uint32_t native_layer_pixels[256*224],wide_layer_pixels[384*224];
static unsigned long long message_pixels,mat_pixels;

static void message_layer(void) {
  /* Isolate the actual BG3 tiles to compare projection with native 4:3.
   * The message geometry comes from the real game, not a synthetic tilemap. */
  native_layer=wide_layer=*g_ppu;
  Ppu *p[2]={&native_layer,&wide_layer};
  for(unsigned i=0;i<2;i++) {
    p[i]->screenEnabled[0]=4;p[i]->screenEnabled[1]=0;
    p[i]->screenWindowed[0]=p[i]->screenWindowed[1]=0;
    p[i]->windowsel=0;p[i]->cgadsub=p[i]->cgwsel=0;
    p[i]->mosaic=0;
  }
  native_layer.wideTile=NULL;native_layer.wideSpan=NULL;native_layer.wideMapTile=NULL;
  native_layer.extraLeftCur=native_layer.extraRightCur=native_layer.extraLeftRight=0;
  PpuBeginDrawing(&native_layer,(uint8_t*)native_layer_pixels,256*4,kPpuRenderFlags_NewRenderer);
  PpuBeginDrawing(&wide_layer,(uint8_t*)wide_layer_pixels,384*4,kPpuRenderFlags_NewRenderer);
  for(unsigned line=0;line<=224;line++) {
    ppu_runLine(&native_layer,line);ppu_runLine(&wide_layer,line);
  }
  unsigned offset=wide_layer.extraLeftCur;
  for(unsigned y=39;y<224;y++) for(unsigned x=0;x<256;x++) {
    if(native_layer_pixels[y*256+x]!=wide_layer_pixels[y*384+x+offset]) {
      fprintf(stderr,"message BG3 differs x=%u y=%u offset=%u native=%08x wide=%08x\n",x,y,offset,native_layer_pixels[y*256+x],wide_layer_pixels[y*384+x+offset]);abort();
    }
    message_pixels++;
  }
}
static void message_case(unsigned number,const char *folder) {
  load_level(0x105);smw_set_widescreen(true);
  ow_level_number_lo=0x29;misc_display_message=number;
  flag_message_window_size_change_direction=0;
  timer_wait_before_message_window_size_change=0;
  for(unsigned f=0;f<100;f++) {
    smw_step(0,0);draw();smw_audio(pcm,534);
    if(f==79) {
      assert(misc_display_message && timer_wait_before_message_window_size_change==80);
      if(folder) {char name[64];snprintf(name,sizeof(name),"message-%s-%u",SmwLocale_Japanese()?"jp":"en",number);ppm(folder,name);}
    }
    if(f>=24) message_layer();
  }
  /* Closing uses the same top edge; native input/timers close the panel. */
  for(unsigned f=0;f<40;f++) {
    smw_step(f==0?SMW_B:0,0);draw();smw_audio(pcm,534);
    if(misc_display_message) message_layer();
  }
  assert(!misc_display_message);
}
static void map_glyphs(unsigned map) {
  map_fixture(map,NULL);
  for(int y=0;y<224;y++) {
    if(map==0 && y>=40) continue; /* Main-map terrain/frame has its own bands. */
    int sx=-43;
    while(sx<299) {
      PpuWideSpan s;
      assert(g_ppu->wideSpan(g_ppu,2,sx,y,&s));
      assert(s.end>sx && s.end<=299 && !s.map16);
      /* Interior joins must be whole tiles in both source and destination.
       * Only the outside display edges may crop an 8px glyph. */
      assert(((s.x-g_ppu->hScroll[2]-sx)&7)==0);
      assert(((s.y-g_ppu->vScroll[2]-y)&7)==0);
      if(s.end!=299) assert((s.end&7)==0);
      for(int x=sx;x<s.end;x++) {
        PpuWideTile tile;
        assert(g_ppu->wideTile(g_ppu,2,x,y,&tile));
        assert(((tile.x-g_ppu->hScroll[2]-x)&7)==0);
        mat_pixels++;
      }
      sx=s.end;
    }
  }
}
int main(int argc,char **argv) {
  assert(argc>=3);guards();bootstrap(argv[1]);load_japanese(argv[2]);
  for(unsigned language=0;language<2;language++) {
    assert(SmwLocale_Select(language));smw_set_widescreen(false);bootstrap(argv[1]);
    message_case(1,argc>3?argv[3]:NULL);message_case(2,argc>3?argv[3]:NULL);
    for(unsigned map=0;map<7;map++) {
      map_glyphs(map);
      if(argc>3 && map==1) {char name[64];snprintf(name,sizeof(name),"map-seams-%s",language?"jp":"en");ppm(argv[3],name);}
    }
  }
  printf("PASS: %llu native message-layer pixels; %llu map mat pixels on whole glyph boundaries; both languages, open/close and seven maps\n",message_pixels,mat_pixels);
  return 0;
}
