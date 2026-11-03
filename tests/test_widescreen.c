#include "native_fixture.h"

static void native_projection(void) {
  restore();
  /* Keep the same background projection so all native-area pixels, priority,
   * windows and colour math can be compared exactly, excluding the moved HUD. */
  flag_layer2_horiz_scroll_level_setting=1;
  g_snes->vIrqEnabled=false;
  g_ppu->screenEnabled[0]&=3; g_ppu->screenEnabled[1]&=3;
  Ppu p=*g_ppu;
  smw_set_widescreen(false); smw_draw(classic);
  *g_ppu=p;
  smw_set_widescreen(true); draw();
  assert(g_ppu->wideTile);
  unsigned offset=g_ppu->extraLeftCur;
  for(unsigned y=0;y<224;y++) for(unsigned x=0;x<256;x++) {
    if(classic[y*256+x]!=image.pixels[y*384+x+offset]) {
      fprintf(stderr,"native projection differs at %u,%u\n",x,y); abort();
    }
  }
  puts("PASS: every native-area background pixel, priority and colour-math result preserved");
}
static void sprite_edge(int camera,int x) {
  restore(); smw_set_widescreen(true);
  mirror_current_layer1_xpos=camera;
  memset(spr_current_status,0,12); ResetSpritesFunc(0);
  flag_sprites_locked=1;
  spr_current_status[0]=8; spr_spriteid[0]=0;
  InitializeNormalSpriteRAMTables(0);
  SetSprXYPos(0,camera+x,mirror_current_layer1_ypos+140);
  SmwWide_BeginFrame(); ProcessNormalSprites(); CompressOamEntExt();
  UploadOAMBuffer(); SmwWide_Latch(); SmwWide_PrepareDraw(g_ppu);
  unsigned owned=0;
  for(unsigned i=0;i<128;i++) if(g_ppu->wideOamValid[i]) {
    assert(g_ppu->wideOamX[i]>=x-32 && g_ppu->wideOamX[i]<=x+32); owned++;
  }
  if(SmwWide_InView(x,0)) assert(owned>0);
  g_snes->vIrqEnabled=false; g_ppu->screenEnabled[0]=16; g_ppu->screenEnabled[1]=0;
  g_ppu->cgadsub=g_ppu->cgwsel=0; g_ppu->screenWindowed[0]=0;
  draw();
  int out=x+g_ppu->extraLeftCur;
  uint32_t backdrop=image.pixels[100*384]; unsigned colored=0, aliased=0;
  for(int y=80;y<210;y++) for(int px=0;px<342;px++) if(image.pixels[y*384+px]!=backdrop) {
    if(px>=out-32 && px<out+48) colored++; else aliased++;
  }
  if(SmwWide_InView(x,0)) assert(colored>0);
  else assert(colored==0);
  assert(aliased==0);
  printf("PASS: sprite x=%d, camera=%d, %u signed pieces, %u visible pixels, no wrapped alias\n",x,camera,owned,colored);
}
static void spawns(void) {
  restore(); smw_set_widescreen(true);
  memset(spr_current_status,0,12); memset(sprites_load_status,0,128);
  counter_global_frames&=~1;
  const uint8 *p=GetSpriteListPtr(); int chosen=-1,world=0;
  for(unsigned j=1,index=0;index<84 && p[j]!=255;j+=3,index++) {
    unsigned x=(((p[j]<<3)&16)|(p[j+1]&15))*256+(p[j+1]&0xf0);
    if(x>400 && p[j+2]<0xc9) { chosen=index; world=x; break; }
  }
  assert(chosen>=0);
  mirror_current_layer1_xpos=world-300;
  SmwWide_Spawn();
  unsigned count=0;
  for(unsigned i=0;i<12;i++) if(spr_current_status[i] && spr_load_status_table_index[i]==chosen) count++;
  assert(count==1);
  SmwWide_Spawn(); SmwWide_Spawn();
  count=0;
  for(unsigned i=0;i<12;i++) if(spr_current_status[i] && spr_load_status_table_index[i]==chosen) count++;
  assert(count==1);
  puts("PASS: expanded-frontier placement activates once and cannot duplicate its active slot");
}
static unsigned placement_x(const uint8 *p) {
  return (((p[0]&2)<<3)|(p[1]&15))*256+(p[1]&0xf0);
}
static int placement_slot(unsigned record) {
  for(unsigned s=0;s<12;s++)
    if(spr_current_status[s] && spr_load_status_table_index[s]==record) return s;
  return -1;
}
static unsigned isolate_level_placement(unsigned level,unsigned id,unsigned *world) {
  load_level(level); smw_set_widescreen(true);
  memset(spr_current_status,0,12); ResetSpritesFunc(0);
  memset(sprites_load_status,1,128);
  const uint8 *p=GetSpriteListPtr();
  for(unsigned j=1,index=0;index<84 && p[j]!=255;j+=3,index++) if(p[j+2]==id) {
    *world=placement_x(p+j);
    sprites_load_status[index]=0;
    mirror_current_layer1_xpos=*world-160;
    player_xpos=player_current_xpos=mirror_current_layer1_xpos+80;
    player_ypos=player_current_ypos=0x300; /* Keep the fixture away from Mario. */
    flag_sprites_locked=0; flag_run_cluster_sprites=0;
    counter_global_frames=counter_local_frames=0;
    camera_layer1_scrolling_direction=2;
    return index;
  }
  assert(!"retail placement missing"); return 0;
}
static unsigned isolate_placement(unsigned id,unsigned *world) {
  return isolate_level_placement(0x105,id,world);
}
static void banzai_lifecycle(void) {
  unsigned world,record=isolate_placement(0x9f,&world),loads=0;
  /* Run the real initializer, movement, offscreen erasure and sprite loader
   * with a stationary camera for 12 seconds. The trigger stays in view. */
  for(unsigned f=0;f<720;f++) {
    counter_global_frames=counter_local_frames=f;
    SmwWide_BeginFrame(); ProcessNormalSprites(); GameMode14_InLevel_Bank02();
    int s=placement_slot(record);
    if(s>=0 && spr_current_status[s]==1) ++loads;
  }
  printf("BANZAI stationary camera: record=%u x=%u, %u loads in 720 updates\n",record,world,loads);
  assert(loads==1 && placement_slot(record)<0 && !sprites_load_status[record]);
  puts("PASS: Banzai Bill leaves the view without repeatedly respawning at its visible trigger");
  /* Even the original 4:3 frontier must not bypass the wide visit guard. */
  mirror_current_layer1_xpos=world;
  camera_layer1_scrolling_direction=1;
  for(unsigned n=0;n<120;n++) ParseLevelSpriteList_Entry2();
  assert(placement_slot(record)<0 && !sprites_load_status[record]);
  puts("PASS: native frontier cannot reload a consumed placement still in the wider view");
  mirror_current_layer1_xpos=world+400; ParseLevelSpriteList_Entry2();
  mirror_current_layer1_xpos=world-160; ParseLevelSpriteList_Entry2();
  assert(placement_slot(record)>=0 && sprites_load_status[record]);
  puts("PASS: leaving the activation region rearms a cleared native placement on return");
}
static void spawn_deferral(void) {
  unsigned world,record=isolate_placement(0x9f,&world);
  memset(spr_current_status,8,12);
  memset(spr_load_status_table_index,255,12);
  ParseLevelSpriteList_Entry2();
  assert(!sprites_load_status[record] && placement_slot(record)<0);
  memset(spr_current_status,0,12); ParseLevelSpriteList_Entry2();
  assert(sprites_load_status[record] && placement_slot(record)>=0);
  /* Match the native load-flag-clearing behavior of a transformation. */
  sprites_load_status[record]=0;
  SmwWide_Reset(); ParseLevelSpriteList_Entry2();
  unsigned owned=0;
  for(unsigned s=0;s<12;s++) owned+=spr_current_status[s] && spr_load_status_table_index[s]==record;
  assert(owned==1);
  memset(spr_current_status,0,12);
  for(unsigned n=0;n<120;n++) ParseLevelSpriteList_Entry2();
  assert(placement_slot(record)<0 && !sprites_load_status[record]);
  puts("PASS: full pool defers activation; a transformed/consumed record cannot duplicate");
  misc_game_mode=14; SmwWide_BeginFrame(); misc_game_mode=20;
  ParseLevelSpriteList_Entry2();
  assert(placement_slot(record)>=0 && sprites_load_status[record]);
  puts("PASS: a scene exit/re-entry resets activation state even with the same sprite list");
  memset(spr_current_status,0,12);
  mirror_current_layer1_xpos=world+400; ParseLevelSpriteList_Entry2();
  mirror_current_layer1_xpos=world-160; ParseLevelSpriteList_Entry2();
  assert(placement_slot(record)<0 && sprites_load_status[record]);
  puts("PASS: a retained native one-shot/death flag remains consumed after leaving and returning");
  /* YI1's ordinary pool is slots 1..7, with slot 0 reserved for the Bill.
   * A blocked early record must not starve that later reserved allocation. */
  record=isolate_placement(0x9f,&world);
  assert(record==1 && sprites_sprite_memory_setting==4);
  memset(spr_current_status,8,12); spr_current_status[0]=0;
  memset(spr_load_status_table_index,255,12);
  sprites_load_status[0]=0; mirror_current_layer1_xpos=200;
  ParseLevelSpriteList_Entry2();
  assert(!sprites_load_status[0] && placement_slot(0)<0);
  assert(sprites_load_status[record] && placement_slot(record)==0);
  spr_current_status[1]=0; ParseLevelSpriteList_Entry2();
  assert(sprites_load_status[0] && placement_slot(0)==1 && placement_slot(record)==0);
  puts("PASS: a full ordinary pool does not starve the later reserved Banzai slot");
}
static void group_deferral(void) {
  unsigned levels[]={0x005,0x021},ids[]={0xe0,0xde},expected[]={3,5};
  for(unsigned n=0;n<2;n++) {
    unsigned world,record=isolate_level_placement(levels[n],ids[n],&world);
    assert(SmwWide_Active() && !(misc_level_layout_flags&1));
    memset(spr_current_status,8,12); memset(spr_load_status_table_index,255,12);
    ParseLevelSpriteList_Entry2(); assert(!sprites_load_status[record]);
    memset(spr_current_status,0,12); ParseLevelSpriteList_Entry2();
    assert(sprites_load_status[record]);
    for(unsigned i=0;i<60;i++) ParseLevelSpriteList_Entry2();
    unsigned count=0; for(unsigned s=0;s<12;s++) count+=spr_current_status[s]!=0;
    assert(count==expected[n]);
    memset(spr_current_status,0,12); sprites_load_status[record]=0;
    ParseLevelSpriteList_Entry2();
    for(unsigned s=0;s<12;s++) assert(!spr_current_status[s]);
  }
  puts("PASS: retail platform/Eerie groups defer with zero slots and activate their factory once");
}
static void native_scene_frontier(void) {
  unsigned world,record=isolate_level_placement(0x013,0xe3,&world);
  mirror_current_layer1_xpos=world-320;
  flag_run_cluster_sprites=0; cluster_spr04_boo_ring_index=0;
  assert(SmwWide_InView(world-mirror_current_layer1_xpos,32));
  ParseLevelSpriteList_Entry2();
  assert(!sprites_load_status[record] && !flag_run_cluster_sprites);
  mirror_current_layer1_xpos=world-288; ParseLevelSpriteList_Entry2();
  assert(sprites_load_status[record] && flag_run_cluster_sprites);
  record=isolate_level_placement(0x009,0xe8,&world);
  mirror_current_layer1_xpos=0; l1_l2_scroll_spr_spriteid[0]=l1_l2_scroll_spr_spriteid[1]=0;
  assert(SmwWide_InView(world,32)); ParseLevelSpriteList_Entry2();
  assert(!sprites_load_status[record] && !l1_l2_scroll_spr_spriteid[0]);
  mirror_current_layer1_xpos=world+48; camera_layer1_scrolling_direction=0;
  ParseLevelSpriteList_Entry2();
  assert(sprites_load_status[record] && l1_l2_scroll_spr_spriteid[0]);
  puts("PASS: visible scene effects and scroll commands wait for their native activation frontier");
}
static void despawn_boundaries(void) {
  typedef void (*Offscreen)(uint8,uint8);
  Offscreen banks[]={SubOffscreen_Bank01_01AC33,SubOffscreen_Bank02_02D027,SubOffscreen_Bank03_03B85F};
  unsigned samples=0;
  for(unsigned bank=0;bank<3;bank++) for(unsigned entry=0;entry<4;entry++)
    for(unsigned parity=0;parity<2;parity++) {
      const int positions[]={-35,270,-256,640};
      for(unsigned pos=0;pos<4;pos++) {
        restore(); smw_set_widescreen(true);
        mirror_current_layer1_xpos=300;
        spr_current_status[0]=8; spr_spriteid[0]=0; spr_load_status_table_index[0]=255;
        spr_property_bits167a[0]=0; spr_yoffscreen_flag[0]=1; spr_xoffscreen_flag[0]=1;
        SetSprXYPos(0,300+positions[pos],100);
        counter_global_frames=parity;
        banks[bank](0,entry*2);
        /* The native routine checks alternating left/right sides. */
        counter_global_frames=parity^1; banks[bank](0,entry*2);
        assert((spr_current_status[0]!=0)==(pos<2)); samples++;
      }
    }
  for(unsigned bank=0;bank<3;bank++) {
    restore(); smw_set_widescreen(true);
    spr_current_status[0]=8; spr_load_status_table_index[0]=255;
    spr_property_bits167a[0]=0; spr_yoffscreen_flag[0]=1;
    SetSprXYPos(0,mirror_current_layer1_xpos+100,0x250);
    banks[bank](0,0); assert(!spr_current_status[0]);
    spr_current_status[0]=8; spr_property_bits167a[0]=4;
    SetSprXYPos(0,mirror_current_layer1_xpos+640,100);
    banks[bank](0,0); assert(spr_current_status[0]);
  }
  printf("PASS: %u boundary cases across all three native offscreen banks; fall/keep-alive rules retained\n",samples);
}
static unsigned map_columns(void) {
  if(!SmwWide_Active()) return 0;
  unsigned compared=0;
  int width=(misc_level_layout_flags&1)?512:camera_last_screen_horiz*256;
  int positions[]={0,248,256,504,width-256};
  for(unsigned pos=0;pos<5;pos++) {
    if(positions[pos]<0 || positions[pos]>width-256) continue;
    mirror_current_layer1_xpos=positions[pos];
    CalculateRowOrColumnOfTilemapToUpdate();
    InitializeLevelLayer1And2Tilemaps();
    SmwVectorNMI();
    for(unsigned layer=0;layer<2;layer++) {
      if(layer==1 && !(0x800081feu&(1u<<misc_level_mode_setting))) continue;
      int camera_x=layer?mirror_current_layer2_xpos:mirror_current_layer1_xpos;
      int camera_y=layer?mirror_current_layer2_ypos:mirror_current_layer1_ypos;
      for(unsigned y=48;y<224;y+=8) for(unsigned x=0;x<256;x+=8) {
      unsigned px=(x+g_ppu->hScroll[layer])&1023, py=(y+g_ppu->vScroll[layer])&1023;
      int dx=((g_ppu->hScroll[layer]-camera_x+512)&1023)-512;
      int dy=((g_ppu->vScroll[layer]-camera_y+512)&1023)-512;
      uint16_t tile;
      if(!SmwWide_MapTile(layer,camera_x+x+dx,camera_y+y+dy,&tile)) continue;
      unsigned address=PPU_bgTilemapAdr(g_ppu,layer)+((px>>3)&31)+(((py>>3)&31)*32);
      if(PPU_bgTilemapWider(g_ppu,layer) && (px&256)) address+=0x400;
      if(PPU_bgTilemapHigher(g_ppu,layer) && (py&256)) address+=PPU_bgTilemapWider(g_ppu,layer)?0x800:0x400;
      uint16_t native=g_ppu->vram[address&0x7fff];
      if(tile!=native) {
        fprintf(stderr,"Map16/stream differs cam=%d x=%u y=%u mode=%u tile=%04x native=%04x\n",
          positions[pos],x,y,misc_level_mode_setting,tile,native);
        fprintf(stderr,"cameraY=%u scroll=%u,%u world=%d,%d sc=%u address=%x\n",mirror_current_layer1_ypos,g_ppu->hScroll[0],g_ppu->vScroll[0],mirror_current_layer1_xpos+x+dx,mirror_current_layer1_ypos+y+dy,g_ppu->bgXsc[0],address); abort();
      }
      compared++;
      }
    }
    draw();
  }
  return compared;
}
static void pipes(void) {
  const uint8_t xs[]={0xf8,8,0,0}, ys[]={0,0,0xf0,0x10};
  for(unsigned wide=0;wide<2;wide++) for(unsigned direction=0;direction<4;direction++) {
    restore(); smw_set_widescreen(wide);
    player_pipe_action=direction; player_timer_pipe_warping=3;
    player_xspeed=player_yspeed=0;
    PlayerStateXX_EnterPipe_00D22D();
    assert(player_xspeed==xs[direction] && player_yspeed==ys[direction]);
  }
  puts("PASS: original pipe motion in all four directions, with widescreen on and off");
}
static void cache_invalidation(void) {
  restore(); smw_set_widescreen(true); SmwWide_PrepareDraw(g_ppu);
  uint16_t cached,direct;
  /* Column collisions and successive tile rows must not reuse stale data. */
  int positions[][2]={{16,64},{528,64},{16,72},{16,64},{-16,64},{16,-8}};
  for(unsigned i=0;i<sizeof(positions)/sizeof(*positions);i++) {
    int x=positions[i][0],y=positions[i][1];
    bool a=SmwWide_MapTile(0,x,y,&direct),b=g_ppu->wideMapTile(0,x,y,&cached);
    assert(a==b && (!a || direct==cached));
  }
  /* A tile mutation between draws must be visible even at the same key. */
  assert(SmwWide_MapTile(0,16,64,&direct));
  unsigned address=GetLayerLayoutPtr(0,misc_level_mode_setting)[0]+4*16+1;
  uint8_t lo=g_ram[address],hi=g_ram[address+0x10000];
  uint16_t original=direct;
  bool different=false;
  for(unsigned block=0;block<512;block++) {
    g_ram[address]=block;g_ram[address+0x10000]=block>>8;
    if(SmwWide_MapTile(0,16,64,&direct) && direct!=original) { different=true;break; }
  }
  assert(different); SmwWide_PrepareDraw(g_ppu);
  assert(g_ppu->wideMapTile(0,16,64,&cached) && cached==direct);
  g_ram[address]=lo;g_ram[address+0x10000]=hi;
  SmwWide_PrepareDraw(g_ppu);
  assert(g_ppu->wideMapTile(0,16,64,&cached) && cached==original);
  puts("PASS: side-tile cache collisions, invalid coordinates and per-draw mutations");
}
int main(int argc,char **argv) {
  setvbuf(stdout,NULL,_IONBF,0);
  assert(argc>=2); guards(); bootstrap(argv[1]);
  if(argc>2 && !strcmp(argv[2],"--spawns")) {
    banzai_lifecycle(); spawn_deferral(); group_deferral(); native_scene_frontier();
    despawn_boundaries(); return 0;
  }
  assert(SmwWide_ViewOffset(0,2560)==0);
  assert(SmwWide_ViewOffset(200,2560)==43);
  assert(SmwWide_ViewOffset(2304,2560)==86);
  native_projection();
  sprite_edge(200,-35); sprite_edge(200,270); sprite_edge(0,330);
  sprite_edge(200,330); sprite_edge(200,400); spawns(); pipes(); cache_invalidation();
  banzai_lifecycle(); spawn_deferral(); group_deferral(); native_scene_frontier(); despawn_boundaries();
  /* Known retail sublevels: normal/vertical, caves, water, ghost house,
   * layer-2 and Mode-7 rooms. Use the actual level loading pipeline. */
  unsigned levels[]={0x105,0x106,0x102,0x103,0x109,0x10a,0x10b,0x11a,0x11b,0x013,0x014,0x101,0x009,0x00e,0x1ce,0x10e,0x095,0x096,0x1c7};
  for(unsigned i=0;i<sizeof(levels)/sizeof(*levels);i++) {
    load_level(levels[i]); smw_set_widescreen(true); draw();
    printf("LEVEL %03x mode=%u layout=%u tileset=%u screens=%u wide=%u\n",levels[i],
      misc_level_mode_setting,misc_level_layout_flags,misc_level_tileset_setting,
      camera_last_screen_horiz,g_ppu->wideTile!=NULL);
    char name[64]; snprintf(name,sizeof(name),"level-%03x",levels[i]);
    ppm(argc>2?argv[2]:NULL,name);
#ifdef SMW_WIDE_COMPARE
    SmwTestWideBenchmark(g_ppu,levels[i]);
#endif
    unsigned compared=map_columns();
    if(g_ppu->wideTile) assert(compared>500);
    printf("PASS: %u Map16/VRAM tile descriptors across camera and 256px wrap positions\n",compared);
  }
  restore();
  for(unsigned i=0;i<200;i++) {
    smw_set_widescreen(i&1); *g_ppu=baseline_ppu; *g_snes=baseline_snes; draw();
    assert(smw_video_width()==(i&1?342:256));
  }
  restore(); smw_set_widescreen(true);
  player_current_item_box=1; UpdateStatusBarCounters_DrawItemBoxItem();
  CompressOamEntExt(); UploadOAMBuffer(); SmwWide_PrepareDraw(g_ppu);
  assert(g_ppu->wideOamValid[56] && g_ppu->wideOamX[56]+g_ppu->extraLeftCur==163);
  DropReservedItem();
  bool centered=false;
  for(unsigned i=0;i<12;i++) if(spr_current_status[i] && spr_spriteid[i]==116)
    centered|=GetSprXPos(i)-mirror_current_layer1_xpos+g_ppu->extraLeftCur==163;
  assert(centered);
  puts("PASS: HUD reserve icon/drop alignment, 200 mode changes and framebuffer guards");
  puts("PASS: native widescreen integration; PS2/PCSX2 execution still required");
#ifdef SMW_WIDE_COMPARE
  SmwTestWideReferenceSummary();
#endif
  return 0;
}
