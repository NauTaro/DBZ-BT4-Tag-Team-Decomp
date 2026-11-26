// stb_image 2.30, upstream commit 6b5799bae8a1fe3792691ef4b72ae26434c3e799.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_GIF
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
// Keep the private decoder's state out of the relocated macOS TLS segment.
// The codec uses default decoding flags and reports errors via return values.
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_FAILURE_STRINGS
#define STBI_MAX_DIMENSIONS 2048
#include "../../third_party/stb/stb_image.h"

static std::string profileBase64(const unsigned char *data,size_t length){
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve((length+2)/3*4);
    for(size_t i=0;i<length;i+=3){
        unsigned group=(unsigned)data[i]<<16;
        if(i+1<length)group|=(unsigned)data[i+1]<<8;
        if(i+2<length)group|=data[i+2];
        output+=alphabet[(group>>18)&63];output+=alphabet[(group>>12)&63];
        output+=i+1<length?alphabet[(group>>6)&63]:'=';
        output+=i+2<length?alphabet[group&63]:'=';
    }
    return output;
}

static bool profilePixels(const std::string &input,unsigned char *out,size_t bytes){
    const size_t length=bytes/2;
    if(input.size()!=(length+2)/3*4)return false;
    unsigned group=0,bits=0;
    size_t index=0;
    unsigned char low=0;
    for(const unsigned char c:input){
        if(c=='=')break;
        const int value=c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:
                        c>='0'&&c<='9'?c-'0'+52:c=='+'?62:c=='/'?63:-1;
        if(value<0)return false;
        group=(group<<6)|(unsigned)value;bits+=6;
        if(bits<8)continue;
        bits-=8;
        if(index>=length)return false;
        const auto next=(unsigned char)(group>>bits);
        if(index&1){
            const unsigned color=low|((unsigned)next<<8);
            *out++=(unsigned char)(((color>>11)&31)*255/31);
            *out++=(unsigned char)(((color>>5)&63)*255/63);
            *out++=(unsigned char)((color&31)*255/31);
            *out++=255;
        }else low=next;
        ++index;
    }
    return index==length;
}

// Count GIF image blocks before decoding so a short compressed file cannot allocate
// an unbounded stack of frames. The decoder also checks every block it consumes.
static bool boundedProfileGif(const unsigned char *data,size_t size){
    if(size<14||memcmp(data,"GIF",3))return false;
    auto tableSize=[](unsigned flags)->size_t{return flags&128?size_t(3)<<(1+(flags&7)):0;};
    size_t pos=13+tableSize(data[10]);
    auto skipBlocks=[&]{
        while(pos<size){
            size_t block=data[pos++];
            if(!block)return true;
            if(block>size-pos)return false;
            pos+=block;
        }
        return false;
    };
    unsigned count=0;
    while(pos<size){
        const unsigned marker=data[pos++];
        if(marker==0x3b)return count>0;
        if(marker==0x21){
            if(pos==size)return false;
            ++pos;
        }else if(marker==0x2c){
            if(size-pos<9||++count>48)return false;
            pos+=9+tableSize(data[pos+8]);
            if(pos>=size)return false;
            ++pos;
        }else return false;
        if(!skipBlocks())return false;
    }
    return false;
}

static std::string profileResize(const unsigned char *image,int width,int height,int w,int h,float posX=0.5f,float posY=0.5f,float zoom=1.0f){
    std::vector<unsigned char> packed(size_t(w)*h*2);
    const int cropWidth=std::max(1,int(std::min(width,height*w/h)/zoom));
    const int cropHeight=std::max(1,int(std::min(height,width*h/w)/zoom));
    const int left=int((width-cropWidth)*posX),top=int((height-cropHeight)*posY);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        unsigned sum[3]={};
        for(int sampleY=0;sampleY<2;++sampleY)for(int sampleX=0;sampleX<2;++sampleX){
            const int srcX=left+(4*x+2*sampleX+1)*cropWidth/(w*4);
            const int srcY=top+(4*y+2*sampleY+1)*cropHeight/(h*4);
            const auto *p=image+(size_t(srcY)*width+srcX)*4;
            for(int c=0;c<3;++c)sum[c]+=(p[c]*p[3]+35*(255-p[3]))/255;
        }
        const unsigned color=(sum[0]/32<<11)|(sum[1]/16<<5)|sum[2]/32;
        const size_t dst=(size_t(y)*w+x)*2;
        packed[dst]=(unsigned char)color;packed[dst+1]=(unsigned char)(color>>8);
    }
    return profileBase64(packed.data(),packed.size());
}

