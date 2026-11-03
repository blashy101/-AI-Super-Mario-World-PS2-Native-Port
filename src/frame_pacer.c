#include "frame_pacer.h"
void smw_pacer_reset(SmwFramePacer *p,uint32_t field) {
  *p=(SmwFramePacer){.last_field=field,.next_present=field+2,.pending=1};
}
unsigned smw_pacer_ticks(SmwFramePacer *p,uint32_t field) {
  uint32_t elapsed=field-p->last_field;
  p->last_field=field;
  /* Snapshot ticks before work. Audio/IO delays must not keep extending the
     current catch-up batch while the screen waits for its next render. */
  if(elapsed>SMW_MAX_BACKLOG || p->pending>SMW_MAX_BACKLOG-elapsed) {
    p->lost_ticks+=elapsed+p->pending-SMW_MAX_BACKLOG;
    p->pending=SMW_MAX_BACKLOG;
  } else p->pending+=elapsed;
  unsigned n=p->pending<SMW_MAX_CATCHUP?p->pending:SMW_MAX_CATCHUP;
  p->pending-=n; return n;
}
uint32_t smw_pacer_deadline(SmwFramePacer *p,uint32_t field) {
  if((int32_t)(field-p->next_present)>=0) {
    p->missed_presents++;
    return field+1; /* Change display buffers on a fresh blank after overruns. */
  }
  return p->next_present;
}
void smw_pacer_presented(SmwFramePacer *p,uint32_t field) {
  p->next_present=field+SMW_FIELDS_PER_PRESENT;
}
