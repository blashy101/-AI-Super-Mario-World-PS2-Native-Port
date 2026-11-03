/* PCM input uses a signed 16-bit volume register. Map audsrv's public
 * 0..0x3fff volume table to positive input gain 0..0x7fff (unity).
 * Copyright 2026 SMW PS2 contributors. SPDX-License-Identifier: MIT
 */
#ifndef SMW_AUDSRV_PCM_VOLUME_H
#define SMW_AUDSRV_PCM_VOLUME_H
static inline unsigned smw_audsrv_pcm_volume(unsigned volume) {
  if(volume>0x3fffu) volume=0x3fffu;
  return volume*0x7fffu/0x3fffu;
}
#endif
