#ifndef GS_VOICE_FILES_H
#define GS_VOICE_FILES_H
/* Global file IDs, not archive-local indices. BT4 B14 has 250 voice slots. */
static int PortVoice_OtherStream(int id,int bt4) {
    if(bt4)return id>=0x16E6A&&id<0x1D012?id-0x61A8:0;
    if(id>=0x87D4&&id<0x8D30)return id-0x55C;
    if(id>=0xCC32&&id<0x10B16)return id-0x3EE4;
    if(id>=0x10B6C&&id<0x10BA4)return id-0x38;
    return 0;
}
static int PortVoice_Bank(int chara,int alternate,int bt4) {
    if(chara<0||chara>=(bt4?250:161))return 0;
    return chara+(bt4?(alternate?0x1D012:0x1D10C):(alternate?0xBDA:0xC7B));
}
#endif
