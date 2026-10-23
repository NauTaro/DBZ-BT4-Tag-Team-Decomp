import {VoiceHub} from './voice.mjs';
// One serialized Durable Object per match. C kernel owns room authority and barriers.
export class SessionKernel {
    constructor(module) {
        this.api=new WebAssembly.Instance(module,{}).exports;
        this.input=new Uint8Array(this.api.memory.buffer,this.api.Kernel_Input(),512);
        this.output=new Uint8Array(this.api.memory.buffer,this.api.Kernel_Output(),512);
    }
    command(id,packet){this.input.set(new Uint8Array(packet));return this.api.Kernel_Command(id);}
    snapshot(id,result,code){
        const ptr=this.api.Kernel_Input();this.input.fill(0);this.input.set(new TextEncoder().encode(code));
        this.api.Kernel_Snapshot(id,result,ptr);return this.output.slice().buffer;
    }
    take(){return this.api.Kernel_TakeFrame()?this.output.slice().buffer:null;}
}
export function createSessionClass(module) {
    return class {
        constructor(ctx,env) {
            this.ctx=ctx;this.env=env;this.kernel=new SessionKernel(module);this.peers=new Map();this.serial=100;this.code='';this.born=Date.now();this.voice=new VoiceHub(id=>this.voiceMember(id));
        }
        voiceMember(id){
            const peer=[...this.peers.values()].find(p=>p.id===id);if(!peer?.joined)return null;
            const seat=this.kernel.api.Kernel_Seat(id);if(seat<0)return null;
            const b=this.kernel.snapshot(id,0,this.code),v=new DataView(b),o=72+seat*40;
            return {id,seat,team:(v.getUint32(o+32,true)>>4)&1,host:id===this.kernel.api.Kernel_Host(),name:new TextDecoder().decode(new Uint8Array(b,o+4,20)).split('\0')[0]};
        }
        async fetch(request) {
            if(request.headers.get('Upgrade')?.toLowerCase()!=='websocket')return new Response('WebSocket required',{status:426});
            if(new URL(request.url).pathname.startsWith('/v3/voice/tag/')){
                const token=request.headers.get('Authorization')?.replace(/^Bearer /,'');
                const peer=[...this.peers.values()].find(p=>p.voiceToken&&p.voiceToken===token);
                return peer?this.voice.accept(request,peer.id):new Response('Invalid voice membership',{status:403});
            }
            if(this.peers.size>=10)return new Response('Room full',{status:409});
            if(Date.now()-this.born>3*3600000)return new Response('Room expired',{status:410});
            this.code=new URL(request.url).pathname.split('/').pop();
            const [client,server]=Object.values(new WebSocketPair());
            const id=++this.serial;server.accept();
            this.peers.set(server,{id,joined:false,window:Date.now(),messages:0,chats:0,ack:0});
            server.addEventListener('message',event=>this.message(server,event.data));
            server.addEventListener('close',()=>{try{server.close(1000,'');}catch{}this.close(server);});
            server.addEventListener('error',()=>this.close(server));
            server.send(this.kernel.snapshot(id,0,this.code));
            return new Response(null,{status:101,webSocket:client});
        }
        broadcast(result=0,only=null){
            this.voice.refresh();
            for(const [ws,p] of this.peers)if(!only||ws===only){try{const packet=this.kernel.snapshot(p.id,result,this.code);new DataView(packet).setUint32(508,p.ack,true);ws.send(packet);}catch{this.close(ws);}}
        }
        message(ws,data){
            const peer=this.peers.get(ws);if(!peer)return;
            if(Date.now()-this.born>3*3600000){this.abort('Room expired');return;}
            if(!(data instanceof ArrayBuffer)||data.byteLength!==512){ws.close(1008,'Invalid packet');return;}
            const now=Date.now();if(now-peer.window>=1000){peer.window=now;peer.messages=0;peer.chats=0;}
            if(++peer.messages>180){ws.close(1008,'Rate limit');return;}
            const view=new DataView(data),cmd=view.getUint32(4,true);
            peer.ack=view.getUint32(508,true);
            if(cmd===20){
                if(this.kernel.api.Kernel_Seat(peer.id)<0)return;
                if(peer.voiceAt&&now-peer.voiceAt<1000)return;peer.voiceAt=now;
                if(!peer.voiceToken)peer.voiceToken=[...crypto.getRandomValues(new Uint8Array(32))].map(b=>b.toString(16).padStart(2,'0')).join('');
                const packet=new Uint8Array(512),out=new DataView(packet.buffer);out.setUint32(0,0x54540007,true);out.setUint32(4,102,true);
                packet.set(new TextEncoder().encode(peer.voiceToken),16);ws.send(packet.buffer);return;
            }
            if(cmd===16){
                if(this.kernel.api.Kernel_Seat(peer.id)<0 || ++peer.chats>2)return;
                const packet=new Uint8Array(data.slice(0));const out=new DataView(packet.buffer);out.setUint32(12,peer.id,true);
                // Text only, maximum 160 UTF-8 bytes; sender identity cannot be forged.
                packet.fill(0,176);
                for(const [other] of this.peers){try{other.send(packet.buffer);}catch{this.close(other);}}return;
            }
            let result;
            try{result=this.kernel.command(peer.id,data);}catch{this.abort('Invalid session state');return;}
            if(result){this.broadcast(result,this.kernel.api.Kernel_Phase()===6?null:ws);return;}
            if(cmd===1){this.kernel.api.Kernel_Seed(crypto.getRandomValues(new Uint32Array(1))[0]||1);peer.joined=true;}
            if(cmd===2)peer.joined=true;
            if(cmd===18&&view.getUint32(12,true)<2)this.kernel.api.Kernel_Seed(crypto.getRandomValues(new Uint32Array(1))[0]||1);
            if(cmd===11){this.frames();}
            else if(cmd===17)return;
            else if(cmd===9){this.broadcast();this.frames();}
            else if(cmd===10)this.broadcast(0,ws);
            else this.broadcast();
        }
        frames(){let packet;while((packet=this.kernel.take()))for(const [ws]of this.peers){try{ws.send(packet);}catch{this.close(ws);}}}
        close(ws){
            const peer=this.peers.get(ws);if(!peer)return;this.peers.delete(ws);
            this.voice.drop(peer.id);
            if(peer.joined){this.kernel.api.Kernel_Leave(peer.id);this.broadcast();}
        }
        abort(why){for(const [ws]of this.peers)ws.close(1011,why);this.peers.clear();}
    };
}
