/* Native presentation extensions. MIT; see LICENSE. */
#ifndef SMW_WIDESCREEN_H
#define SMW_WIDESCREEN_H
#include <stdbool.h>
#include <stdint.h>
struct Ppu;
enum { SMW_WIDE_WIDTH = 342, SMW_WIDE_EXTRA = 43, SMW_WIDE_PITCH = 384 };
void SmwWide_SetEnabled(bool enabled);
bool SmwWide_Enabled(void);
bool SmwWide_Active(void);
bool SmwWide_Overworld(void);
bool SmwWide_MapInView(int x,int padding);
void SmwWide_BeginMapActor(unsigned index);
void SmwWide_RecordMapPiece(unsigned slot,int x);
void SmwWide_MapPlayer(unsigned player,unsigned first,unsigned last);
void SmwWide_Reset(void);
void SmwWide_BeginFrame(void);
void SmwWide_Latch(void);
void SmwWide_PrepareDraw(struct Ppu *ppu);
void SmwWide_BeginActor(unsigned kind, unsigned index);
void SmwWide_EndActor(void);
void SmwWide_DrawInfo(unsigned index);
void SmwWide_RecordPiece(unsigned slot, int x);
void SmwWide_RecordExtendedPiece(unsigned index, unsigned slot);
bool SmwWide_InView(int x, int padding);
bool SmwWide_ExtendedInView(unsigned index);
int SmwWide_ReserveX(void);
/* True when the expanded horizontal sweep handled this native loader call. */
bool SmwWide_Spawn(void);
bool SmwWide_ShouldErase(unsigned index, int native_bound, unsigned side);
/* Public for map/camera regression fixtures, also used by the tile callback. */
int SmwWide_ViewOffset(int camera, int level_width);
bool SmwWide_MapTile(unsigned layer, int x, int y, uint16_t *tile);
#endif
