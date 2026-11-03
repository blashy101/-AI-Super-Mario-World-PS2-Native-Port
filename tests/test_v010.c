/* Exercise real Japanese assets, native map loaders and the production PPU. */
#include "native_fixture.h"
#include "localization.h"
#include "assets/smw_assets.h"

static int16_t pcm[1068];
static void load_japanese(const char *path) {
  FILE *f=fopen(path,"rb"); assert(f);
  fseek(f,0,SEEK_END); long length=ftell(f); rewind(f);
  uint8_t *data=malloc(length); assert(data && fread(data,1,length,f)==(size_t)length); fclose(f);
  const uint8_t *old[kNumberOfAssets]; memcpy(old,g_asset_ptrs,sizeof(old));
  assert(SmwLocale_Load(data,length));
  for(unsigned n=0;n<64;n++) assert(!SmwLocale_Load(data,n));
  assert(!SmwLocale_Load(data,length-4));
  assert(SmwLocale_Select(true) && SmwLocale_Japanese());
  unsigned changed=0;
  for(unsigned i=0;i<kNumberOfAssets;i++) {
    bool localized=i==0 || i==3 || i==4 || i==32 || i==33 || i==39 || i==41 || i==60 || i==85;
    assert((g_asset_ptrs[i]!=old[i])==localized); changed+=localized;
  }
  assert(changed==9); /* Audio, level/exit/sprite data stay in the shared pack. */
  assert(SmwLocale_Select(false));
  assert(!memcmp(old,g_asset_ptrs,sizeof(old)));
  assert(SmwLocale_Select(true)); free(data);
}
static void japanese_streams(void) {
  /* All 25 original messages include intro, both Yoshi states and switches.
   * Verify complete streams without borrowing any following asset's bytes. */
  for(unsigned message=0;message<25;message++) {
    memset(stripe_image_upload_data,0xa7,512); stripe_image_upload=0;
    SmwLocale_Message(message);
    assert(stripe_image_upload==320 && stripe_image_upload_data[320]==255);
    for(unsigned row=0;row<4;row++) {
      unsigned at=row*80;
      assert(WORD(stripe_image_upload_data[at+2])==0x2300);
      assert(WORD(stripe_image_upload_data[at+42])==0x2300);
      for(unsigned c=0;c<18;c++) {
        assert(stripe_image_upload_data[at+5+c*2]==0x39);
        assert(stripe_image_upload_data[at+45+c*2]==0x39);
      }
    }
    for(unsigned i=322;i<512;i++) assert(stripe_image_upload_data[i]==0xa7);
  }
  for(unsigned i=0;i<256;i++) {
    memset(stripe_image_upload_data,0xa7,256); stripe_image_upload=0;
    UpdateLevelName(kLevelNames[i]);
    assert(stripe_image_upload==72 && stripe_image_upload_data[72]==255);
    assert(WORD(stripe_image_upload_data[0])==0x8c50 && WORD(stripe_image_upload_data[36])==0x6c50);
    assert(WORD(stripe_image_upload_data[2])==0x1f00 && WORD(stripe_image_upload_data[38])==0x1f00);
    /* Append the real cursor stream at the reserved position. */
    HandleMenuCursor_009E82(0);
    assert(WORD(stripe_image_upload_data[36])==0x6c50);
    assert(stripe_image_upload==84 && stripe_image_upload_data[84]==255);
    for(unsigned p=85;p<256;p++) assert(stripe_image_upload_data[p]==0xa7);
  }
  /* Castle text uses each original Japanese line once, in display order. */
  for(unsigned castle=1;castle<=7;castle++) {
    misc_currently_active_boss_end_cutscene=castle;
    InitializeCastleDestructionTextTimers_DontIncrementCutsceneProcess();
    unsigned lines=0;
    for(unsigned f=0;f<127;f++) {
      graphics_stripe_image_to_upload=0;
      DisplayCastleDestructionText();
      if(graphics_stripe_image_to_upload) {
        unsigned index=graphics_stripe_image_to_upload/3;
        assert(index==11+(castle-1)*8+7-lines);
        MemBlk stream=kLoadStripeImagePtrs(index); assert(stream.size>1 && stream.ptr[0]<128);
        lines++;
      }
    }
    assert(lines==4);
  }
  puts("PASS: original Japanese messages (25), level-name streams (256), cursor append and seven castle narratives");
}
static void capture_japanese(const char *assets,const char *folder,bool half) {
  assert(smw_init(assets,"")); smw_set_widescreen(true);
  for(unsigned f=0;f<1600;f++) {
    smw_step(route(f),0);
    if(half && f&1) smw_skip_draw(); else draw();
    smw_audio(pcm,534);
    if(!half) {
      if(f==199) ppm(folder,"japanese-file-menu");
      if(f==799) ppm(folder,"japanese-intro");
      if(f==1339) ppm(folder,"japanese-map");
      if(f==1599) ppm(folder,"japanese-game");
    }
  }
  assert(misc_game_mode==20 && !misc_intro_level_flag);
}
static void map_fixture(unsigned submap,const char *folder) {
  restore(); smw_set_widescreen(true);
  ow_players_map[0]=ow_players_map[1]=submap;
  misc_intro_level_flag=flag_show_continue_and_end=ow_warping_on_pipe_or_star_flag=0;
  misc_mosaic_direction=1; misc_game_mode=12;
  get_PointU16(ow_players_pos,0)->x=submap?104:296;
  get_PointU16(ow_players_pos,0)->y=submap?120:296;
  GameMode0C_LoadOverworld();
  for(unsigned f=0;f<64;f++) { smw_step(0,0); smw_skip_draw(); smw_audio(pcm,534); }
  assert(misc_game_mode==14);
  mirror_screen_display_register=15;
  ow_level_number_lo=0x29;
  stripe_image_upload=0; graphics_stripe_image_to_upload=0;
  UpdateLevelName(kLevelNames[0x29]);
  SmwVectorNMI(); RtlPpuWrite(INIDISP,15); SmwWide_Latch(); draw();
  assert(g_ppu->wideTile && g_ppu->wideSpan && !g_ppu->wideMapTile);
  assert(!SmwWide_Active() && SmwWide_Overworld());
  assert(g_ppu->extraLeftCur==43 && g_ppu->extraRightCur==43);
  char label[64]; snprintf(label,sizeof(label),"overworld-%s-%u",SmwLocale_Japanese()?"jp":"en",submap); ppm(folder,label);
  /* Drawing is independent of whether the last game update was presented. */
  uint8_t state[0x20000];memcpy(state,g_ram,sizeof(state));
  draw();assert(!memcmp(state,g_ram,sizeof(state)));
  if(!submap) for(unsigned camera=0;camera<=256;camera+=128) {
    mirror_current_layer1_xpos=mirror_current_layer2_xpos=camera;
    SmwVectorNMI();RtlPpuWrite(INIDISP,15);draw();
    assert(g_ppu->wideTile);
  }
}
int main(int argc,char **argv) {
  assert(argc>=3); const char *folder=argc>3?argv[3]:NULL;
  guards(); bootstrap(argv[1]); load_japanese(argv[2]);
  japanese_streams(); capture_japanese(argv[1],folder,false);
  uint8_t full_state[0x20000];memcpy(full_state,g_ram,sizeof(full_state));
  capture_japanese(argv[1],NULL,true);assert(!memcmp(full_state,g_ram,sizeof(full_state)));
  memcpy(baseline_ram,g_ram,sizeof(g_ram)); baseline_ppu=*g_ppu; baseline_snes=*g_snes;
  for(unsigned map=0;map<7;map++) map_fixture(map,folder);
  assert(SmwLocale_Select(false));
  bootstrap(argv[1]);
  for(unsigned map=0;map<7;map++) map_fixture(map,folder);
  puts("PASS: Japanese title-to-game replay with alternate draws; all seven map loaders in both languages and main-map camera bounds");
  return 0;
}
