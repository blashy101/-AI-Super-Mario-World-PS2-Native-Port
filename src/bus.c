/* Register and DMA bridge only; this file does not emulate a CPU. */
#include "src/common_rtl.h"
#include "src/common_cpu_infra.h"
#include "src/snes/snes.h"
#include "src/smw_spc_player.h"

static uint16_t reverse16(uint16_t x) {
  x=(x>>1&0x5555)|(x&0x5555)<<1;
  x=(x>>2&0x3333)|(x&0x3333)<<2;
  x=(x>>4&0x0f0f)|(x&0x0f0f)<<4;
  return (x>>8)|(x<<8);
}
uint8_t snes_readBBus(Snes *s,uint8_t a) {
  if(a<0x40) return ppu_read(g_ppu,a);
  if(a<0x80) return g_spc_player->port_to_snes[a&3];
  if(a==0x80) { uint8_t x=g_ram[s->ramAdr]; s->ramAdr=(s->ramAdr+1)&0x1ffff; return x; }
  return 0;
}
void snes_writeBBus(Snes *s,uint8_t a,uint8_t v) {
  if(a<0x40) { ppu_write(g_ppu,a,v); return; }
  if(a<0x80) { RtlApuWrite(0x2140+(a&3),v); return; }
  switch(a) {
  case 0x80: g_ram[s->ramAdr]=v; s->ramAdr=(s->ramAdr+1)&0x1ffff; break;
  case 0x81: s->ramAdr=(s->ramAdr&0x1ff00)|v; break;
  case 0x82: s->ramAdr=(s->ramAdr&0x100ff)|(uint32_t)v<<8; break;
  case 0x83: s->ramAdr=(s->ramAdr&0xffff)|(uint32_t)(v&1)<<16; break;
  }
}
uint8_t snes_read(Snes *s,uint32_t a) {
  uint8_t bank=a>>16; uint16_t lo=a;
  if(bank==0x7e || bank==0x7f) return g_ram[a&0x1ffff];
  if((bank&0x7f)<0x40) {
    if(lo<0x2000) return g_ram[lo];
    if(lo>=0x2100 && lo<0x2200) return snes_readBBus(s,lo);
    if(lo>=0x4300 && lo<0x4380) return dma_read(g_dma,lo);
    switch(lo) {
    case 0x4210: return 0x82;
    case 0x4211: s->inIrq=false; return 0;
    case 0x4212: return 0x40;
    case 0x4214: return s->divideResult;
    case 0x4215: return s->divideResult>>8;
    case 0x4216: return s->multiplyResult;
    case 0x4217: return s->multiplyResult>>8;
    case 0x4218: return reverse16(s->input1_currentState);
    case 0x4219: return reverse16(s->input1_currentState)>>8;
    case 0x421a: return reverse16(s->input2_currentState);
    case 0x421b: return reverse16(s->input2_currentState)>>8;
    case 0x4016: case 0x4017: return 0;
    }
  }
  if((bank&0x7f)>=0x70 && (bank&0x7f)<=0x7d && lo<0x8000)
    return g_sram[lo&(g_sram_size-1)];
  if(g_rom && (lo&0x8000)) return *RomFixedPtr(a);
  Die("Unexpected ROM bus access in native engine"); return 0;
}
void snes_write(Snes *s,uint32_t a,uint8_t v) {
  uint8_t bank=a>>16; uint16_t lo=a;
  if(bank==0x7e || bank==0x7f) { g_ram[a&0x1ffff]=v; return; }
  if((bank&0x7f)>=0x70 && (bank&0x7f)<=0x7d && lo<0x8000) {
    g_sram[lo&(g_sram_size-1)]=v; return;
  }
  if((bank&0x7f)>=0x40) return;
  if(lo<0x2000) { g_ram[lo]=v; return; }
  if(lo>=0x2100 && lo<0x2200) { snes_writeBBus(s,lo,v); return; }
  if(lo>=0x4300 && lo<0x4380) { dma_write(g_dma,lo,v); return; }
  switch(lo) {
  case 0x4200: s->vIrqEnabled=(v&0x20)!=0; s->hIrqEnabled=(v&0x10)!=0; break;
  case 0x4202: s->multiplyA=v; break;
  case 0x4203: s->multiplyResult=s->multiplyA*v; break;
  case 0x4204: s->divideA=(s->divideA&0xff00)|v; break;
  case 0x4205: s->divideA=(s->divideA&0xff)|(uint16_t)v<<8; break;
  case 0x4206: s->divideResult=v?s->divideA/v:0xffff; s->multiplyResult=v?s->divideA%v:s->divideA; break;
  case 0x4209: s->vTimer=(s->vTimer&0x100)|v; break;
  case 0x420a: s->vTimer=(s->vTimer&0xff)|(uint16_t)(v&1)<<8; break;
  case 0x420b: dma_startDma(g_dma,v,false); while(dma_cycle(g_dma)) {} break;
  case 0x420c: dma_startDma(g_dma,v,true); break;
  }
}
