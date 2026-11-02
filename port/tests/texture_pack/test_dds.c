#include "../../src/gs/texture_dds.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t data[164];
static void put(int at,uint32_t n){for(int i=0;i<4;++i)data[at+i]=(uint8_t)(n>>(8*i));}
int main(void){uint32_t cc=0,offset=0;
 memset(data,0,sizeof(data));put(128,77);put(132,3);put(140,1);memset(data+148,0xCD,16);
 assert(TextureDds_Dx10(data,sizeof(data),&cc,&offset)&&cc==0x35545844&&offset==148&&data[offset]==0xCD);
 assert(!TextureDds_Dx10(data,147,&cc,&offset));
 put(128,71);assert(TextureDds_Dx10(data,sizeof(data),&cc,&offset)&&cc==0x31545844);
 put(128,74);assert(TextureDds_Dx10(data,sizeof(data),&cc,&offset)&&cc==0x33545844);
 put(128,98);assert(!TextureDds_Dx10(data,sizeof(data),&cc,&offset));
 put(128,77);put(140,2);assert(!TextureDds_Dx10(data,sizeof(data),&cc,&offset));
 put(140,1);put(136,4);assert(!TextureDds_Dx10(data,sizeof(data),&cc,&offset));
 put(136,0);put(132,4);assert(!TextureDds_Dx10(data,sizeof(data),&cc,&offset));
 puts("DDS DX10 payload offset, BC1/2/3 and unsupported arrays/cubes/formats: PASS");return 0;}
