#ifndef BT4_TEXTURE_DDS_H
#define BT4_TEXTURE_DDS_H
#include <stdint.h>
#include <stddef.h>
static uint32_t TextureDds_U32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
/* DX10 is a header variant. The supplied HD pack's DXGI 77 payload is
   ordinary BC3/DXT5, starting 20 bytes later than a legacy DDS payload. */
static int TextureDds_Dx10(const uint8_t *p,size_t size,uint32_t *fourcc,uint32_t *offset) {
    uint32_t dxgi;
    if(size<148||TextureDds_U32(p+132)!=3||(TextureDds_U32(p+136)&4)||TextureDds_U32(p+140)!=1)return 0;
    dxgi=TextureDds_U32(p+128);
    if(dxgi==71||dxgi==72)*fourcc=0x31545844;
    else if(dxgi==74||dxgi==75)*fourcc=0x33545844;
    else if(dxgi==77||dxgi==78)*fourcc=0x35545844;
    else return 0;
    *offset=148;return 1;
}
#endif
