#ifndef SMW_MENU_H
#define SMW_MENU_H
#include <stdint.h>
typedef void (*MenuRect)(void *ctx,int x,int y,int w,int h,uint32_t rgb);
typedef struct {
  MenuRect rect; void *ctx;
  int selection;
  int running;
  int fps_tenths;
  int widescreen;
  int japanese;
  int restart_language;
  int running_variant;
  int arcade_ready;
  const char *status;
} Menu;
void menu_text(MenuRect rect,void *ctx,int x,int y,int scale,uint32_t color,const char *s);
void menu_draw(const Menu *m);
#endif
