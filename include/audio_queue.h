#ifndef SMW_AUDIO_QUEUE_H
#define SMW_AUDIO_QUEUE_H
#include <stdint.h>
enum {
  SMW_OUTPUT_RATE=48000,SMW_OUTPUT_MAX_SAMPLES=804,
  SMW_AUDIO_QUEUE_FRAMES=4096,SMW_IOP_AUDIO_BYTES=20480,
  SMW_IOP_AUDIO_GUARD=2048,SMW_AUDIO_TARGET_BYTES=16384
};
typedef struct SmwAudioQueue {
  int16_t data[SMW_AUDIO_QUEUE_FRAMES*2];
  unsigned frames,fraction,queue_ms,peak_frames,dropped,errors;
  int correction;
} SmwAudioQueue;
void smw_audio_queue_reset(SmwAudioQueue *q);
unsigned smw_audio_queue_samples(SmwAudioQueue *q);
void smw_audio_queue_push(SmwAudioQueue *q,const int16_t *data,unsigned frames);
unsigned smw_audio_queue_offer(SmwAudioQueue *q,int available_bytes);
void smw_audio_queue_sent(SmwAudioQueue *q,int sent_bytes);
#endif
