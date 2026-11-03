#include "save_icon.h"
#include <stdio.h>
#include <string.h>
#include "save_icon_data.h"

static bool matches(const char *path,const unsigned char *data,size_t size) {
  FILE *f=fopen(path,"rb");
  if(!f) return false;
  unsigned char buffer[256];
  size_t at=0;
  while(at<size) {
    size_t n=size-at;
    if(n>sizeof(buffer)) n=sizeof(buffer);
    if(fread(buffer,1,n,f)!=n || memcmp(buffer,data+at,n)) break;
    at+=n;
  }
  bool same=at==size && fgetc(f)==EOF && !ferror(f);
  return fclose(f)==0 && same;
}
static bool install(const char *directory,const char *name,const unsigned char *data,size_t size) {
  char path[256];
  int length=snprintf(path,sizeof(path),"%s/%s",directory,name);
  if(length<0 || (size_t)length>=sizeof(path)) return false;
  if(matches(path,data,size)) return true;
  FILE *f=fopen(path,"wb");
  if(!f) return false;
  bool ok=fwrite(data,1,size,f)==size;
  if(fclose(f)) return false;
  return ok && matches(path,data,size);
}
bool SmwSaveIcon_Ensure(const char *directory) {
  /* Model first: a completed descriptor always refers to an installed model.
   * Partial metadata can be safely repaired on the next launch. */
  return directory && install(directory,"yoshi.icn",yoshi_icn,sizeof(yoshi_icn)) &&
    install(directory,"icon.sys",icon_sys,sizeof(icon_sys));
}
