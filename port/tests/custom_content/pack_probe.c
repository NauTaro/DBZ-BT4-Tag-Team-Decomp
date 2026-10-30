#include "../../src/gs/gs_texpack.c"
#include <assert.h>
const char *Port_GetEnv(const char *name){return SDL_getenv(name);}
/* The probe selects actual entries by ID; live-GS matching is tested in game. */
const uint8_t *Gs_BlockPtr(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return NULL;}
uint32_t Gs_VramRead(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
int main(int argc,char **argv){
    assert(argc==2);SDL_setenv_unsafe("BT3_TEXTURES",argv[1],1);TexPack_Init();assert(sCount>0);
    unsigned checked=0;int beyond2gb=0;
    for(uint32_t i=0;i<sCount;i+=(sCount>32?sCount/31:1)){
        TexPackImage image;assert(load(&sEntries[i],&image));assert(image.w&&image.h&&image.levels);
        if(sEntries[i].archive>=0){mz_zip_archive_file_stat st;assert(mz_zip_reader_file_stat(&sZip[sEntries[i].archive].zip,sEntries[i].member,&st));beyond2gb|=st.m_local_header_ofs>UINT64_C(2147483648);}
        image_free(&image);checked++;
    }
    printf("pack_entries=%u sampled_images=%u read_beyond_2gb=%d\n",sCount,checked,beyond2gb);
    return 0;
}
