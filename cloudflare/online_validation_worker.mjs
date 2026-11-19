import {disallowedRequest} from './online_access.mjs';
// Legacy URL stays a proxy; admin and panel access use its existing DO.
import {OnlineCoordinator} from './online_worker.mjs';
const json=(body,status=200)=>Response.json(body,{status,headers:{'Cache-Control':'no-store'}});
var online_worker_default = {
  async fetch(request, env) {
    if(disallowedRequest(request))return json({error:"CLIENT_NOT_ALLOWED"},403);
    const url = new URL(request.url), path = url.pathname;
    if (!path.startsWith("/v5/")) return json({ error: "NOT_FOUND" }, 404);
    if (path === "/v5/panel/sql" || path === "/v5/panel/exec" || path.startsWith("/v5/admin/")) {
      try {
        return await env.ONLINE.get(env.ONLINE.idFromName(env.COORDINATOR_ID || "bt4-online-authority-v5"), { locationHint: "sam" }).fetch(request);
      } catch (doErr) {
        return json({ do_error: String(doErr && doErr.stack || doErr).slice(0, 1500) }, 500);
      }
    }
    const headers = new Headers(request.headers);
    headers.set("x-bt4-via", "validation-legacy");
    return env.MAIN.fetch(new Request(new Request("https://bt4-online-v5.mellamannau22.workers.dev" + url.pathname + url.search, request), { headers }));
  }
};

export {OnlineCoordinator,online_worker_default as default};
