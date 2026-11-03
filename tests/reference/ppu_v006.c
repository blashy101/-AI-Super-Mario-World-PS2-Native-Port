/* Independent v006 raster oracle; original notice: licenses/snesrev.txt. */
#define ppu_init v006_ppu_init
#define ppu_free v006_ppu_free
#define ppu_copy v006_ppu_copy
#define ppu_reset v006_ppu_reset
#define ppu_saveload v006_ppu_saveload
#define PpuBeginDrawing v006_PpuBeginDrawing
#define ppu_checkOverscan v006_ppu_checkOverscan
#define ppu_handleVblank v006_ppu_handleVblank
#define ppu_runLine v006_ppu_runLine
#define ppu_read v006_ppu_read
#define ppu_write v006_ppu_write
#define PpuGetCurrentRenderScale v006_PpuGetCurrentRenderScale
#include "ppu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "snes.h"
#include "snes_regs.h"
#include "ppu_decode.h"
#ifdef _EE
#include <tamtypes.h>
#include <malloc.h>
#endif


extern bool g_new_ppu;
void PpuDrawWholeLineOldPpu(Ppu *ppu, int line);
static void PpuDrawWholeLine(Ppu *ppu, uint y);

static bool ppu_evaluateSprites(Ppu* ppu, int line);
static uint16_t ppu_getVramRemap(Ppu* ppu);


Ppu* ppu_init(void) {
#ifdef _EE
  Ppu* ppu = memalign(64, sizeof(Ppu));
  if (ppu) memset(ppu, 0, sizeof(Ppu));
#else
  Ppu* ppu = calloc(1, sizeof(Ppu));
#endif
  return ppu;
}

void ppu_free(Ppu* ppu) {
  free(ppu);
}

void ppu_copy(Ppu *ppu, Ppu *ppu_src) {
  size_t pitch = ppu->renderPitch;
  uint8_t *renderBuffer = ppu->renderBuffer;
  memcpy(ppu, ppu_src, sizeof(*ppu));
  ppu->renderBuffer = renderBuffer;
  ppu->renderPitch = (uint32_t)pitch;
}

void ppu_reset(Ppu* ppu) {
  {
    size_t pitch = ppu->renderPitch;
    uint8_t *renderBuffer = ppu->renderBuffer;
    memset(ppu, 0, sizeof(*ppu));
    ppu->renderBuffer = renderBuffer;
    ppu->renderPitch = (uint32_t)pitch;
  }
  ppu->vramIncrement = 1;
}

void ppu_saveload(Ppu *ppu, SaveLoadInfo *sli) {
  assert(offsetof(Ppu, cgwsel) + 1 - offsetof(Ppu, inidisp) == PPU_SAVESTATE_REGS_SIZE);
  assert(offsetof(Ppu, vram) + 0x10000 - offsetof(Ppu, cgram) == PPU_SAVESTATE_MEM_SIZE);
  uint32_t version[2] = {'P' | 'P' << 8 | 'U' << 16 | '0' << 24, PPU_SAVESTATE_REGS_SIZE + PPU_SAVESTATE_MEM_SIZE};
  sli->func(sli, version, 8);
  sli->func(sli, &ppu->inidisp, PPU_SAVESTATE_REGS_SIZE);
  sli->func(sli, &ppu->cgram, PPU_SAVESTATE_MEM_SIZE);
  ppu->paletteValid = false;
  ppu->spriteListsValid = false;
}

void PpuBeginDrawing(Ppu *ppu, uint8_t *pixels, size_t pitch, uint32_t render_flags) {
  ppu->renderPitch = (uint)pitch;
  ppu->renderBuffer = pixels;
}

bool ppu_checkOverscan(Ppu* ppu) {
  // called at (0,225)
  ppu->frameOverscan = PPU_overscan(ppu); // set if we have a overscan-frame
  return ppu->frameOverscan;
}

void ppu_handleVblank(Ppu* ppu) {
  // called either right after ppu_checkOverscan at (0,225), or at (0,240)
  if(!PPU_forcedBlank(ppu)) {
    ppu->oamAdr = ppu->oamaddl;
    ppu->oamInHigh = ppu->oamaddh & 1;
    ppu->oamSecondWrite = false;
  }
  ppu->frameInterlace = PPU_interlace(ppu); // set if we have a interlaced frame
}

static inline void ClearBackdrop(PpuPixelPrioBufs *buf, bool wide) {
  unsigned start=wide?0:kPpuExtraLeftRight;
  unsigned length=wide?kPpuXPixels:256;
#ifdef _EE
  static const uint32_t fill[4] __attribute__((aligned(16))) = {
    0x05000500,0x05000500,0x05000500,0x05000500
  };
  CASSERT(sizeof(buf->data)%64==0);
  uint8_t *dst=(uint8_t*)(buf->data+start);
  unsigned blocks=length*sizeof(buf->data[0])/64;
  __asm__ volatile(
      "lq $8,0(%2)\n"
      "1:\n"
      "sq $8,0(%0)\n"
      "sq $8,16(%0)\n"
      "sq $8,32(%0)\n"
      "sq $8,48(%0)\n"
      "addiu %0,%0,64\n"
      "addiu %1,%1,-1\n"
      "bnez %1,1b\n"
      "nop\n"
      : "+&r"(dst), "+&r"(blocks) : "r"(fill) : "$8","memory");
#else
  for (size_t i = start; i != start+length; i += 4)
    *(uint64_t*)&buf->data[i] = 0x0500050005000500;
#endif
}

void ppu_runLine(Ppu* ppu, int line) {
  if(line == 0) {
    if (PPU_mosaicSize(ppu) != ppu->lastMosaicModulo) {
      int mod = PPU_mosaicSize(ppu);
      ppu->lastMosaicModulo = mod;
      for (int i = 0, j = 0; i < countof(ppu->mosaicModulo); i++) {
        ppu->mosaicModulo[i] = i - j;
        j = (j + 1 == mod ? 0 : j + 1);
      }
    }


    // pre-render line
    // TODO: this now happens halfway into the first line
    ppu->mosaicStartLine = 1;
    ppu->rangeOver = false;
    ppu->timeOver = false;
    ppu->evenFrame = !ppu->evenFrame;
    /* Native OAM uploads happen before this point; skip frames build no list. */
    ppu->spriteListsValid = false;
  } else {  
    // Cache the brightness computation
    if (PPU_brightness(ppu) != ppu->lastBrightnessMult) {
      uint8_t ppu_brightness = PPU_brightness(ppu);
      ppu->lastBrightnessMult = ppu_brightness;
      for (int i = 0; i < 32; i++)
        ppu->brightnessMultHalf[i * 2] = ppu->brightnessMultHalf[i * 2 + 1] = ppu->brightnessMult[i] =
        ((i << 3) | (i >> 2)) * ppu_brightness / 15;
      // Store 31 extra entries to remove the need for clamping to 31.
      memset(&ppu->brightnessMult[32], ppu->brightnessMult[31], 31);
    }

    // evaluate sprites
    ClearBackdrop(&ppu->objBuffer,ppu->wideTile!=NULL);
    ppu->lineHasSprites = !PPU_forcedBlank(ppu) && ppu_evaluateSprites(ppu, line - 1);

    if (g_new_ppu) {
      PpuDrawWholeLine(ppu, line);
    } else {
      PpuDrawWholeLineOldPpu(ppu, line);
    }
  }
}

