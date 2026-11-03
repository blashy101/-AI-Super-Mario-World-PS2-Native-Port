#include "smw_ps2.h"
#include "menu.h"
#ifdef SMW_LOOP_TEST
#include "tests/platform_mock.h"
#else
#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <sifrpc.h>
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
#ifdef SMW_EMBEDDED_ASSETS
extern unsigned char smw_embedded_assets[];
extern unsigned int size_smw_embedded_assets;
#endif
#define LOAD(n) load_module(#n,n##_irx,size_##n##_irx)
static GSGLOBAL *gs;
static GSTEXTURE texture;
static char padbuf[2][256] __attribute__((aligned(64)));
static uint32_t game_pixels[256*224] __attribute__((aligned(128)));
static int16_t audio[534*2] __attribute__((aligned(64)));
static bool audio_ready, game_ready;
static bool pad_connected[2];
static bool memorycard_ready;
static char assets[256],sram[256],status[96];

static int load_module(const char *name,void *buf,unsigned size) {
  int result=0; int id=SifExecModuleBuffer(buf,size,0,NULL,&result);
  printf("IRX %s: id=%d result=%d\n",name,id,result);
  return id>=0 && result>=0;
}
static void modules(void) {
  SifInitRpc(0); sbv_patch_enable_lmb();
  /* Keep the loader's IOP and host: transport alive. No IOP reset. */
  SifLoadModule("rom0:SIO2MAN",0,NULL);
  SifLoadModule("rom0:PADMAN",0,NULL);
  padInit(0); padPortOpen(0,0,padbuf[0]); padPortOpen(1,0,padbuf[1]);
  /* Existing modules may report already resident; mass: may still work. */
  LOAD(iomanX); LOAD(usbd); LOAD(bdm); LOAD(bdmfs_fatfs); LOAD(usbmass_bd);
  /* Bind once at startup; no memory-card polling in the gameplay loop. */
  SifLoadModule("rom0:MCMAN",0,NULL);
  SifLoadModule("rom0:MCSERV",0,NULL);
  memorycard_ready=mcInit(MC_TYPE_MC)>=0;
  SifLoadModule("rom0:LIBSD",0,NULL);
  LOAD(audsrv);
  if(audsrv_init()==0) {
    struct audsrv_fmt_t fmt={.freq=SMW_AUDIO_RATE,.bits=16,.channels=2};
    audio_ready=audsrv_set_format(&fmt)==0;
    audsrv_set_volume(100);
  }
}
static void video(void) {
  dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
  dmaKit_chan_init(DMA_CHANNEL_GIF);
  gs=gsKit_init_global(); gs->Mode=GS_MODE_NTSC;
  gs->Interlace=GS_INTERLACED; gs->Field=GS_FIELD;
  gs->Width=640; gs->Height=448; gs->PSM=GS_PSM_CT32;
  gs->DoubleBuffering=GS_SETTING_ON; gs->ZBuffering=GS_SETTING_OFF;
  gs->PrimAlphaEnable=GS_SETTING_OFF;
  gsKit_init_screen(gs); gsKit_set_test(gs,GS_ZTEST_OFF);
  gsKit_mode_switch(gs,GS_ONESHOT);
  memset(&texture,0,sizeof(texture));
  texture.Width=256; texture.Height=224; texture.PSM=GS_PSM_CT32;
  texture.Filter=GS_FILTER_NEAREST; texture.Mem=(u32*)game_pixels;
  texture.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(256,224,GS_PSM_CT32),GSKIT_ALLOC_USERBUFFER);
}
static void rectangle(void *ctx,int x,int y,int w,int h,uint32_t rgb) {
  (void)ctx;
  gsKit_prim_sprite(gs,x,y,x+w,y+h,1,GS_SETREG_RGBAQ(rgb>>16&255,rgb>>8&255,rgb&255,0x80,0));
}
static unsigned poll_pad(int port) {
  struct padButtonStatus b;
  int state=padGetState(port,0);
  pad_connected[port]=false;
  if(state!=PAD_STATE_STABLE && state!=PAD_STATE_FINDCTP1) return 0;
  if(!padRead(port,0,&b)) return 0;
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
static void choose_embedded_save(void) {
  sram[0]=0;
  if(memorycard_ready) for(int port=0;port<2;port++) {
    int type=0,free_clusters=0,formatted=0,result=-99;
    if(mcGetInfo(port,0,&type,&free_clusters,&formatted)<0) continue;
    mcSync(0,NULL,&result);
    if(type!=MC_TYPE_PS2 || result<-1) continue;
    char dir[64]; snprintf(dir,sizeof(dir),"mc%d:/SMWPS2",port);
    mkdir(dir,0777);
    snprintf(sram,sizeof(sram),"%s/smw.srm",dir);
    return;
  }
  /* USB is optional. An ISO still boots and plays with no writable device. */
  const char *roots[]={"mass:/SMWPS2","mass0:/SMWPS2"};
  for(unsigned i=0;i<2;i++) {
    mkdir(roots[i],0777);
    char probe[256]; snprintf(probe,sizeof(probe),"%s/write.tmp",roots[i]);
    FILE *f=fopen(probe,"wb");
    if(f) { fclose(f); remove(probe); snprintf(sram,sizeof(sram),"%s/smw.srm",roots[i]); return; }
  }
}
static bool launch_game(int argc,char **argv) {
#ifdef SMW_EMBEDDED_ASSETS
  (void)argc; (void)argv;
  choose_embedded_save();
  bool result=smw_init_memory(smw_embedded_assets,size_smw_embedded_assets,sram);
  if(result) snprintf(status,sizeof(status),"%s",sram[0]?"GAME READY":"NO WRITABLE SAVE DEVICE - SESSION ONLY");
  return result;
#else
  return locate_assets(argc,argv) && smw_init(assets,sram);
#endif
}
static void present_game(void) {
  /* The PPU writes native GS RGBA directly. No second framebuffer conversion. */
  gsKit_texture_upload(gs,&texture);
  /* 640x448 NTSC framebuffer maps to 4:3 with the TV set to 4:3. */
  gsKit_prim_sprite_texture(gs,&texture,0,0,0,0,640,448,256,224,1,GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0));
}
int main(int argc,char **argv) {
  modules(); video();
  Menu menu={.rect=rectangle}; bool in_menu=true,show_perf=true; unsigned previous=0;
  char perf_label[96]="V003 - MEASURING PERFORMANCE - L2 + R2 TO HIDE";
  unsigned present_count=0,tick_count=0,audio_fraction=0;
  uint64_t sample_start=GetTimerSystemTime();
  uint64_t next_tick=sample_start;
  /* Catch up 60 Hz simulation, render once, then flip at the next refresh.
     A second wall-clock gate can accidentally discard an entire refresh when
     it expires just after simulation. Let actual draw cost determine cadence. */
  const uint64_t tick=kBUSCLK/60;
  uint64_t ppu_time=0,gpu_time=0,cpu_time=0,apu_time=0,io_time=0;
  unsigned pad1=0,pad2=0;
  for(;;) {
    pad1=poll_pad(0); pad2=poll_pad(1); unsigned pressed=pad1&~previous; previous=pad1;
    if((pad1&(PAD_L2|PAD_R2))==(PAD_L2|PAD_R2) && !(pad1&PAD_START) && (pressed&(PAD_L2|PAD_R2))) show_perf=!show_perf;
    if((pad1&(PAD_START|PAD_L2|PAD_R2))==(PAD_START|PAD_L2|PAD_R2) && !in_menu) {
      in_menu=true; if(audio_ready) audsrv_stop_audio();
      if(!smw_save()) snprintf(status,sizeof(status),"SAVE FAILED - CHECK WRITABLE MEDIA");
      next_tick=GetTimerSystemTime();
    }
    if(in_menu) {
      if(pressed&(PAD_LEFT|PAD_RIGHT|PAD_UP|PAD_DOWN)) menu.selection^=1;
      if((pressed&PAD_CIRCLE) && game_ready) { in_menu=false; next_tick=GetTimerSystemTime(); }
      if(pressed&(PAD_CROSS|PAD_START)) {
        if(menu.selection==1) snprintf(status,sizeof(status),"SUPER SYSTEM ENGINE IS NOT PORTED YET");
        else if(game_ready) { in_menu=false; next_tick=GetTimerSystemTime(); }
        else if(launch_game(argc,argv)) {
          game_ready=true; in_menu=false; next_tick=GetTimerSystemTime();
        } else snprintf(status,sizeof(status),"GAME LOAD FAILED - CHECK ASSETS");
      }
      if(!in_menu) { sample_start=next_tick; present_count=tick_count=0; ppu_time=gpu_time=cpu_time=apu_time=io_time=0; }
      menu.running=game_ready; menu.status=status[0]?status:NULL; menu_draw(&menu);
      gsKit_queue_exec(gs); gsKit_finish(); gsKit_sync_flip(gs);
      continue;
    }
    uint64_t now=GetTimerSystemTime(); unsigned ticks=0;
    smw_set_connected_pads((pad_connected[0]?1:0)|(pad_connected[1]?2:0));
    do {
      uint64_t before_tick=GetTimerSystemTime();
      smw_step(native_pad(pad1),native_pad(pad2));
      uint64_t after_step=GetTimerSystemTime();
      audio_fraction+=SMW_AUDIO_RATE;
      int samples=audio_fraction/60; audio_fraction%=60;
      smw_audio(audio,samples);
      uint64_t after_audio=GetTimerSystemTime();
      if(audio_ready) { audsrv_wait_audio(samples*4); audsrv_play_audio((char*)audio,samples*4); }
      uint64_t after_io=GetTimerSystemTime();
      cpu_time+=after_step-before_tick; apu_time+=after_audio-after_step; io_time+=after_io-after_audio;
      next_tick+=tick; tick_count++; ticks++;
      now=GetTimerSystemTime();
      if(now>next_tick && ticks<4) smw_skip_draw();
    } while(now>next_tick && ticks<4);
    /* Bound backlog, report overload in the measured simulation rate. */
    if(now>next_tick+tick*4) next_tick=now;
    {
      uint64_t before=GetTimerSystemTime();
      smw_draw(game_pixels);
      uint64_t after_ppu=GetTimerSystemTime();
      present_game();
      if(show_perf) {
        rectangle(NULL,0,424,640,24,0x101e29);
        menu_text(rectangle,NULL,9,428,2,0x96d6ad,perf_label);
      }
      gsKit_queue_exec(gs); gsKit_finish();
      uint64_t after_gpu=GetTimerSystemTime();
      ppu_time+=after_ppu-before; gpu_time+=after_gpu-after_ppu;
      present_count++;
    }
    gsKit_sync_flip(gs);
    now=GetTimerSystemTime();
    if(now-sample_start>=(uint64_t)kBUSCLK*5) {
      unsigned render_rate=(unsigned)((uint64_t)present_count*10*kBUSCLK/(now-sample_start));
      unsigned sim_rate=(unsigned)((uint64_t)tick_count*10*kBUSCLK/(now-sample_start));
      unsigned ppu_ms=present_count?(unsigned)(ppu_time*10000/((uint64_t)kBUSCLK*present_count)):0;
      unsigned gpu_ms=present_count?(unsigned)(gpu_time*10000/((uint64_t)kBUSCLK*present_count)):0;
      unsigned apu_ms=tick_count?(unsigned)(apu_time*10000/((uint64_t)kBUSCLK*tick_count)):0;
      unsigned cpu_ms=tick_count?(unsigned)(cpu_time*10000/((uint64_t)kBUSCLK*tick_count)):0;
      unsigned io_ms=tick_count?(unsigned)(io_time*10000/((uint64_t)kBUSCLK*tick_count)):0;
      snprintf(perf_label,sizeof(perf_label),"DRAW %u.%u SIM %u.%u PPU %u.%u GPU %u.%u APU %u.%u AUD %s",
        render_rate/10,render_rate%10,sim_rate/10,sim_rate%10,ppu_ms/10,ppu_ms%10,gpu_ms/10,gpu_ms%10,
        apu_ms/10,apu_ms%10,audio_ready?"ON":"OFF");
      printf("SMWPS2 V003 PERF: %s CPU %u.%u IO %u.%u mode=%u\n",perf_label,
        cpu_ms/10,cpu_ms%10,io_ms/10,io_ms%10,smw_game_mode());
      present_count=tick_count=0; ppu_time=gpu_time=cpu_time=apu_time=io_time=0; sample_start=now;
    }
  }
}
