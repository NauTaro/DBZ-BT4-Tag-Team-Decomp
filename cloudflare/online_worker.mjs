import {OnlineCore,clientAtLeast} from './online_core.mjs';
import {verifyAccess,legacyProof,legacyBootstrap,createSession,sha256} from './online_auth.mjs';
import {roomTurn} from './online_turn.mjs';
import {importRows} from './online_migration.mjs';

const json=(body,status=200)=>Response.json(body,{status,headers:{'Cache-Control':'no-store'}});
async function readJson(request,limit=24576){
    const reader=request.body?.getReader();if(!reader)return {};
    let total=0;const chunks=[];
    while(true){const r=await reader.read();if(r.done)break;total+=r.value.length;if(total>limit){await reader.cancel();throw new Error('BAD_REQUEST');}chunks.push(r.value);}
    const body=new Uint8Array(total);let offset=0;for(const c of chunks){body.set(c,offset);offset+=c.length;}
    return JSON.parse(new TextDecoder().decode(body));
}
const bearer=request=>(request.headers.get('Authorization')||'').replace(/^Bearer /,'');
function safeEqual(a,b){if(typeof a!=='string'||typeof b!=='string'||a.length!==b.length)return false;let mismatch=0;for(let n=0;n<a.length;n++)mismatch|=a.charCodeAt(n)^b.charCodeAt(n);return !mismatch;}
const ADMIT=new Set(['create','join','queue','ranked_queue']);

