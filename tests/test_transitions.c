/* Real room/pipe state machines plus an independent mosaic pixel oracle. */
#include "native_fixture.h"
#include "assets/smw_assets.h"

static int16_t pcm[1068];
static unsigned transition_wide,pipe_runs;
static void tick(uint16_t pad,bool half,unsigned frame,bool wide) {
  smw_step(pad,0);
  if(half && (frame&1)) smw_skip_draw(); else draw();
  smw_audio(pcm,534);
  if(wide && !(half && (frame&1)) &&
      (misc_game_mode==0x0f || misc_game_mode==0x13 || misc_game_mode==0x14)) {
    assert(g_ppu->wideTile && g_ppu->extraLeftCur+g_ppu->extraRightCur==86);
    transition_wide++;
  }
}
static void pipe_round_trip(bool wide,bool half) {
  load_level(0x105); smw_set_widescreen(wide);
  ow_level_number_lo=0x29;
  misc_mosaic_direction=1; /* load_level skips the initial fade for fixtures. */
  unsigned main_sprite_list=Load24(ptr_sprite_list_data);
  /* Place Mario on the real downward pipe, then use normal input, collision,
   * warp timers, loaders and entrance records. Never patch an exit table. */
  player_xpos=player_current_xpos=0x788;
  player_ypos=player_current_ypos=0x140;
  player_xspeed=player_yspeed=0; player_current_power_up=1; player_current_state=0;
  mirror_current_layer1_xpos=0x680; mirror_current_layer2_xpos=0x340;
  for(unsigned f=0;f<180;f++) tick(SMW_DOWN,half,f,wide);
  assert(misc_game_mode==20 && player_current_state==0);
  assert(Load24(ptr_sprite_list_data)==0x07c3ee && camera_last_screen_horiz==2);
  assert(counter_sublevels_entered==2 && flag_use_secondary_entrance);
  assert(misc_subscreen_exit_entrance_number_lo[1]==0xcb);
  /* Start immediately before the underground room's genuine horizontal
   * return pipe. This avoids making the test depend on coin-route movement. */
  player_xpos=player_current_xpos=0x1a8;
  player_ypos=player_current_ypos=0x160; player_xspeed=player_yspeed=0;
  bool shoot_out=false;
  for(unsigned f=0;f<260;f++) {
    tick(SMW_RIGHT,half,f,wide);
    shoot_out|=Load24(ptr_sprite_list_data)==main_sprite_list && player_current_state==7;
    assert(player_current_state!=9 && !flag_active_bonus_game);
  }
  assert(shoot_out && misc_game_mode==20 && player_current_state==0);
  assert(Load24(ptr_sprite_list_data)==main_sprite_list && camera_last_screen_horiz==20);
  assert(counter_sublevels_entered==3 && !flag_use_secondary_entrance);
  assert(player_xpos>0x880 && player_xpos<0xa00 && player_ypos<0x1b0);
  printf("PASS: YI1 -> 1CB -> YI1 secondary 1CB, %s/%s, alive at %04x,%04x\n",
    wide?"16:9":"4:3",half?"alternate draws":"full draws",player_xpos,player_ypos);
  pipe_runs++;
}
static void level_entry(const char *assets) {
  assert(smw_init(assets,"")); smw_set_widescreen(true);
  unsigned incoming=0,maps=0;
  for(unsigned f=0;f<1540;f++) {
    smw_step(route(f),0); draw(); smw_audio(pcm,534);
    if(misc_game_mode==0x13 && !misc_intro_level_flag && PPU_mode(g_ppu)==1 &&
       !(misc_nmito_use_flag&0x80)) {
      assert(g_ppu->wideTile); incoming++;
    }
    if(misc_game_mode==0x0e) { assert(g_ppu->wideTile); maps++; }
  }
  assert(incoming && maps && misc_game_mode==20);
  printf("PASS: first visible level fade is wide (%u incoming frames); map scene remains stable\n",incoming);
}
static void narrow_and_iris(void) {
  load_level(0x000); smw_set_widescreen(true); draw();
  assert(camera_last_screen_horiz==1 && g_ppu->wideTile);
  assert(g_ppu->extraLeftCur==43 && g_ppu->extraRightCur==43);
  assert(!SmwWide_Active()); /* Presentation does not widen gameplay bounds. */
  load_level(0x105); smw_set_widescreen(true); draw();
  timer_hdmawindow_scaling_factor=128; draw(); assert(g_ppu->wideTile);
  misc_game_mode=0x15; SmwWide_Latch(); draw(); assert(g_ppu->wideTile);
  misc_game_mode=0x0e; SmwWide_Latch(); draw(); assert(g_ppu->wideTile);
  misc_game_mode=0x0b; SmwWide_Latch(); draw(); assert(g_ppu->wideTile);
  misc_game_mode=0x14; SmwWide_Latch(); SmwWide_RecordPiece(0,300);
  SmwWide_Latch(); SmwWide_PrepareDraw(g_ppu); assert(g_ppu->wideOamValid[0]);
  misc_game_mode=0x12; SmwWide_Latch();
  misc_game_mode=0x13; SmwWide_Latch(); SmwWide_PrepareDraw(g_ppu);
  assert(!g_ppu->wideOamValid[0]);
  load_level(0x095); smw_set_widescreen(true); draw(); assert(!g_ppu->wideTile);
  puts("PASS: narrow Mode-1 rooms and outgoing iris/fades retain projection; map projection and title/Mode-7 fallback stay separate");
}

