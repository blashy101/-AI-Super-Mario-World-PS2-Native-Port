/* Widen SMW's native projection without changing its physics camera.
 * The PPU still owns scanline IRQ/HDMA, priority, windows and colour math.
 * Map16 supplies columns that the original 256px VRAM streamer has not loaded.
 * Sprite pieces retain signed coordinates alongside their exact OAM image.
 * Coordinate/viewport/HUD and sprite lifecycle adaptation reference:
 * SuperMarioWorldRecomp, Copyright (c) 2026 Matthew Stanley.
 * PS2 integration additions, Copyright (c) 2026 SMW PS2 contributors.
 * SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
 * See licenses/SMWRecomp-PolyForm-Noncommercial-1.0.0.txt and docs/SOURCES.md.
 */
#include "widescreen.h"
#include "src/smw_rtl.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "src/consts.h"
#include "src/snes/ppu.h"
#include "assets/smw_assets.h"
#include <string.h>

static bool enabled;
/* Shared fades also serve title/overworld scenes. Keep the outgoing scene's
 * projection until a loader takes over; incoming level fade starts wide. */
static bool level_scene,map_scene;
/* Geometry stays fixed during a draw; scroll registers resolve per line. */
static bool draw_main_map;
static int draw_map_offset;
static void track_scene(void) {
  unsigned mode=misc_game_mode;
  if(mode==0x13 || mode==0x14) { level_scene=!misc_intro_level_flag; map_scene=false; }
  else if(mode==0x0d || mode==0x0e) { map_scene=true; level_scene=false; }
  else if(mode!=0x0b && mode!=0x0f && mode!=0x15 && mode!=0x18) level_scene=map_scene=false;
}
/* Placement activation is gameplay state, independent of rendering/skips.
 * Adapted from SMWRecomp's SpawnState/visited guard in smw_renderer_hooks.c.
 * A cleared native load flag may mean a transformation or a departing actor,
 * rather than another entry of the placement into the expanded view. */
static const uint8_t *spawn_list;
static uint8_t spawn_visited[128];
static void reset_spawns(void) {
  spawn_list=NULL; memset(spawn_visited,0,sizeof(spawn_visited));
}
typedef struct WidePiece { uint32_t image; int16_t x; bool valid; } WidePiece;
static WidePiece pending[128], latched[128];
/* One tile row per column/layer, 1.5 KiB total. All Map16 state is stable
 * during a supported Mode-1 scanout; a new draw always changes generation. */
typedef struct WideTileCache {
  uint32_t key,generation;
  uint16_t tile;
  bool valid;
} WideTileCache;
static WideTileCache tile_cache[2][64];
static uint32_t tile_generation;
static struct {
  uint32_t before[128];
  int x, y;
  bool active;
} actor;