export class OnlineCoordinator {
    constructor(ctx,env){
        this.ctx=ctx;this.env=env;this.core=new OnlineCore(ctx.storage.sql);
        this.authTail=Promise.resolve();this.turnPending=new Map();
        ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('ping','pong'));
        ctx.blockConcurrencyWhile(async()=>{if(await ctx.storage.getAlarm()===null)await ctx.storage.setAlarm(Date.now()+600000);});
    }
    budget(subject,operation,maximum=120){
        const minute=Math.floor(Date.now()/60000);
        const row=this.core.one('INSERT INTO online_budgets VALUES(?,?,?,1) ON CONFLICT(subject,operation,minute) DO UPDATE SET n=n+1 RETURNING n',subject,operation,minute);
        if(row.n>maximum)throw new Error('RATE_LIMITED');
    }
    async user(request){return verifyAccess(bearer(request),this.env.AUTH_SECRET);}
    async execute(user,op,args){
        if(op!=='turn_credentials')return this.dispatch(user,op,args);
        const member=this.core.member(user);if(!member||member.generation!==args?.generation)throw new Error('NOT_A_MEMBER');
        const cached=this.core.one('SELECT body,expires FROM online_turn WHERE user_id=? AND generation=?',user,member.generation);
        if(cached&&cached.expires>Date.now()+120000)return JSON.parse(cached.body);
        const id=user+':'+member.generation;if(this.turnPending.has(id))return this.turnPending.get(id);
        this.budget(user,'turn_credentials',4);
        const pending=(async()=>{
            const result=await roomTurn(this.env);
            const current=this.core.member(user);if(!current||current.generation!==member.generation)throw new Error('NOT_A_MEMBER');
            this.core.run('INSERT INTO online_turn VALUES(?,?,?,?) ON CONFLICT(user_id,generation) DO UPDATE SET body=excluded.body,expires=excluded.expires',user,member.generation,JSON.stringify(result),result.expires_at*1000);
            return result;
        })().finally(()=>this.turnPending.delete(id));
        this.turnPending.set(id,pending);return pending;
    }
    dispatch(user,op,args){
        const committed=this.ctx.storage.transactionSync(()=>{
            this.budget(user,op,op==='signal'?300:op==='lease'||op==='get'?120:60);
            if(ADMIT.has(op)&&this.core.one("SELECT value FROM online_meta WHERE key='admission'")?.value!=='open')throw new Error('MIGRATION_IN_PROGRESS');
            const minimum=this.core.one("SELECT value FROM online_meta WHERE key='minimum_client'")?.value;
            if(ADMIT.has(op)&&minimum&&(!clientAtLeast(args?._client?.version,minimum)||args?._client?.capability!==2))throw new Error('UPDATE_REQUIRED');
            if(this.core.one("SELECT 1 n FROM online_legacy WHERE table_name='banned_users' AND json_extract(body,'$.user_id')=? LIMIT 1",user))throw new Error('ACCOUNT_UNAVAILABLE');
            return {reply:this.core.command(user,op,args),events:this.core.events};
        });
    if (committed.events.length) {
      const byUser = /* @__PURE__ */ new Map();
      for (const e of committed.events) {
        const list = byUser.get(e.user);
        if (list) list.push(e);
        else byUser.set(e.user, [e]);
      }
      for (const ws of this.ctx.getWebSockets()) {
        const u = ws.deserializeAttachment()?.user;
        const list = u && byUser.get(u);
        if (!list) continue;
        for (const e of list) try {
          ws.send(JSON.stringify({ event: e.event, body: e.body }));
        } catch {
        }
      }
    }
        return committed.reply;
    }
    async auth(path,body,ip){
        // All token rotations are serialized across awaits; one refresh token can be consumed only once.
        const previous=this.authTail;let release;this.authTail=new Promise(resolve=>release=resolve);await previous;
        try{
            this.budget(ip,'auth',10);let user,oldHash=null;
            if(path==='/v5/auth/refresh'){
                if(typeof body.refresh_token!=='string'||body.refresh_token.length>256)throw new Error('AUTH_REQUIRED');
                oldHash=await sha256(body.refresh_token);user=this.core.one('SELECT user_id FROM online_sessions WHERE hash=? AND expires>?',oldHash,Date.now())?.user_id;
                if(!user)throw new Error('AUTH_REQUIRED');
            }else if(path==='/v5/auth/exchange'){
                const subject=await legacyProof(body.source,body.access_token);
                const alias=this.core.one('SELECT user_id FROM online_aliases WHERE source=? AND subject=?',body.source,subject);
                if(alias)user=alias.user_id;
                else{
                    // Accounts created while the old service drained prove
                    // ownership with its Auth API, never a submitted player ID.
              let profile = null;
          try { profile = await legacyBootstrap(body.source, body.access_token); } catch (err) { profile = null; }
          const cutTo = (s, maxBytes) => { let out = "", n = 0; for (const ch of s) { const w = new TextEncoder().encode(ch).length; if (n + w > maxBytes) break; out += ch; n += w; } return out; };
          this.ctx.storage.transactionSync(() => {
            this.core.run("INSERT OR IGNORE INTO online_users VALUES(?,?)", subject, Date.now());
            this.core.run("INSERT INTO online_aliases VALUES(?,?,?) ON CONFLICT(source,subject) DO NOTHING", body.source, subject, subject);
            let finalName = null;
            const wanted = profile && typeof profile.name === "string" ? profile.name.normalize("NFKC").trim() : "";
            if (wanted.length) {
              const stem = cutTo(wanted, 16);
              const candidates = [wanted, ...[2, 3, 4, 5, 6, 7, 8, 9].map((i) => stem + "~" + i)];
              for (const cand of candidates) {
                try { this.core.command(subject, "nickname_set", { name: cand }); finalName = cand; break; }
                catch (err) { if (!(err instanceof Error && (err.message === "NICKNAME_TAKEN" || err.message === "INVALID_NICKNAME"))) throw err; }
              }
            }
            const r = profile && profile.ranked && typeof profile.ranked === "object" ? profile.ranked : null;
            if (body.source === "secondary" && r && Number.isInteger(r.rating) && r.rating >= 0 && r.rating <= 1e5 && Number.isInteger(r.wins) && Number.isInteger(r.losses))
              this.core.run("INSERT OR IGNORE INTO ranked_players VALUES(?,'solo',?,?,?,?)", subject, finalName ?? cutTo(wanted, 19), r.rating, r.wins, r.losses);
          });
          user=subject;

                }
            }else{
                if(this.core.one("SELECT value FROM online_meta WHERE key='admission'")?.value!=='open')throw new Error('MIGRATION_IN_PROGRESS');
                user=crypto.randomUUID();this.core.run('INSERT INTO online_users VALUES(?,?)',user,Date.now());
            }
            return await createSession(this.core,user,this.env.AUTH_SECRET,oldHash);
        }finally{release();}
    }
    async fetch(request){
        const url=new URL(request.url),path=url.pathname;
        try{
            if(path==='/v5/health')return json({service:'BT4decomp Online',schema:5,admission:this.core.one("SELECT value FROM online_meta WHERE key='admission'")?.value||'closed'});
            if(path.startsWith('/v5/admin/')){
                if(!this.env.ADMIN_SECRET||!safeEqual(bearer(request),this.env.ADMIN_SECRET))return json({error:'AUTH_REQUIRED'},401);
                const body=await readJson(request,1048576);
                if(path==='/v5/admin/import'||path==='/v5/admin/sync'){
                    if(await sha256(JSON.stringify(body.rows))!==body.digest)throw new Error('IMPORT_CHECKSUM');
                    return json(this.ctx.storage.transactionSync(()=>importRows(this.core,body,path.endsWith('/sync'))));
                }
                if(path==='/v5/admin/inventory')return json({names:this.core.one('SELECT count(*) n FROM online_names').n,users:this.core.one('SELECT count(*) n FROM online_users').n,
                    ratings:this.core.one('SELECT count(*) n FROM ranked_players').n,imports:this.core.rows('SELECT source,table_name,count(*) n FROM online_legacy GROUP BY source,table_name'),
                    rooms:this.core.one('SELECT count(*) n FROM online_rooms').n,
                    match_characters:{ranked:this.core.one('SELECT count(*) n FROM ranked_matches WHERE chars IS NOT NULL').n,
                        casual:this.core.one("SELECT count(*) n FROM online_legacy WHERE source='cloudflare' AND table_name='online_matches'").n}});
                if(path==='/v5/admin/admission'&&['open','closed'].includes(body.state)){
                    if(body.minimum){if(!clientAtLeast(body.minimum,body.minimum))throw new Error('BAD_VERSION');this.core.run("INSERT INTO online_meta VALUES('minimum_client',?) ON CONFLICT(key) DO UPDATE SET value=excluded.value",body.minimum);}
                    this.core.run("INSERT INTO online_meta VALUES('admission',?) ON CONFLICT(key) DO UPDATE SET value=excluded.value",body.state);return json({state:body.state});
                }
                if(path==='/v5/admin/ranked'&&['paused','open'].includes(body.state)){
                    this.core.run("INSERT INTO online_meta VALUES('ranked_paused',?) ON CONFLICT(key) DO UPDATE SET value=excluded.value",body.state==='paused'?'1':'0');return json({state:body.state});
                }
                return json({error:'BAD_REQUEST'},400);
            }
      if (path.startsWith("/v5/panel/")) {
        if (!this.env.PANEL_SECRET || !safeEqual(bearer(request), this.env.PANEL_SECRET)) return json({ error: "AUTH_REQUIRED" }, 401);
        this.budget("panel", "panel", 600);
        if (request.method !== "POST") return json({ error: "BAD_REQUEST" }, 400);
        let panelSql = "";
        try {
          const panelBody = await readJson(request, 262144);
          panelSql = typeof panelBody.sql === "string" ? panelBody.sql.trim() : "";
        } catch {
          return json({ error: "BAD_REQUEST" }, 400);
        }
        if (!panelSql || panelSql.length > 16384) return json({ error: "BAD_REQUEST" }, 400);
        const bare = panelSql.replace(/--[^\n]*|\/\*[\s\S]*?\*\//g, " ");
        if (path === "/v5/panel/sql") {
          if (!/^(select|with)\b/i.test(bare)) return json({ error: "READ_ONLY" }, 400);
          if (/;(?![\s;]*$)/.test(bare) || /\b(insert|update|delete|drop|alter|create|attach|detach|pragma|vacuum|reindex)\b/i.test(bare)) return json({ error: "READ_ONLY" }, 400);
          if (!/\blimit\b/i.test(bare)) panelSql += " LIMIT 5000";
          try {
            return json({ rows: this.core.rows(panelSql) });
          } catch (e2) {
            return json({ error: "BAD_SQL", msg: e2 instanceof Error ? e2.message : String(e2) }, 400);
          }
        }
        if (path === "/v5/panel/exec") {
          if (/^(select|with)\b/i.test(bare) || /;(?![\s;]*$)/.test(bare) || /\b(attach|detach|pragma|vacuum|reindex)\b/i.test(bare)) return json({ error: "FORBIDDEN" }, 400);
          try {
            const cursor = this.core.sql.exec(panelSql);
            return json({ changes: cursor.rowsWritten ?? 0 });
          } catch (e2) {
            return json({ error: "BAD_SQL", msg: e2 instanceof Error ? e2.message : String(e2) }, 400);
          }
        }
        return json({ error: "BAD_REQUEST" }, 400);
      }
            if(path.startsWith('/v5/auth/')){
                if(request.method!=='POST'||!['/v5/auth/start','/v5/auth/exchange','/v5/auth/refresh'].includes(path))return json({error:'BAD_REQUEST'},400);
                return json(await this.auth(path,await readJson(request),request.headers.get('CF-Connecting-IP')||'local'));
            }
            if(path==='/v5/socket'){
                if(request.headers.get('Upgrade')?.toLowerCase()!=='websocket')return json({error:'WEBSOCKET_REQUIRED'},426);
                this.budget(request.headers.get('CF-Connecting-IP')||'local','ws',20);
                const pair=new WebSocketPair(),client=pair[0],server=pair[1];
                this.ctx.acceptWebSocket(server);server.serializeAttachment({user:null,expires:Date.now()+10000});
                return new Response(null,{status:101,webSocket:client});
            }
            const user=await this.user(request);if(!user)return json({error:'AUTH_REQUIRED'},401);
            const body=await readJson(request);
            if(path==='/v5/api'&&request.method==='POST')return json(await this.execute(user,body.op,body.args));
            return json({error:'BAD_REQUEST'},400);
        }catch(e){const allowed=/^(AUTH_|BAD_|INVALID_|IMPORT|PROFILE_|MIGRATION_|NICKNAME_|NOT_|ROOM_|RATE_|ALREADY_|INCOMPATIBLE_|RANKED_|RESULT_|ACCOUNT_|UPDATE_)/.test(e.message);
            return json({error:allowed?e.message:'SERVICE_UNAVAILABLE'},e.message==='AUTH_REQUIRED'?401:e.message==='RATE_LIMITED'?429:400);}
    }
    async webSocketMessage(ws,message){
        let data;
        try{
            if(typeof message!=='string'||new TextEncoder().encode(message).length>24576){ws.close(1009,'Message too large');return;}
            data=JSON.parse(message);const session=ws.deserializeAttachment();
            if(data.op==='authenticate'){
                const user=await verifyAccess(data.access_token,this.env.AUTH_SECRET);if(!user){ws.close(1008,'Authentication required');return;}
                // Re-accepting a socket is not supported: tag it via a serialized attachment and filter broadcasts.
                ws.serializeAttachment({user,expires:Date.now()+900000});ws.send(JSON.stringify({event:'bt4.ready',body:{}}));return;
            }
            if(!session?.user||session.expires<Date.now()){ws.close(1008,'Authentication expired');return;}
            if(!Number.isSafeInteger(data.serial)||!Number.isInteger(data.consumer)||data.consumer<0||data.consumer>5)throw new Error('BAD_REQUEST');
            const reply=await this.execute(session.user,data.op,data.args||{});
            ws.send(JSON.stringify({event:'reply',serial:data.serial,consumer:data.consumer,body:reply}));
        }catch(e){try{ws.send(JSON.stringify({event:'reply',serial:data?.serial,consumer:data?.consumer,body:{error:/^[A-Z_]+$/.test(e.message)?e.message:'BAD_REQUEST'}}));}catch{}}
    }
    webSocketClose(){} webSocketError(){}
    async alarm(){
        this.ctx.storage.transactionSync(()=>{
            this.core.prune();this.core.run('DELETE FROM online_budgets WHERE minute<?',Math.floor(Date.now()/60000)-5);
            this.core.run('DELETE FROM online_turn WHERE expires<=?',Date.now());
        });
    if (this.core.events.length) {
      const byUser = /* @__PURE__ */ new Map();
      for (const e of this.core.events) {
        const list = byUser.get(e.user);
        if (list) list.push(e);
        else byUser.set(e.user, [e]);
      }
      for (const ws of this.ctx.getWebSockets()) {
        const u = ws.deserializeAttachment()?.user;
        const list = u && byUser.get(u);
        if (!list) continue;
        for (const e of list) try {
          ws.send(JSON.stringify({ event: e.event, body: e.body }));
        } catch {
        }
      }
    }
        await this.ctx.storage.setAlarm(Date.now()+600000);
    }
}
export default {
    async fetch(request,env){
        const path=new URL(request.url).pathname;if(!path.startsWith('/v5/'))return json({error:'NOT_FOUND'},404);
        return env.ONLINE.get(env.ONLINE.idFromName(env.COORDINATOR_ID||'bt4-online-authority-v5'),{locationHint:'sam'}).fetch(request);
    }
};
