#ifndef SMW_FRAME_PACER_H
#define SMW_FRAME_PACER_H
#include <stdint.h>
enum { SMW_FIELDS_PER_PRESENT=2, SMW_MAX_CATCHUP=4, SMW_MAX_BACKLOG=120 };
typedef struct SmwFramePacer {
  uint32_t last_field,next_present,pending,missed_presents,lost_ticks;
} SmwFramePacer;
void smw_pacer_reset(SmwFramePacer *p,uint32_t field);
unsigned smw_pacer_ticks(SmwFramePacer *p,uint32_t field);
uint32_t smw_pacer_deadline(SmwFramePacer *p,uint32_t field);
void smw_pacer_presented(SmwFramePacer *p,uint32_t field);
#endif
