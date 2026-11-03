#include "audio_queue.h"
#include <string.h>
void smw_audio_queue_reset(SmwAudioQueue *q) { memset(q,0,sizeof(*q)); }
unsigned smw_audio_queue_samples(SmwAudioQueue *q) {
  /* NTSC fields are nominally 60000/1001 Hz, so retain 800.8 samples/tick. */
  q->fraction+=SMW_OUTPUT_RATE*1001u;
  unsigned n=q->fraction/60000u; q->fraction%=60000u;
  return (unsigned)((int)n+q->correction);
}
void smw_audio_queue_push(SmwAudioQueue *q,const int16_t *data,unsigned n) {
  if(n>SMW_AUDIO_QUEUE_FRAMES) {
    unsigned skip=n-SMW_AUDIO_QUEUE_FRAMES;
    data+=skip*2; n-=skip; q->dropped+=skip;
  }
  if(q->frames+n>SMW_AUDIO_QUEUE_FRAMES) {
    unsigned skip=q->frames+n-SMW_AUDIO_QUEUE_FRAMES;
    memmove(q->data,q->data+skip*2,(q->frames-skip)*4);
    q->frames-=skip; q->dropped+=skip;
  }
  memcpy(q->data+q->frames*2,data,n*4); q->frames+=n;
  if(q->frames>q->peak_frames) q->peak_frames=q->frames;
}
unsigned smw_audio_queue_offer(SmwAudioQueue *q,int available) {
  if(available<0 || available>SMW_IOP_AUDIO_BYTES) { q->errors++; return 0; }
  unsigned total=SMW_IOP_AUDIO_BYTES-(unsigned)available+q->frames*4;
  q->queue_ms=total*1000u/(SMW_OUTPUT_RATE*4u);
  /* Absorb small audio/video clock differences in resampling, not game speed. */
  int error=(int)total-SMW_AUDIO_TARGET_BYTES;
  q->correction=error>4096?-2:error>2048?-1:error<-4096?2:error<-2048?1:0;
  int room=available-SMW_IOP_AUDIO_GUARD;
  if(room<=0) return 0;
  unsigned n=(unsigned)room/4;
  if(n>q->frames) n=q->frames;
  if(n>4095) n=4095; /* One audsrv RPC packet, at most 16380 bytes. */
  return n;
}
void smw_audio_queue_sent(SmwAudioQueue *q,int bytes) {
  if(bytes<0 || (bytes&3) || (unsigned)bytes>q->frames*4) { q->errors++; return; }
  unsigned n=(unsigned)bytes/4; q->frames-=n;
  if(q->frames) memmove(q->data,q->data+n*2,q->frames*4);
}
