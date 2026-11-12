/* Native Direct3D 11 GS replay. No EGL, ANGLE or OpenGL entry points.
 * The canonical GLSL programs are compiled OFFLINE to SM5 DXBC. D3D11 owns
 * textures, MRT/depth/stencil, pipelines, presentation and the ImGui renderer.
 */
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_5.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
extern "C" {
#include "gs_internal.h"
#include "gs_draw.h"
#include "ui.h"
int Port_Setting(const char *,int),Port_PageAspectMilli(void),Port_LateInput(void);
void Port_SettingSave(const char *,int),Port_SettingsWrite(void),PortA11y_Event(const SDL_Event *);
SDL_Window *GsDraw_WindowCreate(int);
extern unsigned gPortVBlanks;
extern int gsInterp;
extern volatile int gUiPresentX,gUiPresentY,gUiPresentW,gUiPresentH;
}
#include "generated/shaders_d3d11.h"

namespace {
template<class T> void drop(T *&p){if(p){p->Release();p=nullptr;}}
struct Texture {
    ID3D11Texture2D *image=nullptr;
    ID3D11ShaderResourceView *srv=nullptr;
    ID3D11RenderTargetView *rtv=nullptr;
    ID3D11DepthStencilView *dsv=nullptr;
    unsigned levels=1;
    ~Texture(){drop(srv);drop(rtv);drop(dsv);drop(image);}
};
struct Pipeline {uint32_t key;ID3D11BlendState *blend=nullptr;ID3D11DepthStencilState *depth=nullptr;};
ID3D11Device *dev;
ID3D11DeviceContext *ctx;
ID3D11DeviceContext1 *ctx1;
IDXGISwapChain *swap;
ID3D11RenderTargetView *back;
ID3D11Texture2D *backImage;
ID3D11RasterizerState *raster;
ID3D11VertexShader *vs[4],*fxVs;
ID3D11PixelShader *ps[3],*outlinePs,*keyPs,*tablePs,*gradePs,*avgPs,*copyPs;
ID3D11InputLayout *gsLayout,*vuLayout;
ID3D11Buffer *vbuf,*vuvbuf,*ibuf,*vconstants,*pconstants,*uniformRing,*paramRing;
unsigned vcap,vucap,icap,ucap,pcap;
ID3D11SamplerState *samplers[16];
ID3D11BlendState *replaceBlend,*outlineBlend,*keyBlend,*averageBlend;
ID3D11DepthStencilState *noDepth;
Texture *colour[MAX_TARGETS],*aux[MAX_TARGETS],*depth[MAX_TARGETS];
Texture *white,*dateCopy,*auxCopy,*feedback,*gradeCopy,*balance,*movies[2];
bool balanceFresh=true,rangeConstants=false,tearing=false;
unsigned swapFlags,errors;
int swapW,swapH;
SDL_Window *window;
HMODULE library;
Pipeline pipes[1024];int pipeCount;
const unsigned PARAM_STRIDE=768,UNIFORM_STRIDE=512;
bool checked(HRESULT hr,const char *where){
    if(SUCCEEDED(hr))return true;
    if(errors++<24)std::fprintf(stderr,"bt3: D3D11 %s failed: %08lx\n",where,(unsigned long)hr);
    return false;
}
#define DX(call) checked((call),#call)
int width(){return (int)(GS_W*SCALE);}int height(){return (int)(GS_H*SCALE);}
Texture *texture(GsTex t){return (Texture *)(uintptr_t)t;}
GsTex handle(Texture *t){return (GsTex)(uintptr_t)t;}
void unbind(){ID3D11ShaderResourceView *empty[3]={};ctx->PSSetShaderResources(0,3,empty);ctx->OMSetRenderTargets(0,nullptr,nullptr);}
Texture *make_texture(unsigned w,unsigned h,DXGI_FORMAT format,bool target=false,bool z=false,unsigned levels=1){
    auto t=new Texture;D3D11_TEXTURE2D_DESC d={};
    d.Width=w;d.Height=h;d.MipLevels=levels;d.ArraySize=1;d.SampleDesc.Count=1;
    d.Format=z?DXGI_FORMAT_R32G8X24_TYPELESS:format;d.Usage=D3D11_USAGE_DEFAULT;
    d.BindFlags=D3D11_BIND_SHADER_RESOURCE|(z?D3D11_BIND_DEPTH_STENCIL:target?D3D11_BIND_RENDER_TARGET:0);
    if(levels>1&&target)d.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
    if(!DX(dev->CreateTexture2D(&d,nullptr,&t->image))){delete t;return nullptr;}
    D3D11_SHADER_RESOURCE_VIEW_DESC s={};s.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    s.Format=z?DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:format;s.Texture2D.MipLevels=levels;
    if(!DX(dev->CreateShaderResourceView(t->image,&s,&t->srv))){delete t;return nullptr;}
    if(z){D3D11_DEPTH_STENCIL_VIEW_DESC v={};v.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;v.Format=DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        if(!DX(dev->CreateDepthStencilView(t->image,&v,&t->dsv))){delete t;return nullptr;}}
    else if(target&&!DX(dev->CreateRenderTargetView(t->image,nullptr,&t->rtv))){delete t;return nullptr;}
    t->levels=levels;return t;
}
void target_drop(int i){delete colour[i];colour[i]=nullptr;delete aux[i];aux[i]=nullptr;delete depth[i];depth[i]=nullptr;}
void target_ensure(int i){
    if(colour[i])return;
    colour[i]=make_texture(width(),height(),DXGI_FORMAT_R8G8B8A8_UNORM,true);
    aux[i]=make_texture(width(),height(),DXGI_FORMAT_R8_UNORM,true);
    depth[i]=make_texture(width(),height(),DXGI_FORMAT_UNKNOWN,false,true);
}
GsTex target_colour(int i){return handle(colour[i]);}GsTex target_aux(int i){return handle(aux[i]);}GsTex target_depth(int i){return handle(depth[i]);}
GsTex tex_create(uint32_t w,uint32_t h,int format,int levels){
    static const DXGI_FORMAT formats[]={DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_BC1_UNORM,DXGI_FORMAT_BC2_UNORM,DXGI_FORMAT_BC3_UNORM};
    if(format<0||format>3||!w||!h)return 0;
    return handle(make_texture(w,h,formats[format],false,false,std::max(1,levels)));
}
int tex_upload(GsTex h,int level,uint32_t w,uint32_t ht,int format,const void *pixels,uint32_t bytes){
    Texture *t=texture(h);unsigned pitch=format==0?w*4:((w+3)/4)*(format==1?8:16);
    unsigned rows=format==0?ht:(ht+3)/4;
    bool valid=t&&level>=0&&(unsigned)level<t->levels&&pixels&&(!bytes||(uint64_t)pitch*rows<=bytes);
    if(valid)ctx->UpdateSubresource(t->image,(UINT)level,nullptr,pixels,pitch,0);
    std::free((void *)pixels);return valid;
}
void tex_destroy(GsTex t){delete texture(t);}int slots(){return 1<<20;}GsTex white_tex(){return handle(white);}
void viewport(int w,int h,int x=0,int y=0){D3D11_VIEWPORT v={(float)x,(float)y,(float)w,(float)h,0,1};ctx->RSSetViewports(1,&v);}
void scissor(int x,int y,int w,int h){D3D11_RECT r={x,y,x+std::max(0,w),y+std::max(0,h)};ctx->RSSetScissorRects(1,&r);}
void bindings(Texture *a,int sa,Texture *b=nullptr,int sb=6,Texture *c=nullptr,int sc=6){
    ID3D11ShaderResourceView *views[]={a?a->srv:white->srv,b?b->srv:white->srv,c?c->srv:white->srv};
    ID3D11SamplerState *states[]={samplers[sa&15],samplers[sb&15],samplers[sc&15]};
    ctx->PSSetShaderResources(0,3,views);ctx->PSSetSamplers(0,3,states);
}
D3D11_BLEND factor(int code,int c){
    if(!code)return D3D11_BLEND_ZERO;if(code==1||code==5)return D3D11_BLEND_ONE;
    if(code==4)return c==0?D3D11_BLEND_INV_SRC_ALPHA:c==1?D3D11_BLEND_INV_DEST_ALPHA:D3D11_BLEND_INV_BLEND_FACTOR;
    return c==0?D3D11_BLEND_SRC_ALPHA:c==1?D3D11_BLEND_DEST_ALPHA:D3D11_BLEND_BLEND_FACTOR;
}
ID3D11BlendState *blend_state(uint32_t key){
    D3D11_BLEND_DESC d={};d.IndependentBlendEnable=TRUE;auto &a=d.RenderTarget[0];auto &b=d.RenderTarget[1];
    a.RenderTargetWriteMask=(key>>16)&15;a.BlendEnable=(key&1)!=0;
    int src=(key>>1)&7,dst=(key>>4)&7,c=(key>>7)&3;
    a.SrcBlend=key&(1<<9)?D3D11_BLEND_ONE:factor(src,c);a.DestBlend=factor(dst,c);
    a.BlendOp=src==3?D3D11_BLEND_OP_REV_SUBTRACT:dst==3?D3D11_BLEND_OP_SUBTRACT:D3D11_BLEND_OP_ADD;
    a.SrcBlendAlpha=D3D11_BLEND_ONE;a.DestBlendAlpha=D3D11_BLEND_ZERO;a.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    b=a;b.BlendEnable=FALSE;b.RenderTargetWriteMask=((key>>16)&8)&&!(key&(1<<26))?1:0;
    ID3D11BlendState *state=nullptr;DX(dev->CreateBlendState(&d,&state));return state;
}
int pipe_get(uint32_t key){
    for(int i=0;i<pipeCount;i++)if(pipes[i].key==key)return i;
    if(pipeCount==1024)return 0;Pipeline &p=pipes[pipeCount];p.key=key;p.blend=blend_state(key);
    D3D11_DEPTH_STENCIL_DESC d={};bool effect=key&(1<<22);
    static const D3D11_COMPARISON_FUNC tests[]={D3D11_COMPARISON_NEVER,D3D11_COMPARISON_ALWAYS,D3D11_COMPARISON_GREATER_EQUAL,D3D11_COMPARISON_GREATER};
    d.DepthEnable=!effect;d.DepthWriteMask=!effect&&(key&(1<<12))?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;d.DepthFunc=tests[(key>>10)&3];
    d.StencilEnable=!effect&&(key&(1<<24));d.StencilReadMask=d.StencilWriteMask=255;
    d.FrontFace={D3D11_STENCIL_OP_KEEP,D3D11_STENCIL_OP_KEEP,D3D11_STENCIL_OP_INCR_SAT,D3D11_COMPARISON_EQUAL};d.BackFace=d.FrontFace;
    DX(dev->CreateDepthStencilState(&d,&p.depth));gGpuNewPipes++;return pipeCount++;
}
bool reserve_buffer(ID3D11Buffer *&buffer,unsigned &capacity,unsigned need,unsigned maximum,unsigned bind){
    if(need<=capacity)return true;if(need>maximum)return false;
    unsigned size=capacity?capacity:65536;while(size<need)size=std::min(maximum,size*2);
    D3D11_BUFFER_DESC d={};d.ByteWidth=size;d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=bind;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer *next=nullptr;if(!DX(dev->CreateBuffer(&d,nullptr,&next)))return false;
    drop(buffer);buffer=next;capacity=size;return true;
}
bool upload(ID3D11Buffer *b,const void *data,unsigned bytes){
    if(!bytes)return true;D3D11_MAPPED_SUBRESOURCE m={};if(!DX(ctx->Map(b,0,D3D11_MAP_WRITE_DISCARD,0,&m)))return false;
    std::memcpy(m.pData,data,bytes);ctx->Unmap(b,0);return true;
}
struct Params {int32_t mode[4];float misc[4],rect[4],orig[4],extra[4];};
void params(const GsDraw &d,void *output){
    if(d.native==6){
        float *p=(float *)output;std::memcpy(p,d.misc,16);std::memcpy(p+4,d.rect,16);std::memcpy(p+8,d.orig,16);
        p[12]=d.blendc;p[13]=d.mode[0]/100.0f;p[14]=d.mode[1]/100.0f;
        int set=d.src>=0&&d.src<GS_LIGHT_SETS?d.src:0,n=std::min(GS_LIGHTS,gsGradeLightCount[set]);p[15]=(float)n;
        std::memcpy(p+16,gsGradeLights[set],n*sizeof(GsLight));std::memset(p+16+n*8,0,(GS_LIGHTS-n)*sizeof(GsLight));
    }else if(d.native==3){float p[4]={(float)d.src,d.misc[0],0,0};std::memcpy(output,p,16);}
    else if(d.native==1||d.native==2){float p[4]={1333.333f/Port_PageAspectMilli()/GS_W,1.0f/GS_H,100.0f/255.0f,getenv("BT3_FX_DEBUG")?1.0f:0.0f};std::memcpy(output,p,16);}
    else{
        auto &p=*(Params *)output;std::memcpy(p.mode,d.mode,16);std::memcpy(p.misc,d.misc,16);std::memcpy(p.rect,d.rect,16);std::memcpy(p.orig,d.orig,16);
        p.extra[0]=d.vu?std::max(gsVuUni[d.uniform].misc[2],0.0f):gsZMax;p.extra[1]=(pipes[d.pipeline].key&(1<<9))?1.0f:0.0f;
        p.extra[2]=d.portrait[0];p.extra[3]=d.portrait[1];
    }
}
static int clear_draw(const GsDraw *d, float *depth) {
    static int on = -1;
    uint32_t key, j;
    const Vtx *v;
    float x0, y0, x1, y1, z;

    if (on < 0) { on = getenv("BT3_FAST_CLEAR") != NULL ? atoi(getenv("BT3_FAST_CLEAR")) : 1; }
    if (!on || d->native || d->vu || d->wall || d->mode[0] != 0 || d->mode[3] != 0 || d->misc[1] != 0.0f || d->misc[2] != 0.0f ||
        d->count != 6 || d->scissor[0] != 0 || d->scissor[1] != 0) {
        return 0;
    }
    key = pipes[d->pipeline].key;
    /* no blending, depth test "always" (1), depth written, all four channels, GS vertices, no stencil */
    if ((key & 0x3FFu) != 0 || ((key >> 10) & 3) != 1 || !((key >> 12) & 1) || ((key >> 16) & 0xF) != 0xF || ((key >> 20) & 3) != 0 ||
        (key & (1u << 24)) != 0) {
        return 0;
    }
    v = &gsVerts[d->first];
    z = v[0].z;
    x0 = y0 = 1e9f;
    x1 = y1 = -1e9f;
    for (j = 0; j < 6; j++) {
        if (v[j].r != 0 || v[j].g != 0 || v[j].b != 0 || v[j].a != 0 || v[j].z != z) {
            return 0;
        }
        if (v[j].x < x0) { x0 = v[j].x; }
        if (v[j].x > x1) { x1 = v[j].x; }
        if (v[j].y < y0) { y0 = v[j].y; }
        if (v[j].y > y1) { y1 = v[j].y; }
    }
    /* the two triangles of the rectangle (0,0)..(x1,y1), and that rectangle is the scissor and the whole screen at
       least (a view of the split screen clears its half: a clear of the texture would take the other half too) */
    if (x0 != 0.0f || y0 != 0.0f || x1 < 512.0f || y1 < 448.0f ||
        x1 * (float)SCALE != (float)d->scissor[2] || y1 * (float)SCALE != (float)d->scissor[3] ||
        !(v[0].x != v[1].x || v[0].y != v[1].y) || (v[0].x == v[1].x && v[1].x == v[2].x) || (v[3].x == v[4].x && v[4].x == v[5].x)) {
        return 0;
    }
    {   /* the area of the two triangles together is the rectangle's: they do not overlap and leave nothing out */
        float a = 0.0f;
        int t;
        for (t = 0; t < 2; t++) {
            const Vtx *p = &v[t * 3];
            a += fabsf((p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[2].x - p[0].x) * (p[1].y - p[0].y)) * 0.5f;
        }
        if (a != x1 * y1) {
            return 0;
        }
    }
    if (gsZMax > 0.0f && gsZMax < 2.0e7f) { /* (gs.frag, its end) */
        float zn = floorf(z * gsZMax), zq = zn / gsZMax;
        *depth = zq > z ? (zn - 1.0f) / gsZMax : zq;
    } else {
        *depth = z;
    }
    return 1;
}

bool constant_upload(){
    if(!rangeConstants)return true;
    if(gsVuUniCount){
        if(!reserve_buffer(uniformRing,ucap,gsVuUniCount*UNIFORM_STRIDE,MAX_VU_UNIFORMS*UNIFORM_STRIDE,D3D11_BIND_CONSTANT_BUFFER))return false;
        D3D11_MAPPED_SUBRESOURCE m={};if(!DX(ctx->Map(uniformRing,0,D3D11_MAP_WRITE_DISCARD,0,&m)))return false;
        for(unsigned i=0;i<gsVuUniCount;i++)std::memcpy((char *)m.pData+i*UNIFORM_STRIDE,&gsVuUni[i],sizeof(Vu0Uniform));ctx->Unmap(uniformRing,0);
    }
    if(gsDrawCount){
        if(!reserve_buffer(paramRing,pcap,gsDrawCount*PARAM_STRIDE,MAX_DRAWS*PARAM_STRIDE,D3D11_BIND_CONSTANT_BUFFER))return false;
        D3D11_MAPPED_SUBRESOURCE m={};if(!DX(ctx->Map(paramRing,0,D3D11_MAP_WRITE_DISCARD,0,&m)))return false;
        for(unsigned n=0;n<gsDrawCount;n++)params(gsDraws[n],(char *)m.pData+n*PARAM_STRIDE);ctx->Unmap(paramRing,0);
    }return true;
}
void draw_constants(const GsDraw &d,unsigned n){
    if(rangeConstants){UINT start=n*(PARAM_STRIDE/16),count=d.native==6?48:16;ctx1->PSSetConstantBuffers1(0,1,&paramRing,&start,&count);
        if(d.vu){start=d.uniform*(UNIFORM_STRIDE/16);count=UNIFORM_STRIDE/16;ctx1->VSSetConstantBuffers1(0,1,&uniformRing,&start,&count);}}
    else{unsigned char p[PARAM_STRIDE]={};params(d,p);upload(pconstants,p,sizeof(p));ctx->PSSetConstantBuffers(0,1,&pconstants);
        if(d.vu){upload(vconstants,&gsVuUni[d.uniform],sizeof(Vu0Uniform));ctx->VSSetConstantBuffers(0,1,&vconstants);}}
}
void rt_bind(int i,bool z,bool clear){
    unbind();ID3D11RenderTargetView *targets[]={colour[i]->rtv,aux[i]->rtv};ctx->OMSetRenderTargets(2,targets,z?depth[i]->dsv:nullptr);
    viewport(width(),height());if(clear){const float black[4]={};ctx->ClearRenderTargetView(targets[0],black);ctx->ClearRenderTargetView(targets[1],black);ctx->ClearDepthStencilView(depth[i]->dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,0,0);gsTargets[i].cleared=1;}
}
void copy_texture(Texture *from,Texture *to){if(!from||!to)return;unbind();ctx->CopySubresourceRegion(to->image,0,0,0,0,from->image,0,nullptr);}
void copies(){dateCopy=make_texture(width(),height(),DXGI_FORMAT_R8_UNORM);auxCopy=make_texture(width(),height(),DXGI_FORMAT_R8_UNORM);feedback=make_texture(width(),height(),DXGI_FORMAT_R8G8B8A8_UNORM);}
void scale_changed(){
    unbind();for(int i=0;i<gsTargetCount;i++)target_drop(i);
    delete dateCopy;delete auxCopy;delete feedback;delete gradeCopy;gradeCopy=nullptr;balanceFresh=true;copies();
}
void full_screen(ID3D11RenderTargetView *rt,ID3D11PixelShader *shader,ID3D11BlendState *blend,int w,int h){
    unbind();ctx->OMSetRenderTargets(1,&rt,nullptr);viewport(w,h);scissor(0,0,w,h);
    ctx->IASetInputLayout(nullptr);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(fxVs,nullptr,0);ctx->PSSetShader(shader,nullptr,0);ctx->OMSetDepthStencilState(noDepth,0);ctx->OMSetBlendState(blend,nullptr,~0u);
}
void blit(Texture *from,ID3D11RenderTargetView *rt,int sw,int sh,int x,int y,int w,int h,float u,float v){
    full_screen(rt,copyPs,replaceBlend,sw,sh);viewport(w,h,x,y);scissor(x,y,w,h);bindings(from,7);
    float p[4]={u,v,0,0};upload(pconstants,p,sizeof(p));ctx->PSSetConstantBuffers(0,1,&pconstants);ctx->Draw(3,0);
}
void grade(const GsDraw &d,unsigned n){
    if(!gradeCopy){unsigned levels=1;for(int m=width();m>1;m>>=1)levels++;gradeCopy=make_texture(width(),height(),DXGI_FORMAT_R8G8B8A8_UNORM,true,false,levels);}
    if(!gradeCopy||!balance)return;
    copy_texture(colour[d.target],gradeCopy);ctx->GenerateMips(gradeCopy->srv);
    if(d.mode[0]>0){
        full_screen(balance->rtv,avgPs,averageBlend,1,1);float k[4];std::fill(k,k+4,balanceFresh?1.0f:.06f);ctx->OMSetBlendState(averageBlend,k,~0u);
        float p[4]={(float)std::max(0,(int)gradeCopy->levels-3),.5f,448.0f/GS_H,0};upload(pconstants,p,sizeof(p));ctx->PSSetConstantBuffers(0,1,&pconstants);bindings(gradeCopy,15);ctx->Draw(3,0);balanceFresh=false;
    }
    full_screen(colour[d.target]->rtv,gradePs,replaceBlend,width(),height());scissor(d.scissor[0],d.scissor[1],d.scissor[2],d.scissor[3]);
    bindings(gradeCopy,15,depth[d.target],6,balance,6);draw_constants(d,n);ctx->Draw(3,0);
}
void target_copy(int a,int b){copy_texture(colour[a],colour[b]);}
int read_texture(Texture *t,uint8_t *rgba,int ow,int oh,bool reduce){
    if(!t)return 0;D3D11_TEXTURE2D_DESC d={};t->image->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MipLevels=1;
    ID3D11Texture2D *read=nullptr;if(!DX(dev->CreateTexture2D(&d,nullptr,&read)))return 0;
    unbind();ctx->CopySubresourceRegion(read,0,0,0,0,t->image,0,nullptr);D3D11_MAPPED_SUBRESOURCE m={};
    if(!DX(ctx->Map(read,0,D3D11_MAP_READ,0,&m))){drop(read);return 0;}
    for(int y=0;y<oh;y++)for(int x=0;x<ow;x++){int sx=reduce?(int)((x+.5f)*SCALE):x,sy=reduce?(int)((y+.5f)*SCALE):y;
        std::memcpy(rgba+(y*ow+x)*4,(const char *)m.pData+sy*m.RowPitch+sx*4,4);}
    ctx->Unmap(read,0);drop(read);return 1;
}
int read_target(int i,uint8_t *rgba){return read_texture(colour[i],rgba,512,448,true);}
void screenshot(int best){
    static bool done;bool shot=false;const char *at=getenv("BT3_SHOT_VBLANK");
    if(at&&!done&&gPortVBlanks>=(unsigned)atoi(at)){done=true;shot=true;}
    else if(!at&&getenv("BT3_SHOT")&&atoi(getenv("BT3_SHOT"))>0&&gGsFrame%atoi(getenv("BT3_SHOT"))==0)shot=true;
    if(getenv("BT3_SHOT_FROM")&&gGsFrame<(unsigned)atoi(getenv("BT3_SHOT_FROM")))shot=false;
    if(getenv("BT3_SHOT_TO")&&gGsFrame>(unsigned)atoi(getenv("BT3_SHOT_TO")))shot=false;
    if(!shot)return;std::vector<uint8_t> pixels(512*448*4);if(!read_target(best,pixels.data()))return;
    char path[128];std::snprintf(path,sizeof(path),"port/build/shots/gpu_%05u.ppm",gGsFrame);
    if(FILE *f=std::fopen(path,"wb")){std::fprintf(f,"P6\n512 448\n255\n");for(int i=0;i<512*448;i++)std::fwrite(pixels.data()+i*4,1,3,f);std::fclose(f);}
}
void window_shot(){
    static bool done;const char *request=getenv("BT3_UI_SHOT"),*path=request?strchr(request,':'):nullptr;
    if(done||!path||atoi(request)<=0||gGsFrame<(unsigned)atoi(request))return;
    done=true;Texture view;view.image=backImage;std::vector<uint8_t> pixels((size_t)swapW*swapH*4);
    bool ok=read_texture(&view,pixels.data(),swapW,swapH,false);view.image=nullptr;
    if(!ok)return;if(FILE *f=std::fopen(path+1,"wb")){std::fprintf(f,"P6\n%d %d\n255\n",swapW,swapH);
        for(int i=0;i<swapW*swapH;i++)std::fwrite(pixels.data()+i*4,1,3,f);std::fclose(f);}
}
bool swap_resize(int w,int h){
    unbind();drop(back);drop(backImage);
    if(!DX(swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,swapFlags)))return false;
    if(!DX(swap->GetBuffer(0,__uuidof(ID3D11Texture2D),(void **)&backImage))||!DX(dev->CreateRenderTargetView(backImage,nullptr,&back)))return false;
    swapW=w;swapH=h;return true;
}
struct RecordedFrame {~RecordedFrame(){for(int i=0;i<gsTargetCount;i++)gsTargets[i].draws=0;gsVertCount=gsVuVertCount=gsVuIdxCount=gsVuUniCount=gsDrawCount=0;gsAnchor=0;gsSkipped=gsNative=0;}};
void frame(){
    RecordedFrame resetCounters;if(!ctx)return;GsGpu_TitlePoll();SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        if(ev.type==SDL_EVENT_QUIT)std::exit(0);PortA11y_Event(&ev);
        if(PortInput_Hotkey(&ev)){Ui_Toggle();continue;}if(Ui_Event(&ev))continue;
        if(ev.type==SDL_EVENT_KEY_DOWN&&!ev.key.repeat){if(ev.key.key==SDLK_ESCAPE&&!Ui_QuitAsk())std::exit(0);
            if(ev.key.key==SDLK_F11)GsDraw_FullscreenToggle();
            if(ev.key.key==SDLK_F9){static int was=1;if(gsInterp>0){was=gsInterp;gsInterp=0;}else gsInterp=was;Port_SettingSave("interp",gsInterp);Port_SettingsWrite();}}
    }
    int w,h;SDL_GetWindowSizeInPixels(window,&w,&h);if(w<1||h<1)return;
    if((swapW!=w||swapH!=h)&&!swap_resize(w,h))return;
    if(!reserve_buffer(vbuf,vcap,gsVertCount*sizeof(Vtx),MAX_VERTS*sizeof(Vtx),D3D11_BIND_VERTEX_BUFFER)||
       !reserve_buffer(vuvbuf,vucap,gsVuVertCount*48,MAX_VU_VERTS*48,D3D11_BIND_VERTEX_BUFFER)||
       !reserve_buffer(ibuf,icap,gsVuIdxCount*4,MAX_VU_IDX*4,D3D11_BIND_INDEX_BUFFER)||
       !upload(vbuf,gsVerts,gsVertCount*sizeof(Vtx))||!upload(vuvbuf,gsVuVerts,gsVuVertCount*48)||!upload(ibuf,gsVuIdx,gsVuIdxCount*4)||!constant_upload())return;
    for(int i=0;i<2;i++)if(gGsFbUploads&(1u<<i)){
        if(!movies[i])movies[i]=texture(tex_create(GS_W,GS_H,0,1));
        uint32_t *p=(uint32_t *)std::malloc(GS_W*GS_H*4);if(!p)continue;
        for(int y=0;y<GS_H;y++)for(int x=0;x<GS_W;x++)p[y*GS_W+x]=Gs_VramRead((i?0x70:0)*32,8,0,x,y)|0xff000000;
        tex_upload(handle(movies[i]),0,GS_W,GS_H,0,p,GS_W*GS_H*4);
    }gGsFbUploads=0;ctx->RSSetState(raster);
    int current=-1,currentZ=-1,lastPipe=-1,lastVu=-1,lastUniform=-1,lastSampler=-1;
    Texture *lastTexture=nullptr;float lastBlend=-1;int lastScissor[4]={-1};
    auto invalidate=[&](){current=currentZ=lastPipe=lastVu=lastUniform=lastSampler=-1;lastTexture=nullptr;lastBlend=-1;lastScissor[0]=-1;};
    for(unsigned n=0;n<gsDrawCount;n++){
        const GsDraw &d=gsDraws[n];int t=d.target;if(t<0||t>=gsTargetCount||!colour[t]||!aux[t]||!depth[t])continue;
        if(d.native==4||d.native==5){if(gsTargets[t].cleared)copy_texture(aux[t],d.native==4?auxCopy:dateCopy);invalidate();continue;}
        if(d.native>=100){if(d.native<102&&movies[d.native-100]){blit(movies[d.native-100],colour[t]->rtv,width(),height(),0,0,width(),height(),1,1);gsTargets[t].cleared=1;}invalidate();continue;}
        if(d.native&&!gsTargets[t].cleared)continue;
        if(d.native==6){grade(d,n);invalidate();continue;}
        if(d.native==1||d.native==2){full_screen(colour[t]->rtv,d.native==1?outlinePs:keyPs,d.native==1?outlineBlend:keyBlend,width(),height());
            bindings(auxCopy,6);draw_constants(d,n);scissor(d.scissor[0],d.scissor[1],d.scissor[2],d.scissor[3]);ctx->Draw(3,0);invalidate();continue;}
        Pipeline &p=pipes[d.pipeline];uint32_t key=p.key;
        Texture *sample=texture(d.tex);
        if(sample==colour[t]){copy_texture(sample,feedback);sample=feedback;invalidate();}
        float clearDepth;
        if(clear_draw(&d,&clearDepth)){
            unbind();const float black[4]={};ctx->ClearRenderTargetView(colour[t]->rtv,black);ctx->ClearRenderTargetView(aux[t]->rtv,black);
            ctx->ClearDepthStencilView(depth[t]->dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,clearDepth,0);gsTargets[t].cleared=1;invalidate();continue;
        }
        bool clear=!gsTargets[t].cleared;
        if(current!=t||currentZ!=(d.native==0)){rt_bind(t,d.native==0,clear);invalidate();current=t;currentZ=d.native==0;}
        if((key&(1<<24))&&(n==0||gsDraws[n-1].native||gsDraws[n-1].target!=t||!(pipes[gsDraws[n-1].pipeline].key&(1<<24))))ctx->ClearDepthStencilView(depth[t]->dsv,D3D11_CLEAR_STENCIL,0,0);
        if(lastPipe!=d.pipeline){ctx->OMSetDepthStencilState(p.depth,0);ctx->PSSetShader(ps[key&(1<<25)?2:key&(1<<23)?1:0],nullptr,0);}
        if(lastPipe!=d.pipeline||lastBlend!=d.blendc){float blend[4]={d.blendc,d.blendc,d.blendc,d.blendc};ctx->OMSetBlendState(p.blend,blend,~0u);lastBlend=d.blendc;}
        if(std::memcmp(lastScissor,d.scissor,sizeof(lastScissor))){scissor(d.scissor[0],d.scissor[1],d.scissor[2],d.scissor[3]);std::memcpy(lastScissor,d.scissor,sizeof(lastScissor));}
        if(rangeConstants){UINT start=n*(PARAM_STRIDE/16),count=16;ctx1->PSSetConstantBuffers1(0,1,&paramRing,&start,&count);
            if(d.vu&&(lastUniform!=d.uniform||lastVu!=d.vu)){start=d.uniform*(UNIFORM_STRIDE/16);count=UNIFORM_STRIDE/16;ctx1->VSSetConstantBuffers1(0,1,&uniformRing,&start,&count);}}
        else draw_constants(d,n);
        if(d.native==3){bindings(depth[t],6,auxCopy,6,texture(d.tex),6);ctx->IASetInputLayout(nullptr);ctx->VSSetShader(fxVs,nullptr,0);ctx->PSSetShader(tablePs,nullptr,0);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ctx->Draw(3,0);invalidate();continue;}
        if(sample!=lastTexture||d.sampler!=lastSampler){bindings(sample,d.sampler,dateCopy,6);lastTexture=sample;lastSampler=d.sampler;}
        if(d.vu!=lastVu){UINT stride=d.vu?48:sizeof(Vtx),offset=0;ID3D11Buffer *vb=d.vu?vuvbuf:vbuf;
            ctx->IASetVertexBuffers(0,1,&vb,&stride,&offset);ctx->IASetInputLayout(d.vu?vuLayout:gsLayout);ctx->VSSetShader(vs[d.vu],nullptr,0);}
        if(lastPipe!=d.pipeline){unsigned topo=(key>>13)&3;ctx->IASetPrimitiveTopology(topo==0?D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:topo==1?D3D11_PRIMITIVE_TOPOLOGY_LINELIST:D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);}
        if(d.vu){ctx->IASetIndexBuffer(ibuf,DXGI_FORMAT_R32_UINT,0);ctx->DrawIndexed(d.count,d.first,0);}else ctx->Draw(d.count,d.first);
        lastPipe=d.pipeline;lastVu=d.vu;lastUniform=d.uniform;
    }
    int best=gGsMainFbp>=0?GsDraw_TargetGet((uint32_t)gGsMainFbp,0):-1;if(best<0&&gsTargetCount)best=0;
    if(best<0||!colour[best])return;screenshot(best);unbind();const float black[4]={0,0,0,1};ctx->ClearRenderTargetView(back,black);
    float aspect=Port_PageAspectMilli()/1000.0f;int pw=w,ph=(int)(pw/aspect+.5f);if(ph>h){ph=h;pw=(int)(ph*aspect+.5f);}int x=(w-pw)/2,y=(h-ph)/2;
    blit(colour[best],back,w,h,x,y,pw,ph,.5f,448.0f/GS_H);gUiPresentX=x;gUiPresentY=y;gUiPresentW=pw;gUiPresentH=ph;
    viewport(w,h);Ui_DrawD3D11();window_shot();unbind();bool vsync=Port_Setting("vsync",0)!=0;
    HRESULT hr=swap->Present(vsync?1:0,!vsync&&tearing?DXGI_PRESENT_ALLOW_TEARING:0);
    if(hr==DXGI_STATUS_OCCLUDED&&!getenv("BT4_RENDER_TEST"))SDL_Delay(10);else DX(hr);
    if(getenv("BT3_GS_VERBOSE")&&gGsFrame%60==0)std::fprintf(stderr,"bt3: D3D11 frame=%u draws=%u errors=%u\n",gGsFrame,gsDrawCount,errors);
}
bool make_constant(ID3D11Buffer **out,unsigned size){D3D11_BUFFER_DESC d={};d.ByteWidth=size;d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;return DX(dev->CreateBuffer(&d,nullptr,out));}
int init_impl(){
    if(!SDL_Init(SDL_INIT_VIDEO))return 0;window=GsDraw_WindowCreate(0);if(!window)return 0;
    HWND hwnd=(HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
    library=LoadLibraryExW(L"d3d11.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto create=library?(decltype(&D3D11CreateDevice))GetProcAddress(library,"D3D11CreateDevice"):nullptr;if(!create)return 0;
    D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0},level;UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,levels,2,D3D11_SDK_VERSION,&dev,&level,&ctx);
    if(hr==E_INVALIDARG)hr=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,levels+1,1,D3D11_SDK_VERSION,&dev,&level,&ctx);
    if(!DX(hr))return 0;
    D3D11_FEATURE_DATA_D3D11_OPTIONS options={};dev->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS,&options,sizeof(options));
    ctx->QueryInterface(__uuidof(ID3D11DeviceContext1),(void **)&ctx1);rangeConstants=ctx1&&options.ConstantBufferOffsetting;
    IDXGIDevice *device=nullptr;IDXGIAdapter *adapter=nullptr;IDXGIFactory *factory=nullptr;IDXGIFactory5 *factory5=nullptr;
    dev->QueryInterface(__uuidof(IDXGIDevice),(void **)&device);if(!device)return 0;
    device->GetAdapter(&adapter);drop(device);if(!adapter)return 0;
    adapter->GetParent(__uuidof(IDXGIFactory),(void **)&factory);DXGI_ADAPTER_DESC id={};adapter->GetDesc(&id);drop(adapter);if(!factory)return 0;
    factory->QueryInterface(__uuidof(IDXGIFactory5),(void **)&factory5);BOOL allow=FALSE;
    if(factory5){factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&allow,sizeof(allow));drop(factory5);}tearing=allow;
    int w,h;SDL_GetWindowSizeInPixels(window,&w,&h);DXGI_SWAP_CHAIN_DESC desc={};desc.BufferDesc.Width=w;desc.BufferDesc.Height=h;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.OutputWindow=hwnd;desc.Windowed=TRUE;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;desc.Flags=tearing?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0;
    hr=factory->CreateSwapChain(dev,&desc,&swap);
    if(FAILED(hr)){desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;desc.Flags=0;tearing=false;hr=factory->CreateSwapChain(dev,&desc,&swap);}
    factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);drop(factory);swapFlags=desc.Flags;if(!DX(hr)||!swap_resize(w,h))return 0;
    IDXGIDevice1 *latency=nullptr;if(SUCCEEDED(dev->QueryInterface(__uuidof(IDXGIDevice1),(void **)&latency))){latency->SetMaximumFrameLatency(Port_LateInput()?1:2);drop(latency);}
