#ifndef SMW_SUPER_SYSTEM_H
#define SMW_SUPER_SYSTEM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Native NSS game-program branches; cabinet credits use standalone free play. */
bool SmwArcade_Load(const void *data,size_t length);
bool SmwArcade_Ready(void);
bool SmwArcade_Select(bool enabled);
bool SmwArcade_Active(void);
void SmwArcade_ApplyAssets(void);
void SmwArcade_InitSaveData(void);
void SmwArcade_DrawZoneMenu(void);
void SmwArcade_ZoneSelect(void);
uint16_t SmwArcade_Cursor(unsigned menu,uint16_t original);
uint16_t SmwArcade_MessageOffset(unsigned message,uint16_t original);
uint16_t SmwArcade_SwitchPosition(unsigned offset,uint16_t original);
void SmwArcade_UpdateSaveBuffer(void);
void SmwArcade_MessagePrompt(void);
void SmwArcade_CastleText(void);
bool SmwArcade_EndCastle(void);
#endif
