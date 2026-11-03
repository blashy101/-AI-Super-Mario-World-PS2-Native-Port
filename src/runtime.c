/* PS2-native glue for snesrev/smw. No 65C816 or SPC700 instruction loop. */
#include "smw_ps2.h"
#include "src/smw_rtl.h"
#include "src/common_cpu_infra.h"
#include "src/smw_spc_player.h"
#include "src/snes/snes.h"
#include "src/snes/dsp.h"
#include "src/variables.h"
#include "assets/smw_assets.h"
#include <stdarg.h>
#include "save_journal.h"
#include "widescreen.h"
#include "localization.h"
#include "super_system.h"

uint8 g_ram[0x20000] __attribute__((aligned(128)));
static uint8 save_ram[2048];
uint8 *g_sram = save_ram;
int g_sram_size = sizeof(save_ram);
const uint8 *g_rom;
Ppu *g_ppu, *g_my_ppu;
Dma *g_dma;
Snes *g_snes;
SpcPlayer *g_spc_player;
bool g_use_my_apu_code=true, g_debug_flag, g_fail, g_new_ppu=true;
bool g_custom_music, g_did_finish_level_hook;
int snes_frame_counter, g_dbg_ctr_mine;
uint8 game_id=kGameID_SMW;
const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];
static uint8 *asset_blob;
static size_t asset_length;
static char last_error[192], save_file[256];
static uint8 apu_ports[4];
static unsigned connected_pads=1;

static bool error(const char *fmt, ...) {
  va_list ap; va_start(ap,fmt);
  vsnprintf(last_error,sizeof(last_error),fmt,ap); va_end(ap);
  fprintf(stderr,"%s\n",last_error); return false;
}
void Die(const char *s) { fprintf(stderr,"Fatal: %s\n",s); abort(); }
void Warning(const char *s) { fprintf(stderr,"%s\n",s); }
const char *smw_error(void) { return last_error; }
static uint32_t le32(const uint8_t *p) {
  return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}