typedef struct PpuWindows {
  int16_t edges[6];
  uint8 nr;
  uint8 bits;
} PpuWindows;

static void PpuWindows_Clear(PpuWindows *win, Ppu *ppu, uint layer) {
  win->edges[0] = -(layer != 2 || ppu->wideTile ? ppu->extraLeftCur : 0);
  win->edges[1] = 256 + (layer != 2 || ppu->wideTile ? ppu->extraRightCur : 0);
  win->nr = 1;
  win->bits = 0;
}

static void PpuWindows_Calc(PpuWindows *win, Ppu *ppu, uint layer) {
  // Evaluate which spans to render based on the window settings.
  // There are at most 5 windows.
  // Algorithm from Snes9x
  uint32_t winflags = GET_WINDOW_FLAGS(ppu, layer);
  uint nr = 1;
  int window_right = 256 + (layer != 2 || ppu->wideTile ? ppu->extraRightCur : 0);
  win->edges[0] = - (layer != 2 || ppu->wideTile ? ppu->extraLeftCur : 0);
  win->edges[1] = window_right;
  uint i, j;
  int t;
  bool w1_ena = (winflags & kWindow1Enabled) && ppu->window1left <= ppu->window1right;
  if (w1_ena) {
    if (ppu->window1left > win->edges[0]) {
      win->edges[nr] = ppu->window1left;
      win->edges[++nr] = window_right;
    }
    if (ppu->window1right + 1 < window_right) {
      win->edges[nr] = ppu->window1right + 1;
      win->edges[++nr] = window_right;
    }
  }
  bool w2_ena = (winflags & kWindow2Enabled) && ppu->window2left <= ppu->window2right;
  if (w2_ena) {
    for (i = 0; i <= nr && (t = ppu->window2left) != win->edges[i]; i++) {
      if (t < win->edges[i]) {
        for (j = nr++; j >= i; j--)
          win->edges[j + 1] = win->edges[j];
        win->edges[i] = t;
        break;
      }
    }
    for (; i <= nr && (t = ppu->window2right + 1) != win->edges[i]; i++) {
      if (t < win->edges[i]) {
        for (j = nr++; j >= i; j--)
          win->edges[j + 1] = win->edges[j];
        win->edges[i] = t;
        break;
      }
    }
  }
  win->nr = nr;
  // get a bitmap of how regions map to windows
  uint8 w1_bits = 0, w2_bits = 0;
  if (w1_ena) {
    for (i = 0; win->edges[i] != ppu->window1left; i++);
    for (j = i; win->edges[j] != ppu->window1right + 1; j++);
    w1_bits = ((1 << (j - i)) - 1) << i;
  }
  if ((winflags & (kWindow1Enabled | kWindow1Inversed)) == (kWindow1Enabled | kWindow1Inversed))
    w1_bits = ~w1_bits;
  if (w2_ena) {
    for (i = 0; win->edges[i] != ppu->window2left; i++);
    for (j = i; win->edges[j] != ppu->window2right + 1; j++);
    w2_bits = ((1 << (j - i)) - 1) << i;
  }
  if ((winflags & (kWindow2Enabled | kWindow2Inversed)) == (kWindow2Enabled | kWindow2Inversed))
    w2_bits = ~w2_bits;
  win->bits = w1_bits | w2_bits;
}

/* Decode once per tile row, then compose packed nibbles. The clipping,
   tilemap wrap, palette and per-pixel priority rules match the reference PPU. */
static void PpuDrawBackground_wide(Ppu *ppu,uint y,bool sub,uint layer,
                                    PpuZbufType zhi,PpuZbufType zlo,bool four_bpp) {
  PpuWindows win;
  if(IS_SCREEN_WINDOWED(ppu,sub,layer)) PpuWindows_Calc(&win,ppu,layer);
  else PpuWindows_Clear(&win,ppu,layer);
  unsigned base=PPU_bgTileAdr(ppu,layer);
  for(unsigned span=0;span<win.nr;span++) {
    if(win.bits&(1u<<span)) continue;
    int sx=win.edges[span], end=win.edges[span+1];
    PpuZbufType *dst=ppu->bgBuffers[sub].data+sx+kPpuExtraLeftRight;
    while(sx<end) {
      PpuWideTile sample={.run=8};
      bool valid=ppu->wideTile(ppu,layer,sx,y,&sample);
      unsigned count=UintMin(sample.run,end-sx);
      if(valid) {
        unsigned offset=sample.x&7, tile=sample.tile;
        count=UintMin(count,8-offset);
        unsigned row=sample.y&7;
        if(tile&0x8000) row=7-row;
        unsigned address=(base+(tile&1023)*(four_bpp?16:8)+row)&0x7fff;
        uint32_t bits=ppu->vram[address];
        if(four_bpp) bits|=(uint32_t)ppu->vram[(address+8)&0x7fff]<<16;
        if(bits) {
          uint32_t packed=four_bpp?PpuDecode4(bits,(tile&0x4000)!=0):PpuDecode2(bits,(tile&0x4000)!=0);
          packed>>=offset*4;
          PpuZbufType z=((tile&0x2000)?zhi:zlo)+((tile&0x1c00)>>(four_bpp?6:8));
          for(unsigned i=0;i<count;i++,packed>>=4) {
            unsigned color=packed&15;
            if(color && z>dst[i]) dst[i]=z+color;
          }
        }
      }
      assert(count>0);
      sx+=count; dst+=count;
    }
  }
}

