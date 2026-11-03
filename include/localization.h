#ifndef SMW_LOCALIZATION_H
#define SMW_LOCALIZATION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Original SNES presentation overlay; no game-state or audio replacement. */
bool SmwLocale_Load(const void *data,size_t length);
void SmwLocale_BindEnglish(void);
bool SmwLocale_Select(bool japanese);
bool SmwLocale_Japanese(void);
void SmwLocale_Message(unsigned message);
void SmwLocale_LevelName(uint16_t name);
void SmwLocale_FileSelect(uint8_t kind,uint8_t erased);
uint16_t SmwLocale_Cursor(unsigned menu,uint16_t english);
uint16_t SmwLocale_SwitchPosition(unsigned offset,uint16_t english);
#endif
