/* Adapted from snesrev/smw common_rtl.c; see licenses/snesrev.txt. */
#include "src/smw_rtl.h"
#include "src/common_cpu_infra.h"
#include "src/snes/snes.h"
void MemCpy(void *dst, const void *src, int size) {
  memcpy(dst, src, size);
}

void mov24(struct LongPtr *a, uint32 d) {
  a->addr = d & 0xffff;
  a->bank = d >> 16;
}

uint32 Load24(LongPtr src) {
  return (uint32)src.addr | (uint32)src.bank << 16;
}

bool Unreachable(void) {
  printf("Unreachable!\n");
  assert(0);
  g_ram[0x1ffff] = 1;
  return false;
}

uint8 *RomPtr(uint32_t addr) {
  if (!(addr & 0x8000) || addr >= 0x7e0000) {
    printf("RomPtr - Invalid access 0x%x!\n", addr);
    if (!g_fail) {
      g_fail = true;
    }
  }
  return (uint8 *)&g_rom[(((addr >> 16) << 15) | (addr & 0x7fff)) & 0x3fffff];
}

void WriteReg(uint16 reg, uint8 value) {
  snes_write(g_snes, reg, value);
}

uint16 Mult8x8(uint8 a, uint8 b) {
  return a * b;
}

uint16 SnesDivide(uint16 a, uint8 b) {
  return (b == 0) ? 0xffff : a / b;
}

uint16 SnesModulus(uint16 a, uint8 b) {
  return (b == 0) ? a : a % b;
}


PointU16 *get_PointU16(PointU16 *base, uint8 off) {
  return (PointU16 *)((uint8 *)base + off);
}


uint8 ReadReg(uint16 reg) {
  return snes_read(g_snes, reg);
}

uint16 ReadRegWord(uint16 reg) {
  uint16_t rv = ReadReg(reg);
  rv |= ReadReg(reg + 1) << 8;
  return rv;
}

void WriteRegWord(uint16 reg, uint16 value) {
  WriteReg(reg, (uint8)value);
  WriteReg(reg + 1, value >> 8);
}

uint8 *IndirPtr(LongPtr ptr, uint16 offs) {
  uint32 a = Load24(ptr) + offs;
  if ((a >> 16) >= 0x7e && (a >> 16) <= 0x7f || (a & 0xffff) < 0x2000) {
    return &g_ram[a & 0x1ffff];
  } else {
    return RomPtr(a);
  }
}

void IndirWriteByte(LongPtr ptr, uint16 offs, uint8 value) {
  uint8 *p = IndirPtr(ptr, offs);
  p[0] = value;
}

void SetHiLo(uint8 *hi, uint8 *lo, uint16 v) {
  *hi = v >> 8;
  *lo = v;
}

void AddHiLo(uint8 *hi, uint8 *lo, uint16 v) {
  SetHiLo(hi, lo, PAIR16(*hi, *lo) + v);
}


void SmwCopyToVram(uint16 vram_addr, const uint8 *src, int n) {
  for (size_t i = 0; i < (n >> 1); i++)
    g_ppu->vram[vram_addr + i] = WORD(src[i * 2]);
}

void SmwCopyToVramPitch32(uint16 vram_addr, const uint8 *src, int n) {
  for (size_t i = 0; i < (n >> 1); i++)
    g_ppu->vram[vram_addr + i * 32] = WORD(src[i * 2]);
}

void SmwCopyToVramLow(uint16 vram_addr, const uint8 *src, int n) {
  for (size_t i = 0; i < n; i++)
    g_ppu->vram[vram_addr + i] = (g_ppu->vram[vram_addr + i] & 0xff00) | src[i];
}

void SmwCopyFromVram(uint16 vram_addr, uint8 *dst, int n) {
  for (size_t i = 0; i < (n >> 1); i++)
    WORD(dst[i * 2]) = g_ppu->vram[vram_addr + i];
}


