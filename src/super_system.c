/* Native translations of SMWDisX's SS-specific game-program routines.
 * Source: IsoFrieze/SMWDisX, SS symbols in tools/super_system_addresses.json.
 * Shared physics, native sound engine and renderer retain the retail paths.
 * SPDX-License-Identifier: MIT
 */
#include "super_system.h"
#include "localization.h"
#include "src/smw_rtl.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "assets/smw_assets.h"
#include <stdlib.h>
#include <string.h>

enum { TABLES=188 };
static const unsigned slots[]={3,39,178,179,180,181,182,183,184,185,186,187};
static const uint32_t lengths[]={9205,2820,50,16,19,7,8,92,288,14,10,161};
static const uint8_t *tables[TABLES];
static uint8_t *owned;
static bool active;
static uint16_t rd16(const uint8_t *p) { return p[0]|p[1]<<8; }
static uint32_t rd32(const uint8_t *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void stripe16(unsigned at,uint16_t v) { stripe_image_upload_data[at]=v;stripe_image_upload_data[at+1]=v>>8; }

bool SmwArcade_Ready(void) { return owned!=NULL; }
bool SmwArcade_Active(void) { return active; }
void SmwArcade_ApplyAssets(void) {
  if(!active || !owned) return;
  for(unsigned n=0;n<2;n++) {
    unsigned id=slots[n];g_asset_ptrs[id]=tables[id];g_asset_sizes[id]=lengths[n];
  }
}
bool SmwArcade_Load(const void *data,size_t length) {
  if(!data || length<12 || length>64*1024 || memcmp(data,"SMWSS001",8) ||
      rd32((const uint8_t*)data+8)!=sizeof(slots)/sizeof(*slots)) return false;
  const uint8_t *bytes=data;size_t at=12,offsets[TABLES]={0};
  for(unsigned n=0;n<sizeof(slots)/sizeof(*slots);n++) {
    if(at>length-8) return false;
    unsigned id=rd32(bytes+at),size=rd32(bytes+at+4);at+=8;
    if(id!=slots[n] || size!=lengths[n] || size>length-at) return false;
    offsets[id]=at;at=(at+size+3)&~(size_t)3;
  }
  if(at!=length) return false;
  uint8_t *copy=malloc(length);if(!copy) return false;
  memcpy(copy,data,length);
  for(unsigned n=0;n<sizeof(slots)/sizeof(*slots);n++) tables[slots[n]]=copy+offsets[slots[n]];
  free(owned);owned=copy;SmwArcade_ApplyAssets();return true;
}
bool SmwArcade_Select(bool enabled) {
  if(enabled && !owned) return false;
  active=enabled;
  /* NSS was English only; the retail language preference lives in the menu. */
  SmwLocale_Select(false);
  return true;
}

void SmwArcade_InitSaveData(void) { /* SS InitSaveData $009F2A */
  unsigned zone=l1_l2_scroll_spr_current_state[1];
  if(zone>6) zone=l1_l2_scroll_spr_current_state[1]=0;
  memset(ow_save_buffer,0,141);
  for(unsigned at=0;at<tables[182][zone+1];at+=2)
    ow_save_buffer[tables[183][at]]=tables[183][at+1];
  unsigned start=rd16(tables[185]+zone*2)-40;
  memcpy(ow_save_buffer+96,tables[184]+start,42);
}
void SmwArcade_DrawZoneMenu(void) { /* SS DrawFileSelect $009C96 */
  memcpy(stripe_image_upload_data,tables[187],161);
}
void SmwArcade_ZoneSelect(void) { /* SS GM08FileSelect $009C00 */
  unsigned zone=l1_l2_scroll_spr_current_state[1];
  if(zone>6) zone=l1_l2_scroll_spr_current_state[1]=0;
  if((io_controller_press1&0x90) || (io_controller_press2&0x80)) {
    io_sound_ch3=1;
    if(zone) misc_intro_level_flag=0;
    ++misc_game_mode;SmwArcade_InitSaveData();
    ++misc_game_mode;GameMode08_FileSelect_SetStripeImage(0x12);
    return;
  }
  if(io_controller_press1&0x0c) {
    io_sound_ch3=6;
    if(io_controller_press1&8) zone=zone?zone-1:6;
    else zone=zone==6?0:zone+1;
    l1_l2_scroll_spr_current_state[1]=zone;
  }
  memcpy(stripe_image_upload_data,tables[180],19);
  stripe_image_upload_data[13]=tables[181][zone];
  stripe16(16,(counter_global_frames&31)<24?0x3d2e:0x38fc);
}
uint16_t SmwArcade_Cursor(unsigned menu,uint16_t original) {
  return active && menu<5?rd16(tables[186]+menu*2):original;
}
uint16_t SmwArcade_MessageOffset(unsigned message,uint16_t original) {
  return active && message<25?rd16(tables[178]+message*2):original;
}
uint16_t SmwArcade_SwitchPosition(unsigned offset,uint16_t original) {
  return active && offset<16?rd16(tables[179]+offset):original;
}
void SmwArcade_UpdateSaveBuffer(void) { /* SS CODE_049037 $048FD3 */
  if(!flag_show_save_prompt) return;
  for(unsigned i=0;i<45;i++) ow_save_buffer[96+i]=ow_event_flags[i];
  unsigned from=player_current_characterx4,to=from^4;
  for(unsigned i=0;i<8;i+=2) {
    unsigned offset=i<4?117+i:121+i;
    WORD(ow_save_buffer[to+offset])=WORD(ow_save_buffer[from+offset]);
  }
  WORD(ow_save_buffer[((from>>1)^2)+113])=WORD(ow_save_buffer[(from>>1)+113]);
  ow_save_buffer[((from>>2)^1)+111]=ow_save_buffer[(from>>2)+111];
  for(unsigned i=0;i<96;i++) ow_save_buffer[i]=ow_level_tile_settings[i];
  flag_show_save_prompt=0;
}
static void switch_exit(void) {
  ++counter_enemy_rollcall_screen;flag_got_midpoint=1;sub_5B165(1);
}
static void intro_exit(void) {
  LOBYTE(get_PointU16(ow_players_pos,0)->y)=0x8e;
  DisplayMessage_ExitToOverworldNoEvent();
}
void SmwArcade_MessagePrompt(void) { /* SS CODE_05B132 */
  if((misc_color_of_palace_switch_pressed1|misc_intro_level_flag) &&
      !(counter_global_frames&3) && !--timer_title_screen_input_timer) {
    if(misc_color_of_palace_switch_pressed1) switch_exit();else intro_exit();
    return;
  }
  bool accepted=(io_controller_hold1&0xf0) &&
    (!((io_controller_press1^(io_controller_hold1&0xf0))&0xf0) ||
     ((io_controller_hold2&0xc0) && !((io_controller_press2^(io_controller_hold2&0xc0))&0xc0)));
  if(!accepted) return;
  if(misc_color_of_palace_switch_pressed1) switch_exit();
  else if(misc_intro_level_flag) intro_exit();
  else ++flag_message_window_size_change_direction;
}
void SmwArcade_CastleText(void) { /* SS CODE_0CC94E */
  if(!l1_l2_scroll_spr_current_state[1]) {
    if(!ow_enter_level_flag) ow_enter_level_flag=0x80;
    return;
  }
  if(io_controller_hold1|io_controller_hold2) {
    unsigned block=l1_l2_scroll_spr_current_state[1]&0xe0;
    l1_l2_scroll_spr_current_state[1]=block?block+1:1;
  }
  if(!(--l1_l2_scroll_spr_current_state[1]&31)) {
    unsigned row=8*(misc_currently_active_boss_end_cutscene-1)|(l1_l2_scroll_spr_current_state[1]>>5);
    graphics_stripe_image_to_upload=3*row+33;
  }
}
bool SmwArcade_EndCastle(void) { /* SS CODE_0CCFDE */
  return !--ow_enter_level_flag || (io_controller_press1|io_controller_press2);
}
