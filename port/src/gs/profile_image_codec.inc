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
#define STBI_MAX_DIMENSIONS 4096
#include "../../third_party/stb/stb_image.h"
// Same upstream revision as the private decoder.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../../third_party/stb/stb_image_write.h"

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
static bool boundedProfileGif(const unsigned char *data,size_t size,int width,int height){
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
            if(size-pos<9||++count>240||size_t(count)*width*height>16*1024*1024)return false;
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
static ProfileImageSource loadProfileImage(const char *filename,const std::string &kind,int *error=nullptr){
    constexpr Sint64 maxSize=10*1024*1024;
    if(error)*error=BT4_PROFILE_IMPORT_INVALID;
    SDL_IOStream *stream=SDL_IOFromFile(filename,"rb");
    if(!stream)return {};
    const Sint64 size=SDL_GetIOSize(stream);
    if(kind=="avatar"){
        char signature[6];
        const bool gif=SDL_ReadIO(stream,signature,sizeof(signature))==sizeof(signature)&&
            (!memcmp(signature,"GIF87a",6)||!memcmp(signature,"GIF89a",6));
        if(gif){
            SDL_CloseIO(stream);
            if(error)*error=BT4_PROFILE_IMPORT_DONORS;
            return {};
        }
        if(SDL_SeekIO(stream,0,SDL_IO_SEEK_SET)<0){SDL_CloseIO(stream);return {};}
    }
    if(size>maxSize&&error)*error=BT4_PROFILE_IMPORT_SIZE;
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
        width<1||height<1||width>STBI_MAX_DIMENSIONS||height>STBI_MAX_DIMENSIONS)return {};
    if(motion&&!boundedProfileGif(file.data(),file.size(),width,height))return {};
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
    w=kind=="avatar"?80:kind=="banner"?128:kind=="banner_motion"?64:32;
    h=kind=="avatar"?80:kind=="banner_motion"?16:32;
}

static constexpr size_t profileAvatarWireBytes=11200;

static void profileCrop(const ProfileImageSource &source,unsigned char *output,int w,int h,int channels,
                        float px,float py,float zoom,int frame=0){
    const int width=std::max(1,int(std::min(source.width,source.height*w/h)/zoom));
    const int height=std::max(1,int(std::min(source.height,source.width*h/w)/zoom));
    const int left=int((source.width-width)*px),top=int((source.height-height)*py);
    const auto *framePixels=source.pixels.data()+size_t(frame)*source.width*source.height*4;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        const int x0=x*width/w,x1=std::max(x0+1,(x+1)*width/w);
        const int y0=y*height/h,y1=std::max(y0+1,(y+1)*height/h);
        unsigned sum[3]={};
        for(int sy=y0;sy<y1;++sy)for(int sx=x0;sx<x1;++sx){
            const auto *p=framePixels+(size_t(top+sy)*source.width+left+sx)*4;
            for(int c=0;c<3;c++)sum[c]+=(p[c]*p[3]+35*(255-p[3])+127)/255;
        }
        const unsigned count=(x1-x0)*(y1-y0);
        auto *pixel=output+(size_t(y)*w+x)*channels;
        for(int c=0;c<3;c++)pixel[c]=(unsigned char)((sum[c]+count/2)/count);
        if(channels==4)pixel[3]=255;
    }
}

static int profileFrameDuration(const ProfileImageSource &source,int from,int until){
    int duration=0;
    for(int j=from;j<until;++j) duration+=std::clamp(source.delays[j],20,1000);
    return std::clamp(duration,100,1000);
}

