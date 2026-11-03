#ifndef SMW_TEST_NATIVE_FIXTURE_H
#define SMW_TEST_NATIVE_FIXTURE_H
/* Exercise the production native engine and renderer, not a second widescreen
 * implementation. Test-only camera/sprite placements never enter the PS2 ELF. */
#include "smw_ps2.h"
#include "widescreen.h"
#include "src/smw_rtl.h"
#include "src/common_cpu_infra.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "src/snes/snes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Image {
  uint32_t before[32];
  uint32_t pixels[SMW_MAX_PITCH*SMW_HEIGHT];
  uint32_t after[32];
} Image;
static Image image;
static uint32_t classic[256*224];
static uint8_t baseline_ram[0x20000];
static Ppu baseline_ppu;
static Snes baseline_snes;
static const uint32_t canary=0xd6a98231;
void ResetSpritesFunc(int wh);
#ifdef SMW_WIDE_COMPARE
void SmwTestWideBenchmark(const Ppu *p,unsigned level);
void SmwTestWideReferenceSummary(void);
#endif

static void guards(void) {
  for(unsigned i=0;i<32;i++) image.before[i]=image.after[i]=canary;
}
static void draw(void) {
  smw_draw(image.pixels);
  for(unsigned i=0;i<32;i++) assert(image.before[i]==canary && image.after[i]==canary);
}
static void ppm(const char *folder,const char *name) {
  if(!folder) return;
  char path[512]; snprintf(path,sizeof(path),"%s/%s.ppm",folder,name);
  FILE *f=fopen(path,"wb"); assert(f);
  fprintf(f,"P6\n%u 224\n255\n",smw_video_width());
  for(unsigned y=0;y<224;y++) for(unsigned x=0;x<smw_video_width();x++) {
    uint32_t p=image.pixels[y*smw_video_pitch()+x];
#ifdef SMW_PS2_PIXEL_FORMAT
    uint8_t rgb[]={p,p>>8,p>>16};
#else
    uint8_t rgb[]={p>>16,p>>8,p};
#endif
    fwrite(rgb,1,3,f);
  }
  assert(fclose(f)==0);
}
static void restore(void) {
  memcpy(g_ram,baseline_ram,sizeof(g_ram)); *g_ppu=baseline_ppu; *g_snes=baseline_snes;
  SmwWide_Reset();
}
static uint16_t route(int f) {
  if(f==180 || f==260 || f==340 || f==750) return SMW_START;
  if(misc_game_mode==20 && misc_intro_level_flag) return f%120==0?SMW_B:0;
  if(misc_game_mode==14 && pointer_current_overworld_process==3)
    return f%20==0?(ow_players_pos[player_current_character].x==104?SMW_LEFT:SMW_B):0;
  if(misc_game_mode==20) return SMW_RIGHT|SMW_Y|((f%120)<25?SMW_B:0);
  return 0;
}
static void bootstrap(const char *assets) {
  assert(smw_init(assets,""));
  static int16_t audio[534*2];
  for(int f=0;f<1540;f++) {
    smw_step(route(f),0); draw(); smw_audio(audio,534);
  }
  assert(misc_game_mode==20 && !misc_intro_level_flag);
  memcpy(baseline_ram,g_ram,sizeof(g_ram)); baseline_ppu=*g_ppu; baseline_snes=*g_snes;
}
static void load_level(unsigned level) {
  restore();
  player_xpos=player_ypos=0;
  player_current_state=0;
  misc_level_layout_flags=0; misc_intro_level_flag=0;
  flag_use_secondary_entrance=0; counter_sublevels_entered=1;
  ow_players_map[0]=level>>8;
  misc_subscreen_exit_entrance_number_lo[0]=level&255;
  misc_game_mode=17;
  GameMode11_LoadSublevel_0096D5();
  assert(misc_game_mode==18);
  GameMode12_PrepareLevel();
  misc_game_mode=20; mirror_screen_display_register=15;
  mirror_mosaic_size_and_bgenable=0; timer_hdmawindow_scaling_factor=0;
  CompressOamEntExt(); SmwVectorNMI(); SmwWide_Latch();
}

#endif
