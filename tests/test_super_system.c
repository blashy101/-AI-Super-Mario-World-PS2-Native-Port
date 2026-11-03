/* Real native NSS title/menu/map/gameplay runs and original-code comparisons. */
#include "native_fixture.h"
#include "super_system.h"
#include "localization.h"
#include "assets/smw_assets.h"

void SmwTestSSReference(const char *path);
static int16_t audio[1068];
static unsigned ticks,draws,skips;
static bool half_draw;
static uint8_t full_ram[0x20000];
static uint32_t full_pcm;
static void tick(uint16_t buttons) {
  smw_step(buttons,0);
  if(half_draw && (ticks&1)) { smw_skip_draw();skips++; }
  else { draw();draws++; }
  smw_audio(audio,534);ticks++;
}
static void pack(const char *path,bool japanese) {
  FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
  uint8_t *data=malloc(n);assert(data && fread(data,1,n,f)==(size_t)n);fclose(f);
  if(japanese) assert(SmwLocale_Load(data,n));
  else {
    assert(!SmwArcade_Load(NULL,0));
    for(unsigned i=0;i<12;i++) assert(!SmwArcade_Load(data,i));
    assert(SmwArcade_Load(data,n));
    assert(!SmwArcade_Load(data,n-4));
    uint8_t keep=data[12];data[12]=255;assert(!SmwArcade_Load(data,n));data[12]=keep;
  }
  free(data);
}
static uint32_t hash(const void *data,size_t size) {
  uint32_t h=2166136261u;const uint8_t *p=data;
  while(size--) { h^=*p++;h*=16777619u; }return h;
}
static uint32_t session(const char *assets,const char *save,unsigned zone,bool wide,bool half,const char *folder) {
  assert(SmwArcade_Select(true));smw_set_widescreen(wide);smw_set_connected_pads(wide?3:1);
  assert(smw_init(assets,save));assert(!SmwLocale_Japanese() && !SmwLocale_Select(true));
  half_draw=half;ticks=0;
  for(unsigned f=0;f<180;f++) tick(0);
  assert(misc_game_mode==6 || misc_game_mode==7);
  tick(SMW_START);tick(0);
  assert(misc_game_mode==8 && l1_l2_scroll_spr_current_state[1]==0);
  for(unsigned z=0;z<zone;z++) { tick(SMW_DOWN);tick(0); }
  assert(l1_l2_scroll_spr_current_state[1]==zone);
  if(folder && wide && !half) { draw();char name[64];snprintf(name,sizeof(name),"nss-zone-menu-%u",zone+1);ppm(folder,name); }
  tick(SMW_START);tick(0);assert(misc_game_mode==10);
  if(wide) { tick(SMW_DOWN);tick(0); }
  tick(SMW_START);tick(0);
  for(unsigned f=0;!(misc_game_mode==14 && pointer_current_overworld_process==3) && f<2000;f++)
    tick(misc_game_mode==20 && misc_intro_level_flag && f%32==0?SMW_B:0);
  assert(misc_game_mode==14 && !misc_intro_level_flag);
  static const unsigned map[]={1,0,2,0,3,0,0},x[]={104,88,88,328,136,392,232},y[]={120,280,296,88,376,360,376};
  assert(ow_players_map[0]==map[zone] && ow_players_pos[0].x==x[zone] && ow_players_pos[0].y==y[zone]);
  assert(flag_two_player_game==wide);
  for(unsigned f=0;f<12;f++) tick(0);
  if(folder && wide && !half) { draw();char name[64];snprintf(name,sizeof(name),"nss-map-%u",zone+1);ppm(folder,name); }
  for(unsigned f=0;!(misc_game_mode==20 && !misc_intro_level_flag) && f<1000;f++) {
    unsigned p=0;
    if(misc_game_mode==14 && pointer_current_overworld_process==3 && f%20==0)
      p=zone==0 && ow_players_pos[0].x==104?SMW_LEFT:SMW_B;
    tick(p);
  }
  assert(misc_game_mode==20 && !misc_intro_level_flag);
  draw();assert(!wide || (g_ppu->wideTile && smw_video_width()==342));
  if(folder && wide && !half) { char name[64];snprintf(name,sizeof(name),"nss-level-%u",zone+1);ppm(folder,name); }
  uint32_t pcm_hash=0;
  for(unsigned f=0;f<360;f++) {
    tick(SMW_RIGHT|SMW_Y|((f%90)<25?SMW_B:0));
    pcm_hash=pcm_hash*16777619u^hash(audio,sizeof(audio));
    assert(misc_game_mode<=0x29);
  }
  draw();assert(smw_save());
  printf("NSS zone %u: %s %s, map %u at %u,%u, level %03x, final mode %u\n",
    zone+1,wide?"16:9/2P":"4:3/1P",half?"alternate draws":"full draws",map[zone],x[zone],y[zone],ow_level_number_lo,smw_game_mode());
  if(wide && !half) { memcpy(full_ram,g_ram,sizeof(full_ram));full_pcm=pcm_hash; }
  if(half) {
    unsigned diff=0;
    for(unsigned i=0;i<sizeof(full_ram);i++) if(full_ram[i]!=g_ram[i]) {
      if(diff++<16) fprintf(stderr,"NSS session RAM differs %05x full=%02x half=%02x\n",i,full_ram[i],g_ram[i]);
    }
    if(diff || full_pcm!=pcm_hash) fprintf(stderr,"NSS difference zone=%u RAMbytes=%u PCM=%08x/%08x ticks=%u\n",zone+1,diff,full_pcm,pcm_hash,ticks);
  }
  fflush(stdout);
  return hash(g_ram,sizeof(g_ram))^pcm_hash;
}
int main(int argc,char **argv) {
  assert(argc==6);const char *assets=argv[1],*folder=argv[5];guards();
  assert(!SmwArcade_Ready() && !SmwArcade_Select(true));bootstrap(assets);
  const uint8_t *retail[178];memcpy(retail,g_asset_ptrs,sizeof(retail));
  pack(argv[2],false);pack(argv[3],true);assert(SmwArcade_Ready());
  assert(SmwLocale_Select(true) && SmwLocale_Japanese());
  assert(SmwArcade_Select(true) && !SmwLocale_Japanese());
  assert(g_asset_ptrs[3]!=retail[3] && g_asset_ptrs[39]!=retail[39]);
  SmwTestSSReference(argv[4]);fflush(stdout);
  char save[512];snprintf(save,sizeof(save),"%s/retail-sentinel.bin",folder);
  uint8_t sentinel[2048];for(unsigned i=0;i<sizeof(sentinel);i++) sentinel[i]=i*17+43;
  FILE *f=fopen(save,"wb");assert(f && fwrite(sentinel,1,sizeof(sentinel),f)==sizeof(sentinel));fclose(f);
  for(unsigned z=0;z<7;z++) {
    session(assets,save,z,false,false,NULL);
    uint32_t full=session(assets,save,z,true,false,folder);
    uint32_t half=session(assets,save,z,true,true,NULL);assert(full==half);
  }
  /* Game-over returns to the title; no continue/save/life-transfer UI appears. */
  assert(smw_init(assets,save));misc_game_mode=12;misc_intro_level_flag=0;flag_show_continue_and_end=1;
  GameMode0C_LoadOverworld();assert(misc_game_mode==3);
  memset(g_sram,0xee,g_sram_size);SaveGame();assert(smw_save());
  f=fopen(save,"rb");assert(f);uint8_t actual[2048];assert(fread(actual,1,sizeof(actual),f)==sizeof(actual));fclose(f);
  assert(!memcmp(sentinel,actual,sizeof(actual)));
  assert(SmwArcade_Select(false) && smw_init(assets,save));
  assert(!memcmp(g_sram,sentinel,sizeof(sentinel)));
  for(unsigned i=0;i<178;i++) assert(g_asset_sizes[i]==0 || g_asset_ptrs[i]!=NULL);
  assert(g_asset_sizes[39]==2854);
  assert(SmwLocale_Select(true));smw_reset();assert(SmwLocale_Japanese());
  assert(SmwArcade_Select(true) && smw_init(assets,save) && !SmwLocale_Japanese());
  assert(g_asset_sizes[39]==2820);
  printf("PASS: 21 native NSS title/zone/player/map/gameplay sessions, all seven zones in 4:3/16:9, 1P/2P, deterministic RAM/PCM with alternate draws; %u draws / %u skips\n",draws,skips);
  puts("PASS: retail save sentinel unchanged by arcade, game-over returns to title, language/edition reinitialization and asset validation");
  return 0;
}