void RtlUpdatePalette(const uint16 *src, int dst, int n) {
  g_ppu->paletteValid=false;
  for(int i = 0; i < n; i++)
    g_ppu->cgram[dst + i] = src[i];
}

void SmwClearVram(uint16 vram_addr, uint16 value, int n) {
  for (int i = 0; i < n; i++)
    g_ppu->vram[vram_addr + i] = value;
}

uint16 *RtlGetVramAddr() {
  return g_ppu->vram;
}

void RtlPpuWrite(uint16 addr, uint8 value) {
  assert((addr & 0xff00) == 0x2100);
  ppu_write(g_ppu, addr, value);
}

void RtlPpuWriteTwice(uint16 addr, uint16 value) {
  RtlPpuWrite(addr, value);
  RtlPpuWrite(addr, value >> 8);
}

void RtlHdmaSetup(uint8 which, uint8 transfer_unit, uint8 reg, uint32 addr, uint8 indirect_bank) {
  Dma *dma = g_dma;
  dma_write(dma, DMAP0 + which * 16, transfer_unit);
  dma_write(dma, BBAD0 + which * 16, reg);
  dma_write(dma, A1T0L + which * 16, addr);
  dma_write(dma, A1T0H + which * 16, addr >> 8);
  dma_write(dma, A1B0 + which * 16, addr >> 16);
  dma_write(dma, DAS00 + which * 16, indirect_bank);
}

void RtlEnableVirq(int line) {
  g_snes->vIrqEnabled = line >= 0;
  if (line >= 0)
    g_snes->vTimer = line;
}

static const uint8 kSetupHDMAWindowingEffects_DATA_00927C[] = { 0xF0,0xA0,   4,0xF0,0x80,   5,   0 };
static const uint8 *SimpleHdma_GetPtr(uint32 p) {
  if (game_id == kGameID_SMW) {
    switch (p) {
    case 0x927c: return kSetupHDMAWindowingEffects_DATA_00927C;
    }
    if (p < 0x2000)
      return g_ram + p;
  }
  printf("SimpleHdma_GetPtr: bad addr 0x%x\n", p);
  return NULL;
}

void SimpleHdma_Init(SimpleHdma *c, DmaChannel *dc) {
  if (!dc->hdmaActive) {
    c->table = 0;
    return;
  }
  c->table = SimpleHdma_GetPtr(dc->aAdr | dc->aBank << 16);
  c->rep_count = 0;
  c->mode = dc->mode | dc->indirect << 6;
  c->ppu_addr = dc->bAdr;
  c->indir_bank = dc->indBank;
}

void SimpleHdma_DoLine(SimpleHdma *c) {
  static const uint8 bAdrOffsets[8][4] = {
    {0, 0, 0, 0},
    {0, 1, 0, 1},
    {0, 0, 0, 0},
    {0, 0, 1, 1},
    {0, 1, 2, 3},
    {0, 1, 0, 1},
    {0, 0, 0, 0},
    {0, 0, 1, 1}
  };
  static const uint8 transferLength[8] = {
    1, 2, 2, 4, 4, 4, 2, 4
  };

  if (c->table == NULL)
    return;
  bool do_transfer = false;
  if ((c->rep_count & 0x7f) == 0) {
    c->rep_count = *c->table++;
    if (c->rep_count == 0) {
      c->table = NULL;
      return;
    }
    if(c->mode & 0x40) {
      c->indir_ptr = SimpleHdma_GetPtr(c->indir_bank << 16 | c->table[0] | c->table[1] * 256);
      c->table += 2;
    }
    do_transfer = true;
  }
  if(do_transfer || c->rep_count & 0x80) {
    for(int j = 0, j_end = transferLength[c->mode & 7]; j < j_end; j++) {
      uint8 v = c->mode & 0x40 ? *c->indir_ptr++ : *c->table++;
      RtlPpuWrite(0x2100 + c->ppu_addr + bAdrOffsets[c->mode & 7][j], v);
    }
  }
  c->rep_count--;
}
