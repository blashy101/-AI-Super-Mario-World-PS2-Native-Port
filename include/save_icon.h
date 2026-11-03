#ifndef SMW_SAVE_ICON_H
#define SMW_SAVE_ICON_H
#include <stdbool.h>
/* Repair the PS2 Browser descriptor/model in the existing save directory.
 * Call only at launch/menu time. SRAM journal files are never opened here. */
bool SmwSaveIcon_Ensure(const char *directory);
#endif