static void PpuDrawBackground_tiles(Ppu *ppu, uint y, bool sub, uint layer,
                                    PpuZbufType zhi, PpuZbufType zlo, bool four_bpp) {
  if (!IS_SCREEN_ENABLED(ppu, sub, layer)) return;
  if(ppu->wideTile) { PpuDrawBackground_wide(ppu,y,sub,layer,zhi,zlo,four_bpp); return; }
  PpuWindows win;
  if (IS_SCREEN_WINDOWED(ppu, sub, layer)) PpuWindows_Calc(&win, ppu, layer);
  else PpuWindows_Clear(&win, ppu, layer);
  y += ppu->vScroll[layer];
  int sc = PPU_bgTilemapAdr(ppu, layer) + (((y >> 3) & 31) << 5);
  if ((y & 256) && PPU_bgTilemapHigher(ppu, layer))
    sc += PPU_bgTilemapWider(ppu, layer) ? 0x800 : 0x400;
  const uint16_t *maps[2] = {
    ppu->vram + (sc & 0x7fff),
    ppu->vram + ((sc + (PPU_bgTilemapWider(ppu, layer) ? 0x400 : 0)) & 0x7fff)
  };
  int base = PPU_bgTileAdr(ppu, layer);
  int rows[2] = {base + (y & 7), base + 7 - (y & 7)};
  for (unsigned windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1u << windex)) continue;
    unsigned x = win.edges[windex] + ppu->hScroll[layer];
    unsigned width = win.edges[windex + 1] - win.edges[windex];
    PpuZbufType *dst = ppu->bgBuffers[sub].data + win.edges[windex] + kPpuExtraLeftRight;
    const uint16_t *tileptr = maps[x >> 8 & 1] + (x >> 3 & 31);
    const uint16_t *last = maps[x >> 8 & 1] + 31;
    const uint16_t *next = maps[(x >> 8 & 1) ^ 1];
    while (width) {
      uint16_t tile = *tileptr;
      if (tileptr != last) tileptr++;
      else { tileptr = next; next = last - 31; last = tileptr + 31; }
      unsigned offset = x & 7;
      unsigned count = UintMin(8 - offset, width);
      unsigned row = rows[tile >> 15] + (tile & 1023) * (four_bpp ? 16 : 8);
      const uint16_t *planes = ppu->vram + (row & 0x7fff);
      uint32_t bits = planes[0];
      if (four_bpp) bits |= (uint32_t)planes[8] << 16;
      if (bits) {
        uint32_t packed = four_bpp ? PpuDecode4(bits, (tile & 0x4000) != 0)
                                   : PpuDecode2(bits, (tile & 0x4000) != 0);
        packed >>= offset * 4;
        PpuZbufType z = ((tile & 0x2000) ? zhi : zlo) +
                        ((tile & 0x1c00) >> (four_bpp ? 6 : 8));
#define PIXEL(i) do { unsigned color = packed >> ((i) * 4) & 15; \
  if (color && z > dst[i]) dst[i] = z + color; } while (0)
        if (count == 8) {
          PIXEL(0); PIXEL(1); PIXEL(2); PIXEL(3);
          PIXEL(4); PIXEL(5); PIXEL(6); PIXEL(7);
        } else {
          for (unsigned i = 0; i < count; i++) {
            unsigned color = packed & 15;
            if (color && z > dst[i]) dst[i] = z + color;
            packed >>= 4;
          }
        }
#undef PIXEL
      }
      dst += count; x += count; width -= count;
    }
  }
}
static void PpuDrawBackground_4bpp(Ppu *ppu, uint y, bool sub, uint layer, PpuZbufType hi, PpuZbufType lo) {
  PpuDrawBackground_tiles(ppu, y, sub, layer, hi, lo, true);
}
static void PpuDrawBackground_2bpp(Ppu *ppu, uint y, bool sub, uint layer, PpuZbufType hi, PpuZbufType lo) {
  PpuDrawBackground_tiles(ppu, y, sub, layer, hi, lo, false);
}

// Draw a whole line of a 4bpp background layer into bgBuffers, with mosaic applied
static void PpuDrawBackground_4bpp_mosaic(Ppu *ppu, uint y, bool sub, uint layer, PpuZbufType zhi, PpuZbufType zlo) {
#define GET_PIXEL() pixel = (bits) & 1 | (bits >> 7) & 2 | (bits >> 14) & 4 | (bits >> 21) & 8
#define GET_PIXEL_HFLIP() pixel = (bits >> 7) & 1 | (bits >> 14) & 2 | (bits >> 21) & 4 | (bits >> 28) & 8
#define READ_BITS(ta, tile) (addr = &ppu->vram[((ta) + (tile) * 16) & 0x7fff], addr[0] | addr[8] << 16)
  enum { kPaletteShift = 6 };
  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;  // layer is completely hidden
  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);
  y = ppu->mosaicModulo[y] + ppu->vScroll[layer];
  int sc_offs = PPU_bgTilemapAdr(ppu, layer) + (((y >> 3) & 0x1f) << 5);
  if ((y & 0x100) && PPU_bgTilemapHigher(ppu, layer))
    sc_offs += PPU_bgTilemapWider(ppu, layer) ? 0x800 : 0x400;
  const uint16_t *tps[2] = {
    &ppu->vram[sc_offs & 0x7fff],
    &ppu->vram[sc_offs + (PPU_bgTilemapWider(ppu, layer) ? 0x400 : 0) & 0x7fff]
  };
  int tileadr = PPU_bgTileAdr(ppu, layer), pixel;
  int tileadr1 = tileadr + 7 - (y & 0x7), tileadr0 = tileadr + (y & 0x7);
  const uint16_t *addr;
  for (size_t windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1 << windex))
      continue;  // layer is disabled for this window part
    int sx = win.edges[windex];
    PpuZbufType *dstz = ppu->bgBuffers[sub].data + sx + kPpuExtraLeftRight;
    PpuZbufType *dstz_end = ppu->bgBuffers[sub].data + win.edges[windex + 1] + kPpuExtraLeftRight;
    uint x = sx + ppu->hScroll[layer];
    const uint16_t *tp = tps[x >> 8 & 1] + ((x >> 3) & 0x1f);
    const uint16_t *tp_last = tps[x >> 8 & 1] + 31, *tp_next = tps[(x >> 8 & 1) ^ 1];
    x &= 7;
    int mosaic_size = PPU_mosaicSize(ppu);
    int w = mosaic_size - (sx - ppu->mosaicModulo[sx]);
    do {
      w = IntMin(w, dstz_end - dstz);
      uint32_t tile = *tp;
      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32_t bits = READ_BITS(ta, tile & 0x3ff);
      if (tile & 0x4000) bits >>= x, GET_PIXEL(); else bits <<= x, GET_PIXEL_HFLIP();
      if (pixel) {
        pixel += (tile & 0x1c00) >> kPaletteShift;
        int i = 0;
        do {
          if (z > dstz[i])
            dstz[i] = pixel + z;
        } while (++i != w);
      }
      dstz += w, x += w;
      for (; x >= 8; x -= 8)
        tp = (tp != tp_last) ? tp + 1 : tp_next;
      w = mosaic_size;
    } while (dstz_end - dstz != 0);
  }
#undef READ_BITS
#undef GET_PIXEL
#undef GET_PIXEL_HFLIP
}