static bool bind_assets(uint8 *data,size_t len) {
  static const uint8 sig[]={kAssets_Sig};
  if(len<800 || len>8*1024*1024 || memcmp(data,sig,sizeof(sig)) || le32(data+80)!=kNumberOfAssets) {
    free(data); return error("Wrong assets: run tools/prepare_assets.py with SMW USA");
  }
  size_t offset=88+kNumberOfAssets*4;
  if(le32(data+84)>len-offset) { free(data); return error("Invalid asset names"); }
  offset+=le32(data+84);
  const uint8 *ptrs[kNumberOfAssets];
  uint32 sizes[kNumberOfAssets];
  for(size_t i=0;i<kNumberOfAssets;i++) {
    size_t size=le32(data+88+i*4); offset=(offset+3)&~(size_t)3;
    if(offset>len || size>len-offset) {
      free(data); return error("Corrupt asset %u",(unsigned)i);
    }
    ptrs[i]=data+offset; sizes[i]=size; offset+=size;
  }
  /* Format slots 150..153: secondary entrance type, destination, Y and X.
   * Old vanilla extraction emitted empty arrays, so ordinary return pipes
   * indexed into unrelated following assets. Require the complete tables. */
  for(unsigned i=150;i<=153;i++) if(sizes[i]!=512) {
    free(data);
    return error("Incomplete secondary entrance tables: regenerate assets with tools/prepare_assets.py");
  }
  free(asset_blob); asset_blob=data; asset_length=len;
  memcpy(g_asset_ptrs,ptrs,sizeof(ptrs)); memcpy(g_asset_sizes,sizes,sizeof(sizes));
  SmwLocale_BindEnglish();
  return true;
}
static bool load_assets(const char *path) {
  FILE *f=fopen(path,"rb");
  if(!f) return error("Cannot open %s",path);
  if(fseek(f,0,SEEK_END)) { fclose(f); return error("Cannot seek assets"); }
  long len=ftell(f); rewind(f);
  if(len<800 || len>8*1024*1024) { fclose(f); return error("Invalid asset size"); }
  uint8 *data=malloc((size_t)len);
  if(!data) { fclose(f); return error("Out of memory loading assets"); }
  bool ok=fread(data,1,(size_t)len,f)==(size_t)len; fclose(f);
  if(!ok) { free(data); return error("Incomplete asset read"); }
  return bind_assets(data,(size_t)len);
}
MemBlk FindInAssetArray(int asset,int idx) {
  if(asset<0 || asset>=kNumberOfAssets || idx<0) Die("Invalid asset index");
  return FindIndexInMemblk((MemBlk){g_asset_ptrs[asset],g_asset_sizes[asset]},idx);
}
const uint8 *FindPtrInAsset(int asset,uint32 addr) {
  if(asset<0 || asset>=kNumberOfAssets) Die("Invalid asset pointer");
  return FindAddrInMemblk((MemBlk){g_asset_ptrs[asset],g_asset_sizes[asset]},addr);
}
static bool start_game(const char *save) {
  if(!g_snes) {
    g_snes=calloc(1,sizeof(*g_snes));
    g_ppu=ppu_init(); g_my_ppu=g_ppu;
    if(!g_snes || !g_ppu) return error("Cannot allocate native runtime");
    g_dma=dma_init(g_snes); g_spc_player=SmwSpcPlayer_Create();
    if(!g_dma || !g_spc_player) return error("Cannot allocate native sound/graphics");
    g_snes->ram=g_ram; g_snes->ppu=g_ppu; g_snes->dma=g_dma;
  }
  if(strlen(save)>=sizeof(save_file)) return error("Save path too long");
  if(SmwArcade_Active()) save=""; /* Arcade sessions never read or write retail SRAM. */
  strcpy(save_file,save); memset(save_ram,0,sizeof(save_ram));
  if(!strncmp(save,"mc",2) && save[2]>='0' && save[2]<='1' && save[3]==':')
    smw_journal_load(save,save_ram,sizeof(save_ram));
  else {
  FILE *f=fopen(save,"rb");
  if(f) {
    if(fread(save_ram,1,sizeof(save_ram),f)!=sizeof(save_ram)) {
      memset(save_ram,0,sizeof(save_ram)); Warning("Ignoring incomplete save");
    }
    fclose(f);
  }
  }
  smw_reset(); return true;
}
bool smw_init(const char *assets,const char *save) {
  last_error[0]=0;
  return load_assets(assets) && start_game(save);
}
bool smw_init_memory(const void *assets,size_t length,const char *save) {
  last_error[0]=0;
  if(length<800 || length>8*1024*1024) return error("Invalid embedded assets");
  uint8 *copy=malloc(length);
  if(!copy) return error("Out of memory loading embedded assets");
  memcpy(copy,assets,length);
  return bind_assets(copy,length) && start_game(save);
}
void smw_reset(void) {
  SmwWide_Reset();
  memset(g_ram,0,sizeof(g_ram)); memset(apu_ports,0,sizeof(apu_ports));
  g_snes->vIrqEnabled=false; g_snes->runningWhichVersion=2;
  g_snes->input1_currentState=g_snes->input2_currentState=0;
  g_snes->ramAdr=0; ppu_reset(g_ppu); dma_reset(g_dma);
  g_lunar_magic=false; g_custom_music=false;
  /* A new edition/session is a cold boot. The SPC reset only clears its
   * low work area; clear sample/echo RAM too so a previous song cannot leak. */
  memset(g_spc_player->ram,0,0x10000);
  g_spc_player->initialize(g_spc_player); snes_frame_counter=0;
}
static uint16_t clean_pad(uint16_t x) {
  if((x&(SMW_UP|SMW_DOWN))==(SMW_UP|SMW_DOWN)) x&=~(SMW_UP|SMW_DOWN);
  if((x&(SMW_LEFT|SMW_RIGHT))==(SMW_LEFT|SMW_RIGHT)) x&=~(SMW_LEFT|SMW_RIGHT);
  return x&0xfff;
}
void smw_set_connected_pads(unsigned mask) { connected_pads=mask&3; }
void smw_step(uint16_t p1,uint16_t p2) {
  g_snes->input1_currentState=clean_pad(p1); g_snes->input2_currentState=clean_pad(p2);
  g_ram[kSmwRam_my_flags]=connected_pads;
  g_ram[kSmwRam_APUI02]=g_spc_player->port_to_snes[2];
  SmwRunOneFrameOfGame(); snes_frame_counter++;
}
void smw_draw(uint32_t *pixels) {
  SmwWide_PrepareDraw(g_ppu);
  /* Clear pillarboxes/padding only in wide mode. Regular rendering has no copy. */
  if(SmwWide_Enabled() && !g_ppu->wideTile)
    memset(pixels,0,smw_video_pitch()*SMW_HEIGHT*sizeof(*pixels));
  PpuBeginDrawing(g_ppu,(uint8_t*)pixels,smw_video_pitch()*4,kPpuRenderFlags_NewRenderer);
  SmwDrawPpuFrame();
}
void smw_set_widescreen(bool enabled) { SmwWide_SetEnabled(enabled); }
unsigned smw_video_width(void) { return SmwWide_Enabled()?SMW_WIDE_WIDTH:SMW_WIDTH; }
unsigned smw_video_pitch(void) { return SmwWide_Enabled()?SMW_WIDE_PITCH:SMW_WIDTH; }
void SmwSkipPpuFrame(void);
void smw_skip_draw(void) { SmwSkipPpuFrame(); }
void smw_audio(int16_t *out,int samples) {
  memcpy(g_spc_player->input_ports,apu_ports,4);
  g_spc_player->gen_samples(g_spc_player);
  dsp_getSamples(g_spc_player->dsp,out,samples);
}
void RtlApuWrite(uint16 adr,uint8 val) {
  if(adr<APUI00 || adr>APUI03) Die("Unexpected audio register");
  apu_ports[adr&3]=val;
}
void RtlApuUpload(const uint8 *p) {
  g_spc_player->upload(g_spc_player,p);
  memcpy(apu_ports,g_spc_player->input_ports,4);
}
void RtlApuReset(void) { g_spc_player->initialize(g_spc_player); memset(apu_ports,0,4); }
void RtlApuLock(void) {} /* Main thread owns the native sound engine. */
void RtlApuUnlock(void) {}
void RtlSetUploadingApu(bool uploading) { (void)uploading; }
bool smw_save(void) {
  if(SmwArcade_Active() || !save_file[0]) return true; /* Explicit session-only mode. */
  if(!strncmp(save_file,"mc",2) && save_file[2]>='0' && save_file[2]<='1' && save_file[3]==':') {
    if(!smw_journal_save(save_file,save_ram,sizeof(save_ram))) return error("Memory card save failed");
    return true;
  }
  char tmp[272]; snprintf(tmp,sizeof(tmp),"%s.tmp",save_file);
  FILE *f=fopen(tmp,"wb");
  if(!f) return error("Cannot create save at %s",tmp);
  bool ok=fwrite(save_ram,1,sizeof(save_ram),f)==sizeof(save_ram);
  if(fclose(f)) ok=false;
  if(!ok) { remove(tmp); return error("Cannot write save"); }
  /* Never delete the existing save before rename succeeds. */
  if(rename(tmp,save_file)) return error("Cannot replace save; new data is in %s",tmp);
  return true;
}
void RtlWriteSram(void) { smw_save(); }
void RtlReadSram(void) {} /* Read once at game load. */
void RtlSaveSnapshot(const char *path,bool bug) { (void)path; (void)bug; }
uint8_t smw_game_mode(void) { return misc_game_mode; }
uint32_t smw_frame_count(void) { return (uint32_t)snes_frame_counter; }
