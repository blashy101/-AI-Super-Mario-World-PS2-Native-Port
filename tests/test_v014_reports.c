/* Real map/Chocolate Island regressions for the v013 hardware reports. */
#include "native_fixture.h"
#include "super_system.h"
#include "localization.h"
static int16_t pcm[1068];
static unsigned ticks,half_draw,map_trips,hidden_pieces,visible_pieces,carried_frames;
static Ppu obj_ppu;
static uint32_t obj_pixels[384*224];
static void tick(unsigned buttons) {
  smw_step(buttons,0);
  if(half_draw && (ticks&1)) smw_skip_draw(); else draw();
  smw_audio(pcm,534);ticks++;
}
static void load_pack(const char *path,bool japanese) {
  FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
  void *data=malloc(n);assert(data && fread(data,1,n,f)==(size_t)n);fclose(f);
  assert(japanese?SmwLocale_Load(data,n):SmwArcade_Load(data,n));free(data);
}
static void map_start(const char *assets,unsigned edition,bool wide,bool half) {
  assert(SmwArcade_Select(edition==2));
  assert(SmwLocale_Select(edition==1));
  smw_set_widescreen(wide);half_draw=half;ticks=0;
  if(edition==2) {
    assert(smw_init(assets,""));
    for(unsigned f=0;f<180;f++) tick(0);
    tick(SMW_START);tick(0);
    for(unsigned z=0;z<6;z++) {tick(SMW_DOWN);tick(0);}
    tick(SMW_START);tick(0);tick(SMW_START);tick(0);
    for(unsigned f=0;!(misc_game_mode==14 && pointer_current_overworld_process==3) && f<2000;f++) tick(0);
  } else {
    /* A real retail map loader, starting from the native title-to-level route. */
    bootstrap(assets);
    ow_players_map[0]=ow_players_map[1]=0;
    misc_intro_level_flag=flag_show_continue_and_end=ow_warping_on_pipe_or_star_flag=0;
    misc_mosaic_direction=1;misc_game_mode=12;
    ow_players_pos[0]=(PointU16){296,296};
    GameMode0C_LoadOverworld();
    for(unsigned f=0;f<64;f++) tick(0);
  }
  for(unsigned f=0;f<8;f++) tick(0);
  assert(misc_game_mode==14 && pointer_current_overworld_process==3 && !ow_players_map[0] && !flag_pause);
}
static void camera_return(unsigned direction) {
  uint16_t x=mirror_current_layer1_xpos,y=mirror_current_layer1_ypos;
  PointU16 mario=ow_players_pos[0];
  tick(SMW_START);tick(0);assert(flag_pause==1);
  for(unsigned f=0;f<220;f++) tick(direction);
  assert(x!=mirror_current_layer1_xpos || y!=mirror_current_layer1_ypos);
  tick(SMW_START);
  unsigned f;
  for(f=0;flag_pause && f<600;f++) tick(0);
  if(flag_pause || mirror_current_layer1_xpos!=x || mirror_current_layer1_ypos!=y) {
    fprintf(stderr,"map return failed direction=%x after=%u pos=%d,%d target=%d,%d pause=%u\n",direction,f,(int16_t)mirror_current_layer1_xpos,(int16_t)mirror_current_layer1_ypos,(int16_t)x,(int16_t)y,flag_pause);abort();
  }
  assert(misc_game_mode==14 && !memcmp(&mario,&ow_players_pos[0],sizeof(mario)));
  tick(0);map_trips++;
}
static void enter_chocolate(void) {
  unsigned at;
  for(at=0;at<1024;at++) if(ow_level_number_of_each_tiletbl[at]==0x1c) break;
  assert(at<1024);
  unsigned x=(at&15)|((at>>4)&16),y=((at>>4)&15)|((at>>5)&16);
  ow_players_pos[0]=(PointU16){x*16+8,y*16+8};
  ow_players_grid_aligned_pos[0]=(PointU16){x,y};
  player_current_character=player_current_characterx4=ow_enter_level_flag=flag_pause=0;
  ow_tile_player_is_standing_on=blocks_map16_table_lo[at];
  assert(ow_tile_player_is_standing_on<=0x80);
  misc_exit_level_action=0;
  ow_level_tile_settings[0x1c]|=0x80; /* Test fixture: unlock this map tile. */
  tick(SMW_B);tick(0);
  for(unsigned f=0;misc_game_mode!=20 && f<500;f++) tick(0);
  if(misc_game_mode!=20 || ow_level_number_lo!=0x1c) {fprintf(stderr,"enter CI5: mode=%u level=%02x process=%u tile=%02x paused=%u\n",misc_game_mode,ow_level_number_lo,pointer_current_overworld_process,ow_tile_player_is_standing_on,flag_pause);abort();}
}
static void chocolate_run(bool wide,const char *folder,unsigned edition) {
  unsigned above=0,carried=0,spiny=0;
  for(unsigned f=0;f<420;f++) {
    tick(SMW_RIGHT|SMW_Y|((f%120)<28?SMW_B:0));
    assert(misc_game_mode==20);
    /* Inspect uploaded OAM without rendering the alternate frames. */
    SmwWide_PrepareDraw(g_ppu);
    for(unsigned k=0;k<12;k++) if(spr_current_status[k]) {
      if(spr_spriteid[k]==0x13) spiny++;
      if(spr_spriteid[k]!=0x3e) continue;
      if(spr_current_status[k]==11) {carried++;carried_frames++;}
      int sy=(int16_t)(GetSprYPos(k)-mirror_current_layer1_ypos);
      if(sy>=-16 || sy<-200 || !spr_yoffscreen_flag[k]) continue;
      unsigned slot=64+(spr_oamindex[k]>>2);
      unsigned raw=g_ppu->oam[slot*2],hi=g_ppu->highOam[slot>>2]>>((slot&3)*2);
      int nx=(raw&255)-((hi&1)<<8);
      assert(nx==-128);
      if(wide) {
        assert(g_ppu->wideOamValid[slot]);
        if(g_ppu->wideOamX[slot]!=-128) {
          fprintf(stderr,"hidden P-switch revived frame=%u slot=%u y=%d native=%d wide=%d\n",f,slot,sy,nx,g_ppu->wideOamX[slot]);abort();
        }
      }
      above++;
    }
    if(folder && f==240 && wide && !half_draw) {
      char name[64];snprintf(name,sizeof(name),"chocolate-fixed-%u",edition);ppm(folder,name);
    }
  }
  assert(above>20 && carried>20 && spiny>100);
  printf("PASS: edition %u %s %s, %u hidden P-switch frames, %u carried frames, %u spiny samples\n",edition,wide?"16:9":"4:3",half_draw?"alternate draws":"full draws",above,carried,spiny);
}
static void yellow_switch(bool wide) {
  /* Start at the cleared Yoshi's Island 1 node. The real walk, ladder,
   * HDMA submap transition and level entrance run from controller input. */
  ow_players_map[0]=ow_players_map[1]=1;
  player_current_character=player_current_characterx4=0;
  misc_intro_level_flag=flag_show_continue_and_end=ow_warping_on_pipe_or_star_flag=0;
  misc_exit_level_action=flag_got_midpoint=0;
  misc_mosaic_direction=mirror_screen_display_register=mirror_mosaic_size_and_bgenable=0;
  misc_game_mode=12;
  ow_players_pos[0]=(PointU16){56,136};
  GameMode0C_LoadOverworld();
  for(unsigned f=0;f<64;f++) tick(0);
  assert(misc_game_mode==14 && pointer_current_overworld_process==3);
  ow_level_tile_settings[0x29]|=15;
  tick(SMW_UP);
  unsigned frames=0;
  while((ow_players_map[0] || pointer_current_overworld_process!=3) && frames++<400) tick(0);
  assert(!ow_players_map[0] && ow_players_pos[0].x==40 && ow_players_pos[0].y==280);
  assert(misc_hdmawindow_effect_table[240]==16 && misc_hdmawindow_effect_table[241]==240);
  draw();
  if(wide) {
    /* At the reported location the old iris masked both of these bands. */
    const uint32_t black=PpuPackRgb(0,0,0);
    assert(image.pixels[60*SMW_MAX_PITCH+40]!=black);
    assert(image.pixels[60*SMW_MAX_PITCH+300]!=black);
    uint32_t marker[24][32];
    for(unsigned y=0;y<24;y++)
      memcpy(marker[y],&image.pixels[(108+y)*SMW_MAX_PITCH+41],sizeof(marker[y]));
    Ppu saved=*g_ppu;
    g_ppu->screenWindowed[0]&=~16;
    draw();
    for(unsigned y=0;y<24;y++)
      assert(!memcmp(marker[y],&image.pixels[(108+y)*SMW_MAX_PITCH+41],sizeof(marker[y])));
    /* Verify the comparison included actual Mario pixels in the old mask. */
    g_ppu->screenEnabled[0]&=~16;
    draw();
    unsigned recovered=0;
    for(unsigned y=0;y<24;y++) for(unsigned x=0;x<18;x++)
      recovered+=marker[y][x]!=image.pixels[(108+y)*SMW_MAX_PITCH+41+x];
    assert(recovered>16);
    *g_ppu=saved;
  }
  /* The late arcade start zone has already cleared this switch. */
  if(SmwArcade_Active()) {
    assert(ow_tile_player_is_standing_on==0x86);
    return;
  }
  tick(SMW_B);tick(0);
  for(unsigned f=0;misc_game_mode!=20 && f<500;f++) tick(0);
  assert(misc_game_mode==20 && ow_level_number_lo==0x14);
}
static void sprite_edges(void) {
  /* Exercise the exact generic graphics path, both hide bits, partial top
   * tiles and signed side coordinates. Native RAM/OAM remains authoritative. */
  memcpy(baseline_ram,g_ram,sizeof(g_ram));baseline_ppu=*g_ppu;baseline_snes=*g_snes;
  const int xs[]={-35,39,128,270,328};
  const int ys[]={-256,-128,-32,-17,-16,-15,-1,0,208,223,224,239,240,255,256,384};
  for(unsigned stacked=0;stacked<2;stacked++) for(unsigned xi=0;xi<5;xi++) for(unsigned yi=0;yi<16;yi++) {
    restore();smw_set_widescreen(true);
    unsigned k=0,slot=64;
    for(unsigned i=0;i<128;i++) g_ram[0x201+i*4]=240;
    memset(sprites_oamtile_size_buffer,0,128);
    spr_current_status[k]=9;spr_spriteid[k]=0x3e;spr_oamindex[k]=0;
    spr_table1602[k]=spr_table157c[k]=0;spr_table15f6[k]=0x22;
    spr_property_bits190f[k]=0;spr_current_slotid=k;
    SetSprXYPos(k,mirror_current_layer1_xpos+xs[xi],mirror_current_layer1_ypos+ys[yi]);
    SmwWide_BeginFrame();SmwWide_BeginActor(0,k);
    if(stacked) {
      spr_current_status[k]=8;spr_property_bits190f[k]=0x20;
      GenericGFXRtDraw2Tiles16x16sStacked_Sub(k);
    } else GenericGFXRtDraw1Tile16x16(k);
    SmwWide_EndActor();CompressOamEntExt();UploadOAMBuffer();SmwWide_Latch();draw();
    unsigned pieces=stacked?2:1;
    bool all_hidden=true;
    for(unsigned j=0;j<pieces;j++) {
      unsigned s=slot+j;
      if(spr_yoffscreen_flag[k]&(1u<<j)) {
        assert(g_ppu->wideOamValid[s] && g_ppu->wideOamX[s]==-128);
        unsigned hi=g_ppu->highOam[s>>2]>>((s&3)*2);
        if((g_ppu->oam[s*2]&255)!=128 || !(hi&1)) { fprintf(stderr,"edge hidden raw: stacked=%u x=%d y=%d piece=%u raw=%04x hi=%x buffer=%x\n",stacked,xs[xi],ys[yi],j,g_ppu->oam[s*2],hi&3,sprites_oamtile_size_buffer[s]);abort(); } hidden_pieces++;
      } else {all_hidden=false;visible_pieces++;}
    }
    if(all_hidden) {
      obj_ppu=*g_ppu;obj_ppu.screenEnabled[0]=16;obj_ppu.screenEnabled[1]=0;
      obj_ppu.cgadsub=obj_ppu.cgwsel=0;obj_ppu.screenWindowed[0]=0;
      obj_ppu.cgram[0]=0;obj_ppu.paletteValid=false;obj_ppu.spriteListsValid=false;
      PpuBeginDrawing(&obj_ppu,(uint8_t*)obj_pixels,384*4,kPpuRenderFlags_NewRenderer);
      for(unsigned line=0;line<=224;line++) ppu_runLine(&obj_ppu,line);
      uint32_t black=obj_pixels[0];
      for(unsigned y=0;y<224;y++) for(unsigned x=0;x<342;x++) assert(obj_pixels[y*384+x]==black);
    }
  }
}
int main(int argc,char **argv) {
  assert(argc>=4);setbuf(stdout,NULL);guards();bootstrap(argv[1]);
  load_pack(argv[2],false);load_pack(argv[3],true);
  smw_set_widescreen(true);load_level(0x1c);sprite_edges();
  const unsigned dirs[]={SMW_LEFT,SMW_RIGHT,SMW_UP,SMW_DOWN,SMW_LEFT|SMW_UP,SMW_RIGHT|SMW_DOWN,SMW_LEFT|SMW_DOWN,SMW_RIGHT|SMW_UP};
  for(unsigned edition=0;edition<3;edition++) for(unsigned wide=0;wide<2;wide++) for(unsigned half=0;half<2;half++) {
    map_start(argv[1],edition,wide,half);
    for(unsigned d=0;d<8;d++) camera_return(dirs[d]);
    enter_chocolate();chocolate_run(wide,argc>4?argv[4]:NULL,edition);
    yellow_switch(wide);
  }
  puts("PASS: Yellow Switch Palace ladder, full widescreen marker and both map edges, all editions and draw schedules, plus Original level entrance");
  printf("PASS: %u long main-map pans return exactly to Mario; %u hidden / %u unhidden edge pieces; %u carried P-switch frames; EN/JP/Super System, 4:3/16:9, full/alternate draws\n",map_trips,hidden_pieces,visible_pieces,carried_frames);
  return 0;
}