// Draw a whole line of a 2bpp background layer into bgBuffers, with mosaic applied
static void PpuDrawBackground_2bpp_mosaic(Ppu *ppu, int y, bool sub, uint layer, PpuZbufType zhi, PpuZbufType zlo) {
#define GET_PIXEL() pixel = (bits) & 1 | (bits >> 7) & 2
#define GET_PIXEL_HFLIP() pixel = (bits >> 7) & 1 | (bits >> 14) & 2
#define READ_BITS(ta, tile) (addr = &ppu->vram[((ta) + (tile) * 8) & 0x7fff], addr[0])
  enum { kPaletteShift = 8 };
  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;  // layer is completely hidden
  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);
  y = ppu->mosaicModulo[y] + ppu->vScroll[layer];
  int sc_offs = PPU_bgTilemapAdr(ppu, layer) + (((y >> 3) & 0x1f) << 5);
  if ((y & 0x100) && PPU_bgTilemapHigher(ppu, layer))
    sc_offs += PPU_bgTilemapWider(ppu, layer) ? 0x800 : 0x400;
  const uint16_t *tps[2] = {
    &ppu->vram[sc_offs & 0x7fff],
    &ppu->vram[sc_offs + (PPU_bgTilemapWider(ppu, layer) ? 0x400 : 0) & 0x7fff]
  };
  int tileadr = PPU_bgTileAdr(ppu, layer), pixel;
  int tileadr1 = tileadr + 7 - (y & 0x7), tileadr0 = tileadr + (y & 0x7);
  const uint16_t *addr;
  for (size_t windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1 << windex))
      continue;  // layer is disabled for this window part
    int sx = win.edges[windex];
    PpuZbufType *dstz = ppu->bgBuffers[sub].data + sx + kPpuExtraLeftRight;
    PpuZbufType *dstz_end = ppu->bgBuffers[sub].data + win.edges[windex + 1] + kPpuExtraLeftRight;
    uint x = sx + ppu->hScroll[layer];
    const uint16_t *tp = tps[x >> 8 & 1] + ((x >> 3) & 0x1f);
    const uint16_t *tp_last = tps[x >> 8 & 1] + 31, *tp_next = tps[(x >> 8 & 1) ^ 1];
    x &= 7;
    int mosaic_size = PPU_mosaicSize(ppu);
    int w = mosaic_size - (sx - ppu->mosaicModulo[sx]);
    do {
      w = IntMin(w, dstz_end - dstz);
      uint32_t tile = *tp;
      int ta = (tile & 0x8000) ? tileadr1 : tileadr0;
      PpuZbufType z = (tile & 0x2000) ? zhi : zlo;
      uint32_t bits = READ_BITS(ta, tile & 0x3ff);
      if (tile & 0x4000) bits >>= x, GET_PIXEL(); else bits <<= x, GET_PIXEL_HFLIP();
      if (pixel) {
        pixel += (tile & 0x1c00) >> kPaletteShift;
        uint i = 0;
        do {
          if (z > dstz[i])
            dstz[i] = pixel + z;
        } while (++i != w);
      }
      dstz += w, x += w;
      for (; x >= 8; x -= 8)
        tp = (tp != tp_last) ? tp + 1 : tp_next;
      w = mosaic_size;
    } while (dstz_end - dstz != 0);
  }
#undef READ_BITS
#undef GET_PIXEL
#undef GET_PIXEL_HFLIP
}


// Assumes it's drawn on an empty backdrop
static void PpuDrawBackground_mode7(Ppu *ppu, uint y, bool sub, PpuZbufType z) {
  int layer = 0;
  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;  // layer is completely hidden
  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);

  // expand 13-bit values to signed values
  int hScroll = ((int16_t)(ppu->m7matrix[6] << 3)) >> 3;
  int vScroll = ((int16_t)(ppu->m7matrix[7] << 3)) >> 3;
  int xCenter = ((int16_t)(ppu->m7matrix[4] << 3)) >> 3;
  int yCenter = ((int16_t)(ppu->m7matrix[5] << 3)) >> 3;
  int clippedH = hScroll - xCenter;
  int clippedV = vScroll - yCenter;
  clippedH = (clippedH & 0x2000) ? (clippedH | ~1023) : (clippedH & 1023);
  clippedV = (clippedV & 0x2000) ? (clippedV | ~1023) : (clippedV & 1023);
  uint8 mosaic_enabled = PPU_mosaicEnabled(ppu, 0);
  if (mosaic_enabled)
    y = ppu->mosaicModulo[y];
  uint32_t ry = PPU_m7yFlip(ppu) ? 255 - y : y;
  uint32_t m7startX = (ppu->m7matrix[0] * clippedH & ~63) + (ppu->m7matrix[1] * ry & ~63) +
    (ppu->m7matrix[1] * clippedV & ~63) + (xCenter << 8);
  uint32_t m7startY = (ppu->m7matrix[2] * clippedH & ~63) + (ppu->m7matrix[3] * ry & ~63) +
    (ppu->m7matrix[3] * clippedV & ~63) + (yCenter << 8);
  for (size_t windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1 << windex))
      continue;  // layer is disabled for this window part
    int x = win.edges[windex], x2 = win.edges[windex + 1], tile;
    PpuZbufType *dstz = ppu->bgBuffers[sub].data + x + kPpuExtraLeftRight;
    PpuZbufType *dstz_end = ppu->bgBuffers[sub].data + x2 + kPpuExtraLeftRight;
    uint32_t rx = PPU_m7xFlip(ppu) ? 255 - x : x;
    uint32_t xpos = m7startX + ppu->m7matrix[0] * rx;
    uint32_t ypos = m7startY + ppu->m7matrix[2] * rx;
    uint32_t dx = PPU_m7xFlip(ppu) ? -ppu->m7matrix[0] : ppu->m7matrix[0];
    uint32_t dy = PPU_m7xFlip(ppu) ? -ppu->m7matrix[2] : ppu->m7matrix[2];
    uint32_t outside_value = PPU_m7largeField(ppu) ? 0x3ffff : 0xffffffff;
    bool char_fill = PPU_m7charFill(ppu);
    if (mosaic_enabled) {
      int w = PPU_mosaicSize(ppu) - (x - ppu->mosaicModulo[x]);
      do {
        w = IntMin(w, dstz_end - dstz);
        if ((uint32_t)(xpos | ypos) > outside_value) {
          if (!char_fill)
            continue;
          tile = 0;
        } else {
          tile = ppu->vram[(ypos >> 11 & 0x7f) * 128 + (xpos >> 11 & 0x7f)] & 0xff;
        }
        uint8 pixel = ppu->vram[tile * 64 + (ypos >> 8 & 7) * 8 + (xpos >> 8 & 7)] >> 8;
        if (pixel) {
          int i = 0;
          do dstz[i] = pixel + z; while (++i != w);
        }
      } while (xpos += dx * w, ypos += dy * w, dstz += w, w = PPU_mosaicSize(ppu), dstz_end - dstz != 0);
    } else {
      do {
        if ((uint32_t)(xpos | ypos) > outside_value) {
          if (!char_fill)
            continue;
          tile = 0;
        } else {
          tile = ppu->vram[(ypos >> 11 & 0x7f) * 128 + (xpos >> 11 & 0x7f)] & 0xff;
        }
        uint8 pixel = ppu->vram[tile * 64 + (ypos >> 8 & 7) * 8 + (xpos >> 8 & 7)] >> 8;
        if (pixel)
          dstz[0] = pixel + z;
      } while (xpos += dx, ypos += dy, ++dstz != dstz_end);
    }
  }
}

