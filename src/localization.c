/* Native presentation branches from original SNES Japanese SMWDisX routines.
 * Shared gameplay, secondary entrances, save format and audio remain unchanged.
 * See tools/prepare_japanese.py and docs/SOURCES.md for source provenance. */
#include "localization.h"
#include "super_system.h"
#include "src/smw_rtl.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "assets/smw_assets.h"
#include <string.h>
#include <stdlib.h>

enum { AUX=178, TABLES=187 };
static const uint8_t *jp[TABLES],*english[kNumberOfAssets];
static uint32_t jp_size[TABLES],english_size[kNumberOfAssets];
static uint8_t *owned;
static bool selected,bound;
static const unsigned slots[]={0,3,4,32,33,39,41,60,85,178,179,180,181,182,183,184,185,186};
static uint32_t rd32(const uint8_t *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint16_t rd16(const uint8_t *p) { return p[0]|p[1]<<8; }
static void wr16(unsigned p,unsigned v) { stripe_image_upload_data[p]=v; stripe_image_upload_data[p+1]=v>>8; }
static uint16_t table(unsigned id,unsigned index) {
  return index*2+1<jp_size[id]?rd16(jp[id]+index*2):0;
}
static void apply(void) {
  if(!bound) return;
  for(unsigned i=0;i<sizeof(slots)/sizeof(*slots);i++) if(slots[i]<AUX) {
    unsigned id=slots[i];
    g_asset_ptrs[id]=selected?jp[id]:english[id];
    g_asset_sizes[id]=selected?jp_size[id]:english_size[id];
  }
  SmwArcade_ApplyAssets();
}
bool SmwLocale_Load(const void *data,size_t length) {
  if(!data || length<12 || length>512*1024 || memcmp(data,"SMWJP001",8)) return false;
  const uint8_t *bytes=data; const unsigned count=sizeof(slots)/sizeof(*slots);
  if(rd32(bytes+8)!=count) return false;
  size_t p=12,offsets[TABLES]={0}; uint32_t sizes[TABLES]={0};
  static const uint32_t aux_size[]={8,50,38,28,22,4,10,16,16};
  for(unsigned n=0;n<count;n++) {
    if(p>length-8) return false;
    unsigned id=rd32(bytes+p),size=rd32(bytes+p+4); p+=8;
    if(id!=slots[n] || !size || size>length-p) return false;
    if(id>=AUX && size!=aux_size[id-AUX]) return false;
    if((id==33 && size!=404) || (id==41 && size!=512) || (id==60 && size!=176) || (id==85 && size!=283)) return false;
    offsets[id]=p; sizes[id]=size; p=(p+size+3)&~(size_t)3;
  }
  if(p!=length) return false;
  uint8_t *copy=malloc(length); if(!copy) return false;
  memcpy(copy,data,length);
  for(unsigned n=0;n<count;n++) { unsigned id=slots[n]; jp[id]=copy+offsets[id]; jp_size[id]=sizes[id]; }
  free(owned); owned=copy; apply(); return true;
}
void SmwLocale_BindEnglish(void) {
  memcpy(english,g_asset_ptrs,sizeof(english)); memcpy(english_size,g_asset_sizes,sizeof(english_size));
  bound=true; apply();
}
bool SmwLocale_Select(bool value) {
  if(value && (!owned || SmwArcade_Active())) return false;
  selected=value; apply(); return true;
}
bool SmwLocale_Japanese(void) { return selected; }

void SmwLocale_Message(unsigned message) {
  unsigned source=table(179,message),dest=stripe_image_upload;
  for(int row=3;row>=0;row--) {
    unsigned header=table(178,row);
    wr16(dest,header); wr16(dest+0x28,swap16(swap16(header)+0x20));
    wr16(dest+2,0x2300); wr16(dest+0x2a,0x2300);
    for(unsigned col=0;col<18;col++) {
      unsigned upper=0x5d;
      if(source<kDisplayMessage_DATA_05A5D9_SIZE && (kDisplayMessage_DATA_05A5D9[source]==0x59 || kDisplayMessage_DATA_05A5D9[source]==0x5b))
        upper=kDisplayMessage_DATA_05A5D9[source++];
      unsigned lower=source<kDisplayMessage_DATA_05A5D9_SIZE?kDisplayMessage_DATA_05A5D9[source++]:0x5d;
      wr16(dest+4+col*2,0x3900|upper); wr16(dest+0x2c+col*2,0x3900|lower);
    }
    dest+=0x50;
  }
  wr16(dest,255); stripe_image_upload=dest;
  flag_disable_layer3_scroll=1; mirror_layer3_xpos=mirror_layer3_ypos=0;
}
static unsigned name_fragment(unsigned dest,unsigned source,unsigned end) {
  while(source<kUpdateLevelName_LevelNameStrings_SIZE) {
    unsigned t=kUpdateLevelName_LevelNameStrings[source++];
    if(dest<end) {
      if((t&0x7f)==0x59 || (t&0x7f)==0x5b) stripe_image_upload_data[dest+0x26]=t&0x7f;
      else { wr16(dest+4,0x3900|(t&0x7f)); wr16(dest+0x28,0x395d); dest+=2; }
    }
    if(t&0x80) break;
  }
  return dest;
}
void SmwLocale_LevelName(uint16_t name) {
  unsigned dest=stripe_image_upload,end=dest+0x20;
  /* Two complete 4-byte headers plus 32 bytes per row. Reserve the full
   * stream so a following cursor/portrait update cannot overwrite its tail. */
  stripe_image_upload+=0x48;
  wr16(dest,0x8c50); wr16(dest+2,0x1f00); wr16(dest+0x24,0x6c50); wr16(dest+0x26,0x1f00);
  unsigned s=table(180,(name>>8)&31);
  if(s<kUpdateLevelName_LevelNameStrings_SIZE && !(kUpdateLevelName_LevelNameStrings[s]&0x80)) {
    dest=name_fragment(dest,s,end);
    dest=name_fragment(dest,table(183,(name>>14)&3),end);
  }
  s=table(181,(name>>4)&15);
  if(s<kUpdateLevelName_LevelNameStrings_SIZE && kUpdateLevelName_LevelNameStrings[s]!=0xdd) {
    dest=name_fragment(dest,s,end);
    dest=name_fragment(dest,table(183,(name>>13)&1),end);
  }
  dest=name_fragment(dest,table(182,name&15),end);
  while(dest<end) dest=name_fragment(dest,0x11a,end);
  stripe_image_upload_data[dest+0x28]=255;
}
void SmwLocale_FileSelect(uint8_t kind,uint8_t erased) {
  memcpy(stripe_image_upload_data,kFileSelectText_EraseFile,176);
  unsigned at=118;
  for(int file=2;file>=0;file--,at-=42,erased>>=1) if(!(erased&1)) {
    unsigned saved=BufferFileSelectText_009DB5(file).first;
    if(saved==0xffff) continue;
    PairU16 digits=HexToDec(g_sram[saved+140]);
    stripe_image_upload_data[at+4]=digits.first;
    stripe_image_upload_data[at+2]=digits.second?digits.second:0xfc;
    stripe_image_upload_data[at+3]=stripe_image_upload_data[at+5]=0x38;
    wr16(at+18,0x38fc);
    for(int t=12;t>=0;t-=4) { wr16(at+6,table(185,(t+(kind?2:0))/2)); at+=2; }
    at-=8;
  }
}
uint16_t SmwLocale_Cursor(unsigned menu,uint16_t original) { return selected?table(184,menu):SmwArcade_Cursor(menu,original); }
uint16_t SmwLocale_SwitchPosition(unsigned offset,uint16_t original) { return selected?table(186,offset/2):SmwArcade_SwitchPosition(offset,original); }
