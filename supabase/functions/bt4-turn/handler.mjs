// Injected dependencies keep authorization and provider handling testable without a live project.
export function createTurnHandler({ env, fetch: request = fetch }) {
  const reply = (body, status = 200) => Response.json(body, {
    status, headers: { 'Cache-Control': 'no-store' },
  });
  return async req => {
    if (req.method !== 'POST') return reply({ error: 'METHOD_NOT_ALLOWED' }, 405);
    const authorization = req.headers.get('authorization') || '';
    if (!/^Bearer \S{1,8192}$/.test(authorization)) return reply({ error: 'AUTH_REQUIRED' }, 401);
    const project = env('SUPABASE_URL');
    const apiKey = env('SUPABASE_ANON_KEY');
    const keyId = env('BT4_TURN_KEY_ID');
    const keyToken = env('BT4_TURN_KEY_TOKEN');
    if (!project || !apiKey || !keyId || !keyToken) return reply({ error: 'RELAY_NOT_CONFIGURED' });
    let generation;
    try {
      if (Number(req.headers.get('content-length')) > 512) return reply({ error: 'BAD_REQUEST' }, 400);
      const text = await req.text();
      if (text.length > 512) return reply({ error: 'BAD_REQUEST' }, 400);
      const body = JSON.parse(text);
      if (!body || Object.keys(body).length !== 1 ||
          !/^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$/.test(body.generation))
        return reply({ error: 'BAD_REQUEST' }, 400);
      generation = body.generation;
    } catch { return reply({ error: 'BAD_REQUEST' }, 400); }
    try {
      // PostgREST verifies the user JWT; SQL then checks ownership, a real opponent and quotas.
      const admission = await request(`${project}/rest/v1/rpc/bt4_turn_admission`, {
        method: 'POST', headers: { authorization, apikey: apiKey, 'Content-Type': 'application/json' },
        body: JSON.stringify({ p_generation: generation }), signal: AbortSignal.timeout(2500),
      });
      if (!admission.ok) return reply({ error: 'AUTH_REQUIRED' }, 403);
      const permission = await admission.json();
      if (permission.allowed !== true)
        return reply({ error: ['RATE_LIMITED', 'NOT_OWNER', 'AUTH_REQUIRED'].includes(permission.error) ? permission.error : 'RELAY_UNAVAILABLE' });
      const result = await request(`https://rtc.live.cloudflare.com/v1/turn/keys/${encodeURIComponent(keyId)}/credentials/generate-ice-servers`, {
        method: 'POST', headers: { Authorization: `Bearer ${keyToken}`, 'Content-Type': 'application/json' },
        body: JSON.stringify({ ttl: 10800 }), signal: AbortSignal.timeout(3500),
      });
      if (!result.ok) return reply({ error: 'RELAY_UNAVAILABLE' });
      const data = await result.json();
      const servers = Array.isArray(data.iceServers) ? data.iceServers : [data.iceServers];
      for (const item of servers) {
        if (!item || typeof item.username !== 'string' || !item.username.length || item.username.length >= 160 ||
            typeof item.credential !== 'string' || !item.credential.length || item.credential.length >= 160) continue;
        const urls = Array.isArray(item.urls) ? item.urls : [item.urls];
        // The existing native TURN transport is IPv4/UDP. Never downgrade a TLS URL.
        if (urls.includes('turn:turn.cloudflare.com:3478?transport=udp'))
          return reply({ server: 'turn.cloudflare.com:3478', username: item.username, credential: item.credential });
      }
      return reply({ error: 'RELAY_UNAVAILABLE' });
    } catch { return reply({ error: 'RELAY_UNAVAILABLE' }); }
  };
}