static void PpuDrawSprites(Ppu *ppu, uint y, uint sub, bool clear_backdrop) {
  int layer = 4;
  if (!IS_SCREEN_ENABLED(ppu, sub, layer))
    return;  // layer is completely hidden
  PpuWindows win;
  IS_SCREEN_WINDOWED(ppu, sub, layer) ? PpuWindows_Calc(&win, ppu, layer) : PpuWindows_Clear(&win, ppu, layer);
  for (size_t windex = 0; windex < win.nr; windex++) {
    if (win.bits & (1 << windex))
      continue;  // layer is disabled for this window part
    int left = win.edges[windex];
    int width = win.edges[windex + 1] - left;
    PpuZbufType *src = ppu->objBuffer.data + left + kPpuExtraLeftRight;
    PpuZbufType *dst = ppu->bgBuffers[sub].data + left + kPpuExtraLeftRight;
    if (clear_backdrop) {
      memcpy(dst, src, width * sizeof(uint16_t));
    } else {
      do {
        if (src[0] > dst[0])
          dst[0] = src[0];
      } while (src++, dst++, --width);
    }
  }
}

static void PpuDrawBackgrounds(Ppu *ppu, int y, bool sub) {
  // Top 4 bits contain the prio level, and bottom 4 bits the layer type.
  // SPRITE_PRIO_TO_PRIO can be used to convert from obj prio to this prio.
  //  15: BG3 tiles with priority 1 if bit 3 of $2105 is set
  //  14: Sprites with priority 3 (4 * sprite_prio + 2)
  //  12: BG1 tiles with priority 1
  //  11: BG2 tiles with priority 1
  //  10: Sprites with priority 2 (4 * sprite_prio + 2)
  //  8: BG1 tiles with priority 0
  //  7: BG2 tiles with priority 0
  //  6: Sprites with priority 1 (4 * sprite_prio + 2)
  //  3: BG3 tiles with priority 1 if bit 3 of $2105 is clear
  //  2: Sprites with priority 0 (4 * sprite_prio + 2)
  //  1: BG3 tiles with priority 0
  //  0: backdrop

  if (PPU_mode(ppu) == 1) {
    if (ppu->lineHasSprites)
      PpuDrawSprites(ppu, y, sub, true);

    bool mosaic_size = PPU_mosaicSize(ppu) > 1;
    if (mosaic_size && PPU_mosaicEnabled(ppu, 0))
      PpuDrawBackground_4bpp_mosaic(ppu, y, sub, 0, 0xc000, 0x8000);
    else
      PpuDrawBackground_4bpp(ppu, y, sub, 0, 0xc000, 0x8000);

    if (mosaic_size && PPU_mosaicEnabled(ppu, 1))
      PpuDrawBackground_4bpp_mosaic(ppu, y, sub, 1, 0xb100, 0x7100);
    else
      PpuDrawBackground_4bpp(ppu, y, sub, 1, 0xb100, 0x7100);

    uint bg3prio = PPU_bg3priority(ppu) ? 0xf200 : 0x3200;
    if (mosaic_size && PPU_mosaicEnabled(ppu, 2))
      PpuDrawBackground_2bpp_mosaic(ppu, y, sub, 2, bg3prio, 0x1200);
    else
      PpuDrawBackground_2bpp(ppu, y, sub, 2, bg3prio, 0x1200);
  } else {
    // mode 7
    PpuDrawBackground_mode7(ppu, y, sub, 0x5000);
    if (ppu->lineHasSprites)
      PpuDrawSprites(ppu, y, sub, false);
  }
}

static void PpuPreparePalette(Ppu *ppu) {
  unsigned brightness = PPU_brightness(ppu);
  if (ppu->paletteValid && ppu->paletteBrightness == brightness) return;
  ppu->backdropMathValid = 0;
  for (unsigned i = 0; i < 256; i++) {
    unsigned color = ppu->cgram[i];
    ppu->paletteRgb[i] = PpuPackRgb(ppu->brightnessMult[color & 31],
        ppu->brightnessMult[color >> 5 & 31], ppu->brightnessMult[color >> 10 & 31]);
  }
  ppu->paletteBrightness = brightness; ppu->paletteValid = true;
}

static inline uint32_t PpuBlendRgb(Ppu *ppu, unsigned a, unsigned b, bool half, bool subtract) {
  unsigned r = a & 31, g = a >> 5 & 31, blue = a >> 10 & 31;
  unsigned r2 = b & 31, g2 = b >> 5 & 31, b2 = b >> 10 & 31;
  if (subtract) {
    r = r >= r2 ? r - r2 : 0;
    g = g >= g2 ? g - g2 : 0;
    blue = blue >= b2 ? blue - b2 : 0;
  } else { r += r2; g += g2; blue += b2; }
  const uint8_t *map = half ? ppu->brightnessMultHalf : ppu->brightnessMult;
  return PpuPackRgb(map[r], map[g], map[blue]);
}

/* SMW commonly puts BG2 on the subscreen and blends it with the main
   backdrop. In that case the first operand is constant for the whole span.
   Cache the 255 possible subscreen colours; index zero uses fixed colour and
   deliberately bypasses halving, so is evaluated separately per span. */
static const uint32_t *PpuPrepareBackdropPalette(Ppu *ppu, bool unclipped) {
  unsigned control = ppu->cgadsub & 0xc0;
  if (ppu->backdropMathControl != control) {
    ppu->backdropMathControl = control;
    ppu->backdropMathValid = 0;
  }
  uint32_t *colors = ppu->backdropMathRgb[unclipped];
  if (!(ppu->backdropMathValid & (1u << unclipped))) {
    unsigned main = unclipped ? ppu->cgram[0] : 0;
    for (unsigned i = 1; i < 256; i++)
      colors[i] = PpuBlendRgb(ppu, main, ppu->cgram[i], control & 0x40, control & 0x80);
    ppu->backdropMathValid |= 1u << unclipped;
  }
  colors[0] = PpuBlendRgb(ppu, unclipped ? ppu->cgram[0] : 0, ppu->fixedColor,
                          false, control & 0x80);
  return colors;
}