#define VS(blob,out) if(!DX(dev->CreateVertexShader(blob,sizeof(blob),nullptr,&out)))return 0
#define PS(blob,out) if(!DX(dev->CreatePixelShader(blob,sizeof(blob),nullptr,&out)))return 0
    VS(kGsVertDxbc,vs[0]);VS(kVu0VertDxbc,vs[1]);VS(kVu4VertDxbc,vs[2]);VS(kVu6VertDxbc,vs[3]);VS(kFxVertDxbc,fxVs);
    PS(kGsFragDxbc,ps[0]);PS(kGsnzFragDxbc,ps[1]);PS(kGswbFragDxbc,ps[2]);PS(kOutlineFragDxbc,outlinePs);PS(kAlphakeyFragDxbc,keyPs);PS(kDclutFragDxbc,tablePs);PS(kGradeFragDxbc,gradePs);PS(kAvgFragDxbc,avgPs);PS(kCopyFragDxbc,copyPs);
#undef VS
#undef PS
    D3D11_INPUT_ELEMENT_DESC g[]={{"TEXCOORD",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R8G8B8A8_UNORM,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",2,DXGI_FORMAT_R32G32B32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
    D3D11_INPUT_ELEMENT_DESC v[]={{"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(!DX(dev->CreateInputLayout(g,3,kGsVertDxbc,sizeof(kGsVertDxbc),&gsLayout))||!DX(dev->CreateInputLayout(v,3,kVu0VertDxbc,sizeof(kVu0VertDxbc),&vuLayout)))return 0;
    if(!make_constant(&vconstants,512)||!make_constant(&pconstants,PARAM_STRIDE))return 0;
    D3D11_RASTERIZER_DESC r={};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=FALSE;r.ScissorEnable=TRUE;if(!DX(dev->CreateRasterizerState(&r,&raster)))return 0;
    D3D11_DEPTH_STENCIL_DESC z={};z.DepthFunc=D3D11_COMPARISON_ALWAYS;if(!DX(dev->CreateDepthStencilState(&z,&noDepth)))return 0;
    for(int i=0;i<16;i++){D3D11_SAMPLER_DESC s={};s.Filter=i&8?D3D11_FILTER_MIN_MAG_MIP_LINEAR:i&1?D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT:D3D11_FILTER_MIN_MAG_MIP_POINT;
        s.AddressU=i&2?D3D11_TEXTURE_ADDRESS_CLAMP:D3D11_TEXTURE_ADDRESS_WRAP;s.AddressV=i&4?D3D11_TEXTURE_ADDRESS_CLAMP:D3D11_TEXTURE_ADDRESS_WRAP;s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.ComparisonFunc=D3D11_COMPARISON_ALWAYS;s.MaxLOD=i&8?D3D11_FLOAT32_MAX:0;
        if(!DX(dev->CreateSamplerState(&s,&samplers[i])))return 0;}
    replaceBlend=blend_state(15<<16);
    D3D11_BLEND_DESC b={};auto &a=b.RenderTarget[0];a.RenderTargetWriteMask=7;a.BlendEnable=TRUE;a.SrcBlend=a.DestBlend=D3D11_BLEND_ONE;a.BlendOp=D3D11_BLEND_OP_REV_SUBTRACT;a.SrcBlendAlpha=D3D11_BLEND_ONE;a.DestBlendAlpha=D3D11_BLEND_ZERO;a.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    if(!DX(dev->CreateBlendState(&b,&outlineBlend)))return 0;
    a.SrcBlend=D3D11_BLEND_SRC_ALPHA;a.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;a.BlendOp=D3D11_BLEND_OP_ADD;if(!DX(dev->CreateBlendState(&b,&keyBlend)))return 0;
    a.RenderTargetWriteMask=15;a.SrcBlend=D3D11_BLEND_BLEND_FACTOR;a.DestBlend=D3D11_BLEND_INV_BLEND_FACTOR;if(!DX(dev->CreateBlendState(&b,&averageBlend)))return 0;
    white=texture(tex_create(1,1,0,1));uint32_t *pixel=(uint32_t *)std::malloc(4);if(!pixel)return 0;*pixel=~0u;tex_upload(handle(white),0,1,1,0,pixel,4);
    copies();balance=make_texture(1,1,DXGI_FORMAT_R8G8B8A8_UNORM,true);if(!white||!dateCopy||!auxCopy||!feedback||!balance)return 0;
    char name[512];WideCharToMultiByte(CP_UTF8,0,id.Description,-1,name,sizeof(name),nullptr,nullptr);
    if(!Ui_InitD3D11(window,dev,ctx,name))return 0;
    std::fprintf(stderr,"bt3: GPU renderer: direct3d11 native SM5, feature level %x, constant ranges %d, tearing %d\n",level,rangeConstants,tearing);return 1;
}
int init(){
    if(init_impl())return 1;
    if(ctx){ctx->ClearState();ctx->Flush();}
    for(int i=0;i<MAX_TARGETS;i++)target_drop(i);delete white;white=nullptr;delete dateCopy;dateCopy=nullptr;delete auxCopy;auxCopy=nullptr;delete feedback;feedback=nullptr;delete balance;balance=nullptr;
    for(auto &p:pipes){drop(p.blend);drop(p.depth);}pipeCount=0;
    for(auto &p:vs)drop(p);for(auto &p:ps)drop(p);for(auto &p:samplers)drop(p);
    drop(fxVs);drop(outlinePs);drop(keyPs);drop(tablePs);drop(gradePs);drop(avgPs);drop(copyPs);drop(gsLayout);drop(vuLayout);
    drop(vconstants);drop(pconstants);drop(raster);drop(noDepth);drop(replaceBlend);drop(outlineBlend);drop(keyBlend);drop(averageBlend);
    drop(back);drop(backImage);drop(swap);drop(ctx1);drop(ctx);drop(dev);if(library){FreeLibrary(library);library=nullptr;}
    if(window){GsDraw_WindowDestroy();window=nullptr;}std::fprintf(stderr,"bt3: native D3D11 initialization failed\n");return 0;
}
} // namespace
extern "C" void *GsDx11_StripTexture(const void *pixels,int w,int h){
    if(!dev||w<1||h<1)return nullptr;Texture *t=texture(tex_create(w,h,0,1));if(!t)return nullptr;
    ctx->UpdateSubresource(t->image,0,nullptr,pixels,w*4,0);ID3D11ShaderResourceView *view=t->srv;view->AddRef();delete t;return view;
}
extern "C" void GsDx11_StripTextureFree(void *p){if(p)((ID3D11ShaderResourceView *)p)->Release();}
extern "C" GsBackend sDirect3D11Backend={"direct3d11",init,frame,scale_changed,white_tex,tex_create,tex_upload,tex_destroy,slots,target_ensure,target_drop,target_colour,target_aux,target_depth,pipe_get,read_target,target_copy};
#endif