/* Unmosaicked expanded raster output supplies the expected source pixel for
 * each mosaic cell. Its source surface includes negative-X samples and one
 * extra Y row; no copy of the new mosaic routine produces the oracle. */
static uint32_t source_pixels[512*225];
static Ppu source_ppu,candidate_ppu;
static bool mosaic_tiles(Ppu *p,unsigned layer,int x,int y,PpuWideTile *s) {
  int px=x+p->hScroll[layer],py=y+p->vScroll[layer];
  unsigned tx=((unsigned)px>>3)&127,ty=((unsigned)py>>3)&127;
  s->tile=((tx+ty*7)&1023)|((tx&7)<<10)|((ty&1)<<13)|((tx&1)<<14)|((ty&2)<<14);
  s->x=px;s->y=py;s->run=8; return true;
}
static void mosaic_pixels(void) {
  unsigned long long compared=0;
  Ppu *base=ppu_init(); assert(base); ppu_reset(base);
  base->inidisp=15;base->bgmode=1;base->wideTile=mosaic_tiles;
  base->wideSpan=NULL;base->extraLeftRight=96;base->extraLeftCur=96;base->extraRightCur=96;
  for(unsigned i=0;i<0x8000;i++) base->vram[i]=(i*12289u)^(i>>2)^0x963a;
  for(unsigned i=0;i<256;i++) base->cgram[i]=(i*1973u)&0x7fff;
  for(unsigned layer=0;layer<3;layer++) {
    base->hScroll[layer]=13;base->vScroll[layer]=19;
    base->screenEnabled[0]=1u<<layer;base->screenEnabled[1]=0;
    /* Two ordinary 256-column draws provide logical X -128..383. Some
     * negative mosaic cells begin beyond the PPU's 96-column side buffer. */
    for(unsigned part=0;part<2;part++) {
      source_ppu=*base; source_ppu.vScroll[layer]--;
      source_ppu.hScroll[layer]=base->hScroll[layer]-128+part*256;
      source_ppu.extraLeftCur=source_ppu.extraRightCur=source_ppu.extraLeftRight=0;
      PpuBeginDrawing(&source_ppu,(uint8_t*)(source_pixels+part*256),512*4,kPpuRenderFlags_NewRenderer);
      for(unsigned line=0;line<=225;line++) ppu_runLine(&source_ppu,line);
    }
    for(unsigned offset=0;offset<=86;offset+=43) for(unsigned size=2;size<=16;size++) {
      candidate_ppu=*base;candidate_ppu.mosaic=((size-1)<<4)|(1u<<layer);
      candidate_ppu.extraLeftRight=43;candidate_ppu.extraLeftCur=offset;candidate_ppu.extraRightCur=86-offset;
      PpuBeginDrawing(&candidate_ppu,(uint8_t*)image.pixels,384*4,kPpuRenderFlags_NewRenderer);
      for(unsigned line=0;line<=224;line++) ppu_runLine(&candidate_ppu,line);
      for(unsigned y=0;y<224;y++) for(unsigned out=0;out<342;out++) {
        int logical=(int)out-(int)offset;
        int cell=logical/(int)size;
        if(logical<0 && logical%(int)size) cell--;
        unsigned sy=(y+1)/size*size,sx=cell*(int)size+128;
        uint32_t expected=source_pixels[sy*512+sx],actual=image.pixels[y*384+out];
        if(expected!=actual) {
          fprintf(stderr,"mosaic differs layer=%u size=%u offset=%u x=%u y=%u %08x/%08x\n",
            layer,size,offset,out,y,actual,expected); abort();
        }
        compared++;
      }
      for(unsigned i=0;i<32;i++) assert(image.before[i]==canary && image.after[i]==canary);
    }
  }
  ppu_free(base);
  printf("PASS: %llu wide mosaic pixels, sizes 2..16, all Mode-1 backgrounds and three viewport offsets\n",compared);
}
int main(int argc,char **argv) {
  setvbuf(stdout,NULL,_IONBF,0); assert(argc>=2);guards();bootstrap(argv[1]);
  assert(kLm5FE00_SIZE==512 && kLevelInfo_05F800_SIZE==512 &&
         kLevelInfo_05FA00_SIZE==512 && kLevelInfo_05FC00_SIZE==512);
  assert(kLevelInfo_05F800[0x1cb]==5 && kLevelInfo_05FA00[0x1cb]==0xa9 &&
         kLevelInfo_05FC00[0x1cb]==8 && kLm5FE00[0x1cb]==6);
  for(unsigned wide=0;wide<2;wide++) for(unsigned half=0;half<2;half++) pipe_round_trip(wide,half);
  narrow_and_iris();mosaic_pixels();level_entry(argv[1]);
  assert(pipe_runs==4 && transition_wide>100);
  printf("PASS: %u full native pipe round trips and %u wide fade/gameplay draws\n",pipe_runs,transition_wide);
  return 0;
}