static NOINLINE void PpuDrawWholeLine(Ppu *ppu, uint y) {
  if (PPU_forcedBlank(ppu)) {
    uint8 *dst = &ppu->renderBuffer[(y - 1) * ppu->renderPitch];
    size_t n = sizeof(uint32_t) * (256 + ppu->extraLeftRight * 2);
    memset(dst, 0, n);
    return;
  }

  PpuPreparePalette(ppu);

  // Default background is backdrop
  ClearBackdrop(&ppu->bgBuffers[0],ppu->wideTile!=NULL);

  // Render main screen
  PpuDrawBackgrounds(ppu, y, false);

  // Render also the subscreen?
  bool rendered_subscreen = false;
  if (PPU_preventMathMode(ppu) != 3 && PPU_addSubscreen(ppu) && PPU_mathEnabled(ppu)) {
    ClearBackdrop(&ppu->bgBuffers[1],ppu->wideTile!=NULL);
    if (ppu->screenEnabled[1] != 0) {
      PpuDrawBackgrounds(ppu, y, true);
      rendered_subscreen = true;
    }
  }

  // Color window affects the drawing mode in each region
  PpuWindows cwin;
  PpuWindows_Calc(&cwin, ppu, 5);
  static const uint8 kCwBitsMod[8] = {
    0x00, 0xff, 0xff, 0x00,
    0xff, 0x00, 0xff, 0x00,
  };
  uint32_t cw_clip_math = ((cwin.bits & kCwBitsMod[PPU_clipMode(ppu)]) ^ kCwBitsMod[PPU_clipMode(ppu) + 4]) |
    ((cwin.bits & kCwBitsMod[PPU_preventMathMode(ppu)]) ^ kCwBitsMod[PPU_preventMathMode(ppu) + 4]) << 8;

  uint32_t *dst = (uint32_t*)&ppu->renderBuffer[(y - 1) * ppu->renderPitch], *dst_org = dst;

  if(!ppu->wideTile) dst += ppu->extraLeftRight;

  uint32_t windex = 0;
  do {
    uint32_t left = cwin.edges[windex] + kPpuExtraLeftRight, right = cwin.edges[windex + 1] + kPpuExtraLeftRight;
    // If clip is set, then zero out the rgb values from the main screen.
    uint32_t clip_color_mask = (cw_clip_math & 1) ? 0x1f : 0;
    uint32_t math_enabled_cur = PPU_mathEnabled(ppu) & ((cw_clip_math & 0x100) ? -1 : 0);
    uint32_t fixed_color = ppu->fixedColor;
    if (math_enabled_cur == 0 || fixed_color == 0 && !PPU_halfColor(ppu) && !rendered_subscreen) {
      // Math is disabled (or has no effect), so can avoid the per-pixel maths check
      uint32_t i = left;
      do {
        dst[0] = clip_color_mask ? ppu->paletteRgb[ppu->bgBuffers[0].data[i] & 0xff]
                                 : PpuPackRgb(0, 0, 0);
      } while (dst++, ++i < right);
    } else if (math_enabled_cur == 0x20 && PPU_addSubscreen(ppu)) {
      const uint32_t *blended = PpuPrepareBackdropPalette(ppu, clip_color_mask != 0);
      const uint32_t *plain = ppu->paletteRgb;
      const PpuZbufType *main = ppu->bgBuffers[0].data;
      const PpuZbufType *sub = ppu->bgBuffers[1].data;
      uint32_t black = PpuPackRgb(0, 0, 0);
      for (unsigned i = left; i < right; i++, dst++) {
        unsigned pixel = main[i];
        dst[0] = (pixel & 0xf00) == 0x500 ? blended[sub[i] & 255] :
                 clip_color_mask ? plain[pixel & 255] : black;
      }
    } else {
      uint8 *half_color_map = PPU_halfColor(ppu) ? ppu->brightnessMultHalf : ppu->brightnessMult;
      // Store this in locals
      math_enabled_cur |= PPU_addSubscreen(ppu) << 8 | PPU_subtractColor(ppu) << 9;
      // Need to check for each pixel whether to use math or not based on the main screen layer.
      uint32_t i = left;
      do {
        uint16_t pixel_info=ppu->bgBuffers[0].data[i];
        uint8 main_layer = (pixel_info >> 8) & 0xf;
        if (!(math_enabled_cur & (1u << main_layer))) {
          dst[0]=clip_color_mask?ppu->paletteRgb[pixel_info&255]:PpuPackRgb(0,0,0);
          continue;
        }
        uint32_t color = ppu->cgram[pixel_info & 0xff], color2;
        uint32_t r = color & clip_color_mask;
        uint32_t g = (color >> 5) & clip_color_mask;
        uint32_t b = (color >> 10) & clip_color_mask;
        uint8 *color_map = ppu->brightnessMult;
        if (math_enabled_cur & (1 << main_layer)) {
          if (math_enabled_cur & 0x100) {  // addSubscreen ?
            if ((ppu->bgBuffers[1].data[i] & 0xff) != 0)
              color2 = ppu->cgram[ppu->bgBuffers[1].data[i] & 0xff], color_map = half_color_map;
            else  // Don't halve if PPU_addSubscreen(ppu) && backdrop
              color2 = fixed_color;
          } else {
            color2 = fixed_color, color_map = half_color_map;
          }
          uint32_t r2 = (color2 & 0x1f), g2 = ((color2 >> 5) & 0x1f), b2 = ((color2 >> 10) & 0x1f);
          if (math_enabled_cur & 0x200) {  // subtractColor?
            r = (r >= r2) ? r - r2 : 0;
            g = (g >= g2) ? g - g2 : 0;
            b = (b >= b2) ? b - b2 : 0;
          } else {
            r += r2;
            g += g2;
            b += b2;
          }
        }
        dst[0] = PpuPackRgb(color_map[r], color_map[g], color_map[b]);
      } while (dst++, ++i < right);
    }
  } while (cw_clip_math >>= 1, ++windex < cwin.nr);

}

static void PpuPrepareSpriteLists(Ppu *ppu) {
  static const uint8 spriteSizes[8][2] = {
    {8, 16}, {8, 32}, {8, 64}, {16, 32},
    {16, 64}, {32, 64}, {16, 32}, {16, 32}
  };
  for (unsigned line = 0; line < 256; line++) ppu->spriteLines[line].count = 0;
  uint8_t index = PPU_objPriority(ppu) ? (ppu->oamaddl & 0xfe) : 0;
  for (unsigned i = 0; i < 128; i++, index += 2) {
    PpuSprite *s = &ppu->spriteCache[index >> 1];
    unsigned high = ppu->highOam[index >> 3] >> (index & 7);
    s->x = (ppu->oam[index] & 255) - ((high & 1) << 8);
    if(ppu->wideTile && ppu->wideOamValid[index>>1]) s->x=ppu->wideOamX[index>>1];
    s->y = ppu->oam[index] >> 8;
    s->size = spriteSizes[PPU_objSize(ppu)][high >> 1 & 1];
    s->attributes = ppu->oam[index + 1];
    s->tileAddress = (s->attributes & 0x100) ? PPU_objTileAdr2(ppu) : PPU_objTileAdr1(ppu);
    s->z = 0x80 + ((s->attributes & 0xe00) >> 5) +
      (SPRITE_PRIO_TO_PRIO((s->attributes & 0x3000) >> 12, (s->attributes & 0x800) == 0) << 8);
    if (s->x <= -(int)s->size-(ppu->wideTile?ppu->extraLeftCur:0)) continue;
    if(ppu->wideTile && s->x>=256+ppu->extraRightCur) continue;
    unsigned height = PPU_objInterlace(ppu) ? s->size / 2 : s->size;
    for (unsigned row = 0; row < height; row++) {
      PpuSpriteLine *list = &ppu->spriteLines[(s->y + row) & 255];
      if (list->count < (ppu->wideTile?128:33)) list->sprite[list->count++] = index >> 1;
    }
  }
  ppu->spriteListsValid = true;
}

