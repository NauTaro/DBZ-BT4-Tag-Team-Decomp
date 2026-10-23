/* Constant-size local release check. No extracted assets are enumerated. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../include/bt4_build_identity.h"
int Port_BuildIdentity(unsigned long long *out){
    char path[2048],line[256];FILE *f;size_t n,i;uint64_t hash=0xcbf29ce484222325ULL;
    const char *base=SDL_GetBasePath();if(!base)return -1;
    if(SDL_snprintf(path,sizeof(path),"%s%s",base,BT4_BUILD_MARKER)>=(int)sizeof(path))return -1;
    f=fopen(path,"rb");if(!f)return -1;
    n=fread(line,1,sizeof(line)-1,f);if(ferror(f)||!feof(f)){fclose(f);return -1;}fclose(f);
    while(n&&(line[n-1]=='\r'||line[n-1]=='\n'))--n;line[n]=0;
    if(strlen(line)!=n||strcmp(line,BT4_BUILD_IDENTITY))return -1;
    for(i=0;i<n;++i)hash=(hash^(unsigned char)line[i])*0x100000001b3ULL;
    if(out)*out=hash;return 1;
}
