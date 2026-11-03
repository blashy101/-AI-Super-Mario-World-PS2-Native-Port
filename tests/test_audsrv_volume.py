"""Exercise the actual IOP update_volume body with recorded libsd writes."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
text=(ROOT/"vendor/audsrv/src/audsrv.c").read_text()
start=text.index("static void update_volume()")
brace=text.index("{",start)
depth=1
end=brace+1
while depth:
    depth+=(text[end]=="{")-(text[end]=="}")
    end+=1
body=text[start:end]
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pcm_volume.h"
enum { SD_CORE_0=0,SD_CORE_1=1,MAX_VOLUME=0x3fff,
 SD_PARAM_AVOLL=0x0d80,SD_PARAM_AVOLR=0x0e80,
 SD_PARAM_BVOLL=0x0f80,SD_PARAM_BVOLR=0x1080,
 SD_PARAM_MVOLL=0x0980,SD_PARAM_MVOLR=0x0a80 };
static int playing,core1_volume;
static unsigned regs[0x1200];
static void sceSdSetParam(unsigned reg,unsigned value) { regs[reg]=value; }
'''+body+'''
#define main check_gain_range
#include "tests/test_pcm_volume.c"
#undef main
int main(void) {
  check_gain_range();
  for(playing=0;playing<=1;playing++) {
    for(core1_volume=0;core1_volume<=MAX_VOLUME;core1_volume++) {
      memset(regs,0xab,sizeof(regs));
      update_volume();
      unsigned expected=playing?core1_volume*2+(core1_volume==MAX_VOLUME):0;
      assert(regs[SD_CORE_1|SD_PARAM_BVOLL]==expected);
      assert(regs[SD_CORE_1|SD_PARAM_BVOLR]==expected);
      assert(regs[SD_CORE_0|SD_PARAM_BVOLL]==0);
      assert(regs[SD_CORE_0|SD_PARAM_BVOLR]==0);
      assert(regs[SD_CORE_0|SD_PARAM_MVOLL]==0);
      assert(regs[SD_CORE_0|SD_PARAM_MVOLR]==0);
      assert(regs[SD_CORE_1|SD_PARAM_MVOLL]==0x3fff);
      assert(regs[SD_CORE_1|SD_PARAM_MVOLR]==0x3fff);
      assert(regs[SD_CORE_1|SD_PARAM_AVOLL]==0x7fff);
      assert(regs[SD_CORE_1|SD_PARAM_AVOLR]==0x7fff);
    }
  }
  puts("PASS: production IOP volume routing, both channels, mute and master levels");
  return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    source=Path(tmp)/"volume.c"
    binary=Path(tmp)/"volume"
    source.write_text(fixture)
    subprocess.run([os.environ.get("CC","cc"),"-O2","-std=c11","-I.",
        "-Ivendor/audsrv/src",str(source),"-o",str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary)],check=True)
