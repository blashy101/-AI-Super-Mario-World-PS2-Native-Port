/* Discrete-event model of VBlank, SDK audsrv ring consumption and EE work.
   This is a scheduling regression test, not a PS2 emulator or fps benchmark. */
#include "platform_mock.h"
#include "smw_ps2.h"
#include "menu.h"
#include <assert.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static jmp_buf finished;
static uint64_t now,end_time,next_field,next_spu,field_period,spu_period;
static uint64_t previous_draw,max_gap,window_start;
static unsigned fields,draws,steps,window_draws,wait_calls,underflows,short_writes;
static unsigned format_rate,ring_size,feed,readpos,writepos;
static int queued_bytes,sema,irq_failure;
static bool playing,pad_started,frame_ready;
static bool wide_requested,wide_enabled;
static bool launcher_test,arcade_active,locale_japanese;
static char sessions[32];
static unsigned session_count,menu_draws;
static int (*vblank_callback)(int);
static GSGLOBAL video_state;
static double first_rate,last_rate;
static int boot_case,main_status;
static unsigned resets,syncs,rpc_inits,legacy_services,probes,polls,icons,card_inits,resident_reuses;
static unsigned overlay[4];
static int *pending_type,*pending_free,*pending_format,*pending_result;
static int pending_port;
static bool late_callback;
static char save_path[256];
static unsigned connected_mask,game_inputs,dropped_inputs,early_aspect_changes;
static unsigned failed_pad_sample;
static unsigned menu_uploads,wide_game_frames,standard_game_frames,storage_modules,card_modules;
static unsigned idle_submissions;
static unsigned clean_resets,clean_attempts,reset_register_writes,pad_inits;
static bool dma_stopped;
static void *written_back;
static char boot_screen[96];
int _iop_reboot_count;
static bool game_initialized,menu_wide;
static char aspect_sequence[16];
static unsigned aspect_count;
extern unsigned char mcman_irx[],mcserv_irx[],secrman_irx[];
extern unsigned char usbd_irx[],bdm_irx[],bdmfs_fatfs_irx[],usbmass_bd_irx[];

