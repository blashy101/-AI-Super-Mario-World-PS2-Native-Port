#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static uint32_t pixels[640*448];
static void rect(void *ctx,int x,int y,int w,int h,uint32_t color) {
  (void)ctx;
  for(int j=y;j<y+h;j++) for(int i=x;i<x+w;i++)
    if(i>=0 && i<640 && j>=0 && j<448) pixels[j*640+i]=color;
}
int main(int argc,char **argv) {
  if(argc<2) return 2;
  Menu m={.rect=rect,.selection=argc>2?atoi(argv[2]):0,.arcade_ready=1}; menu_draw(&m);
  FILE *f=fopen(argv[1],"wb"); if(!f) return 1;
  fprintf(f,"P6\n640 448\n255\n");
  for(int i=0;i<640*448;i++) {
    uint8_t c[]={pixels[i]>>16,pixels[i]>>8,pixels[i]}; fwrite(c,1,3,f);
  }
  return fclose(f)?1:0;
}
