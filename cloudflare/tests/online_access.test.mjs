import test from 'node:test';
import assert from 'node:assert/strict';
import worker,{OnlineCoordinator} from '../online_worker.mjs';
import legacy from '../online_validation_worker.mjs';
import {disallowedRequest,disallowedClient} from '../online_access.mjs';
const request=(path,ua,headers={})=>new Request('https://unit.invalid'+path,{headers:{'User-Agent':ua,...headers}});
test('explicit iOS client headers are denied before either worker forwards a game request',async()=>{
 for(const fetcher of [worker,legacy])for(const path of ['/v5/api','/v5/socket','/v5/auth/start','/v5/auth/refresh','/v5/auth/exchange']){
  for(const ua of ['Mozilla/5.0 (iPhone; CPU iPhone OS 18_1 like Mac OS X)','Mozilla/5.0 (iPad; CPU OS 17_5)','Tenkaichi3DecompIOS/1.0.158','BT3Online-iOS/1']){
   const r=await fetcher.fetch(request(path,ua),{});assert.equal(r.status,403);assert.equal((await r.json()).error,'CLIENT_NOT_ALLOWED');
  }
 }
});
test('existing Windows, Linux, macOS, Android and blank client headers are not filtered',()=>{
 for(const ua of ['BT4decomp/Online','BT4-Voice/1','BT4decomp/WebSocket','BT4-Tag-Team/0.1.39','curl/8.11','Mozilla/5.0 (X11; Linux x86_64)','Mozilla/5.0 (Windows NT 10.0; Win64; x64)','Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0)','Desktop CFNetwork/3826 Darwin/24','Mozilla/5.0 (Linux; Android 14)','']){
  for(const path of ['/v5/api','/v5/socket','/v5/auth/refresh'])assert.equal(disallowedRequest(request(path,ua)),false,ua);
 }
});
test('iCloud, Apple networks, shared IPs and desktop CFNetwork are not platform evidence',()=>{
 assert.equal(disallowedRequest(request('/v5/api','BT4decomp/Online',{'CF-Connecting-IP':'1.2.3.4','X-Apple-Relay':'iCloud'})),false);
 assert.equal(disallowedRequest(request('/v5/api','Desktop CFNetwork/1 Darwin/1')),false);
 assert.equal(disallowedRequest(request('/v5/api','BT4decomp/Online',{'Sec-CH-UA-Platform':'"iOS"'})),true);
 assert.equal(disallowedRequest(request('/v5/api','BT4decomp/Online',{'X-BT4-Client-Platform':'iPadOS'})),true);
});
test('owner control and health endpoints are not restricted by this player policy',()=>{
 for(const path of ['/v5/admin/ranked-audit','/v5/panel/sql','/v5/health'])assert.equal(disallowedRequest(request(path,'iPhone')),false);
});
test('positive platform declarations are also rejected on authenticated admission dispatch',()=>{
 for(const platform of ['ios','ipados','iphone','ipad','ipod']){
  assert.equal(disallowedClient({platform}),true);
  assert.throws(()=>OnlineCoordinator.prototype.dispatch.call({},'test-user','ranked_queue',{_client:{platform}}),/CLIENT_NOT_ALLOWED/);
 }
 assert.equal(disallowedClient({app:'Sliu772/Tenkaichi3DecompIOS'}),true);
 for(const platform of ['windows','linux','macos','android',''])assert.equal(disallowedClient({platform}),false);
});
test('the policy cannot detect a client that conceals or falsifies its platform',()=>{
 assert.equal(disallowedRequest(request('/v5/api','BT4decomp/Online')),false);
 assert.equal(disallowedClient({platform:'windows'}),false);
});
test('the legacy proxy forwards desktop JSON and headers through MAIN without using the public route',async()=>{
 let calls=0;
 const req=new Request('https://legacy.invalid/v5/api',{method:'POST',headers:{'User-Agent':'BT4decomp/Online',Authorization:'Bearer unit-only'},body:JSON.stringify({op:'nickname_get',args:{}})});
 const r=await legacy.fetch(req,{MAIN:{async fetch(forwarded){
  calls++;assert.equal(forwarded.method,'POST');assert.equal(forwarded.headers.get('x-bt4-via'),'validation-legacy');
  assert.equal(forwarded.headers.get('Authorization'),'Bearer unit-only');assert.equal((await forwarded.json()).op,'nickname_get');
  return Response.json({state:'ready'});
 }}});
 assert.equal(calls,1);assert.equal(r.status,200);
});