static uint64_t ms(unsigned tenths) { return (uint64_t)kBUSCLK*tenths/10000; }
static void advance(uint64_t target) {
  if((boot_case==2 || boot_case==19) && pending_type && !late_callback && target>(uint64_t)kBUSCLK*3) {
    /* Exercise a genuine delayed write through libmc's retained pointers.
       ASan catches stack output buffers that expired after the timeout. */
    *pending_type=MC_TYPE_PS2;*pending_free=4096;*pending_format=MC_FORMATTED;
    *pending_result=0;late_callback=true;
  }
  if(target>=end_time) { now=end_time; longjmp(finished,1); }
  while(next_field<=target || next_spu<=target) {
    if(next_field<=next_spu) {
      now=next_field; next_field+=field_period; fields++;
      if(vblank_callback) vblank_callback(0);
    } else {
      now=next_spu; next_spu+=spu_period;
      if(playing) {
        if(queued_bytes<(int)feed) underflows++;
        queued_bytes-=(int)feed;
        readpos+=feed; if(readpos>=ring_size) readpos=0;
      }
    }
  }
  now=target;
}
uint64_t GetTimerSystemTime(void) {
  if(boot_case==5 || boot_case==8 || boot_case==16 || boot_case==17)
    advance(now+ms(1000));
  return now;
}
void SifInitRpc(int n) { (void)n;rpc_inits++; }
void SifExitRpc(void) {}
int SifIopReset(const char *s,int n) { (void)s;(void)n;resets++;return boot_case!=8; }
int SifIopSync(void) { syncs++;return boot_case!=5 && boot_case!=16; }
uint32_t sceSifGetReg(uint32_t reg) {
  assert(reg==SIF_SYSREG_SUBADDR);
  return 0x12300;
}
void sceSifStopDma(void) { dma_stopped=true; }
void sceSifWriteBackDCache(void *data,int size) {
  assert(dma_stopped && size>=104);
  written_back=data;
}
int sceSifSetReg(uint32_t reg,int value) {
  static const uint32_t order[]={
    SIF_REG_SMFLAG,SIF_REG_SMFLAG,SIF_REG_SMFLAG,SIF_SYSREG_RPCINIT,SIF_SYSREG_SUBADDR
  };
  static const int values[]={SIF_STAT_BOOTEND,SIF_STAT_SIFINIT,SIF_STAT_CMDINIT,0,0};
  if(reg==SIF_REG_SMFLAG && value==SIF_STAT_BOOTEND) reset_register_writes=0;
  assert(reset_register_writes<5 && reg==order[reset_register_writes]);
  assert(value==values[reset_register_writes++]);
  return 0;
}
int sceSifSetDma(SifDmaTransfer_t *chain,int count) {
  assert(dma_stopped && reset_register_writes==1);
  assert(count==2 && chain[0].attr==0 && chain[0].size==16);
  assert(chain[0].dest==(void*)(uintptr_t)0x12300 && chain[1].dest==chain[0].dest);
  assert(chain[0].src==chain[1].src && chain[1].src==written_back);
  assert(chain[1].attr==(SIF_DMA_ERT|SIF_DMA_INT_O) && chain[1].size==104);
  const SifCmdResetData_t *cmd=chain[1].src;
  assert(cmd->header.cid==SIF_CMD_RESET_CMD && cmd->header.psize==104);
  assert(!cmd->header.dsize && !cmd->arglen && !cmd->mode);
  clean_attempts++;
  if(boot_case==17 || (boot_case==14 && clean_attempts<=2)) return 0;
  clean_resets++;
  return 1;
}
int SleepThread(void) { return 0; }
static bool mx4sio_loaded(void) {
  if(boot_case==15) return true;
  if(boot_case==13) return resets>0 && clean_resets==0;
  return boot_case>=11 && boot_case<=20 && clean_resets==0;
}
int smod_get_mod_by_name(const char *s,smod_mod_info_t *info) {
  (void)info;
  if(mx4sio_loaded() && !strcmp(s,boot_case==12?"mx4sio_bd":"mx4sio")) return 8;
  if(boot_case==10 && (!strcmp(s,"sio2man") || !strcmp(s,"USB_driver") ||
      !strcmp(s,"bdm") || !strcmp(s,"secrman_special") ||
      !strcmp(s,"mcman_cex") || !strcmp(s,"mcserv"))) { resident_reuses++;return 7; }
  return 0;
}
void sbv_patch_enable_lmb(void) {}
int SifExecModuleBuffer(void *b,unsigned n,int a,void *v,int *result) {
  (void)n;(void)a;(void)v; *result=0;
  if(b==mcman_irx || b==mcserv_irx || b==secrman_irx) {
    card_modules++;
    if(boot_case>=11) {
      assert(clean_resets==1 && !mx4sio_loaded() && pad_inits==1);
      assert(boot_case!=12 && boot_case!=14);
      if(boot_case==13) assert(fields>=532);
    }
  }
  if(b==usbd_irx || b==bdm_irx || b==bdmfs_fatfs_irx || b==usbmass_bd_irx) storage_modules++;
  if(boot_case==6 && b==mcman_irx) { *result=-1;return -1; }
  return 1;
}
int SifLoadModule(const char *s,int n,void *p) {
  (void)n;(void)p;
  if(!strcmp(s,"rom0:SIO2MAN") || !strcmp(s,"rom0:PADMAN") ||
      !strcmp(s,"rom0:MCMAN") || !strcmp(s,"rom0:MCSERV")) legacy_services++;
  return 0;
}
int padInit(int a) {
  (void)a;
  pad_inits++;
  if(boot_case>=11 && boot_case<=14) {
    assert(clean_resets==1 && !mx4sio_loaded() && reset_register_writes==5);
    assert(_iop_reboot_count==1);
  }
  return 1;
}
int padPortOpen(int a,int b,void *p) { (void)a;(void)b;(void)p; return 1; }
int padGetState(int port,int slot) {
  (void)slot;
  if(boot_case==11 && port==0) {
    failed_pad_sample=0;
    if(fields>=32 && fields<36) {
      failed_pad_sample=1;
      return PAD_STATE_EXECCMD;
    }
  }
  return PAD_STATE_STABLE;
}
int padRead(int port,int slot,struct padButtonStatus *b) {
  (void)slot; unsigned key=0;
  if(port==0 && (boot_case==13 || boot_case==14)) {
    if(boot_case==14 && fields>=520 && fields<524) key=PAD_RIGHT;
    else if(fields>=532 && fields<537) key=PAD_CROSS;
    else if(fields>=540) key=PAD_RIGHT|PAD_CROSS;
  } else if(port==0 && boot_case==11) {
    if((fields>=3 && fields<6) || (fields>=9 && fields<12) ||
        (fields>=72 && fields<76)) key=PAD_SQUARE;
    else if(fields>=18 && fields<22) key=PAD_CROSS;
    else if(fields>=24 && fields<55) key=PAD_RIGHT|PAD_CROSS;
    else if(fields>=60 && fields<64) key=PAD_START|PAD_L2|PAD_R2;
    else if(fields>=80 && fields<84) key=PAD_CIRCLE;
    else if(fields>=90) key=PAD_RIGHT|PAD_CROSS;
  } else if(port==0 && boot_case==12) {
    if(fields<3) key=PAD_RIGHT;
    else if(fields>=10 && fields<14) key=PAD_CROSS;
    else if(fields>=20) key=PAD_RIGHT|PAD_CROSS;
  } else if(port==0 && boot_case==7) {
    if(fields<3) key=PAD_START;
    else if((fields>=30 && fields<34) || (fields>=100 && fields<104)) key=PAD_L2|PAD_R2;
    else if(fields>=60 && fields<64) key=PAD_START|PAD_L2|PAD_R2;
    else if(fields>=80 && fields<84) key=PAD_CIRCLE;
  } else if(port==0 && launcher_test) {
    if(fields<3) key=PAD_START;
    else if((fields>=15 && fields<17) || (fields>=45 && fields<47) ||
            (fields>=85 && fields<87) || (fields>=115 && fields<117) ||
            (fields>=145 && fields<147) || (fields>=175 && fields<177)) key=PAD_START|PAD_L2|PAD_R2;
    else if((fields>=24 && fields<27) || (fields>=94 && fields<97) || (fields>=124 && fields<127)) key=PAD_RIGHT;
    else if((fields>=60 && fields<63) || (fields>=154 && fields<157)) key=PAD_LEFT;
    else if((fields>=28 && fields<31) || (fields>=68 && fields<71) ||
            (fields>=128 && fields<131) || (fields>=162 && fields<165)) key=PAD_CROSS;
    else if((fields>=55 && fields<58) || (fields>=64 && fields<67) || (fields>=158 && fields<161)) key=PAD_TRIANGLE;
    else if((fields>=98 && fields<101) || (fields>=185 && fields<188)) key=PAD_CIRCLE;
  } else if(port==0 && !pad_started) { key=PAD_START|(wide_requested?PAD_SQUARE:0); pad_started=true; }
  b->ok=0;
  b->btns=(uint16_t)~key;
  if(boot_case==11 && port==0 && fields>=40 && fields<44) {
    failed_pad_sample=2;
    return 2;
  }
  return 4;
}
int mcInit(int n) { (void)n;card_inits++;return boot_case && boot_case!=7 && boot_case!=9?0:-1; }
int mcGetInfo(int a,int b,int *c,int *d,int *e) {
  (void)b;probes++;
  if(boot_case==4 && a==1) return -1;
  assert(!pending_type);pending_type=c;pending_free=d;pending_format=e;pending_port=a;
  return 0;
}
int mcSync(int a,void *b,int *c) {
  (void)b;assert(a==MC_NOWAIT && pending_type);polls++;pending_result=c;
  if(boot_case==2 || boot_case==19) return 0;
  if(boot_case==3) return -1;
  if(polls==1) return 0;
  bool absent=(boot_case==18 && pending_port==0) || boot_case==20;
  *pending_type=absent?0:MC_TYPE_PS2;*pending_free=absent?0:4096;
  *pending_format=boot_case==4 || absent?0:MC_FORMATTED;
  *c=absent?-12:0;pending_type=NULL;return 1;
}
int audsrv_init(void) { return 0; }
int audsrv_set_format(struct audsrv_fmt_t *f) {
  format_rate=f->freq; feed=(512u*format_rate/48000u)*4;
  ring_size=feed*10; readpos=feed*5; writepos=0; queued_bytes=ring_size-readpos;
  return 0;
}
int audsrv_set_volume(int n) { (void)n; return 0; }
static unsigned free_bytes(void) {
  return writepos<=readpos?readpos-writepos:ring_size-(writepos-readpos);
}
int audsrv_available(void) { advance(now+ms(1)); return (int)free_bytes(); }
int audsrv_wait_audio(int bytes) {
  wait_calls++; advance(now+ms(1));
  while(free_bytes()<(unsigned)bytes) advance(next_spu);
  return 0;
}
int audsrv_play_audio(const char *p,int bytes) {
  (void)p; advance(now+ms(1));
  unsigned n=(unsigned)bytes;
  if(n>free_bytes()) { n=free_bytes(); short_writes++; }
  writepos=(writepos+n)%ring_size; queued_bytes+=n; playing=true;
  return (int)n;
}
int audsrv_stop_audio(void) { playing=false; return 0; }
GSGLOBAL *gsKit_init_global(void) { return &video_state; }
void gsKit_init_screen(GSGLOBAL *g) { (void)g; }
int gsKit_texture_size(int a,int b,int c) { (void)c;return a*b*4; }
int gsKit_vram_alloc(GSGLOBAL *g,int a,int b) { (void)g;(void)a;(void)b;return 8192; }
void gsKit_texture_upload(GSGLOBAL *g,GSTEXTURE *t) {
  (void)g;
  if(boot_case!=11) return;
  if(t->Height==448) {
    assert(t->Width==640);
    assert(t->Mem[0]==(menu_wide?0x80291e10:0x80ab5615));
    assert(t->Mem[320]==0x804ddcff);
    menu_uploads++;
  } else {
    assert(t->Height==SMW_HEIGHT);
    assert(t->Width==(wide_enabled?384:256));
    if(wide_enabled) wide_game_frames++;
    else standard_game_frames++;
  }
}
void gsKit_clear(GSGLOBAL *g,uint64_t c) { (void)g;(void)c; }
void gsKit_queue_exec(GSGLOBAL *g) {
  (void)g;
  if((boot_case==13 || boot_case==14) && fields<520) idle_submissions++;
  advance(now+ms(4));
}
void gsKit_finish(void) {}
void gsKit_vsync_wait(void) { advance(next_field); }
void gsKit_display_buffer(GSGLOBAL *g) {
  (void)g;
  if(!frame_ready) return;
  frame_ready=false; draws++;window_draws++;
  if(previous_draw && now-previous_draw>max_gap) max_gap=now-previous_draw;
  previous_draw=now;
  if(now-window_start>=(uint64_t)kBUSCLK*10) {
    last_rate=(double)window_draws*kBUSCLK/(now-window_start);
    if(!first_rate) first_rate=last_rate;
    window_start=now;window_draws=0;
  }
}
void gsKit_setactive(GSGLOBAL *g) { (void)g; }
void gsKit_sync_flip(GSGLOBAL *g) {
  gsKit_vsync_wait();gsKit_display_buffer(g);g->ActiveBuffer^=1;
}
int gsKit_add_vsync_handler(int (*cb)(int)) {
  if(irq_failure) return -1;
  vblank_callback=cb;return 1;
}
int CreateSema(ee_sema_t *s) { sema=s->init_count;return 1; }
int DeleteSema(int n) { (void)n;return 0; }
int WaitSema(int n) { (void)n;while(!sema) advance(next_field);sema--;return 0; }
int iSignalSema(int n) { (void)n;sema=1;return 0; }
void ExitHandler(void) {}
#ifdef SMW_TEST_REAL_MENU
void __real_menu_draw(const Menu *m);
void __real_menu_text(MenuRect r,void *p,int x,int y,int s,uint32_t c,const char *v);
#define menu_draw __wrap_menu_draw
#define menu_text __wrap_menu_text
#endif
void menu_draw(const Menu *m) {
  menu_draws++;
  menu_wide=m->widescreen;
  if(boot_case==11 && (!aspect_count || aspect_sequence[aspect_count-1]!=(menu_wide?'W':'N'))) {
    assert(aspect_count<sizeof(aspect_sequence)-1);
    aspect_sequence[aspect_count++]=menu_wide?'W':'N';
  }
#ifdef SMW_TEST_REAL_MENU
  __real_menu_draw(m);
#endif
}
void menu_text(MenuRect r,void *p,int x,int y,int s,uint32_t c,const char *v) {
  (void)r;(void)p;(void)x;(void)y;(void)s;(void)c;(void)v;
  if(boot_case>=15 && y==210) snprintf(boot_screen,sizeof(boot_screen),"%s",v);
  /* Keep samples away from button/VBlank boundaries: a draw may cross the
     input event before the next poll sees it. */
  if(boot_case==7 && y==405) {
    if(fields<25) overlay[0]++;
    else if(fields>=36 && fields<56) overlay[1]++;
    else if(fields>=86 && fields<96) overlay[2]++;
    else if(fields>=108) overlay[3]++;
  }
#ifdef SMW_TEST_REAL_MENU
  __real_menu_text(r,p,x,y,s,c,v);
#endif
}
bool smw_init(const char *a,const char *b) { (void)a;snprintf(save_path,sizeof(save_path),"%s",b);return true; }
bool smw_init_memory(const void *a,size_t n,const char *b) {
  (void)a;(void)n;snprintf(save_path,sizeof(save_path),"%s",b);
  game_initialized=true;
  if(launcher_test) { assert(session_count<sizeof(sessions)-1);sessions[session_count++]=arcade_active?'A':locale_japanese?'J':'E'; }
  return true;
}
void smw_set_widescreen(bool enabled) {
  if(!game_initialized) early_aspect_changes++;
  wide_enabled=enabled;
}
unsigned smw_video_width(void) { return wide_enabled?342:256; }
unsigned smw_video_pitch(void) { return wide_enabled?384:256; }
void smw_set_connected_pads(unsigned n) { connected_mask=n; }
void smw_step(uint16_t a,uint16_t b) {
  (void)b;
  if(boot_case>=11 && boot_case<=14) {
    if((a&(SMW_RIGHT|SMW_B))==(SMW_RIGHT|SMW_B)) game_inputs++;
    if(boot_case==11 && failed_pad_sample) {
      assert(!a && connected_mask==3);
      dropped_inputs|=failed_pad_sample;
    }
  }
  steps++;
  advance(now+ms(4));
}
void smw_audio(int16_t *a,int n) { memset(a,0,(size_t)n*4);advance(now+ms(40)); }
void smw_skip_draw(void) { advance(now+ms(1)); }
void smw_draw(uint32_t *p) { (void)p;advance(now+ms(wide_enabled?180:114));frame_ready=true; }
bool smw_save(void) { return true; }
void smw_reset(void) {}
bool SmwLocale_Load(const void *p,size_t n) { (void)p;(void)n;return true; }
bool SmwLocale_Select(bool jp) { if(jp && arcade_active) return false;locale_japanese=jp;return true; }
bool SmwLocale_Japanese(void) { return locale_japanese; }
bool SmwSaveIcon_Ensure(const char *p) { (void)p;icons++;return true; }
uint8_t smw_game_mode(void) { return 20; }
#define IRX(n) unsigned char n##_irx[1];unsigned size_##n##_irx=1
IRX(audsrv);IRX(iomanX);IRX(usbd);IRX(bdm);IRX(bdmfs_fatfs);IRX(usbmass_bd);
IRX(sio2man);IRX(padman);IRX(secrman);IRX(mcman);IRX(mcserv);
unsigned char smw_embedded_assets[800];unsigned size_smw_embedded_assets=800;
#ifndef SMW_MOCK_NO_OPTIONAL_PACKS
unsigned char smw_embedded_japanese[12];unsigned size_smw_embedded_japanese=12;
unsigned char smw_embedded_super_system[12];unsigned size_smw_embedded_super_system=12;
#endif
int port_main(int,char**);
int main(int argc,char **argv) {
  unsigned seconds=argc>1?(unsigned)atoi(argv[1]):1800;
  int ppm=argc>2?atoi(argv[2]):0;
  irq_failure=argc>3?atoi(argv[3]):0;
  wide_requested=argc>4?atoi(argv[4]):0;
  launcher_test=argc>5?atoi(argv[5]):0;
  boot_case=argc>6?atoi(argv[6]):0;
  if(boot_case==9) { FILE *f=fopen("smw_assets.dat","wb");assert(f);fclose(f); }
  end_time=(uint64_t)seconds*kBUSCLK;
  field_period=((uint64_t)kBUSCLK*1001+30000)/60000;
  spu_period=((uint64_t)kBUSCLK*512*1000000)/(48000ull*(1000000+ppm));
  next_field=field_period;next_spu=spu_period;
  if(!setjmp(finished)) { char *args[]={"mock",NULL};main_status=port_main(1,args); }
  printf("MODEL_RESULT {\"seconds\":%u,\"format\":%u,\"first_fps\":%.3f,\"last_fps\":%.3f,\"sim_hz\":%.3f,\"wait_calls\":%u,\"underflows\":%u,\"short_writes\":%u,\"max_gap_ms\":%.3f}\n",
    seconds,format_rate,first_rate,last_rate,(double)steps/seconds,wait_calls,underflows,
    short_writes,(double)max_gap*1000/kBUSCLK);
  if(launcher_test) {
    printf("LAUNCHER_RESULT sessions=%s menu_draws=%u arcade=%u japanese=%u\n",sessions,menu_draws,arcade_active,locale_japanese);
    assert(!strcmp(sessions,"EAJAE") && !arcade_active && !locale_japanese && menu_draws>=session_count);
  }
  if(boot_case) {
    printf("BOOT_RESULT case=%d status=%d reset=%u sync=%u rpc=%u legacy=%u init=%u probes=%u polls=%u icons=%u reuse=%u late=%u steps=%u save=%s overlay=%u,%u,%u,%u\n",
      boot_case,main_status,resets,syncs,rpc_inits,legacy_services,card_inits,probes,polls,icons,resident_reuses,late_callback,steps,save_path,overlay[0],overlay[1],overlay[2],overlay[3]);
    if(boot_case>=15 && boot_case<=17) {
      const char *error=boot_case==15?"MX4SIO DRIVER STILL RESIDENT AFTER RESET":
        boot_case==16?"IOP REBOOT TIMED OUT":"IOP RESET REQUEST TIMED OUT";
      assert(main_status==1 && !steps && !pad_inits && !strcmp(boot_screen,error));
      assert(clean_resets==(boot_case==17?0:1));
      printf("ISO_ERROR_RESULT case=%d %s\n",boot_case,boot_screen);
    }
    else if(boot_case==5 || boot_case==8) { assert(main_status==1 && steps==0);assert(resets<40 && syncs<40); }
    else {
      assert(steps>100 && main_status==0);
      if(boot_case==9) assert(!resets && legacy_services==4);
      else if(boot_case>=11) assert(resets==(boot_case==13?1:0) && clean_resets==1 && !legacy_services);
      else assert(resets==1 && rpc_inits==2 && !legacy_services);
      if(boot_case==1 || boot_case==10) assert(probes==1 && polls==2 && icons==1 && !strcmp(save_path,"mc0:/SMWPS2/smw.srm"));
      if(boot_case==2 || boot_case==19) assert(probes==1 && polls>100 && polls<125 && !icons && !save_path[0] && late_callback);
      if(boot_case==3) assert(probes==1 && polls==1 && !icons && !save_path[0]);
      if(boot_case==4) assert(probes==2 && !icons);
      if(boot_case==6) assert(!card_inits && !probes);
      if(boot_case==7) assert(!overlay[0] && overlay[1]>0 && overlay[2]>0 && !overlay[3]);
      if(boot_case==10) assert(resident_reuses==6);
      if(boot_case>=18) {
        assert(!storage_modules && card_modules==3 && card_inits==1);
        if(boot_case==18) assert(probes==2 && icons==1 && !strcmp(save_path,"mc1:/SMWPS2/smw.srm"));
        if(boot_case==20) assert(probes==2 && !icons && !save_path[0]);
      }
      if(boot_case>=11 && boot_case<=14) {
        assert(!storage_modules);
        if(boot_case==12 || boot_case==14)
          assert(!card_modules && !card_inits && !probes && !icons && !save_path[0]);
        else
          assert(card_modules==3 && card_inits==1 && probes==1 && icons==1 &&
            !strcmp(save_path,"mc0:/SMWPS2/smw.srm"));
        assert(game_initialized && game_inputs>20);
        assert(clean_attempts==(boot_case==14?3:1));
        assert(arcade_active==(boot_case==12 || boot_case==14));
        if(boot_case==11) {
          assert(!strcmp(aspect_sequence,"NWNW"));
          assert(!early_aspect_changes && dropped_inputs==3);
          assert(menu_uploads<20 && wide_game_frames>20 && standard_game_frames>10);
        }
        printf("HANDOFF_RESULT case=%d aspects=%s uploads=%u game_inputs=%u dropped_inputs=%u game_4_3=%u game_16_9=%u storage_modules=%u\n",
          boot_case,aspect_sequence,menu_uploads,game_inputs,dropped_inputs,standard_game_frames,wide_game_frames,storage_modules);
        if(boot_case>=13) {
          assert(idle_submissions==1);
          printf("IDLE_RESULT case=%d idle_fields=520 gfx_submissions=%u game_inputs=%u clean_resets=%u\n",
            boot_case,idle_submissions,game_inputs,clean_resets);
        }
      }
    }
  }
  return 0;
}

/* Variant stubs expose the production launcher's actual controller flow. */
bool SmwArcade_Active(void) { return arcade_active; }
bool SmwArcade_Select(bool enabled) {
  if(enabled && !launcher_test && boot_case!=12 && boot_case!=14) return false;
  arcade_active=enabled;locale_japanese=false;return true;
}
bool SmwArcade_Load(const void *data,size_t size) {
  (void)data;(void)size;return launcher_test || boot_case==12 || boot_case==14;
}
