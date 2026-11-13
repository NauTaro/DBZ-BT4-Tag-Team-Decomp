/* Cloudflare authority, shared by native duel, lobbies, Tag Team and voice.
 * HTTP is used for identity exchange; signaling uses one hibernating WebSocket.
 * Native combat packets continue directly between players or through TURN.
 */
#include <map>
#ifndef BT4_CLOUDFLARE_HOST
#define BT4_CLOUDFLARE_HOST "bt4-online-v5.mellamannau22.workers.dev"
#endif
static constexpr const char *cloudHost=BT4_CLOUDFLARE_HOST;
static constexpr const char *cloudUrl="https://" BT4_CLOUDFLARE_HOST;
static bool cloudBackend(){const char *v=SDL_getenv("BT4_ONLINE_BACKEND");return !v||strcmp(v,"supabase");}
static bool cloudStoreSession(const Sj &reply){
    auto &i=identities[2];auto u=reply.find("user");
    auto access=str(reply,"access_token"),refresh=str(reply,"refresh_token");
    if(u==reply.end()||!u->is_object()||str(*u,"id").size()!=36||access.empty()||access.size()>8192||refresh.empty()||refresh.size()>1024)return false;
    i.access=std::move(access);i.refresh=std::move(refresh);i.user=str(*u,"id");i.renewAt=SDL_GetTicks()+840000;saveIdentityFor(i);return true;
}
static bool cloudAuthenticate(){
    auto &i=identities[2];if(!i.access.empty()&&SDL_GetTicks()<i.renewAt)return true;
    if(!identitySlotFor(i,"cloudflare-v5"))return false;
    Sj answer;
    const bool recovering=!i.refresh.empty();
    if(recovering){
        if(http("/v5/auth/refresh",{{"refresh_token",i.refresh}},false,answer,false,cloudHost))return cloudStoreSession(answer);
        // A rejected local token must not permanently block every menu. Recover
        // using an existing, authenticated legacy identity, never a nickname or
        // a fresh anonymous account. Temporary transport failures keep the token.
        if(lastHttpStatus!=401||!answer.is_object()||str(answer,"error")!="AUTH_REQUIRED")return false;
        i.access.clear();i.renewAt=0;
    }
    const int previous=activeProject;
    bool found=false,success=false;
    for(int project:{1,0}){
        activeProject=project;
        if(!identitySlot()||currentIdentity().refresh.empty())continue;
        found=true;
        // Refresh only a pre-existing identity, never sign up on the old host.
        Sj old;
        if(!http("/auth/v1/token?grant_type=refresh_token",{{"refresh_token",currentIdentity().refresh}},false,old)){
            const auto error=old.is_object()?str(old,"error_code"):std::string();
            // A stale secondary cache must not hide a valid primary session.
            if(error=="refresh_token_not_found"||error=="refresh_token_already_used"||error=="session_not_found")continue;
            break;
        }
        if(str(old,"access_token").empty()||str(old,"refresh_token").empty())break;
        currentIdentity().access=str(old,"access_token");currentIdentity().refresh=str(old,"refresh_token");saveIdentity();
        auto user=old.find("user");
        const auto subject=user!=old.end()&&user->is_object()?str(*user,"id"):std::string();
        // New caches record the established account ID. Recovery is forbidden
        // from replacing it with another account held by this installation.
        if(recovering&&!i.user.empty()&&subject!=i.user)continue;
        if(http("/v5/auth/exchange",{{"source",project==1?"secondary":"primary"},{"access_token",currentIdentity().access}},false,answer,false,cloudHost)){
            auto account=answer.find("user");
            if(account!=answer.end()&&account->is_object()&&
               (!recovering||i.user.empty()||str(*account,"id")==i.user))success=cloudStoreSession(answer);
        }
        break;
    }
    activeProject=previous;
    if(found||recovering)return success;
    return http("/v5/auth/start",Sj::object(),false,answer,false,cloudHost)&&cloudStoreSession(answer);
}
static int cloudEntry(void*){
    const bool diagnostics=SDL_getenv("BT4_SIGNAL_DIAGNOSTICS")!=nullptr;
    const bool forceHttp=SDL_getenv("BT4_SIGNAL_HTTPS_ONLY")!=nullptr;
    std::unique_ptr<VoiceWs> socket;bool ready=false,onlineMenu=false,authSent=false;
    Uint64 retry=0,heartbeat=0,authAt=0;unsigned failures=0,cleanupSerial=0xf0000000u;
    Uint64 pollAt=0;std::array<bool,2> searching={false,false};
    std::map<uint64_t,std::pair<Job,Uint64>> pending;
    auto key=[](int c,unsigned s){return (uint64_t)(unsigned)c<<32|s;};
    auto current=[](const Job &j){std::lock_guard<std::mutex> guard(state.lock);return j.session==state.sessions[j.consumer];};
    auto pin=[](const std::string &g){std::lock_guard<std::mutex> guard(state.lock);state.roomGeneration=g;};
    auto complete=[&](const Job &job,const Sj &body){
        if(str(body,"error")=="UPDATE_REQUIRED")Update_SetOnlineRequired(1);
        const bool admission=bt4route::admission(job.op);
        const auto generation=str(body,"generation");
        if(!current(job)){
            if(admission&&(!generation.empty()||str(body,"state")=="waiting")){
                Job cleanup;cleanup.consumer=job.consumer;cleanup.serial=++cleanupSerial;cleanup.op=generation.empty()?"cancel":"leave";
                cleanup.args=generation.empty()?Sj::object():Sj{{"generation",generation}};
                std::lock_guard<std::mutex> guard(state.lock);cleanup.session=state.sessions[job.consumer];state.jobs.push_front(std::move(cleanup));
            }return;
        }
        if(!generation.empty()&&!body.contains("error"))pin(generation);
        if(job.consumer<2){
            if(job.op=="queue"||job.op=="ranked_queue")searching[job.consumer]=str(body,"state")=="waiting";
            if(job.op=="cancel"||job.op=="leave"||!generation.empty())searching[job.consumer]=false;
        }
        if(job.op=="presence"&&!body.contains("error"))onlineMenu=job.args.value("active",false);
        if(job.op=="leave"&&!body.contains("error")){
            std::lock_guard<std::mutex> guard(state.lock);
            if(str(job.args,"generation").empty()||str(job.args,"generation")==state.roomGeneration)state.roomGeneration.clear();
        }
        emit(job.consumer,{{"event","reply"},{"serial",job.serial},{"project",cloudUrl},{"body",body}});
    };
    auto disconnect=[&](){
        if(diagnostics)fprintf(stderr,"bt4: Cloudflare reconnect ready=%d pending=%u\n",(int)ready,(unsigned)pending.size());
        // The server can reject a cached access token before the local renewal
        // deadline. Refresh it after a failed socket authentication instead of
        // presenting the same rejected token on every reconnect.
        if(authSent&&!ready){identities[2].access.clear();identities[2].renewAt=0;}
        socket.reset();ready=false;authSent=false;state.status=0;
        for(auto &item:pending){Job j=std::move(item.second.first);j.notBefore=SDL_GetTicks()+1000;
            std::lock_guard<std::mutex> guard(state.lock);if(state.jobs.size()<64)state.jobs.push_front(std::move(j));}
        pending.clear();retry=SDL_GetTicks()+std::min(10000u,500u<<std::min(4u,failures++));
    };
    while(!state.quit){
        auto now=SDL_GetTicks();bool wanted;
        {std::lock_guard<std::mutex> guard(state.lock);
            wanted=onlineMenu||state.realtime||!state.jobs.empty()||!pending.empty();
            for(const auto &g:state.releasedGenerations)if(g==state.roomGeneration)state.roomGeneration.clear();state.releasedGenerations.clear();}
        if(wanted&&!socket&&now>=retry){
            if(cloudAuthenticate()){
                if(forceHttp){retry=now+60000;state.status=1;}
                else{
                // VoiceWs normalizes HTTPS for curl; WinHTTP's URL parser
                // accepts https://, not the wss:// scheme used by browsers.
                socket=std::make_unique<VoiceWs>(std::string(cloudUrl)+"/v5/socket","",0,"",false,wake,nullptr);
                if(diagnostics)fprintf(stderr,"bt4: Cloudflare opening websocket\n");
                authAt=now;heartbeat=now+20000;
                }
            }else{if(diagnostics)fprintf(stderr,"bt4: Cloudflare identity unavailable HTTP=%ld\n",lastHttpStatus);state.status=-1;retry=now+5000;}
        }
        if(socket&&socket->status<0){disconnect();continue;}
        if(socket&&socket->status==1&&!authSent){socket->text(Sj{{"op","authenticate"},{"access_token",identities[2].access}}.dump());authSent=true;authAt=now;if(diagnostics)fprintf(stderr,"bt4: Cloudflare socket authentication queued\n");}
        if(socket&&authSent){
            VoiceWsMessage packet;
            while(socket->read(packet)){
                if(packet.binary)continue;std::string raw(packet.bytes.begin(),packet.bytes.end());if(raw=="pong")continue;
                auto e=Sj::parse(raw,nullptr,false);if(!e.is_object())continue;auto event=str(e,"event");auto body=e.find("body");if(body==e.end()||!body->is_object())continue;
                if(diagnostics)fprintf(stderr,"bt4: Cloudflare event=%s consumer=%d serial=%u error=%s\n",event.c_str(),e.value("consumer",-1),e.value("serial",0u),str(*body,"error").c_str());
                if(event=="bt4.ready"){
                    ready=true;state.status=1;failures=0;
                    for(int c=0;c<3;c++)emit(c,{{"event",event},{"body",*body}});
                }else if(event=="reply"){
                    auto c=e.find("consumer"),s=e.find("serial");if(c==e.end()||s==e.end()||!c->is_number_integer()||!s->is_number_unsigned())continue;
                    auto it=pending.find(key(c->get<int>(),s->get<unsigned>()));if(it==pending.end())continue;
                    Job j=std::move(it->second.first);pending.erase(it);complete(j,*body);
                }else if(event=="bt4.signal"){
                    auto channel=str(*body,"channel");int c=channel=="duel"?0:channel=="tag"?1:channel=="voice"?2:-1;
                    if(c>=0)emit(c,{{"event",event},{"body",*body}});
                }else if(event=="bt4.room"||event=="bt4.closed"){
                    for(int c=0;c<3;c++)emit(c,{{"event",event},{"body",*body}});
                }
            }
        }
        if(socket&&authSent&&!ready&&now>authAt+12000){disconnect();continue;}
        if(ready&&now>=heartbeat){socket->text("ping");heartbeat=now+20000;}
        if(ready&&now>=identities[2].renewAt){
            if(cloudAuthenticate()){socket->text(Sj{{"op","authenticate"},{"access_token",identities[2].access}}.dump());heartbeat=now+20000;}
            else{disconnect();continue;}
        }
        bool expired=false;for(const auto &p:pending)if(now>p.second.second+12000){expired=true;break;}
        if(expired){disconnect();continue;}
        Job job;bool have=false;
        // A proxy may permit HTTPS but reject WebSocket upgrades. Retained
        // descriptors/signals make that fallback usable without lost offers.
        bool httpFallback=!ready&&!identities[2].access.empty()&&(forceHttp||now>=authAt+3000);
        if((ready||httpFallback)&&pending.size()<24){std::lock_guard<std::mutex> guard(state.lock);
            auto it=std::find_if(state.jobs.begin(),state.jobs.end(),[&](const Job &j){return j.notBefore<=now;});
            if(it!=state.jobs.end()){job=std::move(*it);state.jobs.erase(it);have=true;}}
        if(have){
            if(!current(job)&&job.op!="leave"&&job.op!="cancel"&&job.op!="ranked_report"&&job.op!="match_report")continue;
            if(bt4route::admission(job.op)&&Update_OnlineRequired()){complete(job,Sj{{"error","UPDATE_REQUIRED"}});continue;}
            if(bt4route::admission(job.op))job.args["_client"]={{"version",BT4_CANONICAL_VERSION},{"capability",2}};
            if(httpFallback){
                Sj body;bool ok=cloudAuthenticate()&&http("/v5/api",{{"op",job.op},{"args",job.args}},true,body,false,cloudHost,&identities[2].access);
                if(!ok&&lastHttpStatus==401&&body.is_object()&&str(body,"error")=="AUTH_REQUIRED"){
                    identities[2].access.clear();identities[2].renewAt=0;
                    ok=cloudAuthenticate()&&http("/v5/api",{{"op",job.op},{"args",job.args}},true,body,false,cloudHost,&identities[2].access);
                }
                if(!ok&&!body.is_object())body={{"error","SIGNALING_UNAVAILABLE"}};
                complete(job,body);state.status=ok?1:-1;
            }else{
            socket->text(Sj{{"op",job.op},{"args",job.args},{"serial",job.serial},{"consumer",job.consumer}}.dump());
            if(diagnostics)fprintf(stderr,"bt4: Cloudflare command=%s consumer=%d serial=%u\n",job.op.c_str(),job.consumer,job.serial);
            pending[key(job.consumer,job.serial)]={std::move(job),now};
            }
        }
        if(httpFallback&&now>=pollAt){
            pollAt=now+2000;
            for(int c=0;c<2;c++)if(searching[c]){
                Sj body;if(http("/v5/api",{{"op","get"},{"args",Sj::object()}},true,body,false,cloudHost,&identities[2].access)&&!str(body,"generation").empty()){
                    searching[c]=false;pin(str(body,"generation"));emit(c,{{"event","bt4.room"},{"body",body}});
                }
            }
        }
        SDL_WaitSemaphoreTimeout(state.wake,have?0:20);
    }
    socket.reset();state.status=0;return 0;
}
