// Native voice stays outside the PS2 heap, rollback, game clock and input barrier.
#include <windows.h>
#include <wincrypt.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include "voice.h"
#include "spatial_audio.h"
#include "../../third_party/voice/rtc/rtc.h"
#include "../../third_party/voice/opus/opus.h"
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
#include "voice_transport.inc"
using VJson=nlohmann::json;
#include "voice_audio.inc"
extern "C" {int Port_Setting(const char *,int);void Port_SettingSave(const char *,int);void Port_SettingsWrite(void);void Port_SettingText(const char *,const char *,char *,int);void Port_SettingTextSave(const char *,const char *);SDL_AudioDeviceID Port_AudioPeekDevice(void);BOOLEAN WINAPI SystemFunction036(PVOID,ULONG);}
static struct {
    std::mutex lock;SDL_Thread *thread=nullptr;std::atomic<bool> quit{false},pttKey{false},pttPad{false};
    PortVoiceOptions options={};PortVoiceState state={};bool initialized=false;
    int generation=0,sessionMode=0,seat=0,channel=0,allowRoom=0;std::string url,cap,name;
    std::string inputName,outputName;
    float positions[40]={};int worldMode=0,listener=0,worldCount=0;Uint64 worldAt=0;
} v;
static std::mutex signalingLock;
static std::shared_ptr<VoiceWs> signaling;
static std::atomic<unsigned> selfId{0};
static std::vector<std::string> voiceIceServers={"stun:stun.cloudflare.com:3478"};
static void status(const char *s){std::lock_guard<std::mutex>g(v.lock);SDL_strlcpy(v.state.status,s,sizeof(v.state.status));}
static bool fixture(){return getenv("BT4_RENDER_TEST")&&getenv("BT4_VOICE_TEST_FILE");}
static void logVoice(const char *s){if(getenv("BT4_RENDER_TEST")){fprintf(stderr,"bt4 voice: %s\n",s);fflush(stderr);}}
static void post(const VJson &j){std::shared_ptr<VoiceWs> ws;{std::lock_guard<std::mutex>g(signalingLock);ws=signaling;}if(ws)ws->text(j.dump());}
struct VoiceRtcApi {
    HMODULE module=nullptr;bool ready=false;
#define VF(n) decltype(&n) n=nullptr;
    VF(rtcCreatePeerConnection) VF(rtcClosePeerConnection) VF(rtcDeletePeerConnection)
    VF(rtcSetUserPointer) VF(rtcSetLocalDescriptionCallback) VF(rtcSetLocalCandidateCallback)
    VF(rtcSetRemoteDescription) VF(rtcSetLocalDescription) VF(rtcAddRemoteCandidate)
    VF(rtcSetStateChangeCallback) VF(rtcAddTrackEx) VF(rtcSetMessageCallback) VF(rtcSendMessage) VF(rtcIsOpen)
#undef VF
    bool load(){
        if(module)return ready;module=LoadLibraryExW(L"libdatachannel.dll",nullptr,LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)return false;
#define VF(n) n=reinterpret_cast<decltype(n)>(GetProcAddress(module,#n));if(!n)return false;
        VF(rtcCreatePeerConnection) VF(rtcClosePeerConnection) VF(rtcDeletePeerConnection)
        VF(rtcSetUserPointer) VF(rtcSetLocalDescriptionCallback) VF(rtcSetLocalCandidateCallback)
        VF(rtcSetRemoteDescription) VF(rtcSetLocalDescription) VF(rtcAddRemoteCandidate)
        VF(rtcSetStateChangeCallback) VF(rtcAddTrackEx) VF(rtcSetMessageCallback) VF(rtcSendMessage) VF(rtcIsOpen)
#undef VF
        ready=true;return true;
    }
} rtc;
static struct VoiceOpusApi {
    HMODULE module=nullptr;bool ready=false;
#define VF(n) decltype(&n) n=nullptr;
    VF(opus_encoder_create) VF(opus_encoder_destroy) VF(opus_encoder_ctl) VF(opus_encode)
    VF(opus_decoder_create) VF(opus_decoder_destroy) VF(opus_decode)
#undef VF
    bool load(){if(module)return ready;module=LoadLibraryExW(L"libopus-0.dll",nullptr,LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)return false;
#define VF(n) n=reinterpret_cast<decltype(n)>(GetProcAddress(module,#n));if(!n)return false;
        VF(opus_encoder_create) VF(opus_encoder_destroy) VF(opus_encoder_ctl) VF(opus_encode)
        VF(opus_decoder_create) VF(opus_decoder_destroy) VF(opus_decode)
#undef VF
        ready=true;return true;
    }
} opus;
struct VoicePacket{uint16_t seq;std::vector<unsigned char> data;Uint64 at;};
struct VoicePeer {
    std::atomic<unsigned> id{0};std::atomic<int> pc{-1},track{-1},connected{0},muted{0};
    float gain=1.f;int seat=0,team=0;std::string name;
    std::mutex lock;std::deque<VoicePacket> packets;OpusDecoder *decoder=nullptr;
    uint16_t last=0;bool haveLast=false,remoteReady=false;int missing=0;Uint64 speaking=0,start=0;
    std::vector<VJson> candidates;
};
static VoicePeer peers[10];
static VoicePeer *peerFor(unsigned id){for(auto &p:peers)if(p.id==id)return &p;return nullptr;}
static bool newer(uint16_t a,uint16_t b){return (int16_t)(a-b)>0;}
static void receiveAudio(VoicePeer &p,uint16_t seq,const unsigned char *data,size_t size,bool direct,int expectedTrack=-1){
    if(size==0||size>512||p.muted)return;
    {std::lock_guard<std::mutex>g(p.lock);
        if(!p.id||(expectedTrack>=0&&p.track!=expectedTrack))return;
        if(p.haveLast&&!newer(seq,p.last))return;
        for(const auto &q:p.packets)if(q.seq==seq)return;
        if(p.packets.size()>=8)p.packets.pop_front();
        p.packets.push_back({seq,std::vector<unsigned char>(data,data+size),SDL_GetTicks()});
        std::sort(p.packets.begin(),p.packets.end(),[](const VoicePacket &a,const VoicePacket &b){return (int16_t)(a.seq-b.seq)<0;});
    }
    std::lock_guard<std::mutex>g(v.lock);++v.state.received;if(direct)++v.state.direct;else ++v.state.relayed;
}
static void RTC_API rtcAudio(int track,const char *data,int size,void *ptr){
    auto &p=*static_cast<VoicePeer*>(ptr);if(track!=p.track||size<12)return;const auto *b=(const unsigned char*)data;
    if((b[0]>>6)!=2||(b[1]&127)!=111)return;
    int offset=12+4*(b[0]&15);if(offset>size)return;
    if(b[0]&16){if(offset+4>size)return;int words=(b[offset+2]<<8)|b[offset+3];offset+=4+words*4;if(offset>size)return;}
    if(b[0]&32){int padding=b[size-1];if(!padding||padding>size-offset)return;size-=padding;}
    receiveAudio(p,(uint16_t)((b[2]<<8)|b[3]),b+offset,size-offset,true,track);
}
static void RTC_API rtcDescription(int pc,const char *sdp,const char *type,void *ptr){auto &p=*static_cast<VoicePeer*>(ptr);if(p.pc!=pc||!p.id)return;post({{"type","signal"},{"to",p.id.load()},{"signal",{{"type",type},{"sdp",sdp}}}});}
static void RTC_API rtcCandidate(int pc,const char *candidate,const char *mid,void *ptr){auto &p=*static_cast<VoicePeer*>(ptr);if(p.pc!=pc||!p.id)return;if(getenv("BT4_RENDER_TEST")&&candidate&&strstr(candidate," typ relay"))logVoice("TURN relay candidate gathered");post({{"type","signal"},{"to",p.id.load()},{"signal",{{"type","candidate"},{"candidate",candidate},{"mid",mid}}}});}
static void RTC_API voiceRtcState(int pc,rtcState state,void *ptr){auto &p=*static_cast<VoicePeer*>(ptr);if(p.pc!=pc)return;p.connected=state==RTC_CONNECTED;}
static void closePeer(VoicePeer &p){
    int pc=p.pc.exchange(-1);p.track=-1;p.connected=0;
    if(pc>=0){rtc.rtcClosePeerConnection(pc);rtc.rtcDeletePeerConnection(pc);}
    std::lock_guard<std::mutex>g(p.lock);p.packets.clear();if(p.decoder)opus.opus_decoder_destroy(p.decoder);p.decoder=nullptr;p.id=0;p.haveLast=false;p.remoteReady=false;p.candidates.clear();p.missing=0;p.speaking=0;p.start=0;p.gain=1.f;
}
static void openPeer(VoicePeer &p){
    if(!rtc.load()||getenv("BT4_VOICE_FORCE_RELAY"))return;
    std::vector<const char *> servers;for(const auto &server:voiceIceServers)servers.push_back(server.c_str());
    rtcConfiguration cfg={};cfg.iceServers=servers.data();cfg.iceServersCount=(int)servers.size();cfg.disableAutoNegotiation=true;cfg.forceMediaTransport=true;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_VOICE_ICE_RELAY_TEST"))cfg.iceTransportPolicy=RTC_TRANSPORT_POLICY_RELAY;
    int pc=rtc.rtcCreatePeerConnection(&cfg);if(pc<0)return;p.pc=pc;
    rtc.rtcSetUserPointer(pc,&p);rtc.rtcSetLocalDescriptionCallback(pc,rtcDescription);rtc.rtcSetLocalCandidateCallback(pc,rtcCandidate);rtc.rtcSetStateChangeCallback(pc,voiceRtcState);
    rtcTrackInit ti={};ti.direction=RTC_DIRECTION_SENDRECV;ti.codec=RTC_CODEC_OPUS;ti.payloadType=111;ti.ssrc=selfId.load()+0x42540000;ti.mid="voice";ti.name="BT4 Voice";ti.msid="bt4";ti.trackId="voice";
    p.track=rtc.rtcAddTrackEx(pc,&ti);if(p.track<0){closePeer(p);return;}
    rtc.rtcSetUserPointer(p.track,&p);rtc.rtcSetMessageCallback(p.track,rtcAudio);
    if(selfId<p.id)rtc.rtcSetLocalDescription(pc,"offer");
}
static unsigned number(const VJson &j,const char *key,unsigned fallback=0){auto it=j.find(key);return it!=j.end()&&it->is_number_unsigned()?it->get<unsigned>():fallback;}
static std::string str(const VJson &j,const char *key){auto it=j.find(key);return it!=j.end()&&it->is_string()?it->get<std::string>():std::string();}
static void control(const unsigned char *data,size_t size){
    auto j=VJson::parse(data,data+size,nullptr,false);if(!j.is_object())return;
    auto type=str(j,"type");
    if(type=="hello"){
        auto ice=j.find("iceServers");
        if(ice!=j.end()&&ice->is_array()){
            std::vector<std::string> list={"stun:stun.cloudflare.com:3478"};
            auto encode=[](const std::string &value){std::string out;const char hex[]="0123456789ABCDEF";for(unsigned char c:value){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=(char)c;else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;};
            for(const auto &entry:*ice){
                if(!entry.is_object())continue;auto urls=entry.find("urls");if(urls==entry.end())continue;
                std::vector<std::string> values;
                if(urls->is_string())values.push_back(urls->get<std::string>());
                else if(urls->is_array())for(const auto &u:*urls)if(u.is_string())values.push_back(u.get<std::string>());
                std::string user=str(entry,"username"),password=str(entry,"credential");
                if(user.size()>512||password.size()>512)continue;
                for(const auto &url:values){if(url.size()>1024||list.size()>=10)continue;
                    size_t prefix=url.rfind("turn:",0)==0?5:url.rfind("turns:",0)==0?6:0;
                    if(prefix&&!user.empty()&&!password.empty())list.push_back(url.substr(0,prefix)+encode(user)+":"+encode(password)+"@"+url.substr(prefix));
                }
            }
            voiceIceServers=std::move(list);
            if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"bt4 voice ICE: TURN servers=%u\n",(unsigned)(voiceIceServers.size()-1));
        }
        unsigned self=number(j,"self");auto list=j.find("peers");if(!self||list==j.end()||!list->is_array()||list->size()>9)return;selfId=self;
        for(auto &p:peers)if(p.id){bool keep=false;for(const auto &x:*list)if(number(x,"id")==p.id)keep=true;if(!keep)closePeer(p);}
        for(const auto &x:*list){unsigned id=number(x,"id"),seat=number(x,"seat",99);if(!id||id==self||seat>9)continue;
            auto *p=peerFor(id);if(!p){for(auto &q:peers)if(!q.id){p=&q;break;}if(!p)continue;p->id=id;p->muted=0;p->seat=(int)seat;p->team=(int)number(x,"team");p->name=str(x,"name").substr(0,63);int error=0;p->decoder=opus.opus_decoder_create(16000,1,&error);openPeer(*p);}
            else {p->seat=(int)seat;p->team=(int)number(x,"team");p->name=str(x,"name").substr(0,63);}
        }
        {std::lock_guard<std::mutex>g(v.lock);v.state.connected=1;v.state.host=j.contains("host")&&j["host"]==true;v.state.allowRoom=j.contains("allowRoom")&&j["allowRoom"]==true;v.state.channel=1;v.seat=(int)number(j,"seat",v.seat);}
        logVoice("room authorized");
    }else if(type=="signal"){
        auto *p=peerFor(number(j,"from"));auto it=j.find("signal");if(!p||p->pc<0||it==j.end()||!it->is_object())return;auto &s=*it;auto kind=str(s,"type");
        if(kind=="offer"||kind=="answer"){
            auto sdp=str(s,"sdp");if(sdp.size()>14000||sdp.empty())return;
            if(rtc.rtcSetRemoteDescription(p->pc,sdp.c_str(),kind.c_str())<0)return;p->remoteReady=true;
            for(const auto &candidate:p->candidates){auto c=str(candidate,"candidate"),mid=str(candidate,"mid");rtc.rtcAddRemoteCandidate(p->pc,c.c_str(),mid.c_str());}p->candidates.clear();
            if(kind=="offer")rtc.rtcSetLocalDescription(p->pc,"answer");
        }else if(kind=="candidate"){
            auto c=str(s,"candidate"),mid=str(s,"mid");if(c.size()>2048||mid.size()>32)return;
            if(p->remoteReady)rtc.rtcAddRemoteCandidate(p->pc,c.c_str(),mid.c_str());else if(p->candidates.size()<24)p->candidates.push_back(s);
        }
    }
}
static SDL_AudioDeviceID findDevice(bool recording,const std::string &name){
    if(recording&&name=="@none")return 0;
    if(name.empty()&&!recording)return recording?SDL_AUDIO_DEVICE_DEFAULT_RECORDING:SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    int count=0;auto *ids=recording?SDL_GetAudioRecordingDevices(&count):SDL_GetAudioPlaybackDevices(&count);SDL_AudioDeviceID result=recording?SDL_AUDIO_DEVICE_DEFAULT_RECORDING:SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    for(int i=0;i<count;++i){const char *n=SDL_GetAudioDeviceName(ids[i]);if(n&&name==n){result=ids[i];break;}}if(recording){if(name.empty())result=count?SDL_AUDIO_DEVICE_DEFAULT_RECORDING:0;else if(result==SDL_AUDIO_DEVICE_DEFAULT_RECORDING)result=0;}SDL_free(ids);return result;
}
static std::vector<short> testSamples(){
    std::vector<short> result;if(!fixture())return result;SDL_AudioSpec src,dst={SDL_AUDIO_S16,1,16000};Uint8 *data=nullptr,*converted=nullptr;Uint32 len=0;int count=0;
    if(SDL_LoadWAV(getenv("BT4_VOICE_TEST_FILE"),&src,&data,&len)){
        if(SDL_ConvertAudioSamples(&src,data,(int)len,&dst,&converted,&count)){result.resize(count/2);memcpy(result.data(),converted,count);SDL_free(converted);}SDL_free(data);
    }return result;
}
static int voiceThread(void *){
    OpusEncoder *encoder=nullptr;VoiceAudio capture(true),playback(false);
    int generation=-1,error=0;std::shared_ptr<VoiceWs> ws;Uint64 reconnect=0,tick=SDL_GetTicks(),ping=0,stats=0,lastSpeech=0,start=SDL_GetTicks();uint16_t sequence=0;uint32_t timestamp=0;size_t testOffset=0;
    auto samples=testSamples();FILE *testAudio=nullptr;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_VOICE_TEST_RECEIVE_PCM"))testAudio=fopen(getenv("BT4_VOICE_TEST_RECEIVE_PCM"),"wb");
    while(!v.quit){
        PortVoiceOptions options;int next,mode,seat;float positions[40];int worldMode,listener,worldCount;Uint64 worldAt;std::string url,cap,name,inName,outName;
        {std::lock_guard<std::mutex>g(v.lock);options=v.options;next=v.generation;mode=v.sessionMode;seat=v.seat;url=v.url;cap=v.cap;name=v.name;worldMode=v.worldMode;listener=v.listener;worldCount=v.worldCount;worldAt=v.worldAt;memcpy(positions,v.positions,sizeof(positions));inName=v.inputName;outName=v.outputName;}
        if(generation!=next){
            {std::lock_guard<std::mutex>g(signalingLock);signaling.reset();}ws.reset();
            for(auto &p:peers)closePeer(p);
            capture.close();playback.close();
            if(encoder)opus.opus_encoder_destroy(encoder);encoder=nullptr;selfId=0;generation=next;reconnect=0;sequence=0;testOffset=0;start=SDL_GetTicks();
            {std::lock_guard<std::mutex>g(v.lock);v.state.connected=v.state.capture=v.state.count=v.state.transmitting=v.state.speaking=0;lastSpeech=0;v.state.level=0;}
        }
        if(!options.enabled||!mode||url.empty()){status(options.enabled?"Voice ready; join an online room.":"Voice chat is off.");SDL_Delay(20);continue;}
        if(!opus.load()){status("Voice codec is missing. Reinstall the complete launcher.");SDL_Delay(1000);continue;}
        if(!ws&&SDL_GetTicks()>=reconnect){
            status("Connecting voice...");ws=std::make_shared<VoiceWs>(url,cap,seat,name);{std::lock_guard<std::mutex>g(signalingLock);signaling=ws;}
            for(auto &p:peers)closePeer(p);selfId=0;ping=0;
        }
        if(ws&&ws->status<0){status("Voice reconnecting; gameplay continues.");{std::lock_guard<std::mutex>g(signalingLock);signaling.reset();}ws.reset();reconnect=SDL_GetTicks()+2000;{std::lock_guard<std::mutex>g(v.lock);v.state.connected=0;}continue;}
        VoiceWsMessage msg;while(ws&&ws->read(msg)){
            if(!msg.binary)control(msg.bytes.data(),msg.bytes.size());
            else if(msg.bytes.size()>=17&&msg.bytes.size()<=528){auto *b=msg.bytes.data();unsigned magic=0,id=0;uint16_t seq=0,len=0;memcpy(&magic,b,4);memcpy(&id,b+4,4);memcpy(&seq,b+8,2);memcpy(&len,b+10,2);auto *p=peerFor(id);if(p&&magic==0x31563442&&len==msg.bytes.size()-16&&SDL_GetTicks()-msg.at<160)receiveAudio(*p,seq,b+16,len,false);}
        }
        if(ws&&ws->status==1&&SDL_GetTicks()-ping>=2000){post({{"type","ping"}});ping=SDL_GetTicks();}
        if(!selfId){SDL_Delay(3);continue;}
        if(!encoder){
            encoder=opus.opus_encoder_create(16000,1,OPUS_APPLICATION_VOIP,&error);if(!encoder){status("Voice encoder could not start.");SDL_Delay(500);continue;}
            opus.opus_encoder_ctl(encoder,OPUS_SET_BITRATE(24000));opus.opus_encoder_ctl(encoder,OPUS_SET_COMPLEXITY(4));opus.opus_encoder_ctl(encoder,OPUS_SET_PACKET_LOSS_PERC(5));
            if(!fixture())capture.open(inName);
            playback.open(outName);
            if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"bt4 voice devices: backend=winmm capture=%d playback=%d\n",(bool)capture,(bool)playback);
            {std::lock_guard<std::mutex>g(v.lock);v.state.capture=(bool)capture||(!samples.empty()&&inName!="@none");}
            tick=SDL_GetTicks();
        }
        Uint64 now=SDL_GetTicks();if(now<tick){SDL_Delay(2);continue;}if(now-tick>80)tick=now;tick+=20;
        short input[320]={};int got=0;
        if(!samples.empty()&&inName!="@none"){for(int i=0;i<320;++i)input[i]=samples[(testOffset+i)%samples.size()];testOffset+=320;got=640;}
        else if(capture)got=capture.read(input);
        float level=0;for(auto &s:input){float f=s*(options.gain/100.f);s=(short)std::max(-32768.f,std::min(32767.f,f));level+=(s/32768.f)*(s/32768.f);}level=sqrtf(level/320);
        bool held=v.pttKey||v.pttPad;if(fixture())held=(now-start)>2000&&((now-start)%7000)<5000;
        bool talk=!options.muted&&options.gain>0&&got==640&&(options.mode?level>=options.threshold/1000.f:held);
        if(talk&&level>=.008f)lastSpeech=now;
        bool speaking=talk&&lastSpeech&&now-lastSpeech<160;
        unsigned mask=0;for(auto &p:peers)if(p.id&&!(p.connected&&p.track>=0&&rtc.rtcIsOpen(p.track)))mask|=1u<<p.seat;
        if(talk){
            unsigned char packet[528]={};int len=opus.opus_encode(encoder,input,320,packet+16,512);
            if(len>0){unsigned magic=0x31563442,id=selfId;uint16_t n=(uint16_t)len,m=(uint16_t)mask;memcpy(packet,&magic,4);memcpy(packet+4,&id,4);memcpy(packet+8,&sequence,2);memcpy(packet+10,&n,2);memcpy(packet+12,&m,2);
                if(mask&&ws)ws->send(packet,len+16,true);
                unsigned char rtp[524];rtp[0]=0x80;rtp[1]=111;rtp[2]=sequence>>8;rtp[3]=sequence;rtp[4]=timestamp>>24;rtp[5]=timestamp>>16;rtp[6]=timestamp>>8;rtp[7]=timestamp;uint32_t ssrc=selfId+0x42540000;rtp[8]=ssrc>>24;rtp[9]=ssrc>>16;rtp[10]=ssrc>>8;rtp[11]=ssrc;memcpy(rtp+12,packet+16,len);
                for(auto &p:peers)if(p.id&&p.connected&&p.track>=0&&rtc.rtcIsOpen(p.track))rtc.rtcSendMessage(p.track,(const char*)rtp,len+12);
                ++sequence;std::lock_guard<std::mutex>g(v.lock);++v.state.sent;
            }
        }
        timestamp+=960;
        int mixed[320]={};int count=0,direct=0,decodedFrames=0;PortVoicePeer publicPeers[10]={};
        for(auto &p:peers)if(p.id){
            short pcm[320]={};int decoded=0;float proximity=worldMode==mode&&worldCount>0?0.f:1.f;
            if(worldMode==mode&&worldCount>p.seat&&listener>=0&&listener<worldCount&&now-worldAt<500){
                const float *a=positions+listener*4,*b=positions+p.seat*4;
                float dx=a[0]-b[0],dy=a[1]-b[1],dz=a[2]-b[2];
                proximity=PortVoice_ProximityGain(sqrtf(dx*dx+dy*dy+dz*dz));
            }
            p.gain+=(proximity-p.gain)*.12f;if(fabsf(proximity-p.gain)<.0005f)p.gain=proximity;proximity=p.gain;
            {std::lock_guard<std::mutex>g(p.lock);
                while(!p.packets.empty()&&now-p.packets.front().at>180)p.packets.pop_front();
                if(p.muted){p.packets.clear();p.haveLast=false;}
                else if(p.decoder&&!p.packets.empty()){
                    if(!p.start)p.start=now+40;
                    if(now>=p.start){auto q=std::move(p.packets.front());p.packets.pop_front();
                        if(!p.haveLast||newer(q.seq,p.last)){decoded=opus.opus_decode(p.decoder,q.data.data(),(opus_int32)q.data.size(),pcm,320,0);p.last=q.seq;p.haveLast=true;p.missing=0;}
                    }
                }else if(p.decoder&&p.haveLast){if(p.missing++<2)decoded=opus.opus_decode(p.decoder,nullptr,0,pcm,320,0);else p.start=0;}
                if(decoded>0){++decodedFrames;float energy=0;for(int i=0;i<decoded;++i){energy+=fabsf(pcm[i]/32768.f);mixed[i]+=(int)(pcm[i]*proximity);}if(energy/decoded>.008f)p.speaking=now;}
            }
            auto &pub=publicPeers[count++];pub.id=p.id;pub.seat=p.seat;pub.team=p.team;pub.muted=p.muted;pub.speaking=now-p.speaking<250&&p.speaking;pub.direct=p.connected;pub.proximity=proximity;SDL_strlcpy(pub.name,p.name.c_str(),sizeof(pub.name));direct+=pub.direct;
        }
        {short outputPcm[320];for(int i=0;i<320;++i)outputPcm[i]=(short)std::max(-32768.f,std::min(32767.f,mixed[i]*(options.volume/100.f)));if(testAudio)fwrite(outputPcm,sizeof(short),320,testAudio);if(playback)playback.write(outputPcm);}
        {std::lock_guard<std::mutex>g(v.lock);v.state.capture=(bool)capture||(!samples.empty()&&inName!="@none");v.state.decoded+=decodedFrames;v.state.count=count;v.state.transmitting=talk;v.state.speaking=speaking;v.state.level=level;memcpy(v.state.peers,publicPeers,sizeof(publicPeers));
            SDL_strlcpy(v.state.status,!playback?"Voice output unavailable; check your audio device.":!v.state.capture?"Listening; microphone unavailable.":count==0?"Voice connected; waiting for another player.":direct==count?"Voice connected directly.":"Voice connected; relay available.",sizeof(v.state.status));
            if(getenv("BT4_RENDER_TEST")&&now-stats>=2000){fprintf(stderr,"bt4 voice stats: peers=%d sent=%u received=%u direct=%u relay=%u capture=%d played=%u\n",count,v.state.sent,v.state.received,v.state.direct,v.state.relayed,v.state.capture,playback.played());fflush(stderr);stats=now;}
        }
    }
    {std::lock_guard<std::mutex>g(signalingLock);signaling.reset();}ws.reset();for(auto &p:peers)closePeer(p);capture.close();playback.close();if(encoder)opus.opus_encoder_destroy(encoder);if(testAudio)fclose(testAudio);return 0;
}
static void init(){
    std::lock_guard<std::mutex>g(v.lock);if(v.initialized)return;v.initialized=true;
    v.options.enabled=Port_Setting("voice_enabled",0)!=0;v.options.mode=Port_Setting("voice_mode",0)==1;v.options.key=Port_Setting("voice_key",SDL_SCANCODE_F8);v.options.button=Port_Setting("voice_button",-1);
    v.options.gain=std::max(0,std::min(200,Port_Setting("voice_gain",100)));v.options.volume=std::max(0,std::min(150,Port_Setting("voice_volume",100)));v.options.threshold=std::max(1,std::min(200,Port_Setting("voice_threshold",15)));v.options.indicators=Port_Setting("voice_indicators",1)!=0;v.options.muted=Port_Setting("voice_mic_muted",0)!=0;
    char text[256];Port_SettingText("voice_input","",text,sizeof(text));v.inputName=text;Port_SettingText("voice_output","",text,sizeof(text));v.outputName=text;
    SDL_strlcpy(v.state.status,v.options.enabled?"Voice ready; join an online room.":"Voice chat is off.",sizeof(v.state.status));atexit(PortVoice_Shutdown);
}
static void startThread(){if(!v.thread)v.thread=SDL_CreateThread(voiceThread,"BT4 voice audio",nullptr);}
extern "C" void PortVoice_Options(PortVoiceOptions *out){init();std::lock_guard<std::mutex>g(v.lock);*out=v.options;}
extern "C" void PortVoice_OptionsSet(const PortVoiceOptions *o){
    init();{std::lock_guard<std::mutex>g(v.lock);
        if(o->input!=v.options.input||o->output!=v.options.output||o->enabled!=v.options.enabled){++v.generation;v.pttKey=false;v.pttPad=false;}
        if(o->input!=v.options.input){const char *n=o->input>0?SDL_GetAudioDeviceName(o->input):nullptr;v.inputName=o->input==-1?"@none":n?n:"";Port_SettingTextSave("voice_input",v.inputName.c_str());}
        if(o->output!=v.options.output){const char *n=o->output>0?SDL_GetAudioDeviceName(o->output):nullptr;v.outputName=n?n:"";Port_SettingTextSave("voice_output",v.outputName.c_str());}
        v.options=*o;if(o->enabled)startThread();
    }
    const char *keys[]={"voice_enabled","voice_mode","voice_key","voice_button","voice_gain","voice_volume","voice_threshold","voice_indicators","voice_mic_muted"};int values[]={o->enabled,o->mode,o->key,o->button,o->gain,o->volume,o->threshold,o->indicators,o->muted};for(int i=0;i<9;++i)Port_SettingSave(keys[i],values[i]);Port_SettingsWrite();
}
extern "C" void PortVoice_State(PortVoiceState *out){init();std::lock_guard<std::mutex>g(v.lock);*out=v.state;out->enabled=v.options.enabled;out->sessionMode=v.sessionMode;}
extern "C" void PortVoice_Event(const SDL_Event *e){
    init();PortVoiceOptions o;PortVoice_Options(&o);if(!o.enabled)return;
    if((e->type==SDL_EVENT_AUDIO_DEVICE_ADDED||e->type==SDL_EVENT_AUDIO_DEVICE_REMOVED)){std::lock_guard<std::mutex>g(v.lock);++v.generation;v.state.capture=v.state.transmitting=v.state.speaking=0;v.state.level=0;}
    if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST){v.pttKey=false;v.pttPad=false;}
    if((e->type==SDL_EVENT_KEY_DOWN||e->type==SDL_EVENT_KEY_UP)&&e->key.scancode==o.key)v.pttKey=e->type==SDL_EVENT_KEY_DOWN;
    if((e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN||e->type==SDL_EVENT_GAMEPAD_BUTTON_UP)&&e->gbutton.button==o.button)v.pttPad=e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    if(e->type==SDL_EVENT_GAMEPAD_REMOVED)v.pttPad=false;
}
extern "C" void PortVoice_Session(int mode,const char *endpoint,const char *room,const char *cap,int seat,const char *name){
    if(!endpoint||!room||!cap||strlen(cap)!=64||seat<0||seat>9)return;init();
    std::string url=std::string(endpoint)+"/v3/voice/"+(mode==2?"tag/":"duel/")+room;
    std::lock_guard<std::mutex>g(v.lock);if(v.url==url&&v.cap==cap&&v.sessionMode==mode)return;
    v.sessionMode=mode;v.url=url;v.cap=cap;v.seat=seat;v.name=name?name:"Player";++v.generation;if(v.options.enabled)startThread();
}
extern "C" void PortVoice_Leave(int mode){init();std::lock_guard<std::mutex>g(v.lock);if(v.sessionMode==mode){v.sessionMode=0;v.url.clear();v.cap.clear();++v.generation;v.pttKey=false;v.pttPad=false;}}
extern "C" void PortVoice_Mute(unsigned id,int muted){for(auto &p:peers)if(p.id==id)p.muted=muted!=0;}
extern "C" float PortVoice_ProximityGain(float distance){
    /* Voice uses arena units. Keep the near range clear, then fade smoothly over
       a wider part of the stage so distant fighters remain audible without a
       hard cutoff at the old 120-unit limit. */
    return Bt4_AudioFalloff(distance,12.f,360.f);
}
extern "C" void PortVoice_World(int mode,int listener,int count,const float *positions){
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_VOICE_GAME_TEST")&&mode==2){
        static Uint64 started;static int last=-1;Uint64 now=SDL_GetTicks();
        if(!started)started=now;
        int period=getenv("BT4_VOICE_GAME_PERIOD")?atoi(getenv("BT4_VOICE_GAME_PERIOD")):4000;
        if(period<500)period=500;
        int phase=(int)((now-started)/period);
        if(phase!=last){
            PortVoiceOptions o;PortVoice_Options(&o);o.enabled=phase%2==0;o.muted=1;o.input=getenv("BT4_VOICE_GAME_MIC")?-2:-1;
            PortVoice_OptionsSet(&o);
            unsigned char key[32];for(int i=0;i<32;++i)key[i]=(unsigned char)(i+17);
            PortVoice_DuelKey(key,0,"Voice gameplay regression");
            fprintf(stderr,"voice gameplay fixture: phase=%d enabled=%d\n",phase,o.enabled);fflush(stderr);last=phase;
        }
    }

    if(count<0||count>10||!positions)return;init();std::lock_guard<std::mutex>g(v.lock);
    v.worldMode=mode;v.listener=listener;v.worldCount=count;memcpy(v.positions,positions,count*4*sizeof(float));v.worldAt=SDL_GetTicks();
}
extern "C" void PortVoice_DeviceName(int recording,char *out,int size){init();std::lock_guard<std::mutex>g(v.lock);SDL_strlcpy(out,(recording?v.inputName:v.outputName).c_str(),size);}
extern "C" int PortVoice_NewKey(unsigned char out[32]){return SystemFunction036(out,32)!=0;}
extern "C" void PortVoice_DuelKey(const unsigned char key[32],int seat,const char *name){
    char cap[65],room[65];for(int i=0;i<32;++i)snprintf(cap+i*2,3,"%02x",key[i]);
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;unsigned char digest[32];DWORD size=32;
    if(!CryptAcquireContext(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT))return;
    bool ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)&&CryptHashData(hash,(const BYTE*)cap,64,0)&&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0);
    if(hash)CryptDestroyHash(hash);CryptReleaseContext(provider,0);if(!ok)return;
    for(int i=0;i<32;++i)snprintf(room+i*2,3,"%02x",digest[i]);
    const char *root=getenv("BT4_VOICE_SERVER");if(!root)root="https://bt4-tag-team.mellamannau22.workers.dev";PortVoice_Session(1,root,room,cap,seat,name);
}
extern "C" void PortVoice_Shutdown(void){if(!v.initialized)return;v.quit=true;if(v.thread){SDL_WaitThread(v.thread,nullptr);v.thread=nullptr;}}
extern "C" int PortVoice_TestRun(void){
    if(!getenv("BT4_RENDER_TEST")||!getenv("BT4_VOICE_SELFTEST"))return -1;
    init();{std::lock_guard<std::mutex>g(v.lock);v.options.enabled=1;}
    unsigned char key[32];for(int i=0;i<32;++i)key[i]=(unsigned char)(i+17);int seat=getenv("BT4_VOICE_TEST_SEAT")?atoi(getenv("BT4_VOICE_TEST_SEAT")):0;
    const char *tag=getenv("BT4_VOICE_TEST_TAG"),*cap=getenv("BT4_VOICE_TEST_CAP");
    if(tag&&cap)PortVoice_Session(2,getenv("BT4_VOICE_SERVER")?getenv("BT4_VOICE_SERVER"):"https://bt4-tag-team.mellamannau22.workers.dev",tag,cap,seat,seat?"Voice B":"Voice A");
    else PortVoice_DuelKey(key,seat,seat?"Voice B":"Voice A");
    Uint64 begin=SDL_GetTicks();int seconds=getenv("BT4_VOICE_TEST_SECONDS")?atoi(getenv("BT4_VOICE_TEST_SECONDS")):20;
    int lastMicPhase=-1;Uint64 lastMicLog=0;
    while(SDL_GetTicks()-begin<(Uint64)seconds*1000){
        if(getenv("BT4_VOICE_TEST_MUTE")){
            Uint64 elapsed=SDL_GetTicks()-begin;int phase=(int)(elapsed/6000);
            if(phase!=lastMicPhase){PortVoiceOptions o;PortVoice_Options(&o);o.mode=1;o.threshold=1;o.muted=phase==1;o.input=phase==3?-1:-2;PortVoice_OptionsSet(&o);lastMicPhase=phase;}
            if(elapsed-lastMicLog>=1000){PortVoiceState s;PortVoiceOptions o;PortVoice_State(&s);PortVoice_Options(&o);fprintf(stderr,"bt4 mic audit: ms=%llu phase=%d status=%d sent=%u received=%u capture=%d\n",(unsigned long long)elapsed,phase,PortVoice_MicStatus(&o,&s),s.sent,s.received,s.capture);lastMicLog=elapsed;}
        }
        if(getenv("BT4_VOICE_TEST_PROXIMITY")){
            int phase=(int)((SDL_GetTicks()-begin)/8000);float positions[8]={0};positions[4]=phase==0?8.f:phase==1?120.f:64.f;
            PortVoice_World(1,seat,2,positions);
        }
        SDL_Delay(20);
    }
    PortVoiceState s;PortVoice_State(&s);fprintf(stderr,"bt4 voice test: sent=%u received=%u direct=%u relay=%u peers=%d decoded=%u\n",s.sent,s.received,s.direct,s.relayed,s.count,s.decoded);PortVoice_Shutdown();return s.sent>20&&s.received>20&&s.decoded>20?0:4;
}
