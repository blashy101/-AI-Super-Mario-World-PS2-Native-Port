#include "audio_queue.h"
#include "frame_pacer.h"
#include "snes/dsp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  Dsp dsp={0}; int16_t output[804*2+16];
  for(unsigned i=0;i<534;i++) {
    dsp.sampleBuffer[i*2]=(int)i*61-16000;
    dsp.sampleBuffer[i*2+1]=16000-(int)i*59;
  }
  for(unsigned n=798;n<=804;n++) {
    memset(output,0x5a,sizeof(output)); dsp.sampleOffset=534;
    dsp_getSamples(&dsp,output,n);
    assert(dsp.sampleOffset==0);
    for(unsigned i=0;i<n;i++) {
      unsigned source=i*534/n;
      assert(output[i*2]==dsp.sampleBuffer[source*2]);
      assert(output[i*2+1]==dsp.sampleBuffer[source*2+1]);
    }
    assert(output[n*2]==0x5a5a && output[n*2+1]==0x5a5a);
  }
  SmwAudioQueue q; smw_audio_queue_reset(&q);
  unsigned total=0;
  for(unsigned i=0;i<60000;i++) {
    unsigned n=smw_audio_queue_samples(&q); assert(n==800 || n==801); total+=n;
  }
  assert(total==48048000);
  int16_t data[(SMW_AUDIO_QUEUE_FRAMES+20)*2];
  for(unsigned i=0;i<sizeof(data)/sizeof(*data);i++) data[i]=(int16_t)i;
  smw_audio_queue_push(&q,data,1602);
  assert(smw_audio_queue_offer(&q,2048)==0);
  assert(smw_audio_queue_offer(&q,8192)==1536);
  smw_audio_queue_sent(&q,800*4);
  assert(q.frames==802 && !memcmp(q.data,data+1600,802*4));
  unsigned errors=q.errors;
  smw_audio_queue_sent(&q,3); assert(q.errors==errors+1 && q.frames==802);
  smw_audio_queue_offer(&q,-1); assert(q.errors==errors+2);
  smw_audio_queue_push(&q,data,SMW_AUDIO_QUEUE_FRAMES+20);
  assert(q.frames==SMW_AUDIO_QUEUE_FRAMES && q.dropped==822);
  assert(!memcmp(q.data,data+40,SMW_AUDIO_QUEUE_FRAMES*4));
  SmwFramePacer p; uint32_t field=0xfffffff0u; smw_pacer_reset(&p,field);
  assert(smw_pacer_ticks(&p,field)==1);
  for(unsigned i=0;i<20;i++) {
    uint32_t deadline=smw_pacer_deadline(&p,field);
    assert(deadline==field+2); field=deadline;
    smw_pacer_presented(&p,field);
    assert(smw_pacer_ticks(&p,field)==2);
    assert(smw_pacer_ticks(&p,field)==0);
  }
  assert(!p.lost_ticks && !p.missed_presents);
  assert(smw_pacer_ticks(&p,field+5)==4 && p.pending==1);
  assert(smw_pacer_deadline(&p,field+5)==field+6 && p.missed_presents==1);
  smw_pacer_reset(&p,0);
  assert(smw_pacer_ticks(&p,200)==4 && p.pending==116 && p.lost_ticks==81);
  puts("PASS: stereo 48 kHz rational resampling, fractional samples, queue bounds/order/errors, VBlank wrap and overrun accounting");
  return 0;
}