struct ProfileImageSource {
    std::string kind;
    std::vector<unsigned char> pixels;
    std::vector<int> delays;
    int width=0,height=0;
};
static ProfileImageSource loadProfileImage(const char *filename,const std::string &kind){
    constexpr Sint64 maxSize=4*1024*1024;
    SDL_IOStream *stream=SDL_IOFromFile(filename,"rb");
    if(!stream)return {};
    const Sint64 size=SDL_GetIOSize(stream);
    std::vector<unsigned char> file(size>0&&size<=maxSize?size_t(size):0);
    const bool loaded=!file.empty()&&SDL_ReadIO(stream,file.data(),file.size())==file.size();
    SDL_CloseIO(stream);
    if(!loaded)return {};
    const bool gif=file.size()>=6&&(!memcmp(file.data(),"GIF87a",6)||!memcmp(file.data(),"GIF89a",6));
    const std::string format=kind=="avatar_auto"?(gif?"motion":"avatar"):kind;
    const bool motion=format=="motion"||format=="banner_motion";
    if(motion!=gif)return {};
    int width=0,height=0,channels=0,count=1,*delays=nullptr;
    if(!stbi_info_from_memory(file.data(),(int)file.size(),&width,&height,&channels)||
        width<1||height<1||width>2048||height>2048)return {};
    if(motion&&(width>512||height>512||!boundedProfileGif(file.data(),file.size())))return {};
    unsigned char *pixels=motion?stbi_load_gif_from_memory(file.data(),(int)file.size(),&delays,&width,&height,&count,&channels,4):
        stbi_load_from_memory(file.data(),(int)file.size(),&width,&height,&channels,4);
    if(!pixels||count<1||(format=="banner_motion"&&count<2)){stbi_image_free(pixels);stbi_image_free(delays);return {};}
    ProfileImageSource result;
    result.kind=format;result.width=width;result.height=height;
    result.pixels.assign(pixels,pixels+size_t(count)*width*height*4);
    result.delays.resize(count,200);
    if(delays)std::copy_n(delays,count,result.delays.begin());
    stbi_image_free(pixels);stbi_image_free(delays);
    return result;
}

static void profileImageDimensions(const std::string &kind,int &w,int &h){
    w=kind=="banner"?128:kind=="banner_motion"?64:32;
    h=kind=="banner_motion"?16:32;
}

static Dj encodeProfileImage(const ProfileImageSource &source,float x=0.5f,float y=0.5f,float zoom=1.0f){
    if(source.pixels.empty())return {};
    int w,h;
    profileImageDimensions(source.kind,w,h);
    Dj frames=Dj::array(),timing=Dj::array();
    const int count=int(source.delays.size()),selected=std::min(count,6);
    for(int i=0;i<selected;++i){
        const int from=i*count/selected,until=(i+1)*count/selected;
        int duration=0;
        for(int j=from;j<until;++j) duration+=std::clamp(source.delays[j],20,1000);
        frames.push_back(profileResize(source.pixels.data()+size_t(from)*source.width*source.height*4,
                                      source.width,source.height,w,h,x,y,zoom));
        timing.push_back(std::clamp(duration,100,1000));
    }
    return {{"kind",source.kind},{"frames",std::move(frames)},{"delays",std::move(timing)}};
}

static Dj profileImage(const char *filename,const std::string &kind){
    return encodeProfileImage(loadProfileImage(filename,kind));
}
