/* Execute the original assembled NSS instructions as an independent oracle.
 * The test-only CPU and tiny program capsule are never linked into the PS2 ELF.
 */
#include "smw_ps2.h"
#include "super_system.h"
#include "src/smw_rtl.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "src/snes/cpu.h"
#include "src/snes/snes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t reference_ram[0x20000],reference_rom[0x80000],covered[0x80000];
static unsigned cases;
Cpu *ss_ref_cpu;
int ss_ref_cpu_runOpcode(Cpu *cpu);
uint8_t SmwTestSSRead(Snes *unused,uint32_t address) {
  (void)unused;unsigned bank=address>>16,low=address&65535;
  if(bank==0x7e || bank==0x7f) return reference_ram[address-0x7e0000];
  if((bank&0x7f)<0x40 && low<0x2000) return reference_ram[low];
  if(low>=0x8000) {
    unsigned pc=(bank&0x7f)*0x8000+(low&0x7fff);
    if(pc<sizeof(reference_rom) && covered[pc]) return reference_rom[pc];
  }
  fprintf(stderr,"SS reference read outside frozen capsule: %06x case=%u PC=%02x:%04x DB=%02x A=%04x X=%04x Y=%04x SP=%04x\n",address,cases,ss_ref_cpu->k,ss_ref_cpu->pc,ss_ref_cpu->db,ss_ref_cpu->a,ss_ref_cpu->x,ss_ref_cpu->y,ss_ref_cpu->sp);abort();
}
void SmwTestSSWrite(Snes *unused,uint32_t address,uint8_t value) {
  (void)unused;unsigned bank=address>>16,low=address&65535;
  if(bank==0x7e || bank==0x7f) reference_ram[address-0x7e0000]=value;
  else if((bank&0x7f)<0x40 && low<0x2000) reference_ram[low]=value;
  else { fprintf(stderr,"SS reference write outside RAM: %06x\n",address);abort(); }
}
int SmwTestSSOpcodeHook(uint32_t address) {
  fprintf(stderr,"Unexpected SS reference BRK at %06x\n",address);abort();
}
bool SmwTestSSHookedRts(int unused) { (void)unused;assert(0);return false; }
static unsigned read32(FILE *f) {
  uint8_t b[4];assert(fread(b,1,4,f)==4);
  return b[0]|(unsigned)b[1]<<8|(unsigned)b[2]<<16|(unsigned)b[3]<<24;
}
static void load(const char *path) {
  FILE *f=fopen(path,"rb");assert(f);char sig[8];assert(fread(sig,1,8,f)==8 && !memcmp(sig,"SSREF001",8));
  unsigned count=read32(f);assert(count==7);
  for(unsigned n=0;n<count;n++) {
    unsigned a=read32(f),size=read32(f),pc=(a>>16)*0x8000+(a&0x7fff);
    assert(size && pc<sizeof(reference_rom) && size<=sizeof(reference_rom)-pc);
    assert(fread(reference_rom+pc,1,size,f)==size);memset(covered+pc,1,size);
  }
  assert(fgetc(f)==EOF);fclose(f);
}
static void seed(unsigned value) {
  uint32_t r=value+0xc9fa341d;
  for(unsigned i=0;i<sizeof(g_ram);i++) {
    r^=r<<13;r^=r>>17;r^=r<<5;g_ram[i]=r;
  }
}
static void run(unsigned address,bool is_long) {
  memcpy(reference_ram,g_ram,sizeof(g_ram));
  Cpu c={.mf=true,.xf=true,.i=true,.k=address>>16,.db=address>>16,.pc=address&65535,.sp=is_long?0x1fc:0x1fd};
  reference_ram[c.sp+1]=0xfd;reference_ram[c.sp+2]=0x7f;
  if(is_long) reference_ram[c.sp+3]=0;
  ss_ref_cpu=&c;
  unsigned ticks=0;
  while(!(c.pc==0x7ffe && c.sp==0x1ff)) { assert(++ticks<100000);ss_ref_cpu_runOpcode(&c); }
  ss_ref_cpu=NULL;cases++;
}
static void same(const void *p,unsigned size,const char *name) {
  size_t at=(const uint8_t*)p-g_ram;assert(at+size<=sizeof(g_ram));
  for(unsigned i=0;i<size;i++) if(g_ram[at+i]!=reference_ram[at+i]) {
    fprintf(stderr,"SS assembly mismatch %s +%u (%05zx): native=%02x assembled=%02x case=%u\n",
      name,i,at+i,g_ram[at+i],reference_ram[at+i],cases);abort();
  }
}
#define SAME(v) same(&(v),sizeof(v),#v)
#define REGION(v,n) same((v),(n),#v)
void SmwTestSSReference(const char *path) {
  assert(SmwArcade_Active());load(path);
  for(unsigned z=0;z<7;z++) {
    seed(z);l1_l2_scroll_spr_current_state[1]=z;g_ram[0x1444]=0; /* Native title context. */
    run(0x009f2a,false);InitializeSaveData();REGION(ow_save_buffer,141);
  }
  /* Every cursor phase, both wrap directions and all relevant menu buttons. */
  const uint8_t buttons[]={0,4,8,0x20,0x40,0x80,0x10,0,0};
  const uint8_t extra[]={0,0,0,0,0,0,0,0x80,0x40};
  for(unsigned z=0;z<7;z++) for(unsigned f=0;f<32;f++) for(unsigned b=0;b<sizeof(buttons);b++) {
    seed(z*32+f);l1_l2_scroll_spr_current_state[1]=z;g_ram[0x1444]=0;misc_game_mode=8;
    misc_intro_level_flag=0xe9;counter_global_frames=f;
    io_controller_press1=buttons[b];io_controller_press2=extra[b];stripe_image_upload=0;
    run(0x009c00,false);FileSelectColorMath(0x7393,0x20);SmwArcade_ZoneSelect();
    SAME(palettes_background_color);SAME(mirror_color_math_select_and_enable);
    SAME(misc_game_mode);SAME(misc_intro_level_flag);SAME(io_sound_ch3);
    SAME(l1_l2_scroll_spr_current_state[1]);SAME(graphics_stripe_image_to_upload);
    SAME(stripe_image_upload);REGION(stripe_image_upload_data,204);REGION(ow_save_buffer,141);
  }
  for(unsigned player=0;player<2;player++) for(unsigned f=0;f<16;f++) {
    seed(f);player_current_characterx4=player*4;flag_show_save_prompt=f&1;
    run(0x048fd3,false);UpdateSaveBuffer();REGION(ow_save_buffer,141);
    SAME(flag_show_save_prompt);SAME(pointer_display_overworld_prompt);
  }
  for(unsigned castle=1;castle<=7;castle++) for(unsigned time=0;time<256;time++) for(unsigned held=0;held<3;held++) {
    seed(time);misc_currently_active_boss_end_cutscene=castle;
    l1_l2_scroll_spr_current_state[1]=time;ow_enter_level_flag=0;
    io_controller_hold1=held==1?0x80:0;io_controller_hold2=held==2?0x10:0;
    run(0x0cc94e,false);DisplayCastleDestructionText();
    SAME(l1_l2_scroll_spr_current_state[1]);SAME(ow_enter_level_flag);SAME(graphics_stripe_image_to_upload);
  }
  for(unsigned time=0;time<256;time++) for(unsigned blocked=0;blocked<3;blocked++) for(unsigned pressed=0;pressed<4;pressed++) {
    seed(time);misc_game_mode=25;l1_l2_scroll_spr_current_state[1]=blocked==1;
    HIBYTE(get_PointU16(l1_l2_scroll_spr_speed,0)[1].y)=blocked==2;
    ow_enter_level_flag=time;io_controller_press1=pressed==1?0x80:pressed==2?0x10:0;io_controller_press2=pressed==3?0x10:0;
    run(0x0cd002,false);CheckIfPlayerCanEndCastleDestructionCutscene();
    SAME(misc_game_mode);SAME(ow_enter_level_flag);SAME(l1_l2_scroll_spr_current_state[0]);
  }
  for(unsigned f=0;f<16;f++) {
    seed(f);run(0x0cd5f0,false);InitializeCastleDestructionTextTimers_DontIncrementCutsceneProcess();
    SAME(l1_l2_scroll_spr_current_state[1]);SAME(HIBYTE(get_PointU16(l1_l2_scroll_spr_speed,0)[1].y));SAME(io_music_ch1);
  }
  for(unsigned special=0;special<3;special++) for(unsigned frame=0;frame<8;frame++) for(unsigned time=0;time<5;time++) for(unsigned input=0;input<4;input++) {
    seed(frame);misc_game_mode=20;misc_intro_level_flag=special==1?0xe9:0;
    misc_color_of_palace_switch_pressed1=special==2;counter_global_frames=frame;
    flag_message_window_size_change_direction=0;timer_wait_before_message_window_size_change=80;
    timer_title_screen_input_timer=time;io_controller_hold1=input?0x80:0;
    io_controller_press1=input==1?0x80:0;io_controller_hold2=input==3?0x80:0;io_controller_press2=input==3?0x80:0;
    run(0x05b0ea,true);DisplayMessage();
    SAME(misc_game_mode);SAME(misc_intro_level_flag);SAME(timer_title_screen_input_timer);
    SAME(misc_exit_level_action);SAME(flag_got_midpoint);SAME(counter_enemy_rollcall_screen);
    SAME(flag_message_window_size_change_direction);REGION(ow_players_pos,8);
  }
  printf("PASS: %u native NSS routine cases match original 65816 instructions (seven zones, cursor, session buffer, message and cutscene timing)\n",cases);
}
