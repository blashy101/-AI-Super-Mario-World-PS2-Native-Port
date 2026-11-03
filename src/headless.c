#define _POSIX_C_SOURCE 200809L
#include "smw_ps2.h"
#include "audio_queue.h"
#include "localization.h"
#include "super_system.h"
#include "src/common_rtl.h"
#include "src/variables.h"
#include "src/snes/ppu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t pixels[SMW_MAX_PITCH*SMW_HEIGHT];
static int16_t pcm[SMW_OUTPUT_MAX_SAMPLES*2];
static bool load_game(const char *path,const char *save) {
  if(!getenv("SMW_TEST_MEMORY")) return smw_init(path,save);
  FILE *f=fopen(path,"rb"); if(!f) return false;
  fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
  if(n<800 || n>8*1024*1024) { fclose(f); return false; }
  void *data=malloc((size_t)n);
  bool ok=data && fread(data,1,(size_t)n,f)==(size_t)n;
  fclose(f);
  if(ok) ok=smw_init_memory(data,(size_t)n,save);
  free(data); return ok;
}
static double seconds(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
  return t.tv_sec+t.tv_nsec*1e-9;
}
static uint32_t hash_bytes(const void *data,size_t length) {
  const uint8_t *p=data; uint32_t hash=2166136261u;
  while(length--) { hash^=*p++; hash*=16777619u; }
  return hash;
}
static void ppm(const char *path) {
  FILE *f=fopen(path,"wb"); if(!f) { perror(path); exit(1); }
  fprintf(f,"P6\n%u %d\n255\n",smw_video_width(),SMW_HEIGHT);
  for(int y=0;y<SMW_HEIGHT;y++) for(unsigned x=0;x<smw_video_width();x++) {
    uint32_t color=pixels[y*smw_video_pitch()+x];
    uint8_t rgb[]={color>>16,color>>8,color}; fwrite(rgb,1,3,f);
  }
  fclose(f);
}
int main(int argc,char **argv) {
  if(argc<2) { fprintf(stderr,"Usage: %s ASSETS [frames=1800] [frame.ppm] [title|play] [save-path]\n",argv[0]); return 2; }
  const char *jp=getenv("SMW_TEST_JAPANESE");
  if(jp) {
    FILE *f=fopen(jp,"rb"); if(!f) return 1;
    fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
    if(n<12 || n>512*1024) { fclose(f); return 1; }
    void *data=malloc(n); bool ok=data && fread(data,1,n,f)==(size_t)n;
    fclose(f); if(ok) ok=SmwLocale_Load(data,n); free(data);
    if(!ok || !SmwLocale_Select(true)) return 1;
  }
  const char *ss=getenv("SMW_TEST_SUPER_SYSTEM");
  if(ss) {
    FILE *f=fopen(ss,"rb");if(!f) return 1;
    fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
    if(n<12 || n>64*1024) { fclose(f);return 1; }
    void *data=malloc(n);bool ok=data && fread(data,1,n,f)==(size_t)n;
    fclose(f);if(ok) ok=SmwArcade_Load(data,n);free(data);
    if(!ok || !SmwArcade_Select(true)) return 1;
  }
  if(!load_game(argv[1],argc>5?argv[5]:"build/host/test.srm")) return 1;
  smw_set_widescreen(getenv("SMW_TEST_WIDE")!=NULL);
  int frames=argc>2?atoi(argv[2]):1800;
  if(frames<1 || frames>10000000) return 2;
  bool half=argc>4 && !strcmp(argv[4],"half");
  bool play=half || (argc>4 && !strcmp(argv[4],"play"));
  bool trace=getenv("SMW_TRACE")!=NULL;
  bool output48=getenv("SMW_TEST_OUTPUT48")!=NULL;
  SmwAudioQueue queue={0};
  const char *hash_path=getenv("SMW_HASH_TRACE_PATH");
  FILE *hash_file=hash_path?fopen(hash_path,"wb"):NULL;
  if(hash_path && !hash_file) { perror(hash_path); return 1; }
  double step_time=0,draw_time=0,audio_time=0,skipped_time=0;
  unsigned draws=0,skips=0;
  int previous_mode=-1;
  unsigned modes=0, audio_nonzero=0,level_frames=0,action_frames=0;
  double start=seconds();
  for(int f=0;f<frames;f++) {
    uint16_t pad=0;
    /* Enter the file/player menus, start Yoshi's Island 1, then move/jump. */
    if(play) {
      if(f==180 || f==260 || f==340 || f==750) pad=SMW_START;
      if(misc_game_mode==20 && misc_intro_level_flag) pad=f%120==0?SMW_B:0;
      else if(misc_game_mode==14 && pointer_current_overworld_process==3) {
        if(f%20==0) pad=ow_players_pos[player_current_character].x==104?SMW_LEFT:SMW_B;
      }
      else if(misc_game_mode==20) pad=SMW_RIGHT|SMW_Y|((f%120)<25?SMW_B:0);
    }
    double before=seconds();
    smw_step(pad,0); step_time+=seconds()-before;
    before=seconds();
    if(!half || f%2==0 || f==frames-1) {
      smw_draw(pixels); draw_time+=seconds()-before; draws++;
    } else { smw_skip_draw(); skipped_time+=seconds()-before; skips++; }
    before=seconds();
    unsigned samples=output48?smw_audio_queue_samples(&queue):SMW_AUDIO_RATE/60;
    smw_audio(pcm,samples); audio_time+=seconds()-before;
    if(hash_file) {
      uint32_t hashes[]={hash_bytes(pixels,smw_video_pitch()*SMW_HEIGHT*sizeof(*pixels)),hash_bytes(g_ram,sizeof(g_ram)),
        hash_bytes(pcm,samples*2*sizeof(int16_t))};
      if(fwrite(hashes,sizeof(hashes),1,hash_file)!=1) { perror(hash_path); return 1; }
    }
    if(trace && previous_mode!=misc_game_mode) {
      fprintf(stderr,"frame=%d mode=%u intro=%u level=%u ow=(%u,%u) process=%u\n",f,
        misc_game_mode,misc_intro_level_flag,ow_level_number_lo,
        ow_players_pos[player_current_character].x,ow_players_pos[player_current_character].y,
        pointer_current_overworld_process);
      previous_mode=misc_game_mode;
    }
    if(misc_game_mode==20 && !misc_intro_level_flag) level_frames++;
    if(misc_game_mode==20 && !misc_intro_level_flag && ow_level_number_lo!=40) action_frames++;
    for(unsigned i=0;i<samples*2;i++) if(pcm[i]) { audio_nonzero++; break; }
    if(smw_game_mode()<32) modes|=1u<<smw_game_mode();
  }
  double elapsed=seconds()-start;
  if(hash_file && fclose(hash_file)) return 1;
  uint32_t hash=2166136261u;
  for(int y=0;y<SMW_HEIGHT;y++) for(unsigned x=0;x<smw_video_width();x++) {
    hash^=pixels[y*smw_video_pitch()+x]; hash*=16777619u;
  }
  uint32_t ram_hash=2166136261u;
  for(unsigned i=0;i<sizeof(g_ram);i++) { ram_hash^=g_ram[i]; ram_hash*=16777619u; }
  printf("HOST ONLY: frames=%d seconds=%.3f fps=%.2f mode=%u modes=0x%x level_frames=%u action_frames=%u audio_frames=%u image_hash=%08x ram_hash=%08x\n",
    frames,elapsed,frames/elapsed,smw_game_mode(),modes,level_frames,action_frames,audio_nonzero,hash,ram_hash);
  if(getenv("SMW_PROFILE"))
    printf("HOST ONLY ms/frame: game=%.4f audio=%.4f ppu=%.4f skip=%.4f draws=%u skips=%u\n",
      step_time*1000/frames,audio_time*1000/frames,draws?draw_time*1000/draws:0,
      skips?skipped_time*1000/skips:0,draws,skips);
  if(getenv("SMW_PROFILE"))
    printf("PPU state: mode=%u main=%02x sub=%02x math=%02x select=%02x fixed=%04x windows=%06x\n",
      g_ppu->bgmode,g_ppu->screenEnabled[0],g_ppu->screenEnabled[1],g_ppu->cgadsub,
      g_ppu->cgwsel,g_ppu->fixedColor,(unsigned)g_ppu->windowsel);
  if(argc>3) ppm(argv[3]);
  if(!smw_save()) return 1;
  return 0;
}
