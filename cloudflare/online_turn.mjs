export async function roomTurn(env,fetcher=fetch){
    if(!env.TURN_GATEWAY||!env.TURN_GATEWAY_SECRET)throw new Error('TURN_NOT_CONFIGURED');
    const response=await env.TURN_GATEWAY.fetch('https://turn.internal/internal/v5/turn',{
        method:'POST',headers:{Authorization:'Bearer '+env.TURN_GATEWAY_SECRET},signal:AbortSignal.timeout(6000)});
    if(!response.ok)throw new Error('TURN_UNAVAILABLE');
    const data=await response.json(),servers=Array.isArray(data.iceServers)?data.iceServers:[data.iceServers];
    for(const server of servers.filter(Boolean)){
        const urls=Array.isArray(server.urls)?server.urls:[server.urls];
        const url=urls.find(u=>typeof u==='string'&&/^turn:[A-Za-z0-9.-]+:\d+\?transport=udp$/.test(u));
        if(!url||typeof server.username!=='string'||typeof server.credential!=='string'||server.username.length>128||server.credential.length>128)continue;
        const m=url.match(/^turn:([^:]+):(\d+)/);return {state:'ready',hostname:m[1],udp_port:Number(m[2]),username:server.username,credential:server.credential,expires_at:Math.floor(Date.now()/1000)+10700};
    }
    throw new Error('TURN_UNAVAILABLE');
}