static std::string profileAvatarDetail(const ProfileImageSource &source,float px,float py,float zoom){
    const int crop=std::max(1,int(std::min(source.width,source.height)/zoom));
    int previous=0;
    for(int limit:{BT4_PROFILE_AVATAR_MAX,256,192,128}){
        const int side=std::min(crop,limit);
        if(side==previous)continue;
        previous=side;
        std::vector<unsigned char> image(size_t(side)*side*3);
        profileCrop(source,image.data(),side,side,3,px,py,zoom);
        auto append=[](void *context,void *data,int size){
            auto &bytes=*static_cast<std::vector<unsigned char> *>(context);
            const auto *first=static_cast<const unsigned char *>(data);
            if(bytes.size()<=profileAvatarWireBytes)
                bytes.insert(bytes.end(),first,first+std::min<size_t>(size,profileAvatarWireBytes+1-bytes.size()));
        };
        std::vector<unsigned char> bytes;
        if(stbi_write_png_to_func(append,&bytes,side,side,3,image.data(),side*3)&&
           !bytes.empty()&&bytes.size()<=profileAvatarWireBytes)return profileBase64(bytes.data(),bytes.size());
        for(int quality:{93,86,78,68}){
            bytes.clear();
            if(stbi_write_jpg_to_func(append,&bytes,side,side,3,image.data(),quality)&&
               !bytes.empty()&&bytes.size()<=profileAvatarWireBytes)return profileBase64(bytes.data(),bytes.size());
        }
        if(side==crop&&side<=128)break;
    }
    return {};
}

static bool profileDecodeAvatar(const std::string &encoded,unsigned char *rgba,int *w,int *h){
    if(encoded.empty()||encoded.size()>4*((profileAvatarWireBytes+2)/3)||encoded.size()%4)return false;
    std::vector<unsigned char> data;
    data.reserve(encoded.size()/4*3);
    unsigned group=0,bits=0;
    for(unsigned char c:encoded){
        if(c=='=')break;
        const int value=c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:
                        c>='0'&&c<='9'?c-'0'+52:c=='+'?62:c=='/'?63:-1;
        if(value<0)return false;
        group=(group<<6)|(unsigned)value;bits+=6;
        if(bits>=8){bits-=8;data.push_back((unsigned char)(group>>bits));}
    }
    const bool png=data.size()>8&&!memcmp(data.data(),"\x89PNG\r\n\x1a\n",8);
    const bool jpeg=data.size()>3&&data[0]==0xff&&data[1]==0xd8&&data[2]==0xff;
    int width=0,height=0,channels=0;
    if((!png&&!jpeg)||data.size()>profileAvatarWireBytes||
       !stbi_info_from_memory(data.data(),int(data.size()),&width,&height,&channels)||
       width!=height||width<1||width>BT4_PROFILE_AVATAR_MAX)return false;
    unsigned char *decoded=stbi_load_from_memory(data.data(),int(data.size()),&width,&height,&channels,4);
    if(!decoded)return false;
    memcpy(rgba,decoded,size_t(width)*height*4);
    stbi_image_free(decoded);*w=width;*h=height;
    return true;
}

static Dj encodeProfileImage(const ProfileImageSource &source,float x=0.5f,float y=0.5f,float zoom=1.0f){
    if(source.pixels.empty())return {};
    int w,h;
    profileImageDimensions(source.kind,w,h);
    std::string detail;
    if(source.kind=="avatar"){
        detail=profileAvatarDetail(source,x,y,zoom);
        if(!detail.empty())w=h=32; // leave room for the original client’s thumbnail
    }
    Dj frames=Dj::array(),timing=Dj::array();
    const int count=int(source.delays.size()),selected=std::min(count,6);
    for(int i=0;i<selected;++i){
        const int from=i*count/selected,until=(i+1)*count/selected;
        frames.push_back(profileResize(source.pixels.data()+size_t(from)*source.width*source.height*4,
                                      source.width,source.height,w,h,x,y,zoom));
        timing.push_back(profileFrameDuration(source,from,until));
    }
    Dj body={{"kind",source.kind},{"frames",std::move(frames)},{"delays",std::move(timing)}};
    if(!detail.empty())body["detail"]=std::move(detail);
    return body;
}

static Dj profileImage(const char *filename,const std::string &kind){
    return encodeProfileImage(loadProfileImage(filename,kind));
}