static bool ppu_evaluateSprites(Ppu* ppu, int line) {
  if (!ppu->spriteListsValid) PpuPrepareSpriteLists(ppu);
  const PpuSpriteLine *list = &ppu->spriteLines[line];
  int spritesFound = 0;
  int tilesFound = 0;
  for(unsigned i = 0; i < list->count; i++) {
    const PpuSprite *s = &ppu->spriteCache[list->sprite[i]];
    uint8_t row = line - s->y;
    int spriteSize = s->size;
    int x = s->x;
    // break if we found 32 sprites already
    spritesFound++;
    if(spritesFound > 32 && !ppu->wideTile) {
      ppu->rangeOver = true;
      break;
    }
    // update row according to obj-interlace
    if(PPU_objInterlace(ppu)) row = row * 2 + (ppu->evenFrame ? 0 : 1);
    // get some data for the sprite and y-flip row if needed
    int oam1 = s->attributes;
    int objAdr = s->tileAddress;
    if(oam1 & 0x8000) row = spriteSize - 1 - row;
    // fetch all tiles in x-range
    PpuZbufType z = s->z;

    for(int col = 0; col < spriteSize; col += 8) {
      if(col+x>-8-(ppu->wideTile?ppu->extraLeftCur:0) && col+x<256+(ppu->wideTile?ppu->extraRightCur:0)) {
        // break if we found 34 8*1 slivers already
        tilesFound++;
        if(tilesFound > 34 && !ppu->wideTile) {
          ppu->timeOver = true;
          break;
        }
        // figure out which tile this uses, looping within 16x16 pages, and get it's data
        int usedCol = oam1 & 0x4000 ? spriteSize - 1 - col : col;
        int usedTile = ((((oam1 & 0xff) >> 4) + (row >> 3)) << 4) | (((oam1 & 0xf) + (usedCol >> 3)) & 0xf);
        uint16_t *addr = &ppu->vram[(objAdr + usedTile * 16 + (row & 0x7)) & 0x7fff];
        uint32_t plane = addr[0] | (uint32_t)addr[8] << 16;
        uint32_t packed_pixels = PpuDecode4(plane, (oam1 & 0x4000) != 0);
        // go over each pixel
        int px_left = IntMax(-(col+x+(ppu->wideTile?ppu->extraLeftCur:0)),0);
        int px_right = IntMin(256+(ppu->wideTile?ppu->extraRightCur:0)-(col+x),8);
        PpuZbufType *dst = ppu->objBuffer.data + col + x + px_left + kPpuExtraLeftRight;

        for (int px = px_left; px < px_right; px++, dst++) {
          int pixel = packed_pixels >> (px * 4) & 15;
          // draw it in the buffer if there is a pixel here, and the buffer there is still empty
          if (pixel != 0 && (dst[0] & 0xff) == 0)
            dst[0] = z + pixel;
        }

      }
    }
    if(tilesFound > 34 && !ppu->wideTile) break;
  }
  return tilesFound != 0;
}

static uint16_t ppu_getVramRemap(Ppu* ppu) {
  uint16_t adr = ppu->vramPointer;
  switch(ppu->vramRemapMode) {
    case 0: return adr;
    case 1: return (adr & 0xff00) | ((adr & 0xe0) >> 5) | ((adr & 0x1f) << 3);
    case 2: return (adr & 0xfe00) | ((adr & 0x1c0) >> 6) | ((adr & 0x3f) << 3);
    case 3: return (adr & 0xfc00) | ((adr & 0x380) >> 7) | ((adr & 0x7f) << 3);
  }
  return adr;
}

uint8_t ppu_read(Ppu* ppu, uint8_t adr) {
  switch(adr) {
  case 0x34:
  case 0x35:
  case 0x36: {
    int result = ppu->m7matrix[0] * (ppu->m7matrix[1] >> 8);
    return (result >> (8 * (adr - 0x34))) & 0xff;
  }
    case 0x37: {
      //assert(0);
      return 0;
    }
    case 0x38: {
      uint8_t ret = 0;
      if(ppu->oamInHigh) {
        ret = ppu->highOam[((ppu->oamAdr & 0xf) << 1) | ppu->oamSecondWrite];
        if(ppu->oamSecondWrite) {
          ppu->oamAdr++;
          if(ppu->oamAdr == 0) ppu->oamInHigh = false;
        }
      } else {
        if(!ppu->oamSecondWrite) {
          ret = ppu->oam[ppu->oamAdr] & 0xff;
        } else {
          ret = ppu->oam[ppu->oamAdr++] >> 8;
          if(ppu->oamAdr == 0) ppu->oamInHigh = true;
        }
      }
      ppu->oamSecondWrite = !ppu->oamSecondWrite;
      return ret;
    }
    case 0x39: {
      uint16_t val = ppu->vramReadBuffer;
      if(!ppu->vramIncrementOnHigh) {
        ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
        ppu->vramPointer += ppu->vramIncrement;
      }
      return val & 0xff;
    }
    case 0x3a: {
      uint16_t val = ppu->vramReadBuffer;
      if(ppu->vramIncrementOnHigh) {
        ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
        ppu->vramPointer += ppu->vramIncrement;
      }
      return val >> 8;
    }
    case 0x3b: {
      uint8_t ret = 0;
      if(!ppu->cgramSecondWrite) {
        ret = ppu->cgram[ppu->cgramPointer] & 0xff;
      } else {
        ret = ((ppu->cgram[ppu->cgramPointer++] >> 8) & 0x7f);
      }
      ppu->cgramSecondWrite = !ppu->cgramSecondWrite;
      return ret;
    }
    case 0x3c: {
      uint8_t val = 0;
      if(ppu->hCountSecond) {
        val = ((ppu->hCount >> 8) & 1);
      } else {
        val = ppu->hCount & 0xff;
      }
      ppu->hCountSecond = !ppu->hCountSecond;
      return val;
    }
    case 0x3d: {
      uint8_t val = 0;
      if(ppu->vCountSecond) {
        val = ((ppu->vCount >> 8) & 1);
      } else {
        val = ppu->vCount & 0xff;
      }
      ppu->vCountSecond = !ppu->vCountSecond;
      return val;
    }
    case 0x3e: {
      uint8_t val = 0x1; // ppu1 version (4 bit)
      val |= ppu->rangeOver << 6;
      val |= ppu->timeOver << 7;
      return val;
    }
    case 0x3f: {
      uint8_t val = 0x3; // ppu2 version (4 bit), bit 4: ntsc/pal
      val |= ppu->countersLatched << 6;
      val |= ppu->evenFrame << 7;
      ppu->countersLatched = false; // TODO: only when ppulatch is set
      ppu->hCountSecond = false;
      ppu->vCountSecond = false;
      return val;
    }
    default: {
      assert(0);
      return 0;
    }
  }
}

