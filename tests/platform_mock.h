/* Host-only SDK facade for exercising the actual production main loop. */
#ifndef SMW_PLATFORM_MOCK_H
#define SMW_PLATFORM_MOCK_H
#include <stdint.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef struct { int Mode,Interlace,Field,Width,Height,PSM,DoubleBuffering,
  ZBuffering,PrimAlphaEnable,ActiveBuffer,FirstFrame; } GSGLOBAL;
typedef struct { int Width,Height,PSM,Filter,Vram; u32 *Mem; } GSTEXTURE;
typedef struct { int init_count,max_count,option; } ee_sema_t;
struct audsrv_fmt_t { int freq,bits,channels; };
struct padButtonStatus { uint8_t ok,mode; uint16_t btns; };
typedef struct { int id; } smod_mod_info_t;
typedef struct {
  uint32_t psize:8,dsize:24;
  uint32_t dest,cid,opt;
} SifCmdHeader_t;
typedef struct {
  SifCmdHeader_t header;
  int arglen,mode;
  char arg[80];
} SifCmdResetData_t;
typedef struct { void *src,*dest; int size,attr; } SifDmaTransfer_t;
#define SIF_CMD_RESET_CMD 0x80000003u
#define SIF_SYSREG_SUBADDR 0x80000000u
#define SIF_SYSREG_RPCINIT 0x80000002u
#define SIF_REG_SMFLAG 4
#define SIF_STAT_BOOTEND 0x40000
#define SIF_STAT_SIFINIT 0x10000
#define SIF_STAT_CMDINIT 0x20000
#define SIF_DMA_ERT 0x40
#define SIF_DMA_INT_O 0x4
enum {
  PAD_SELECT=1,PAD_START=8,PAD_UP=16,PAD_RIGHT=32,PAD_DOWN=64,PAD_LEFT=128,
  PAD_L2=256,PAD_R2=512,PAD_L1=1024,PAD_R1=2048,PAD_TRIANGLE=4096,
  PAD_CIRCLE=8192,PAD_CROSS=16384,PAD_SQUARE=32768,
  PAD_STATE_DISCONN=0,PAD_STATE_STABLE=6,PAD_STATE_FINDCTP1=2,PAD_STATE_EXECCMD=5,
  GS_MODE_NTSC=0,GS_INTERLACED=1,GS_FIELD=1,GS_PSM_CT32=0,
  GS_SETTING_ON=1,GS_SETTING_OFF=0,GS_ZTEST_OFF=0,GS_ONESHOT=1,
  GS_FILTER_NEAREST=0,GSKIT_ALLOC_USERBUFFER=0,MC_TYPE_MC=0,MC_TYPE_PS2=2,
  MC_NOWAIT=1,MC_FORMATTED=1,kBUSCLK=147456000
};
#define D_CTRL_RELE_OFF 0
#define D_CTRL_MFD_OFF 0
#define D_CTRL_STS_UNSPEC 0
#define D_CTRL_STD_OFF 0
#define D_CTRL_RCYC_8 0
#define DMA_CHANNEL_GIF 0
#define GS_SETREG_RGBAQ(...) 0
#define GSKIT_ALLOC_ERROR 0
#define dmaKit_init(...) ((void)0)
#define dmaKit_chan_init(...) ((void)0)
#define gsKit_prim_sprite(...) ((void)0)
#define gsKit_prim_sprite_texture(...) ((void)0)
#define gsKit_set_test(...) ((void)0)
#define gsKit_mode_switch(...) ((void)0)
uint64_t GetTimerSystemTime(void);
void SifInitRpc(int);
void SifExitRpc(void);
int SifIopReset(const char*,int);
int SifIopSync(void);
uint32_t sceSifGetReg(uint32_t);
int sceSifSetReg(uint32_t,int);
void sceSifStopDma(void);
void sceSifWriteBackDCache(void*,int);
int sceSifSetDma(SifDmaTransfer_t*,int);
int SleepThread(void);
int smod_get_mod_by_name(const char*,smod_mod_info_t*);
void sbv_patch_enable_lmb(void);
int SifExecModuleBuffer(void*,unsigned,int,void*,int*);
int SifLoadModule(const char*,int,void*);
int padInit(int);
int padPortOpen(int,int,void*);
int padGetState(int,int);
int padRead(int,int,struct padButtonStatus*);
int mcInit(int);
int mcGetInfo(int,int,int*,int*,int*);
int mcSync(int,void*,int*);
int audsrv_init(void);
int audsrv_set_format(struct audsrv_fmt_t*);
int audsrv_set_volume(int);
int audsrv_wait_audio(int);
int audsrv_play_audio(const char*,int);
int audsrv_stop_audio(void);
int audsrv_available(void);
GSGLOBAL *gsKit_init_global(void);
void gsKit_init_screen(GSGLOBAL*);
int gsKit_texture_size(int,int,int);
int gsKit_vram_alloc(GSGLOBAL*,int,int);
void gsKit_texture_upload(GSGLOBAL*,GSTEXTURE*);
void gsKit_clear(GSGLOBAL*,uint64_t);
void gsKit_queue_exec(GSGLOBAL*);
void gsKit_finish(void);
void gsKit_sync_flip(GSGLOBAL*);
void gsKit_vsync_wait(void);
void gsKit_display_buffer(GSGLOBAL*);
void gsKit_setactive(GSGLOBAL*);
int gsKit_add_vsync_handler(int(*)(int));
int CreateSema(ee_sema_t*);
int DeleteSema(int);
int WaitSema(int);
int iSignalSema(int);
void ExitHandler(void);
#endif
