import test from 'node:test';
import assert from 'node:assert/strict';
import { createTurnHandler } from '../functions/bt4-turn/handler.mjs';
const generation = '00000000-0000-0000-0000-000000000001';
const variables = { SUPABASE_URL: 'https://project.example', SUPABASE_ANON_KEY: 'public-fixture', BT4_TURN_KEY_ID: 'key', BT4_TURN_KEY_TOKEN: 'server-only-secret' };
const req = (body = { generation }, headers = { authorization: 'Bearer user-fixture' }) => new Request('https://edge.example', { method: 'POST', headers, body: JSON.stringify(body) });
const credentials = { iceServers: [{ urls: ['turn:turn.cloudflare.com:3478?transport=udp'], username: 'temporary-user', credential: 'temporary-password' }] };
const response = body => Response.json(body);
function fixture(replies = [{ allowed: true }, credentials], vars = variables) {
  const calls = [];
  return { calls, handler: createTurnHandler({ env: key => vars[key], fetch: async (url, options) => {
    calls.push({ url, options }); const body = replies.shift();
    if (body instanceof Error) throw body;
    return body instanceof Response ? body : response(body);
  } }) };
}
test('verifies ownership using caller JWT before obtaining temporary credentials', async () => {
  const { calls, handler } = fixture(); const result = await handler(req());
  assert.equal(result.status, 200);
  assert.deepEqual(await result.json(), { server: 'turn.cloudflare.com:3478', username: 'temporary-user', credential: 'temporary-password' });
  assert.equal(result.headers.get('cache-control'), 'no-store');
  assert.equal(calls.length, 2);
  assert.equal(calls[0].options.headers.authorization, 'Bearer user-fixture');
  assert.equal(calls[1].options.headers.Authorization, 'Bearer server-only-secret');
  assert.equal(JSON.parse(calls[1].options.body).ttl, 10800);
});
test('denies missing authentication and malformed generations before any network call', async () => {
  for (const [body, headers] of [[{ generation }, {}], [{ generation: 'bad' }, { authorization: 'Bearer user-fixture' }], [{ generation, extra: true }, { authorization: 'Bearer user-fixture' }]]) {
    const { calls, handler } = fixture(); assert.ok((await handler(req(body, headers))).status >= 400); assert.equal(calls.length, 0);
  }
});
test('unconfigured TURN fails without consuming an admission or contacting a provider', async () => {
  const { calls, handler } = fixture([], {});
  assert.deepEqual(await (await handler(req())).json(), { error: 'RELAY_NOT_CONFIGURED' }); assert.equal(calls.length, 0);
});
test('rejects non-owners and exhausted quotas without obtaining credentials', async () => {
  for (const error of ['NOT_OWNER', 'RATE_LIMITED', 'AUTH_REQUIRED']) {
    const { calls, handler } = fixture([{ error }]);
    assert.deepEqual(await (await handler(req())).json(), { error }); assert.equal(calls.length, 1);
  }
});
test('never treats a rejected JWT as permission', async () => {
  const { calls, handler } = fixture([new Response('', { status: 401 })]);
  assert.equal((await handler(req())).status, 403); assert.equal(calls.length, 1);
});
test('provider failures and incompatible URLs fail closed without exposing provider secrets', async () => {
  for (const value of [new Response('private provider failure', { status: 500 }), new Error('secret failure'), {}, { iceServers: [{ urls: ['turns:turn.cloudflare.com:443?transport=tcp'], username: 'u', credential: 'p' }] }]) {
    const { handler } = fixture([{ allowed: true }, value]);
    assert.deepEqual(await (await handler(req())).json(), { error: 'RELAY_UNAVAILABLE' });
  }
});
