namespace {
constexpr unsigned clipPixels=BT4_DIOS_GIF_W*BT4_DIOS_GIF_H;
using ClipFrame=std::array<unsigned char,clipPixels>;
std::mutex clipLock;
std::atomic<unsigned> clipEpoch{0};
unsigned clipNextEpoch=0,clipFrames=0;
std::array<ClipFrame,BT4_DIOS_GIF_FRAMES> clipTail;
std::array<ClipFrame,3*BT4_DIOS_GIF_FPS> clipOpening;

void beginClip(){
    std::lock_guard<std::mutex> guard(clipLock);
    clipFrames=0;
    clipEpoch=++clipNextEpoch;
}

void appendGifFrame(std::vector<unsigned char> &gif,const ClipFrame &frame,unsigned delay){
    const unsigned char header[]={0x21,0xf9,4,4,(unsigned char)delay,0,0,0,
        0x2c,0,0,0,0,BT4_DIOS_GIF_W,0,BT4_DIOS_GIF_H,0,0,8};
    gif.insert(gif.end(),std::begin(header),std::end(header));
    std::array<short,1<<17> dictionary;
    dictionary.fill(-1);
    std::array<unsigned,252> touched;
    unsigned touchedCount=0;
    std::vector<unsigned char> compressed;
    unsigned bits=0,used=0,next=258;
    auto put=[&](unsigned code){
        bits|=code<<used;used+=9;
        while(used>=8){compressed.push_back(bits&255);bits>>=8;used-=8;}
    };
    put(256);
    unsigned code=frame.front();
    for(unsigned i=1;i<frame.size();++i){
        const unsigned index=code*256+frame[i];
        const int entry=dictionary[index];
        if(entry>=0){code=(unsigned)entry;continue;}
        put(code);
        if(next==510){
            put(256);
            for(unsigned indexToClear=0;indexToClear<touchedCount;++indexToClear)
                dictionary[touched[indexToClear]]=-1;
            touchedCount=0;next=258;
        }else{
            dictionary[index]=(short)next++;
            touched[touchedCount++]=index;
        }
        code=frame[i];
    }
    put(code);put(257);
    if(used)compressed.push_back(bits&255);
    for(size_t at=0;at<compressed.size();){
        const size_t block=std::min<size_t>(255,compressed.size()-at);
        gif.push_back((unsigned char)block);
        gif.insert(gif.end(),compressed.begin()+at,compressed.begin()+at+block);
        at+=block;
    }
    gif.push_back(0);
}

std::vector<unsigned char> encodeClip(unsigned stride){
    if(clipFrames<8)return {};
    const unsigned char head[]={'G','I','F','8','9','a',BT4_DIOS_GIF_W,0,BT4_DIOS_GIF_H,0,0xf7,0,0};
    std::vector<unsigned char> gif(std::begin(head),std::end(head));
    for(unsigned i=0;i<256;++i){gif.push_back((i>>5)*255/7);gif.push_back(((i>>2)&7)*255/7);gif.push_back((i&3)*85);}
    const unsigned char loop[]={0x21,0xff,11,'N','E','T','S','C','A','P','E','2','.','0',3,1,0,0,0};
    gif.insert(gif.end(),std::begin(loop),std::end(loop));
    const unsigned count=std::min<unsigned>(clipFrames,BT4_DIOS_GIF_FRAMES);
    const unsigned intro=clipFrames>BT4_DIOS_GIF_FRAMES+8?(unsigned)clipOpening.size():0;
    ClipFrame effect;
    for(unsigned i=0;i<count;i+=stride){
        const auto &source=i<intro?clipOpening[i]:clipTail[(clipFrames-count+i)%clipTail.size()];
        const bool cut=i<4||(intro&&i>=intro&&i<intro+4);
        if(cut){
            for(unsigned y=0;y<BT4_DIOS_GIF_H;++y)for(unsigned x=0;x<BT4_DIOS_GIF_W;++x){
                const unsigned shift=(y*19+i*5)%17;
                const unsigned pos=(x+shift)%BT4_DIOS_GIF_W;
                effect[y*BT4_DIOS_GIF_W+x]=(y+i)%5==0?(unsigned char)(16+(x*13+y*83)%224):source[y*BT4_DIOS_GIF_W+pos];
            }
        }
        const unsigned next=std::min(count,i+stride);
        appendGifFrame(gif,cut?effect:source,next*100/BT4_DIOS_GIF_FPS-i*100/BT4_DIOS_GIF_FPS);
        if(gif.size()>BT4_DIOS_GIF_WIRE/4*3-4)return {};
    }
    gif.push_back(0x3b);
    return gif;
}

std::vector<unsigned char> finishClip(){
    clipEpoch=0;
    std::lock_guard<std::mutex> guard(clipLock);
    auto gif=encodeClip(1);
    return gif.empty()?encodeClip(2):std::move(gif);
}

// Walk the container before allocating decoder frames. Only the palette,
// bounded image blocks, graphics controls, and the loop extension are used.
bool validClip(const std::vector<unsigned char> &gif){
    if(gif.size()<14||memcmp(gif.data(),"GIF89a",6)||gif[7]||gif[9])return false;
    const unsigned w=gif[6],h=gif[8];
    if(!((w==128&&h==72)||(w==BT4_DIOS_GIF_W&&h==BT4_DIOS_GIF_H)))return false;
    size_t pos=13+((gif[10]&128)?3u<<(1+(gif[10]&7)):0);
    unsigned frames=0;
    while(pos<gif.size()){
        const unsigned tag=gif[pos++];
        if(tag==0x3b)return frames&&pos==gif.size();
        if(tag==0x21){
            if(pos>=gif.size())return false;
            const unsigned ext=gif[pos++];
            if(ext!=0xf9&&ext!=0xff)return false;
        }else if(tag==0x2c){
            if(++frames>BT4_DIOS_GIF_FRAMES||pos+10>gif.size())return false;
            auto word=[&](size_t i){return gif[pos+i]+256u*gif[pos+i+1];};
            if(!word(4)||!word(6)||word(0)+word(4)>w||word(2)+word(6)>h)return false;
            const unsigned flags=gif[pos+8];
            pos+=9+((flags&128)?3u<<(1+(flags&7)):0);
            if(pos>=gif.size()||gif[pos]<2||gif[pos++]>8)return false;
        }else return false;
        while(pos<gif.size()&&gif[pos])pos+=1+gif[pos];
        if(pos>=gif.size())return false;
        ++pos;
    }
    return false;
}
}