static uint32_t oam_image(unsigned slot) {
  const uint8_t *p=g_ram+0x200+slot*4;
  return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
bool SmwWide_Enabled(void) { return enabled; }
bool SmwWide_Active(void) {
  return enabled && !g_lunar_magic && misc_game_mode==20 && !misc_intro_level_flag &&
    !(misc_nmito_use_flag&0x80) && misc_level_mode_setting<32 &&
    ((misc_level_layout_flags&1) || camera_last_screen_horiz>=2) &&
    GetLayerLayoutPtr(0,misc_level_mode_setting)!=NULL;
}
void SmwWide_Reset(void) {
  level_scene=map_scene=false;
  reset_spawns();
  memset(pending,0,sizeof(pending)); memset(latched,0,sizeof(latched));
  actor.active=false;
  tile_generation=0; memset(tile_cache,0,sizeof(tile_cache));
}
void SmwWide_SetEnabled(bool value) {
  if(enabled!=value) { enabled=value; SmwWide_Reset(); }
}
int SmwWide_ViewOffset(int camera,int level_width) {
  int origin=camera-SMW_WIDE_EXTRA;
  int limit=level_width-SMW_WIDE_WIDTH;
  if(limit<0) return SMW_WIDE_EXTRA;
  if(origin<0) origin=0;
  if(origin>limit) origin=limit;
  int offset=camera-origin;
  if(offset<0) offset=0;
  if(offset>SMW_WIDE_EXTRA*2) offset=SMW_WIDE_EXTRA*2;
  return offset;
}
static int view_offset(void) {
  int width=(misc_level_layout_flags&1)?512:camera_last_screen_horiz*256;
  return SmwWide_ViewOffset(mirror_current_layer1_xpos,width);
}
bool SmwWide_Overworld(void) {
  return enabled && !g_lunar_magic && (misc_game_mode==0x0d || misc_game_mode==0x0e);
}
static bool main_map(void) { return ow_players_map[player_current_character]==0; }
static int map_offset(void) {
  return main_map()?SmwWide_ViewOffset((int16_t)mirror_current_layer1_xpos,512):SMW_WIDE_EXTRA;
}
bool SmwWide_MapInView(int x,int padding) {
  if(!SmwWide_Overworld()) return (unsigned)x<256;
  int offset=map_offset();
  return x>=-offset-padding && x<SMW_WIDE_WIDTH-offset+padding;
}
int SmwWide_ReserveX(void) {
  return SmwWide_Active()?SMW_WIDE_WIDTH/2-8-view_offset():120;
}
bool SmwWide_InView(int x,int padding) {
  int left=view_offset();
  return x>=-left-padding && x<SMW_WIDE_WIDTH-left+padding;
}
bool SmwWide_ExtendedInView(unsigned k) {
  int x=(int16_t)(PAIR16(ext_spr_xpos_hi[k],ext_spr_xpos_lo[k])-mirror_current_layer1_xpos);
  return SmwWide_Active()?SmwWide_InView(x,16):(unsigned)x<256;
}
bool SmwWide_ShouldErase(unsigned k,int bound,unsigned side) {
  /* Keep each native offscreen entry's side/parity and special reach, but
   * move its boundary beyond the wider view. Leave vertical/fall erasure
   * and per-sprite "process offscreen" properties with the original game. */
  int delta=(int16_t)(bound+mirror_current_layer1_xpos-GetSprXPos(k));
  int left=view_offset();
  int extra=((side&1)?left:SMW_WIDE_EXTRA*2-left)+32;
  int threshold=bound<=-64?extra:bound+64+extra;
  return (side&1)?delta>=threshold:delta<-extra;
}

bool SmwWide_MapTile(unsigned layer,int x,int y,uint16_t *tile) {
  if(layer>1 || misc_level_mode_setting>=32 || x<0 || y<0) return false;
  const uint16 *layout=GetLayerLayoutPtr(layer,misc_level_mode_setting);
  if(!layout) return false;
  unsigned screen,index;
  if(misc_level_layout_flags&(1u<<layer)) {
    if(x>=512 || y>=0x1c00) return false;
    screen=(unsigned)y>>8;
    index=((x>>8)*256)+((y&255)>>4)*16+((x&255)>>4);
  } else {
    int count=layer==0?camera_last_screen_horiz:32;
    if(x>=count*256 || y>=432) return false;
    screen=(unsigned)x>>8;
    index=(y>>4)*16+((x&255)>>4);
  }
  unsigned screens=layer?((misc_level_layout_flags&2)?14:16):
    ((misc_level_layout_flags&1)?28:32);
  if(screen>=screens) return false;
  unsigned low=layout[screen]+index;
  if(low<0xc800 || low>0xffff) return false;
  unsigned block=g_ram[low]|g_ram[0x10000+low]<<8;
  if(block>=512) return false;
  unsigned address=pointer_map16_tiles[block];
  if(misc_level_tileset_setting<16 && block>=0x133 && block<=0x13a) {
    unsigned page=(misc_level_layout_flags&(1u<<layer))?(unsigned)y>>8:(unsigned)x>>8;
    address=kCalculateRowOrColumnOfTilemapToUpdate_PipeMap16Ptrs[page&3]+(block-0x133)*8;
  }
  if(address<0x8000) return false;
  const uint16 *definition=GetMap16RomAddr(misc_level_tileset_setting<16?13:5,address);
  *tile=definition[(x&8?2:0)+(y&8?1:0)];
  if(layer==1 && misc_level_tileset_setting==3) *tile|=0x1000;
  return true;
}
static bool cached_map_tile(unsigned layer,int x,int y,uint16_t *tile) {
  uint32_t tx=(uint32_t)x>>3, ty=(uint32_t)y>>3;
  uint32_t key=(tx&0xffffu)|(ty<<16);
  WideTileCache *entry=&tile_cache[layer][tx&63];
  if(entry->generation!=tile_generation || entry->key!=key) {
    entry->generation=tile_generation; entry->key=key;
    entry->valid=SmwWide_MapTile(layer,x,y,&entry->tile);
  }
  *tile=entry->tile; return entry->valid;
}
static bool world_layer(unsigned layer) {
  return layer==0 || (layer==1 && (0x800081feu&(1u<<misc_level_mode_setting)));
}
static bool tile_sample(Ppu *p,unsigned layer,int sx,int y,PpuWideTile *sample) {
  int x=sx, left=p->extraLeftCur;
  sample->run=8;
  /* Source scanline 40 is the message's top edge, beyond the HUD. */
  if(layer==2 && y<40) {
    int out=sx+left, end;
    if(out<112) { x=out; end=112; }
    else if(out>=SMW_WIDE_WIDTH-112) { x=out-(SMW_WIDE_WIDTH-256); end=SMW_WIDE_WIDTH; }
    else if(out>=SMW_WIDE_WIDTH/2-16 && out<SMW_WIDE_WIDTH/2+16) {
      x=out-(SMW_WIDE_WIDTH/2-128); end=SMW_WIDE_WIDTH/2+16;
    } else {
      end=out<SMW_WIDE_WIDTH/2-16?SMW_WIDE_WIDTH/2-16:SMW_WIDE_WIDTH-112;
      sample->run=end-out; return false;
    }
    sample->run=end-out;
  } else if(layer==2 && misc_display_message && (sx<0 || sx>=256)) {
    sample->run=sx<0?-sx:SMW_WIDE_WIDTH-left-sx; return false;
  }
  if(layer==1 && !world_layer(1) && !(misc_level_layout_flags&1)) {
    unsigned setting=flag_layer2_horiz_scroll_level_setting;
    int camera=mirror_current_layer1_xpos, origin=camera-left;
    x+=!setting?left:setting==1?0:left+origin/2-camera/2;
  }
  int px=(x+p->hScroll[layer])&1023, py=(y+p->vScroll[layer])&1023;
  if(layer<2 && world_layer(layer) && (sx<0 || sx>=256)) {
    int cx=layer?mirror_current_layer2_xpos:mirror_current_layer1_xpos;
    int cy=layer?mirror_current_layer2_ypos:mirror_current_layer1_ypos;
    int dx=((p->hScroll[layer]-cx+512)&1023)-512;
    int dy=((p->vScroll[layer]-cy+512)&1023)-512;
    px=cx+sx+dx; py=cy+y+dy;
    if(!SmwWide_MapTile(layer,px,py,&sample->tile)) return false;
  } else {
    unsigned address=PPU_bgTilemapAdr(p,layer)+((px>>3)&31)+(((py>>3)&31)*32);
    if(PPU_bgTilemapWider(p,layer) && (px&256)) address+=0x400;
    if(PPU_bgTilemapHigher(p,layer) && (py&256)) address+=PPU_bgTilemapWider(p,layer)?0x800:0x400;
    sample->tile=p->vram[address&0x7fff];
  }
  sample->x=px; sample->y=py;
  /* A native/Map16 source boundary may split a tile at fractional scroll. */
  if(sx<0 && sample->run>-sx) sample->run=-sx;
  if(sx>=0 && sx<256 && sample->run>256-sx) sample->run=256-sx;
  return true;
}
/* Resolve geometry once for a full band instead of once per eight pixels.
 * This callback runs at scanline time, after any preceding HDMA/IRQ writes. */
static bool prepare_span(Ppu *p,unsigned layer,int sx,int y,PpuWideSpan *span) {
  int x=sx, left=p->extraLeftCur;
  span->end=256+p->extraRightCur;
  span->map16=false;
  if(layer==2 && y<40) {
    int out=sx+left, end;
    if(out<112) { x=out; end=112; }
    else if(out>=SMW_WIDE_WIDTH-112) { x=out-(SMW_WIDE_WIDTH-256); end=SMW_WIDE_WIDTH; }
    else if(out>=SMW_WIDE_WIDTH/2-16 && out<SMW_WIDE_WIDTH/2+16) {
      x=out-(SMW_WIDE_WIDTH/2-128); end=SMW_WIDE_WIDTH/2+16;
    } else {
      end=out<SMW_WIDE_WIDTH/2-16?SMW_WIDE_WIDTH/2-16:SMW_WIDE_WIDTH-112;
      span->end=end-left; return false;
    }
    span->end=end-left;
  } else if(layer==2 && misc_display_message) {
    if(sx<0) { span->end=0; return false; }
    if(sx>=256) return false;
    span->end=256;
  }
  if(layer==1 && !world_layer(1) && !(misc_level_layout_flags&1)) {
    unsigned setting=flag_layer2_horiz_scroll_level_setting;
    int camera=mirror_current_layer1_xpos, origin=camera-left;
    x+=!setting?left:setting==1?0:left+origin/2-camera/2;
  }
  if(layer<2 && world_layer(layer)) {
    if(sx<0) span->end=0;
    else if(sx<256) span->end=256;
    if(sx<0 || sx>=256) {
      int cx=layer?mirror_current_layer2_xpos:mirror_current_layer1_xpos;
      int cy=layer?mirror_current_layer2_ypos:mirror_current_layer1_ypos;
      int dx=((p->hScroll[layer]-cx+512)&1023)-512;
      int dy=((p->vScroll[layer]-cy+512)&1023)-512;
      span->x=cx+sx+dx; span->y=cy+y+dy; span->map16=true;
      return true;
    }
  }
  span->x=x+p->hScroll[layer]; span->y=y+p->vScroll[layer];
  return true;
}
/* Anchor the added mat to the native tile grid, including its 43px inset.
 * Physical-screen alignment cut 8px glyphs into 3px/5px pieces at the joins. */
static inline void map_mat_projection(int sx,int y,int *x,int *py,int *end) {
  *x=sx&15;
  *py=((y/16+((sx+64)>>4)*5)%14)*16+(y&15);
  *end=((sx+16)&~15)+SMW_WIDE_EXTRA;
}
/* The main overworld already has its complete 512x512 VRAM tilemaps. Reveal
 * their extra columns without touching the map's camera, movement or events.
 * Fixed submaps keep their original composition inside a wider patterned mat. */
static bool map_span(Ppu *p,unsigned layer,int sx,int y,PpuWideSpan *span) {
  int out=sx+SMW_WIDE_EXTRA,x=sx,py=y,end=SMW_WIDE_WIDTH;
  span->map16=false;
  if(layer<2) {
    if(!draw_main_map) {
      if(sx<0) { span->end=0; return false; }
      if(sx>=256) { span->end=SMW_WIDE_WIDTH-SMW_WIDE_EXTRA; return false; }
      end=256+SMW_WIDE_EXTRA;
    } else x+=SMW_WIDE_EXTRA-draw_map_offset;
  } else if(y<40) {
    /* Keep portrait, masking tiles and lives in their native header lane.
     * Relocating the portrait into negative X crossed native window bounds. */
    if(out<SMW_WIDE_EXTRA) { map_mat_projection(sx,y,&x,&py,&end); if(end>43) end=43; }
    else if(out<256+SMW_WIDE_EXTRA) { x=out-SMW_WIDE_EXTRA; end=256+SMW_WIDE_EXTRA; }
    else { map_mat_projection(sx,y,&x,&py,&end); }
  } else if(!draw_main_map) {
    if(out<SMW_WIDE_EXTRA) { map_mat_projection(sx,y,&x,&py,&end); if(end>43) end=43; }
    else if(out<256+SMW_WIDE_EXTRA) { x=out-SMW_WIDE_EXTRA; end=256+SMW_WIDE_EXTRA; }
    else { map_mat_projection(sx,y,&x,&py,&end); }
  } else {
    if(out<24) { x=out; end=24; }
    else if(out>=SMW_WIDE_WIDTH-24) x=out-SMW_WIDE_EXTRA*2;
    else if(out<24+SMW_WIDE_EXTRA) { x=24+(out&7); end=(out+8)&~7; if(end>67) end=67; }
    else if(out>=232+SMW_WIDE_EXTRA) { x=224+(out&7); end=(out+8)&~7; if(end>318) end=318; }
    else { x=out-SMW_WIDE_EXTRA; end=232+SMW_WIDE_EXTRA; }
  }
  if(end>SMW_WIDE_WIDTH) end=SMW_WIDE_WIDTH;
  span->end=end-SMW_WIDE_EXTRA;
  span->x=x+p->hScroll[layer]; span->y=py+p->vScroll[layer];
  return true;
}
static bool map_sample(Ppu *p,unsigned layer,int sx,int y,PpuWideTile *sample) {
  PpuWideSpan span;
  bool visible=map_span(p,layer,sx,y,&span);
  sample->run=span.end-sx;
  if(!visible) return false;
  unsigned x=span.x&1023,py=span.y&1023;
  unsigned address=PPU_bgTilemapAdr(p,layer)+(x>>3&31)+(py>>3&31)*32;
  if(PPU_bgTilemapWider(p,layer) && (x&256)) address+=0x400;
  if(PPU_bgTilemapHigher(p,layer) && (py&256)) address+=PPU_bgTilemapWider(p,layer)?0x800:0x400;
  sample->tile=p->vram[address&0x7fff]; sample->x=x; sample->y=py;
  return true;
}
void SmwWide_PrepareDraw(Ppu *p) {
  track_scene();
  if(++tile_generation==0) {
    memset(tile_cache,0,sizeof(tile_cache)); tile_generation=1;
  }
  p->extraLeftRight=enabled?SMW_WIDE_EXTRA:0;
  /* Fade/mosaic/iris keep the same projection. Narrow rooms retain their
   * physical bounds with a centered foreground and expanded background/HUD. */
  bool scene=enabled && level_scene && !g_lunar_magic && !misc_intro_level_flag &&
    !(misc_nmito_use_flag&0x80) && misc_level_mode_setting<32 &&
    GetLayerLayoutPtr(0,misc_level_mode_setting)!=NULL && PPU_mode(p)==1;
  bool map=enabled && map_scene && !g_lunar_magic && PPU_mode(p)==1;
  if(map) { draw_main_map=main_map(); draw_map_offset=map_offset(); }
  p->wideBackdropMath=map;
  p->wideMapIris=map && draw_main_map && (mirror_hdmaenable&0x80) &&
    mirror_bg1_and2_window_mask_settings==0x33 && mirror_object_and_color_window_settings==0x33;
  p->wideTile=scene?tile_sample:map?map_sample:NULL;
  p->wideSpan=scene?prepare_span:map?map_span:NULL;
  p->wideMapTile=scene?cached_map_tile:NULL;
  p->extraLeftCur=scene?view_offset():map?SMW_WIDE_EXTRA:0;
  p->extraRightCur=scene?SMW_WIDE_EXTRA*2-p->extraLeftCur:map?SMW_WIDE_EXTRA:0;
  for(unsigned i=0;i<128;i++) {
    p->wideOamValid[i]=(scene || map) && latched[i].valid && latched[i].image==oam_image(i);
    p->wideOamX[i]=latched[i].x;
    if(map) {
      bool portrait=i<16 || (i>=120 && (p->oam[i*2]>>8)<40);
      if(portrait || !p->wideOamValid[i]) {
        unsigned high=p->highOam[i>>2]>>((i&3)*2);
        p->wideOamX[i]=(p->oam[i*2]&255)-((high&1)<<8);
      }
      p->wideOamValid[i]=true;
      /* UI coordinates come from uploaded OAM, independent of actor metadata. */
      p->wideOamX[i]+=portrait?0:draw_map_offset-SMW_WIDE_EXTRA;
    }
  }
  if(scene && player_current_item_box) {
    unsigned slot=(misc_nmito_use_flag&0x40)?0:56;
    if((g_ppu->oam[slot*2]>>8)==15) {
      p->wideOamValid[slot]=true;
      p->wideOamX[slot]=SMW_WIDE_WIDTH/2-8-p->extraLeftCur;
    }
  }
}
void SmwWide_BeginFrame(void) {
  if(!SmwWide_Active() || (misc_level_layout_flags&1)) reset_spawns();
  if(!enabled) return;
  memset(pending,0,sizeof(pending)); actor.active=false;
}
void SmwWide_BeginActor(unsigned kind,unsigned k) {
  if(!SmwWide_Active()) return;
  SmwWide_EndActor();
  for(unsigned i=0;i<128;i++) actor.before[i]=oam_image(i);
  actor.x=(int16_t)((kind?PAIR16(ext_spr_xpos_hi[k],ext_spr_xpos_lo[k]):GetSprXPos(k))-mirror_current_layer1_xpos);
  actor.y=(int16_t)((kind?PAIR16(ext_spr_ypos_hi[k],ext_spr_ypos_lo[k]):GetSprYPos(k))-mirror_current_layer1_ypos);
  actor.active=true;
}
void SmwWide_BeginMapActor(unsigned k) {
  if(!SmwWide_Overworld()) return;
  SmwWide_EndActor();
  for(unsigned i=0;i<128;i++) actor.before[i]=oam_image(i);
  actor.x=(int16_t)(PAIR16(owspr_xpos_hi[k],owspr_xpos_lo[k])-mirror_current_layer1_xpos);
  actor.y=(int16_t)(PAIR16(owspr_ypos_hi[k],owspr_ypos_lo[k])-mirror_current_layer1_ypos)-owspr_zpos_lo[k];
  actor.active=true;
}
void SmwWide_RecordMapPiece(unsigned slot,int x) {
  if(SmwWide_Overworld() && slot<128)
    pending[slot]=(WidePiece){.image=oam_image(slot),.x=x,.valid=true};
}
void SmwWide_MapPlayer(unsigned player,unsigned first,unsigned last) {
  if(!SmwWide_Overworld()) return;
  int origin=(int16_t)(get_PointU16(ow_players_pos,player*4)->x-mirror_current_layer1_xpos)-8;
  for(unsigned i=first;i<=last;i++) {
    uint32_t image=oam_image(i);
    if(((image>>8)&255)!=240) SmwWide_RecordMapPiece(i,origin+(int8_t)((image&255)-(origin&255)));
  }
}
void SmwWide_DrawInfo(unsigned k) {
  if(!SmwWide_Active() || !actor.active) return;
  actor.x=(int16_t)(GetSprXPos(k)-mirror_current_layer1_xpos);
  actor.y=(int16_t)(GetSprYPos(k)-mirror_current_layer1_ypos);
}
void SmwWide_RecordPiece(unsigned slot,int x) {
  if(!SmwWide_Active() || slot>=128) return;
  pending[slot]=(WidePiece){.image=oam_image(slot),.x=x,.valid=true};
}
void SmwWide_RecordExtendedPiece(unsigned k,unsigned slot) {
  if(!SmwWide_Active() || k>=10 || slot>=128) return;
  int origin=(int16_t)(PAIR16(ext_spr_xpos_hi[k],ext_spr_xpos_lo[k])-mirror_current_layer1_xpos);
  uint32_t image=oam_image(slot);
  SmwWide_RecordPiece(slot,origin+(int8_t)((image&255)-(origin&255)));
}
void SmwWide_EndActor(void) {
  if(!actor.active) return;
  for(unsigned i=0;i<128;i++) {
    uint32_t image=oam_image(i);
    if(image==actor.before[i] || (pending[i].valid && pending[i].image==image) ||
        ((image>>8)&255)==240) continue;
    int dx=(int8_t)((image&255)-(actor.x&255));
    int dy=(int8_t)(((image>>8)&255)-(actor.y&255));
    if(dx>=-96 && dx<=96 && dy>=-96 && dy<=96)
      pending[i]=(WidePiece){.image=image,.x=actor.x+dx,.valid=true};
  }
  actor.active=false;
}
void SmwWide_Latch(void) {
  track_scene();
  if(!enabled) return;
  SmwWide_EndActor();
  /* Loader/title/map OAM must not inherit the preceding room's signed
   * coordinates just because a reused native tile has identical bytes. */
  if(!level_scene && !map_scene) {
    memset(pending,0,sizeof(pending)); memset(latched,0,sizeof(latched));
    return;
  }
  for(unsigned i=0;i<128;i++) {
    if(pending[i].valid && pending[i].image==oam_image(i)) latched[i]=pending[i];
    else if(!latched[i].valid || latched[i].image!=oam_image(i)) latched[i].valid=false;
  }
}
bool SmwWide_Spawn(void) {
  if(!SmwWide_Active() || (misc_level_layout_flags&1)) return false;
  int left=mirror_current_layer1_xpos-view_offset()-32;
  int right=left+SMW_WIDE_WIDTH+64;
  const uint8 *p=GetSpriteListPtr();
  if(spawn_list!=p) { reset_spawns(); spawn_list=p; }
  /* Replace the native frontier sweep, rather than running an independent
   * second loader. Scene commands/generators retain the native frontier.
   * Ordinary actors, shooters and placed groups use the expanded view. */
  int edge=(mirror_current_layer1_xpos+(camera_layer1_scrolling_direction==0?-48:
      camera_layer1_scrolling_direction==2?288:0))&~15;
  for(unsigned j=1,index=0;index<128;j+=3,index++) {
    if(p[j]==255) break;
    unsigned x=(((p[j]<<3)&0x10)|(p[j+1]&15))*256+(p[j+1]&0xf0);
    unsigned id=p[j+2];
    if((id>=0xcb && id<0xda) || id>=0xe1) {
      if((int)x==edge && !sprites_load_status[index])
        LoadOneSprite(p+j,index,(PointU16){.x=x,.y=0});
      continue;
    }
    if((int)x<left || (int)x>=right) { spawn_visited[index]=0; continue; }
    if(sprites_load_status[index]) spawn_visited[index]=1;
    if(spawn_visited[index]) continue;
    /* Transformed sprites can still own a record whose load flag is clear. */
    bool active=false;
    for(unsigned s=0;s<12;s++)
      if(spr_current_status[s] && spr_load_status_table_index[s]==index) { active=true; break; }
    if(active) { spawn_visited[index]=1; continue; }
    /* Native group factories leave the flag set even if they find no slot.
     * Do not consume a whole group before its first actor can be allocated. */
    if((id==0xde || id==0xe0) && (FindFreeNormalSpriteSlot_HighPriority()&0x80)) continue;
    LoadOneSprite(p+j,index,(PointU16){.x=x,.y=0});
    /* Allocation failures clear the native flag: defer, do not consume the
     * record or stop a later actor from using a different reserved pool. */
    if(sprites_load_status[index]) spawn_visited[index]=1;
  }
  return true;
}
