#ifndef SMW_PS2_H
#define SMW_PS2_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum { SMW_WIDTH = 256, SMW_HEIGHT = 224, SMW_AUDIO_RATE = 32000 };
/* The wide buffer has a padded stride for PS2 texture DMA. */
enum { SMW_MAX_PITCH = 384 };
/* Buttons are in the native C engine's order, before SNES bit reversal. */
enum {
  SMW_B=1, SMW_Y=2, SMW_SELECT=4, SMW_START=8,
  SMW_UP=16, SMW_DOWN=32, SMW_LEFT=64, SMW_RIGHT=128,
  SMW_A=256, SMW_X=512, SMW_L=1024, SMW_R=2048
};
bool smw_init(const char *asset_path, const char *save_path);
bool smw_init_memory(const void *assets, size_t length, const char *save_path);
void smw_reset(void);
void smw_set_connected_pads(unsigned mask);
void smw_step(uint16_t pad1, uint16_t pad2);
void smw_draw(uint32_t *pixels);
void smw_set_widescreen(bool enabled);
unsigned smw_video_width(void);
unsigned smw_video_pitch(void);
/* Complete one non-presented frame, retaining scanline HDMA/IRQ effects. */
void smw_skip_draw(void);
/* Exactly one audio tick per game tick, including frames not presented. */
void smw_audio(int16_t *stereo, int samples);
bool smw_save(void);
const char *smw_error(void);
uint8_t smw_game_mode(void);
uint32_t smw_frame_count(void);

#endif
