#include "smw_ps2.h"
#include "menu.h"
#include "frame_pacer.h"
#include "audio_queue.h"
#include "localization.h"
#include "super_system.h"
#include "save_icon.h"
#ifdef SMW_LOOP_TEST
#include "tests/platform_mock.h"
#else
#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <sifrpc.h>
#include <sifcmd.h>
#include <sifdma.h>
#include <iopcontrol.h>
#include <smod.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <audsrv.h>
#include <timer.h>
#include <libmc.h>
#endif
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>

#define MOD(n) extern unsigned char n##_irx[]; extern unsigned int size_##n##_irx
MOD(audsrv); MOD(iomanX); MOD(usbd); MOD(bdm); MOD(bdmfs_fatfs); MOD(usbmass_bd);
MOD(sio2man); MOD(padman); MOD(secrman); MOD(mcman); MOD(mcserv);
#ifdef SMW_EMBEDDED_ASSETS
extern unsigned char smw_embedded_assets[];
extern unsigned int size_smw_embedded_assets;
extern unsigned char smw_embedded_japanese[] __attribute__((weak));
extern unsigned int size_smw_embedded_japanese __attribute__((weak));
extern unsigned char smw_embedded_super_system[] __attribute__((weak));
extern unsigned int size_smw_embedded_super_system __attribute__((weak));
#endif
#define LOAD(n) load_module(#n,n##_irx,size_##n##_irx)
enum { MENU_WIDTH=640, MENU_HEIGHT=448 };
static GSGLOBAL *gs;
static GSTEXTURE texture;
static char padbuf[2][256] __attribute__((aligned(64)));
static uint32_t menu_pixels[MENU_WIDTH*MENU_HEIGHT] __attribute__((aligned(128)));
static uint32_t game_pixels[SMW_MAX_PITCH*SMW_HEIGHT] __attribute__((aligned(128)));
static int16_t audio[SMW_OUTPUT_MAX_SAMPLES*2] __attribute__((aligned(64)));
static SmwAudioQueue audio_queue __attribute__((aligned(64)));
static volatile uint32_t field_count;
static int field_sema=-1;
static bool audio_ready, game_ready;
static bool pad_connected[2];
static bool mx4sio_boot;
static const char *boot_error="IOP INITIALIZATION FAILED";
static bool memorycard_ready,memorycard_timed_out;
#ifdef SMW_EMBEDDED_ASSETS
static bool memorycard_started;
#endif
/* libmc retains these output pointers until its asynchronous RPC completes,
   including a completion that arrives after our launch deadline. */
static struct { int type,free_clusters,formatted,result; } card_probe;
static bool widescreen;
static bool japanese,japanese_ready,arcade_ready,save_icon_failed;
static char assets[256],sram[256],status[96];

static int load_module(const char *name,void *buf,unsigned size) {
#ifdef SMW_EMBEDDED_ASSETS
  /* OPL may restore storage services during an IOP reset. Reuse their actual
     IRX identities instead of stacking another SIO2/MC/BDM driver over them.
     IOMAN and iomanX share a name, so load the iomanX bridge explicitly. */
  static const struct { const char *file,*resident; } ids[]={
    {"sio2man","sio2man"},{"padman","padman"},
    {"secrman","secrman"},{"secrman","secrman_special"},
    {"mcman","mcman_cex"},{"mcserv","mcserv"},
    {"usbd","USB_driver"},{"bdm","bdm"},
    {"bdmfs_fatfs","bdmff"},{"usbmass_bd","usbmass_bd"}
  };
  for(unsigned i=0;i<sizeof(ids)/sizeof(*ids);i++) if(!strcmp(name,ids[i].file)) {
    smod_mod_info_t info;
    int resident=smod_get_mod_by_name(ids[i].resident,&info);
    if(resident>0) { printf("IRX %s: reuse id=%d\n",name,resident);return 1; }
  }
#endif
  int result=0; int id=SifExecModuleBuffer(buf,size,0,NULL,&result);
  printf("IRX %s: id=%d result=%d\n",name,id,result);
  return id>=0 && result>=0;
}
#ifdef SMW_EMBEDDED_ASSETS
static bool has_mx4sio_driver(void) {
  smod_mod_info_t info;
  return smod_get_mod_by_name("mx4sio",&info)>0 ||
    smod_get_mod_by_name("mx4sio_bd",&info)>0;
}