void ppu_write(Ppu* ppu, uint8_t adr, uint8_t val) {
//  if (adr != 24 && adr != 25)
//    printf("ppu_write(%d, %d)\n", adr, val);
  switch(adr) {
    case INIDISP & 0xff:
      ppu->inidisp = val;
      break;
    case OBSEL & 0xff:
      ppu->spriteListsValid = false;
      ppu->obsel = val;
      break;
    case OAMADDL & 0xff:
      ppu->spriteListsValid = false;
      ppu->oamaddl = val;
      ppu->oamAdr = val;
      ppu->oamInHigh = ppu->oamaddh & 1;
      ppu->oamSecondWrite = false;
      break;
    case OAMADDH & 0xff:
      ppu->spriteListsValid = false;
      ppu->oamaddh = val;
      ppu->oamInHigh = val & 1;
      ppu->oamAdr = ppu->oamaddl;
      ppu->oamSecondWrite = false;
      break;
    case 0x04: {
      ppu->spriteListsValid = false;
      if(ppu->oamInHigh) {
        ppu->highOam[((ppu->oamAdr & 0xf) << 1) | ppu->oamSecondWrite] = val;
        if(ppu->oamSecondWrite) {
          ppu->oamAdr++;
          if(ppu->oamAdr == 0) ppu->oamInHigh = false;
        }
      } else {
        if(!ppu->oamSecondWrite) {
          ppu->oamBuffer = val;
        } else {
          ppu->oam[ppu->oamAdr++] = (val << 8) | ppu->oamBuffer;
          if(ppu->oamAdr == 0) ppu->oamInHigh = true;
        }
      }
      ppu->oamSecondWrite = !ppu->oamSecondWrite;
      break;
    }
    case BGMODE & 0xff:
      assert((val & 0xf0) == 0);
      ppu->bgmode = val;
      break;
    case MOSAIC & 0xff:
      ppu->mosaic = val;
      ppu->mosaicStartLine = 0;// ppu->snes->vPos;
      break;
    case BG1SC & 0xff:
    case BG2SC & 0xff:
    case BG3SC & 0xff:
    case BG4SC & 0xff:
      ppu->bgXsc[adr - 7] = val;
      break;
    case BG12NBA & 0xff:
      ppu->bgTileAdr = ppu->bgTileAdr & 0xff00 | val;
      break;
    case BG34NBA & 0xff:
      ppu->bgTileAdr = ppu->bgTileAdr & 0xff | val << 8;
      break;
    case 0x0d: {
      ppu->m7matrix[6] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      // fallthrough to normal layer BG-HOFS
    }
    case 0x0f:
    case 0x11:
    case 0x13: {
      ppu->hScroll[(adr - 0xd) / 2] = ((val << 8) | (ppu->scrollPrev & 0xf8) | (ppu->scrollPrev2 & 0x7)) & 0x3ff;
      ppu->scrollPrev = val;
      ppu->scrollPrev2 = val;
      break;
    }
    case 0x0e: {
      ppu->m7matrix[7] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      // fallthrough to normal layer BG-VOFS
    }
    case 0x10:
    case 0x12:
    case 0x14: {
      ppu->vScroll[(adr - 0xe) / 2] = ((val << 8) | ppu->scrollPrev) & 0x3ff;
      ppu->scrollPrev = val;
      break;
    }
    case 0x15: {
      if((val & 3) == 0) {
        ppu->vramIncrement = 1;
      } else if((val & 3) == 1) {
        ppu->vramIncrement = 32;
      } else {
        ppu->vramIncrement = 128;
      }
      ppu->vramRemapMode = (val & 0xc) >> 2;
      ppu->vramIncrementOnHigh = val & 0x80;
      break;
    }
    case 0x16: {
      ppu->vramPointer = (ppu->vramPointer & 0xff00) | val;
      ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
      break;
    }
    case 0x17: {
      ppu->vramPointer = (ppu->vramPointer & 0x00ff) | (val << 8);
      ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
      break;
    }
    case 0x18: {
      // TODO: vram access during rendering (also cgram and oam)
      uint16_t vramAdr = ppu_getVramRemap(ppu);
      ppu->vram[vramAdr & 0x7fff] = (ppu->vram[vramAdr & 0x7fff] & 0xff00) | val;
      if(!ppu->vramIncrementOnHigh) ppu->vramPointer += ppu->vramIncrement;
      break;
    }
    case 0x19: {
      uint16_t vramAdr = ppu_getVramRemap(ppu);
      ppu->vram[vramAdr & 0x7fff] = (ppu->vram[vramAdr & 0x7fff] & 0x00ff) | (val << 8);
      if(ppu->vramIncrementOnHigh) ppu->vramPointer += ppu->vramIncrement;
      break;
    }
    case M7SEL & 0xff:
      ppu->m7sel = val;
      break;
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
      ppu->m7matrix[adr - 0x1b] = (val << 8) | ppu->m7prev;
      ppu->m7prev = val;
      break;
    case 0x1f:
    case 0x20:
      ppu->m7matrix[adr - 0x1b] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      break;
    case 0x21:
      ppu->cgramPointer = val;
      ppu->cgramSecondWrite = false;
      break;
    case 0x22:
      if(!ppu->cgramSecondWrite) {
        ppu->cgramBuffer = val;
      } else {
        ppu->cgram[ppu->cgramPointer++] = (val << 8) | ppu->cgramBuffer;
        ppu->paletteValid=false;
      }
      ppu->cgramSecondWrite = !ppu->cgramSecondWrite;
      break;
    case 0x23:
      ppu->windowsel = (ppu->windowsel & ~0xff) | val;
      break;
    case 0x24:
      ppu->windowsel = (ppu->windowsel & ~0xff00) | (val << 8);
      break;
    case 0x25:
      ppu->windowsel = (ppu->windowsel & ~0xff0000) | (val << 16);
      break;
    case 0x26:
      ppu->window1left = val;
      break;
    case 0x27:
      ppu->window1right = val;
      break;
    case 0x28:
      ppu->window2left = val;
      break;
    case 0x29:
      ppu->window2right = val;
      break;
    case WBGLOG & 0xff:
      ppu->wbgobjlog = ppu->wbgobjlog & 0xff00 | val;
      break;
    case WOBJLOG & 0xff:
      ppu->wbgobjlog = ppu->wbgobjlog & 0xff | val << 8;
      break;
    case TM & 0xff:
      ppu->screenEnabled[0] = val;
      break;
    case TS & 0xff:
      ppu->screenEnabled[1] = val;
      break;
    case TMW & 0xff:
      ppu->screenWindowed[0] = val;
      break;
    case TSW & 0xff:
      ppu->screenWindowed[1] = val;
      break;
    case CGWSEL & 0xff:
      ppu->cgwsel = val;
      break;
    case CGADSUB & 0xff:
      ppu->cgadsub = val;
      break;
    case COLDATA & 0xff:
      if (val & 0x80) ppu->fixedColor = (ppu->fixedColor & ~(0x1f << 10)) | (val & 0x1f) << 10; // blue
      if (val & 0x40) ppu->fixedColor = (ppu->fixedColor & ~(0x1f <<  5)) | (val & 0x1f) << 5;  // green
      if (val & 0x20) ppu->fixedColor = (ppu->fixedColor & ~(0x1f <<  0)) | (val & 0x1f) << 0;  // red
      break;
    case SETINI & 0xff:
      ppu->spriteListsValid = false;
      ppu->setini = val;
      break;
    default:
      break;
  }
}

int PpuGetCurrentRenderScale(Ppu *ppu, uint32_t render_flags) {
  return 1;
}
