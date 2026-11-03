#include "../vendor/audsrv/src/pcm_volume.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  assert(smw_audsrv_pcm_volume(0)==0);
  assert(smw_audsrv_pcm_volume(0x3fff)==0x7fff);
  assert(smw_audsrv_pcm_volume(0x4000)==0x7fff);
  unsigned previous=0;
  for(unsigned raw=0;raw<=0x3fff;raw++) {
    unsigned mapped=smw_audsrv_pcm_volume(raw);
    assert(mapped>=previous && mapped<=0x7fff);
    assert(mapped>=raw*2 && mapped<=raw*2+1);
    previous=mapped;
    for(int pcm=-32768;pcm<=32767;pcm+=127) {
      int mixed=(int)(((long long)pcm*mapped)>>15);
      assert(mixed>=-32768 && mixed<=32767);
      assert(pcm>=0?mixed>=0:mixed<=0);
    }
  }
  puts("PASS: PCM input gain endpoints, clamping, monotonicity and no over-unity mixing");
  return 0;
}