extern "C" unsigned Port_DiosCaptureEpoch(void){return clipEpoch.load();}

extern "C" void Port_DiosCaptureTestBegin(void){
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_DIOS_CAPTURE_TEST"))beginClip();
}

extern "C" void Port_DiosCaptureFrame(unsigned epoch,const unsigned char *rgba){
    bool finished=false;
    {
        std::lock_guard<std::mutex> guard(clipLock);
        if(!epoch||epoch!=clipEpoch)return;
        auto &frame=clipTail[clipFrames%clipTail.size()];
        for(unsigned i=0;i<frame.size();++i)
            frame[i]=(rgba[i*4]&224)|((rgba[i*4+1]>>3)&28)|(rgba[i*4+2]>>6);
        if(clipFrames<clipOpening.size())clipOpening[clipFrames]=frame;
        finished=++clipFrames==BT4_DIOS_GIF_FRAMES+16;
    }
    if(finished&&getenv("BT4_RENDER_TEST"))if(const char *path=getenv("BT4_DIOS_CAPTURE_TEST")){
        const auto gif=finishClip();
        if(FILE *file=fopen(path,"wb")){
            fwrite(gif.data(),1,gif.size(),file);fclose(file);
            fprintf(stderr,"Dios Tube capture test: wrote %zu GIF bytes\n",gif.size());
        }
    }
}