static bool request_clean_iop_reset(void) {
  /* Match the SDK reset command. The first DMA entry repeats its header
     without an interrupt. The second sends the complete command. OPL's
     reset hook inspects the first entry, so it won't reload its disc drivers
     after this reset. The ISO's executable and assets are already in RAM. */
  static union {
    SifCmdResetData_t command;
    unsigned char padded[128];
  } packet __attribute__((aligned(64)));
  void *destination=(void*)(uintptr_t)sceSifGetReg(SIF_SYSREG_SUBADDR);
  if(!destination) return false;

  packet.command.header.psize=sizeof(packet.command);
  packet.command.header.cid=SIF_CMD_RESET_CMD;
  SifDmaTransfer_t transfers[]={
    {&packet.command, destination, sizeof(SifCmdHeader_t), 0},
    {&packet.command, destination, sizeof(packet.command), SIF_DMA_ERT|SIF_DMA_INT_O}
  };
  sceSifStopDma();
  sceSifWriteBackDCache(&packet,sizeof(packet));
  sceSifSetReg(SIF_REG_SMFLAG,SIF_STAT_BOOTEND);
  if(!sceSifSetDma(transfers,2)) return false;

  /* Invalidate the SDK's cached bindings before it attaches to the new IOP. */
  extern int _iop_reboot_count;
  _iop_reboot_count++;
  sceSifSetReg(SIF_REG_SMFLAG,SIF_STAT_SIFINIT);
  sceSifSetReg(SIF_REG_SMFLAG,SIF_STAT_CMDINIT);
  sceSifSetReg(SIF_SYSREG_RPCINIT,0);
  sceSifSetReg(SIF_SYSREG_SUBADDR,0);
  return true;
}

static bool reset_iop(bool clean) {
  SifExitRpc();
  uint64_t start=GetTimerSystemTime();
  while(!(clean?request_clean_iop_reset():SifIopReset("",0))) {
    if(GetTimerSystemTime()-start>=(uint64_t)kBUSCLK*3) {
      boot_error="IOP RESET REQUEST TIMED OUT";
      return false;
    }
  }
  start=GetTimerSystemTime();
  while(!SifIopSync()) {
    if(GetTimerSystemTime()-start>=(uint64_t)kBUSCLK*3) {
      boot_error="IOP REBOOT TIMED OUT";
      return false;
    }
  }
  SifInitRpc(0);
  return true;
}
#endif

#ifdef SMW_EMBEDDED_ASSETS
static void start_memorycard(void) {
  if(memorycard_started) return;
  memorycard_started=true;
  memorycard_ready=LOAD(secrman) && LOAD(mcman) && LOAD(mcserv) && mcInit(MC_TYPE_MC)>=0;
}
#endif

