/* Controlled native room-loader/input smoke tests, not campaign completion. */
#define main v014_fixture_main
#include "test_v014_reports.c"
#undef main
int main(int argc,char **argv) {
  assert(argc>=8);setbuf(stdout,NULL);guards();bootstrap(argv[1]);
  if(strcmp(argv[2],"-")) load_pack(argv[2],true);
  if(strcmp(argv[3],"-")) load_pack(argv[3],false);
  unsigned edition=strtoul(argv[4],0,0),wide=strtoul(argv[5],0,0),frames=strtoul(argv[6],0,0);
  map_start(argv[1],edition,wide,false);enter_chocolate();
  memcpy(baseline_ram,g_ram,sizeof(g_ram));baseline_ppu=*g_ppu;baseline_snes=*g_snes;
  for(int arg=7;arg<argc;arg++) {
    unsigned level=strtoul(argv[arg],0,0);assert(level<512);printf("ROOM_BEGIN %u\n",level);
    load_level(level);smw_set_widescreen(wide);draw();
    unsigned mode=PPU_mode(g_ppu),layout=misc_level_mode_setting,f;
    uint32_t random=0x19b35a2du^(level*1013u);
    for(f=0;f<frames && misc_game_mode==20;f++) {
      random^=random<<13;random^=random>>17;random^=random<<5;
      unsigned input=(((f/(frames/4+1))&1)?SMW_LEFT:SMW_RIGHT)|SMW_Y|((f%90)<24?SMW_B:0);
      if((random&255)==0) input|=SMW_A;
      if((random&511)==1) input|=SMW_X;
      tick(input);assert(misc_game_mode<0x2a);
    }
    printf("ROOM_PASS {\"room\":%u,\"ticks\":%u,\"final_mode\":%u,\"ppu_mode\":%u,\"layout\":%u}\n",level,f,misc_game_mode,mode,layout);
  }
}
