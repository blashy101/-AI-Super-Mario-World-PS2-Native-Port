#include "menu.h"
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/* Original compact 5x7 glyph table. Each byte is a vertical column. */
static const uint8_t digits[10][5]={
 {62,65,65,65,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},
 {24,20,18,127,16},{39,69,69,69,57},{62,73,73,73,50},{1,113,9,5,3},
 {54,73,73,73,54},{38,73,73,73,62}};
static const uint8_t letters[26][5]={
 {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,58},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {3,4,120,4,3},{97,81,73,69,67}};
void menu_text(MenuRect rect,void *ctx,int x,int y,int scale,uint32_t color,const char *s) {
  int origin=x;
  for(;*s;s++,x+=6*scale) {
    if(*s=='\n') { y+=10*scale; x=origin-6*scale; continue; }
    unsigned c=(unsigned char)toupper((unsigned char)*s); const uint8_t *g=0;
    if(c>='A'&&c<='Z') g=letters[c-'A'];
    if(c>='0'&&c<='9') g=digits[c-'0'];
    if(g) {
      for(int col=0;col<5;col++) for(int row=0;row<7;row++)
        if(g[col]&(1<<row)) rect(ctx,x+col*scale,y+row*scale,scale,scale,color);
    } else if(c=='-') rect(ctx,x,y+3*scale,5*scale,scale,color);
    else if(c=='.') rect(ctx,x+2*scale,y+6*scale,scale,scale,color);
    else if(c==':') { rect(ctx,x+2*scale,y+2*scale,scale,scale,color); rect(ctx,x+2*scale,y+5*scale,scale,scale,color); }
    else if(c=='/') for(int i=0;i<5;i++) rect(ctx,x+(4-i)*scale,y+(i+1)*scale,scale,scale,color);
    else if(c=='+') { rect(ctx,x,y+3*scale,5*scale,scale,color); rect(ctx,x+2*scale,y+scale,scale,5*scale,color); }
  }
}
static void r(const Menu *m,int x,int y,int w,int h,uint32_t c) { m->rect(m->ctx,x,y,w,h,c); }
static void t(const Menu *m,int x,int y,int s,uint32_t c,const char *v) { menu_text(m->rect,m->ctx,x,y,s,c,v); }
/* All decoration is drawn from rectangles; no box scan or external texture. */
static void star(const Menu *m,int x,int y,uint32_t color) {
  const uint16_t rows[]={0x010,0x038,0x1ff,0x0fe,0x07c,0x0fe,0x0c6,0x183};
  for(int row=0;row<8;row++) for(int col=0;col<9;) {
    if(!(rows[row]&(1<<col))) { col++;continue; }
    int start=col;
    while(col<9 && (rows[row]&(1<<col))) col++;
    r(m,x+start*2,y+row*2,(col-start)*2,2,color);
  }
}
static void block(const Menu *m,int x,int y,int size) {
  const uint32_t ink=0x19294c;
  r(m,x+3,y+3,size,size,0xcca029); r(m,x,y,size,size,ink);
  r(m,x+2,y+2,size-4,size-4,0xffc62b);
  r(m,x+3,y+3,size-6,2,0xffef9a);
  r(m,x+size-5,y+5,2,size-10,0xda8c1f);
  int s=size/12,ox=x+(size-5*s)/2,oy=y+(size-7*s)/2;
  r(m,ox+s,oy,3*s,s,ink); r(m,ox,oy+s,s,s,ink);
  r(m,ox+4*s,oy+s,s,2*s,ink); r(m,ox+2*s,oy+3*s,2*s,s,ink);
  r(m,ox+2*s,oy+4*s,s,s,ink); r(m,ox+2*s,oy+6*s,s,s,ink);
}
static void title(const Menu *m,int x,int y,int size,const char *word) {
  const uint32_t colors[]={0xef3f36,0x2095ed,0xffdb37,0x2095ed,0x62bf36};
  int index=0;
  for(;*word;word++,x+=6*size) {
    if(*word==' ') continue;
    char one[]={*word,0};
    t(m,x+3,y+4,size,0x213c6b,one);
    t(m,x-1,y,size,0x18243d,one); t(m,x+1,y,size,0x18243d,one);
    t(m,x,y-1,size,0x18243d,one); t(m,x,y+1,size,0x18243d,one);
    t(m,x,y,size,colors[index++%5],one);
  }
}
void menu_draw(const Menu *m) {
  const uint32_t ink=0x19294c,blue=0x1556ab,white=0xfffbed,red=0xe63832,green=0x28964b,gold=0xffd747;
  r(m,0,0,640,448,0xffdc4d);
  /* The yellow/blue packaging palette with a quiet, repeating star mat. */
  for(int y=10;y<428;y+=42) for(int x=8+(y/42%2)*21;x<640;x+=48)
    star(m,x,y,0xf3c33a);
  for(int i=0;i<8;i++) {
    r(m,0,i*8,64-i*8,8,blue);
    r(m,576+i*8,422-i*8,64-i*8,8,blue);
  }
  r(m,32,25,100,21,ink); t(m,43,32,1,white,"PLAYSTATION 2");
  title(m,33,65,4,"SUPER MARIO WORLD");
  t(m,33,108,2,ink,"SELECT AN EDITION");
  block(m,554,62,36);
  for(int i=0;i<2;i++) {
    int x=32+i*296; bool selected=m->selection==i;
    r(m,x+5,150,280,151,0xd6a327);
    if(selected) r(m,x-4,141,288,158,ink);
    r(m,x,145,280,150,selected?white:blue);
    r(m,x+3,148,274,144,white);
    r(m,x+3,148,274,39,i?green:red);
    t(m,x+16,161,2,white,i?"SUPER SYSTEM":"ORIGINAL");
    block(m,x+237,155,24);
    t(m,x+16,204,2,ink,i?"7 ZONES / FREE PLAY":"CONSOLE EDITION");
    r(m,x+16,233,248,2,0xe7dcb8);
    const char *action;
    if(i && !m->arcade_ready) action="NOT INCLUDED";
    else if(m->running && m->running_variant==i)
      action=i?"RESUME ARCADE":m->restart_language?"RESTART / LANGUAGE":"RESUME ADVENTURE";
    else action=i?"CHOOSE A START ZONE":"BEGIN ADVENTURE";
    if(selected) {
      r(m,x+16,255,3,14,i?green:red); r(m,x+19,258,3,8,i?green:red);
      r(m,x+22,261,3,2,i?green:red);
    }
    t(m,x+29,255,2,i?blue:red,action);
  }
  r(m,32,314,576,68,ink); r(m,35,317,570,62,blue);
  t(m,48,326,2,gold,m->widescreen?"DISPLAY   16:9":"DISPLAY   4:3");
  t(m,370,326,2,white,"SQUARE CHANGE");
  t(m,48,351,2,gold,m->selection==0 && m->japanese?"LANGUAGE  JAPANESE":"LANGUAGE  ENGLISH");
  t(m,370,351,2,white,m->selection?"ARCADE ORIGINAL":"TRIANGLE CHANGE");
  if(m->status) t(m,33,390,1,ink,m->status);
  t(m,33,408,2,ink,"D-PAD CHOOSE   CROSS PLAY   CIRCLE BACK");
  r(m,0,433,640,4,green); r(m,0,437,640,11,0xd49b43);
  for(int x=0;x<640;x+=24) { r(m,x,437,2,11,0x98632e); r(m,x+4,439,16,2,0xf7d47e); }
}