static bool modules(void) {
  SifInitRpc(0);
#ifdef SMW_EMBEDDED_ASSETS
  mx4sio_boot=has_mx4sio_driver();
  if(!reset_iop(mx4sio_boot)) return false;
  /* Some loaders only expose their storage module after the first reset. */
  if(!mx4sio_boot && has_mx4sio_driver()) {
    mx4sio_boot=true;
    if(!reset_iop(true)) return false;
  }
  if(mx4sio_boot && has_mx4sio_driver()) {
    boot_error="MX4SIO DRIVER STILL RESIDENT AFTER RESET";
    return false;
  }
  sbv_patch_enable_lmb();
  if(!LOAD(iomanX) || !LOAD(sio2man) || !LOAD(padman)) return false;
#else
  /* File-backed development builds still need the loader's host: transport. */
  sbv_patch_enable_lmb();
  SifLoadModule("rom0:SIO2MAN",0,NULL);
  SifLoadModule("rom0:PADMAN",0,NULL);
  LOAD(iomanX);
#endif
  padInit(0); padPortOpen(0,0,padbuf[0]); padPortOpen(1,0,padbuf[1]);
  /* Bind once at startup; no memory-card polling in the gameplay loop. */
#ifdef SMW_EMBEDDED_ASSETS
  /* MX4SIO starts card services on the first Original launch, after the
     clean reset. Keep the idle launcher and arcade boot free of card I/O. */
  if(!mx4sio_boot) {
    LOAD(usbd); LOAD(bdm); LOAD(bdmfs_fatfs); LOAD(usbmass_bd);
    start_memorycard();
  }
#else
  LOAD(usbd); LOAD(bdm); LOAD(bdmfs_fatfs); LOAD(usbmass_bd);
  SifLoadModule("rom0:MCMAN",0,NULL);
  SifLoadModule("rom0:MCSERV",0,NULL);
  memorycard_ready=mcInit(MC_TYPE_MC)>=0;
#endif
  SifLoadModule("rom0:LIBSD",0,NULL);
  LOAD(audsrv);
  if(audsrv_init()==0) {
    struct audsrv_fmt_t fmt={.freq=SMW_OUTPUT_RATE,.bits=16,.channels=2};
    audio_ready=audsrv_set_format(&fmt)==0;
    audsrv_set_volume(100);
  }
  return true;
}
static int on_vblank(int cause) {
  (void)cause; field_count++; iSignalSema(field_sema);
  ExitHandler(); return 0;
}
static uint32_t current_field(void) {
  if(field_sema>=0) return field_count;
  const uint64_t period=(uint64_t)kBUSCLK*1001u;
  uint64_t ticks=GetTimerSystemTime();
  return (uint32_t)((ticks/period)*60000u+(ticks%period)*60000u/period);
}
static void wait_field(uint32_t target) {
  while((int32_t)(current_field()-target)<0) {
    if(field_sema>=0) WaitSema(field_sema);
    else gsKit_vsync_wait();
  }
}
static void flip_at_field(uint32_t target) {
  wait_field(target);
  /* gsKit_sync_flip's buffer sequence, after an absolute VBlank wait. */
  gsKit_display_buffer(gs); gs->ActiveBuffer^=1; gsKit_setactive(gs);
}
static bool video(void) {
  dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
  dmaKit_chan_init(DMA_CHANNEL_GIF);
  gs=gsKit_init_global(); gs->Mode=GS_MODE_NTSC;
  gs->Interlace=GS_INTERLACED; gs->Field=GS_FIELD;
  gs->Width=MENU_WIDTH; gs->Height=MENU_HEIGHT; gs->PSM=GS_PSM_CT32;
  gs->DoubleBuffering=GS_SETTING_ON; gs->ZBuffering=GS_SETTING_OFF;
  gs->PrimAlphaEnable=GS_SETTING_OFF;
  gsKit_init_screen(gs); gsKit_set_test(gs,GS_ZTEST_OFF);
  gsKit_mode_switch(gs,GS_ONESHOT);
  memset(&texture,0,sizeof(texture));
  texture.PSM=GS_PSM_CT32;
  texture.Filter=GS_FILTER_NEAREST;
  /* The launcher is larger than the game texture. Both use this same VRAM
     region, so neither aspect changes nor the handoff allocate GS memory. */
  texture.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(MENU_WIDTH,MENU_HEIGHT,GS_PSM_CT32),GSKIT_ALLOC_USERBUFFER);
  if(texture.Vram==GSKIT_ALLOC_ERROR) return false;
  ee_sema_t sema={.init_count=0,.max_count=1,.option=0};
  field_sema=CreateSema(&sema);
  if(field_sema>=0 && gsKit_add_vsync_handler(on_vblank)<0) {
    DeleteSema(field_sema); field_sema=-1;
  }
  printf("SMWPS2 V022 VBLANK: %s\n",field_sema>=0?"INTERRUPT":"TIMER FALLBACK");
  return true;
}
static void rectangle(void *ctx,int x,int y,int w,int h,uint32_t rgb) {
  (void)ctx;
  gsKit_prim_sprite(gs,x,y,x+w,y+h,1,GS_SETREG_RGBAQ(rgb>>16&255,rgb>>8&255,rgb&255,0x80,0));
}
static void show_boot_error(void) {
  if(!video()) return;
  rectangle(NULL,0,0,MENU_WIDTH,MENU_HEIGHT,0x101e29);
  menu_text(rectangle,NULL,32,170,2,0xfffbed,"SMW PS2 / MX4SIO BOOT");
  menu_text(rectangle,NULL,32,210,2,0xffd747,boot_error);
  gsKit_queue_exec(gs);
  gsKit_finish();
  flip_at_field(current_field()+1);
  SleepThread();
}
static void menu_rectangle(void *ctx,int x,int y,int w,int h,uint32_t rgb) {
  int right=x+w,bottom=y+h;
  /* On an anamorphic 16:9 display, retain the launcher's original proportions. */
  if(widescreen) { x=80+x*3/4;right=80+right*3/4; }
  if(x<0) x=0;
  if(y<0) y=0;
  if(right>MENU_WIDTH) right=MENU_WIDTH;
  if(bottom>MENU_HEIGHT) bottom=MENU_HEIGHT;
  if(right<=x || bottom<=y) return;
  uint32_t rgba=0x80000000u|(rgb&0xff00)|((rgb&0xff)<<16)|((rgb>>16)&0xff);
  uint32_t *pixels=ctx;
  for(int row=y;row<bottom;row++) {
    uint32_t *line=pixels+row*MENU_WIDTH;
    for(int col=x;col<right;col++) line[col]=rgba;
  }
}
static void draw_menu(const Menu *menu) {
  if(widescreen) {
    for(size_t i=0;i<sizeof(menu_pixels)/sizeof(*menu_pixels);i++)
      menu_pixels[i]=0x80291e10;
  }
  menu_draw(menu);
  texture.Width=MENU_WIDTH;
  texture.Height=MENU_HEIGHT;
  texture.Mem=(u32*)menu_pixels;
  gsKit_texture_upload(gs,&texture);
  gsKit_prim_sprite_texture(gs,&texture,0,0,0,0,MENU_WIDTH,MENU_HEIGHT,
    MENU_WIDTH,MENU_HEIGHT,1,GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0));
}
static unsigned poll_pad(int port) {
  struct padButtonStatus b={0};
  int state=padGetState(port,0);
  if(state==PAD_STATE_DISCONN) pad_connected[port]=false;
  if(state!=PAD_STATE_STABLE && state!=PAD_STATE_FINDCTP1) return 0;
  /* A busy port or incomplete DMA packet carries no buttons, but does not
     move player one to the other controller during a game transition. */
  if(padRead(port,0,&b)<4 || b.ok!=0) return 0;
  pad_connected[port]=true;
  return (~b.btns)&0xffff;
}
static uint16_t native_pad(unsigned p) {
  uint16_t x=0;
  if(p&PAD_CROSS) x|=SMW_B;
  if(p&PAD_SQUARE) x|=SMW_Y;
  if(p&PAD_CIRCLE) x|=SMW_A;
  if(p&PAD_TRIANGLE) x|=SMW_X;
  if(p&PAD_START) x|=SMW_START;
  if(p&PAD_SELECT) x|=SMW_SELECT;
  if(p&PAD_UP) x|=SMW_UP;
  if(p&PAD_DOWN) x|=SMW_DOWN;
  if(p&PAD_LEFT) x|=SMW_LEFT;
  if(p&PAD_RIGHT) x|=SMW_RIGHT;
  if(p&PAD_L1) x|=SMW_L;
  if(p&PAD_R1) x|=SMW_R;
  return x;
}
static bool locate_assets(int argc,char **argv) {
  char base[192]="";
  if(argc>0 && argv[0]) {
    const char *slash=strrchr(argv[0],'/');
    const char *colon=strrchr(argv[0],':');
    const char *end=slash?slash:colon;
    if(end && (size_t)(end-argv[0]+1)<sizeof(base)) {
      size_t n=end-argv[0]+1; memcpy(base,argv[0],n); base[n]=0;
    }
  }
  const char *roots[]={base,"mass:/SMWPS2/","mass0:/SMWPS2/","host:",""};
  for(unsigned i=0;i<sizeof(roots)/sizeof(*roots);i++) {
    snprintf(assets,sizeof(assets),"%ssmw_assets.dat",roots[i]);
    FILE *f=fopen(assets,"rb");
    if(f) { fclose(f); snprintf(sram,sizeof(sram),"%ssmw.srm",roots[i]); return true; }
  }
  return false;
}
#ifndef SMW_EMBEDDED_ASSETS
static bool optional_pack(const char *name,bool (*load)(const void*,size_t)) {
  char path[256];const char *slash=strrchr(assets,'/'),*colon=strrchr(assets,':');
  const char *end=slash?slash:colon;size_t root=end?(size_t)(end-assets+1):0;
  if(root+strlen(name)>=sizeof(path)) return false;
  memcpy(path,assets,root);strcpy(path+root,name);
  FILE *f=fopen(path,"rb");if(!f) return false;
  if(fseek(f,0,SEEK_END)) { fclose(f);return false; }
  long size=ftell(f);rewind(f);
  if(size<12 || size>512*1024) { fclose(f);return false; }
  void *data=malloc(size);bool ok=data && fread(data,1,size,f)==(size_t)size;
  fclose(f);if(ok) ok=load(data,size);free(data);return ok;
}
#endif
#ifdef SMW_EMBEDDED_ASSETS
static void choose_embedded_save(void) {
  sram[0]=0;
  save_icon_failed=false;
  if(memorycard_timed_out) return;
  start_memorycard();
  if(memorycard_ready) for(int port=0;port<2;port++) {
    memset(&card_probe,0,sizeof(card_probe));card_probe.result=-99;
    if(mcGetInfo(port,0,&card_probe.type,&card_probe.free_clusters,&card_probe.formatted)<0) continue;
    uint64_t start=GetTimerSystemTime();
    for(;;) {
      int sync=mcSync(MC_NOWAIT,NULL,&card_probe.result);
      if(sync==1) break;
      if(sync<0 || GetTimerSystemTime()-start>=(uint64_t)kBUSCLK*2) {
        memorycard_ready=false;memorycard_timed_out=true;
        printf("Memory-card probe did not complete; session-only mode\n");
        /* An RPC may still be pending. Do not issue another card request or
           filesystem probe through that same potentially blocked IOP. */
        return;
      }
      gsKit_vsync_wait();
    }
    if(card_probe.type!=MC_TYPE_PS2 || card_probe.result<-1 || card_probe.formatted!=MC_FORMATTED) continue;
    char dir[64]; snprintf(dir,sizeof(dir),"mc%d:/SMWPS2",port);
    mkdir(dir,0777);
    save_icon_failed=!SmwSaveIcon_Ensure(dir);
    snprintf(sram,sizeof(sram),"%s/smw.srm",dir);
    return;
  }
  /* A clean MX4SIO boot has no mounted disc or mass drivers. */
  if(mx4sio_boot) return;
  /* Mounted mass: storage is optional; an ISO can play without writable media. */
  const char *roots[]={"mass:/SMWPS2","mass0:/SMWPS2"};
  for(unsigned i=0;i<2;i++) {
    mkdir(roots[i],0777);
    char probe[256]; snprintf(probe,sizeof(probe),"%s/write.tmp",roots[i]);
    FILE *f=fopen(probe,"wb");
    if(f) { fclose(f); remove(probe); snprintf(sram,sizeof(sram),"%s/smw.srm",roots[i]); return; }
  }
}
#endif
static bool launch_game(int argc,char **argv) {
#ifdef SMW_EMBEDDED_ASSETS
  (void)argc; (void)argv;
  if(SmwArcade_Active()) sram[0]=0;else choose_embedded_save();
  if(!smw_init_memory(smw_embedded_assets,size_smw_embedded_assets,sram)) return false;
  const char *message="GAME READY";
  if(SmwArcade_Active()) message="SUPER SYSTEM / FREE PLAY / SESSION ONLY";
  else if(memorycard_timed_out) message="MEMORY CARD TIMED OUT - SESSION ONLY";
  else if(!sram[0]) message="NO WRITABLE SAVE DEVICE - SESSION ONLY";
  else if(save_icon_failed) message="SAVE ICON REPAIR FAILED - CHECK CARD SPACE";
  else if(mx4sio_boot) message="MX4SIO / MEMORY CARD SAVE ENABLED";
  snprintf(status,sizeof(status),"%s",message);
  return true;
#else
  return locate_assets(argc,argv) && smw_init(assets,sram);
#endif
}
static void present_game(void) {
  /* The PPU writes native GS RGBA directly. No second framebuffer conversion. */
  texture.Width=smw_video_pitch();
  texture.Height=SMW_HEIGHT;
  texture.Mem=(u32*)game_pixels;
  gsKit_texture_upload(gs,&texture);
  /* Wide mode is anamorphic: set the TV/emulator presentation to 16:9. */
  gsKit_prim_sprite_texture(gs,&texture,0,0,0,0,640,448,smw_video_width(),224,1,GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0));
}
int main(int argc,char **argv) {
  if(!modules()) {
    printf("%s\n",boot_error);
    if(mx4sio_boot) show_boot_error();
    return 1;
  }
  if(!video()) { printf("Cannot allocate GS texture\n");return 1; }
#ifdef SMW_EMBEDDED_ASSETS
  japanese_ready=smw_embedded_japanese && &size_smw_embedded_japanese &&
    SmwLocale_Load(smw_embedded_japanese,size_smw_embedded_japanese);
  arcade_ready=smw_embedded_super_system && &size_smw_embedded_super_system &&
    SmwArcade_Load(smw_embedded_super_system,size_smw_embedded_super_system);
#else
  if(locate_assets(argc,argv)) {
    japanese_ready=optional_pack("smw_japanese.dat",SmwLocale_Load);
    arcade_ready=optional_pack("smw_super_system.dat",SmwArcade_Load);
  }
#endif
  smw_audio_queue_reset(&audio_queue);
  if(mx4sio_boot) snprintf(status,sizeof(status),"MX4SIO ISO / CLEAN IOP");
  Menu menu={.rect=menu_rectangle,.ctx=menu_pixels};
  bool in_menu=true,menu_dirty=true,show_perf=false;
  unsigned previous=0;
  char perf_label[96]="V022 - MEASURING PERFORMANCE";
  char detail_label[96]="30 FPS TARGET - L2 + R2 TO HIDE";
  unsigned present_count=0,tick_count=0;
  uint64_t sample_start=GetTimerSystemTime();
  SmwFramePacer pacer={0};
  uint64_t ppu_time=0,gpu_time=0,cpu_time=0,apu_time=0,io_time=0,wait_time=0;
  unsigned pad1=0,pad2=0;
  for(;;) {
    pad1=poll_pad(0); pad2=poll_pad(1); unsigned pressed=pad1&~previous; previous=pad1;
    if((pad1&(PAD_L2|PAD_R2))==(PAD_L2|PAD_R2) && !(pad1&PAD_START) && (pressed&(PAD_L2|PAD_R2))) show_perf=!show_perf;
    if((pad1&(PAD_START|PAD_L2|PAD_R2))==(PAD_START|PAD_L2|PAD_R2) && !in_menu) {
      in_menu=true;menu_dirty=true;menu.selection=SmwArcade_Active()?1:0;
      if(audio_ready) audsrv_stop_audio();
      if(!smw_save()) snprintf(status,sizeof(status),"SAVE FAILED - CHECK WRITABLE MEDIA");
      pressed=0; /* Returning with Start must not also activate the card. */
    }
    if(in_menu) {
      if(pressed) menu_dirty=true;
      if(pressed&PAD_TRIANGLE) {
        if(menu.selection) {
          snprintf(status,sizeof(status),"SUPER SYSTEM USES ITS ORIGINAL ENGLISH TEXT");
        } else if(japanese_ready) {
          japanese=!japanese;
          snprintf(status,sizeof(status),"%s",game_ready && !SmwArcade_Active() && japanese!=SmwLocale_Japanese()?"CROSS RESTARTS AT TITLE / CIRCLE RESUMES CURRENT GAME":japanese?"ORIGINAL SUPER FAMICOM JAPANESE":"ORIGINAL SNES ENGLISH");
        } else snprintf(status,sizeof(status),"ADD JAPANESE ROM IN THE BUILD TOOL");
      }
      if(pressed&PAD_SQUARE) {
        widescreen=!widescreen;
        snprintf(status,sizeof(status),"%s",widescreen?"SET TV OR PCSX2 ASPECT TO 16:9":"SET TV OR PCSX2 ASPECT TO 4:3");
      }
      if(pressed&(PAD_LEFT|PAD_RIGHT|PAD_UP|PAD_DOWN)) {
        menu.selection^=1;
        snprintf(status,sizeof(status),"%s",game_ready && menu.selection!=(int)SmwArcade_Active()?"SWITCHING EDITIONS STARTS A NEW SESSION":menu.selection?"SEVEN START ZONES / NO MEMORY CARD NEEDED":"ORIGINAL SNES / THREE SHARED SAVE SLOTS");
      }
      if((pressed&PAD_CIRCLE) && game_ready) in_menu=false;
      if(pressed&(PAD_CROSS|PAD_START)) {
        bool want_arcade=menu.selection==1;
        if(want_arcade && !arcade_ready) {
          snprintf(status,sizeof(status),"ADD SUPER SYSTEM ROM IN THE BUILD TOOL");
        } else if(game_ready && want_arcade==SmwArcade_Active() &&
            (want_arcade || japanese==SmwLocale_Japanese())) {
          in_menu=false;
        } else {
          if(game_ready && !smw_save()) {
            snprintf(status,sizeof(status),"SAVE FAILED - EDITION CHANGE CANCELLED");continue;
          }
          if(audio_ready) audsrv_stop_audio();
          smw_audio_queue_reset(&audio_queue);
          game_ready=false;
          bool selected=SmwArcade_Select(want_arcade) && (want_arcade || SmwLocale_Select(japanese));
          if(selected && launch_game(argc,argv)) { game_ready=true;in_menu=false; }
          else snprintf(status,sizeof(status),"GAME LOAD FAILED - CHECK ASSETS");
        }
      }
      menu.running=game_ready; menu.widescreen=widescreen; menu.japanese=japanese;
      menu.restart_language=game_ready && !SmwArcade_Active() && japanese!=SmwLocale_Japanese();
      menu.running_variant=SmwArcade_Active(); menu.arcade_ready=arcade_ready; menu.status=status[0]?status:NULL;
      if(menu_dirty) {
        draw_menu(&menu);
        menu_dirty=false;
        gsKit_queue_exec(gs);
        gsKit_finish();
        flip_at_field(current_field()+1);
      } else {
        /* Scanout retains the menu. Idle input polling needs no GIF DMA. */
        wait_field(current_field()+1);
      }
      if(!in_menu) {
        smw_set_widescreen(widescreen);
        smw_pacer_reset(&pacer,current_field());
        sample_start=GetTimerSystemTime(); present_count=tick_count=0;
        ppu_time=gpu_time=cpu_time=apu_time=io_time=wait_time=0;
      }
      continue;
    }
    unsigned ticks;
    while(!(ticks=smw_pacer_ticks(&pacer,current_field()))) wait_field(current_field()+1);
    smw_set_connected_pads((pad_connected[0]?1:0)|(pad_connected[1]?2:0));
    for(unsigned i=0;i<ticks;i++) {
      uint64_t before_tick=GetTimerSystemTime();
      smw_step(native_pad(pad1),native_pad(pad2));
      uint64_t after_step=GetTimerSystemTime();
      unsigned samples=smw_audio_queue_samples(&audio_queue);
      smw_audio(audio,samples);
      uint64_t after_audio=GetTimerSystemTime();
      if(audio_ready) smw_audio_queue_push(&audio_queue,audio,samples);
      cpu_time+=after_step-before_tick; apu_time+=after_audio-after_step;
      tick_count++;
      if(i+1<ticks) smw_skip_draw();
    }
    if(audio_ready) {
      uint64_t before=GetTimerSystemTime();
      unsigned count=smw_audio_queue_offer(&audio_queue,audsrv_available());
      if(count) smw_audio_queue_sent(&audio_queue,
        audsrv_play_audio((const char*)audio_queue.data,count*4));
      io_time+=GetTimerSystemTime()-before;
    }
    {
      uint64_t before=GetTimerSystemTime();
      smw_draw(game_pixels);
      uint64_t after_ppu=GetTimerSystemTime();
      present_game();
      if(show_perf) {
        rectangle(NULL,0,400,640,48,0x101e29);
        menu_text(rectangle,NULL,9,405,2,0x96d6ad,perf_label);
        menu_text(rectangle,NULL,9,427,2,0x96d6ad,detail_label);
      }
      gsKit_queue_exec(gs); gsKit_finish();
      uint64_t after_gpu=GetTimerSystemTime();
      ppu_time+=after_ppu-before; gpu_time+=after_gpu-after_ppu;
      present_count++;
    }
    uint64_t before_wait=GetTimerSystemTime();
    flip_at_field(smw_pacer_deadline(&pacer,current_field()));
    smw_pacer_presented(&pacer,current_field());
    uint64_t now=GetTimerSystemTime(); wait_time+=now-before_wait;
    if(now-sample_start>=(uint64_t)kBUSCLK*5) {
      unsigned render_rate=(unsigned)((uint64_t)present_count*10*kBUSCLK/(now-sample_start));
      unsigned sim_rate=(unsigned)((uint64_t)tick_count*10*kBUSCLK/(now-sample_start));
      unsigned ppu_ms=present_count?(unsigned)(ppu_time*10000/((uint64_t)kBUSCLK*present_count)):0;
      unsigned gpu_ms=present_count?(unsigned)(gpu_time*10000/((uint64_t)kBUSCLK*present_count)):0;
      unsigned apu_ms=tick_count?(unsigned)(apu_time*10000/((uint64_t)kBUSCLK*tick_count)):0;
      unsigned cpu_ms=tick_count?(unsigned)(cpu_time*10000/((uint64_t)kBUSCLK*tick_count)):0;
      unsigned io_ms=present_count?(unsigned)(io_time*10000/((uint64_t)kBUSCLK*present_count)):0;
      unsigned wait_ms=present_count?(unsigned)(wait_time*10000/((uint64_t)kBUSCLK*present_count)):0;
      unsigned ticks_per_draw=present_count?tick_count*10/present_count:0;
      snprintf(perf_label,sizeof(perf_label),"DRAW %u.%u SIM %u.%u PPU %u.%u GPU %u.%u",
        render_rate/10,render_rate%10,sim_rate/10,sim_rate%10,ppu_ms/10,ppu_ms%10,gpu_ms/10,gpu_ms%10);
      snprintf(detail_label,sizeof(detail_label),"V022 CPU %u.%u APU %u.%u IO %u.%u Q %u T %u.%u %s",
        cpu_ms/10,cpu_ms%10,apu_ms/10,apu_ms%10,io_ms/10,io_ms%10,audio_queue.queue_ms,
        ticks_per_draw/10,ticks_per_draw%10,audio_ready?"ON":"OFF");
      if(show_perf) printf("SMWPS2 V022 PERF: %s / %s WAIT %u.%u MISS %u LOST %u DROP %u ERR %u mode=%u\n",
        perf_label,detail_label,wait_ms/10,wait_ms%10,pacer.missed_presents,pacer.lost_ticks,
        audio_queue.dropped,audio_queue.errors,smw_game_mode());
      present_count=tick_count=0; ppu_time=gpu_time=cpu_time=apu_time=io_time=wait_time=0; sample_start=now;
    }
  }
}
