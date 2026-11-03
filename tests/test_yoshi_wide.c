/* Delayed player/Yoshi graphics, signed OAM, and real Chocolate Island runs. */
#define main v014_fixture_main
#include "test_v014_reports.c"
#undef main
static unsigned projection_cases,pieces_checked,hidden_checked,alias_pixels,live_samples,camera_steps,dismounts;
static Ppu sprite_plane;
static uint32_t sprite_pixels[384*224];
static uint8_t full_state[0x20000];
static uint32_t full_audio;
static uint32_t digest(const void *data,unsigned count) {
  const uint8_t *p=data;uint32_t h=2166136261u;
  while(count--) h=(h^*p++)*16777619u;return h;
}
static void snapshot(void) {
  memcpy(baseline_ram,g_ram,sizeof(g_ram));baseline_ppu=*g_ppu;baseline_snes=*g_snes;
}
static unsigned yoshi_fixture(void) {
  ClearOutNormalSpriteSlots();player_xpos=512;player_ypos=320;
  yoshi_current_yoshi_color=10;SpawnMountedYoshiOnLevelLoad();
  unsigned k=sprites_yoshi_slot_index-1;
  assert(k<12 && spr_spriteid[k]==0x35 && spr_current_status[k]==8);
  SetSprXYPos(k,512,256);spr_oamindex[k]=0x40;
  player_riding_yoshi_flag=0;spr_table00c2[k]=0;
  flag_sprites_locked=1;player_xspeed=player_yspeed=0;
  snapshot();return k;
}
static void projection(unsigned k,int rx,int ry,unsigned action,unsigned facing,bool wide) {
  restore();smw_set_widescreen(wide);ResetSpritesFunc(0);
  mirror_current_layer1_xpos=512-rx;mirror_current_layer1_ypos=512-ry;
  player_xpos=512;player_ypos=496;
  SetSprXYPos(k,512,512);spr_oamindex[k]=0x40;
  yoshi_stray_yoshi_flag=k+1;spr_table157c[k]=facing;
  spr_table00c2[k]=0;player_riding_yoshi_flag=0;
  player_in_air_flag=io_controller_hold1=0;
  if(action==1 || action==3 || action==4) {spr_table00c2[k]=1;player_riding_yoshi_flag=1;}
  if(action==2) {spr_table00c2[k]=2;player_xspeed=32;}
  if(action==3) io_controller_hold1=4;
  if(action==4) {player_in_air_flag=1;player_yspeed=-24;}
  if(action==5) spr_decrementing_table15ac[k]=16;
  if(action==6) {spr_table1594[k]=1;spr_table151c[k]=8;}
  if(action==7) flag_display_yoshis_wings=2;
  unsigned head=64+((yoshi_in_pipe || spr_decrementing_table15ac[k])?1:16),body=81;
  uint16_t before_x=GetSprXPos(k),before_y=GetSprYPos(k);
  SmwWide_BeginFrame();PlayerDraw();CompressOamEntExt();UploadOAMBuffer();
  SmwWide_Latch();SmwWide_PrepareDraw(g_ppu);
  if(!player_riding_yoshi_flag) assert(GetSprXPos(k)==before_x && GetSprYPos(k)==before_y);
  int bx=(int16_t)(GetSprXPos(k)-mirror_current_layer1_xpos);
  int by=(int16_t)(GetSprYPos(k)-mirror_current_layer1_ypos);
  unsigned slots[]={head,body};int sx[2],sy[2];bool show[2];
  for(unsigned j=0;j<2;j++) {
    unsigned slot=slots[j],raw=g_ppu->oam[slot*2];
    sx[j]=bx+(int8_t)((raw&255)-(bx&255));
    sy[j]=by+(int8_t)((raw>>8)-(by&255));
    bool hidden=sprites_oamtile_size_buffer[slot]==1 && (raw&255)==128;
    show[j]=!hidden && (raw>>8)!=240;
    if(hidden) {sx[j]=-128;hidden_checked++;}
    if(wide && (raw>>8)!=240) {
      if(!g_ppu->wideOamValid[slot] || g_ppu->wideOamX[slot]!=sx[j]) {
        fprintf(stderr,"Yoshi piece: action=%u facing=%u rel=%d,%d slot=%u image=%04x expected=%d wide=%d valid=%u hidden=%u\n",action,facing,rx,ry,slot,raw,sx[j],g_ppu->wideOamX[slot],g_ppu->wideOamValid[slot],hidden);abort();
      }
    }
    pieces_checked++;
  }
  /* Isolate the two actual OAM pieces. A wrapped copy cannot hide behind
   * terrain or the other sprites and escape the signed-coordinate check. */
  sprite_plane=*g_ppu;
  for(unsigned i=0;i<128;i++) if(i!=head && i!=body) sprite_plane.oam[i*2]=(sprite_plane.oam[i*2]&255)|0xf000;
  sprite_plane.screenEnabled[0]=16;sprite_plane.screenEnabled[1]=0;
  sprite_plane.screenWindowed[0]=0;sprite_plane.cgadsub=sprite_plane.cgwsel=0;
  sprite_plane.cgram[0]=0;sprite_plane.paletteValid=false;sprite_plane.spriteListsValid=false;
  unsigned width=wide?342:256,pitch=wide?384:256,offset=wide?sprite_plane.extraLeftCur:0;
  PpuBeginDrawing(&sprite_plane,(uint8_t*)sprite_pixels,pitch*4,kPpuRenderFlags_NewRenderer);
  for(unsigned line=0;line<=224;line++) ppu_runLine(&sprite_plane,line);
  uint32_t black=PpuPackRgb(0,0,0);
  for(unsigned y=0;y<224;y++) for(unsigned x=0;x<width;x++) if(sprite_pixels[y*pitch+x]!=black) {
    bool allowed=false;
    for(unsigned j=0;j<2;j++) if(show[j] && (int)x>=sx[j]+(int)offset && (int)x<sx[j]+(int)offset+16 && (int)y>=sy[j] && (int)y<sy[j]+16) allowed=true;
    if(!allowed) {fprintf(stderr,"Yoshi raster alias: action=%u facing=%u rel=%d,%d pixel=%u,%u\n",action,facing,rx,ry,x,y);abort();}
    alias_pixels++;
  }
  projection_cases++;
}
static void live_run(const char *assets,unsigned edition,bool wide,bool half,const char *folder) {
  map_start(assets,edition,wide,half);enter_chocolate();
  /* Native carry-in creation followed by the actual dismount button. */
  yoshi_current_yoshi_color=10;SpawnMountedYoshiOnLevelLoad();
  unsigned k=sprites_yoshi_slot_index-1;
  for(unsigned f=0;f<12;f++) tick(0);
  assert(player_riding_yoshi_flag && spr_spriteid[k]==0x35);
  tick(SMW_A);tick(0);assert(!player_riding_yoshi_flag);dismounts++;
  uint32_t audio_hash=0;int last_camera=mirror_current_layer1_xpos;
  for(unsigned f=0;f<420;f++) {
    uint16_t x=GetSprXPos(k);unsigned index=spr_oamindex[k];
    bool waiting=spr_current_status[k]==8 && spr_spriteid[k]==0x35 && !spr_table00c2[k];
    unsigned head=64+((yoshi_in_pipe || spr_decrementing_table15ac[k])?1:(index>>2)),body=64+((uint8_t)(index+4)>>2);
    tick(SMW_RIGHT|SMW_Y|((f%120)<28?SMW_B:0));
    audio_hash=audio_hash*16777619u^digest(pcm,sizeof(pcm));
    assert(misc_game_mode==20);
    if((int)mirror_current_layer1_xpos!=last_camera) camera_steps++;
    last_camera=mirror_current_layer1_xpos;
    SmwWide_PrepareDraw(g_ppu);
    if(waiting && spr_spriteid[k]==0x35 && GetSprXPos(k)==x) {
      unsigned slots[]={head,body};
      for(unsigned j=0;j<2;j++) {
        unsigned slot=slots[j],raw=g_ppu->oam[slot*2];
        if((g_ppu->oam[slot*2+1]&255)!=(j?8:6) || (raw>>8)==240) continue;
        if(sprites_oamtile_size_buffer[slot]==1 && (raw&255)==128) continue;
        int relative=(int16_t)(x-mirror_current_layer1_xpos);
        int expected=relative+(int8_t)((raw&255)-(relative&255));
        if(wide && (!g_ppu->wideOamValid[slot] || g_ppu->wideOamX[slot]!=expected)) {
          fprintf(stderr,"live Yoshi alias edition=%u frame=%u camera=%u world=%u slot=%u expected=%d actual=%d valid=%u\n",edition,f,mirror_current_layer1_xpos,x,slot,expected,g_ppu->wideOamX[slot],g_ppu->wideOamValid[slot]);abort();
        }
        live_samples++;
      }
    }
    if(folder && wide && !half && (f==80 || f==160)) {
      char name[64];snprintf(name,sizeof(name),"yoshi-%u-%03u",edition,f);ppm(folder,name);
    }
  }
  draw();
  if(!half) {memcpy(full_state,g_ram,sizeof(full_state));full_audio=audio_hash;}
  else {assert(!memcmp(full_state,g_ram,sizeof(full_state)));assert(full_audio==audio_hash);}
  printf("PASS: native carry-in/dismount/camera replay edition=%u %s %s\n",edition,wide?"16:9":"4:3",half?"alternate draws":"full draws");
}
int main(int argc,char **argv) {
  assert(argc>=4);setbuf(stdout,NULL);guards();bootstrap(argv[1]);
  load_pack(argv[2],false);load_pack(argv[3],true);
  const int xs[]={-44,-35,-16,-1,0,15,128,239,255,256,270,298,299,320};
  const int ys[]={-128,-32,-17,-16,-15,-1,0,208,223,224,240,256};
  for(unsigned edition=0;edition<3;edition++) {
    assert(SmwArcade_Select(edition==2) && SmwLocale_Select(edition==1));
    /* Use the real pack/level loader; the geometry fixture only positions actors. */
    assert(smw_init(argv[1],""));
    if(edition==2) {map_start(argv[1],edition,true,false);enter_chocolate();snapshot();}
    else {bootstrap(argv[1]);load_level(0x1c);}
    unsigned k=yoshi_fixture();
    for(unsigned wide=0;wide<2;wide++) for(unsigned facing=0;facing<2;facing++) {
      for(unsigned action=0;action<8;action++) for(unsigned i=0;i<sizeof(xs)/sizeof(*xs);i++) projection(k,xs[i],64,action,facing,wide);
      for(unsigned action=0;action<8;action++) for(unsigned i=0;i<sizeof(ys)/sizeof(*ys);i++) projection(k,39,ys[i],action,facing,wide);
    }
    for(unsigned wide=0;wide<2;wide++) for(unsigned half=0;half<2;half++) live_run(argv[1],edition,wide,half,argc>4?argv[4]:NULL);
  }
  assert(alias_pixels>1000 && live_samples>200 && camera_steps>100 && dismounts==12);
  printf("PASS: %u Yoshi projection fixtures, %u pieces, %u hidden pieces, %u correctly placed raster pixels; %u live pieces across %u camera updates; %u native dismount sessions; EN/JP/NSS, 4:3/16:9, full/alternate drawing\n",projection_cases,pieces_checked,hidden_checked,alias_pixels,live_samples,camera_steps,dismounts);
  return 0;
}
