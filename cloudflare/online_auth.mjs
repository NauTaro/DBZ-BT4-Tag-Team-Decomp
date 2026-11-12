const enc=new TextEncoder(),dec=new TextDecoder();
const b64=data=>btoa(String.fromCharCode(...new Uint8Array(data))).replace(/=/g,'').replace(/\+/g,'-').replace(/\//g,'_');
const un64=s=>Uint8Array.from(atob(s.replace(/-/g,'+').replace(/_/g,'/')),c=>c.charCodeAt(0));
export async function sha256(data){return [...new Uint8Array(await crypto.subtle.digest('SHA-256',typeof data==='string'?enc.encode(data):data))].map(x=>x.toString(16).padStart(2,'0')).join('');}
const randomToken=()=>{const b=new Uint8Array(32);crypto.getRandomValues(b);return b64(b);};
async function key(secret){if(typeof secret!=='string'||secret.length<40)throw new Error('AUTH_NOT_CONFIGURED');return crypto.subtle.importKey('raw',enc.encode(secret),{name:'HMAC',hash:'SHA-256'},false,['sign','verify']);}
export async function issueAccess(user,secret,now=Date.now()){
    const head=b64(enc.encode(JSON.stringify({alg:'HS256',typ:'JWT'})));
    const body=b64(enc.encode(JSON.stringify({sub:user,aud:'bt4-online-v5',iat:Math.floor(now/1000),exp:Math.floor(now/1000)+900})));
    const signature=await crypto.subtle.sign('HMAC',await key(secret),enc.encode(head+'.'+body));return head+'.'+body+'.'+b64(signature);
}
export async function verifyAccess(token,secret,now=Date.now()){
    try{
        if(typeof token!=='string'||token.length>4096)return null;const pieces=token.split('.');if(pieces.length!==3)return null;
        const head=JSON.parse(dec.decode(un64(pieces[0]))),body=JSON.parse(dec.decode(un64(pieces[1])));
        if(head.alg!=='HS256'||head.typ!=='JWT'||body.aud!=='bt4-online-v5'||typeof body.sub!=='string'||!/^[a-f0-9-]{36}$/.test(body.sub)||!Number.isInteger(body.exp)||!Number.isInteger(body.iat)||body.exp<=now/1000||body.iat>now/1000+30||body.exp-body.iat>900)return null;
        if(!await crypto.subtle.verify('HMAC',await key(secret),un64(pieces[2]),enc.encode(pieces[0]+'.'+pieces[1])))return null;return body.sub;
    }catch{return null;}
}
const legacy={
    primary:{host:'mhgksaiitqoijnatpurf.supabase.co',key:'sb_publishable_T2WjPRgHLMAVgRC6juKLqg_BoKiSgww'},
    secondary:{host:'houhjsfukkkicjejvett.supabase.co',key:'sb_publishable_JZJl6DjcnWM6M3FyqF8Ojw_Gn-HHHPN'}
};
export async function legacyProof(source,token,fetcher=fetch){
    const p=legacy[source];if(!p||typeof token!=='string'||token.length>8192)throw new Error('INVALID_MIGRATION_PROOF');
    const response=await fetcher(`https://${p.host}/auth/v1/user`,{headers:{Authorization:'Bearer '+token,apikey:p.key},signal:AbortSignal.timeout(8000)});
    if(!response.ok)throw new Error('INVALID_MIGRATION_PROOF');const body=await response.json();
    if(typeof body.id!=='string'||!/^[a-f0-9-]{36}$/.test(body.id))throw new Error('INVALID_MIGRATION_PROOF');return body.id;
}
export async function legacyBootstrap(source,token,fetcher=fetch){
    const p=legacy[source];if(!p)throw new Error('INVALID_MIGRATION_PROOF');
    async function rpc(path,body){
        const r=await fetcher(`https://${p.host}/rest/v1/rpc/${path}`,{method:'POST',headers:{Authorization:'Bearer '+token,apikey:p.key,'Content-Type':'application/json'},body:JSON.stringify(body),signal:AbortSignal.timeout(6000)});
        if(!r.ok)throw new Error('PROFILE_MIGRATION_PENDING');return r.json();
    }
    const [name,ranked]=await Promise.all([rpc('bt4_api',{p_op:'nickname_get',p_args:{}}),rpc('bt4_ranked_api',{p_op:'status',p_args:{ruleset:'solo'}})]);
    return {name:name.state==='ready'&&typeof name.name==='string'?name.name:null,ranked};
}
export async function createSession(core,user,secret,oldHash=null){
    const refresh=randomToken(),hash=await sha256(refresh),expires=core.now()+30*86400000;
    // Called inside the coordinator's serialized auth critical section.
    // Retain the previous token briefly: a lost HTTP response must not erase a
    // player's identity. It can only renew the same account during this grace.
    if(oldHash)core.run('UPDATE online_sessions SET expires=min(expires,?) WHERE hash=?',core.now()+120000,oldHash);
    core.run('INSERT INTO online_sessions VALUES(?,?,?)',hash,user,expires);
    return {access_token:await issueAccess(user,secret,core.now()),refresh_token:refresh,expires_in:900,user:{id:user}};
}
