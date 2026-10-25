// Voice signaling and bounded Opus relay. Gameplay never waits for this channel.
// WebRTC media goes directly between authorized peers when ICE succeeds.
const MAGIC=0x31563442;
const safeName=value=>String(value||'Player').replace(/[\x00-\x1f\x7f]/g,'').slice(0,32);
export class VoiceHub {
    constructor(lookup,duel=false,env={}){this.env=env;this.iceServers=[];this.iceUntil=0;this.lookup=lookup;this.duel=duel;this.peers=new Map();this.allowRoom=true;}
    allowed(a,b){return a.id!==b.id;}
    async prepareIce(){
        if(Date.now()<this.iceUntil)return;
        this.iceUntil=Date.now()+60000;
        if(!this.env.TURN_KEY_ID||!this.env.TURN_KEY_TOKEN)return;
        try{
            const response=await fetch(`https://rtc.live.cloudflare.com/v1/turn/keys/${this.env.TURN_KEY_ID}/credentials/generate-ice-servers`,{
                method:'POST',signal:AbortSignal.timeout(4000),headers:{Authorization:`Bearer ${this.env.TURN_KEY_TOKEN}`,'Content-Type':'application/json'},body:JSON.stringify({ttl:10800})});
            if(!response.ok)return;
            const body=await response.json(),list=Array.isArray(body.iceServers)?body.iceServers:body.iceServers?[body.iceServers]:[];
            this.iceServers=list.filter(x=>x&&typeof x==='object'&&x.urls).map(x=>({urls:x.urls,...(typeof x.username==='string'?{username:x.username,credential:x.credential}:{})}));
            this.iceUntil=Date.now()+9000000;
        }catch{}
    }
    async accept(request,id){
        const meta=this.lookup(id);if(!meta)return new Response('Not a room member',{status:403});
        if([...this.peers.values()].some(p=>p.id===id))return new Response('Voice already connected',{status:409});
        await this.prepareIce();
        const [client,server]=Object.values(new WebSocketPair());server.accept();
        this.peers.set(server,{...meta,id,channel:'proximity',window:Date.now(),messages:0,controls:0,roster:''});
        server.addEventListener('message',e=>this.message(server,e.data));
        server.addEventListener('close',()=>this.close(server));server.addEventListener('error',()=>this.close(server));
        this.refresh();return new Response(null,{status:101,webSocket:client});
    }
    close(ws){if(this.peers.delete(ws)){try{ws.close(1000,'');}catch{}this.refresh();}}
    drop(id){for(const [ws,p]of this.peers)if(p.id===id)this.close(ws);}
    refresh(){
        for(const [ws,p]of this.peers){const m=this.lookup(p.id);if(!m){this.peers.delete(ws);try{ws.close(1000,'Left game room');}catch{}continue;}Object.assign(p,m);}
        for(const [ws,p]of this.peers){
            const roster={type:'hello',iceServers:this.iceServers,self:p.id,seat:p.seat,host:!!p.host,allowRoom:this.allowRoom,channel:p.channel,
                peers:[...this.peers.values()].filter(q=>this.allowed(p,q)).map(q=>({id:q.id,seat:q.seat,team:q.team,name:safeName(q.name)}))};
            const text=JSON.stringify(roster);if(text!==p.roster){p.roster=text;try{ws.send(text);}catch{this.peers.delete(ws);}}
        }
    }
    message(ws,data){
        const p=this.peers.get(ws);if(!p||!this.lookup(p.id)){this.close(ws);return;}
        const now=Date.now();if(now-p.window>=1000){p.window=now;p.messages=p.controls=0;}
        if(++p.messages>240){this.close(ws);return;}
        if(data instanceof ArrayBuffer){
            if(data.byteLength<17||data.byteLength>528){this.close(ws);return;}
            const v=new DataView(data);if(v.getUint32(0,true)!==MAGIC||v.getUint16(10,true)!==data.byteLength-16){this.close(ws);return;}
            const mask=v.getUint16(12,true);v.setUint32(4,p.id,true);
            for(const [other,q]of this.peers)if((mask&(1<<q.seat))&&this.allowed(p,q)){try{other.send(data);}catch{this.close(other);}}
            return;
        }
        if(typeof data!=='string'||data.length>16384||++p.controls>160){this.close(ws);return;}
        let m;try{m=JSON.parse(data);}catch{this.close(ws);return;}
        if(m.type==='ping'){ws.send('{"type":"pong"}');return;}
        if(m.type==='channel')return;
        if(m.type==='policy')return;
        if(m.type!=='signal'||!Number.isSafeInteger(m.to)||typeof m.signal!=='object'||m.signal===null)return;
        const s=m.signal;if(!['offer','answer','candidate'].includes(s.type))return;
        if(s.type==='candidate'&&(typeof s.candidate!=='string'||s.candidate.length>2048||typeof s.mid!=='string'||s.mid.length>32))return;
        if(s.type!=='candidate'&&(typeof s.sdp!=='string'||s.sdp.length>14000))return;
        for(const [other,q]of this.peers)if(q.id===m.to&&this.allowed(p,q)){
            try{other.send(JSON.stringify({type:'signal',from:p.id,signal:s}));}catch{this.close(other);}break;
        }
    }
}
export class VoiceDuelRoom {
    constructor(ctx,env){this.members=new Map();this.born=Date.now();this.voice=new VoiceHub(id=>this.members.get(id),true,env);}
    async fetch(request){
        if(Date.now()-this.born>3*3600000)return new Response('Voice room expired',{status:410});
        const cap=request.headers.get('Authorization')?.replace(/^Bearer /,'')||'';
        if(!/^[a-f0-9]{64}$/.test(cap))return new Response('Invalid voice capability',{status:403});
        const hash=[...new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(cap)))].map(b=>b.toString(16).padStart(2,'0')).join('');
        if(new URL(request.url).pathname.split('/').pop()!==hash)return new Response('Invalid voice room',{status:403});
        const seat=Number(request.headers.get('X-BT4-Seat'));
        if(!Number.isInteger(seat)||seat<0||seat>1)return new Response('Invalid seat',{status:400});
        let name='Player';try{name=decodeURIComponent(request.headers.get('X-BT4-Name')||'Player');}catch{}
        this.members.set(seat+1,{id:seat+1,seat,team:0,host:seat===0,name:safeName(name)});
        return this.voice.accept(request,seat+1);
    }
}
