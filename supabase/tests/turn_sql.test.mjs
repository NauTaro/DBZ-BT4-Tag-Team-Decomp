import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { PGlite } from '@electric-sql/pglite';

test('TURN migration enforces real PostgreSQL ownership, quotas and signaling validation', async () => {
  const db = new PGlite();
  const host = '00000000-0000-0000-0000-000000000011';
  const guest = '00000000-0000-0000-0000-000000000012';
  const generation = '00000000-0000-0000-0000-000000000021';
  const room = '00000000-0000-0000-0000-000000000031';
  try {
    await db.exec(`create role anon; create role authenticated;
      create schema auth; create schema bt4_private;
      create table auth.users(id uuid primary key);
      insert into auth.users values('${host}'),('${guest}');
      create function auth.uid() returns uuid language sql as $$select nullif(current_setting('request.jwt.claim.sub',true),'')::uuid$$;
      create table bt4_private.rooms(id uuid primary key,owner uuid, generation uuid,mode text,expires_at timestamptz);
      create table bt4_private.members(room_id uuid,user_id uuid,peer integer);
      create table bt4_private.fixture_inbox(recipient uuid,event text,body jsonb);
      create function bt4_private.inbox(who uuid,event_name text,body jsonb) returns void language sql as $$insert into bt4_private.fixture_inbox values(who,event_name,body)$$;
      create function public.bt4_signal(uuid,integer,text,text,jsonb) returns jsonb language sql as $$select '{"legacy":true}'::jsonb$$;
      insert into bt4_private.rooms values('${room}','${host}','${generation}','duel',now()+interval '5 minutes');
      insert into bt4_private.members values('${room}','${host}',1),('${room}','${guest}',2);`);
    await db.exec(await readFile(new URL('../migrations/202611060001_duel_turn.sql', import.meta.url), 'utf8'));
    const identify = id => db.exec(`set request.jwt.claim.sub='${id}'`);
    const admit = async () => (await db.query('select public.bt4_turn_admission($1) as result', [generation])).rows[0].result;
    await identify(''); assert.equal((await admit()).error, 'AUTH_REQUIRED');
    await identify(guest); assert.equal((await admit()).error, 'NOT_OWNER');
    await identify(host); assert.deepEqual(await admit(), { allowed: true });
    assert.equal((await admit()).error, 'RATE_LIMITED');
    await db.exec('delete from bt4_private.turn_grants');
    await db.exec(`insert into bt4_private.turn_grants select gen_random_uuid(),'${host}',now() from generate_series(1,10)`);
    assert.equal((await admit()).error, 'RATE_LIMITED');
    await db.exec('delete from bt4_private.turn_grants');
    await db.exec(`insert into bt4_private.turn_grants select gen_random_uuid(),'${guest}',now() from generate_series(1,100)`);
    assert.equal((await admit()).error, 'RATE_LIMITED');
    const signal = async (body, to = 2) => (await db.query('select public.bt4_signal($1,$2,$3,$4,$5::jsonb) as result', [generation, to, 'duel', 'udp', JSON.stringify(body)])).rows[0].result;
    const relay = { addresses: ['127.0.0.1:1234','relay=127.0.0.1:5678'], nonce: generation.replaceAll('-','') };
    assert.deepEqual(await signal(relay), { state: 'sent', version: 1 });
    const inbox = (await db.query('select * from bt4_private.fixture_inbox')).rows;
    assert.equal(inbox.length, 1); assert.equal(inbox[0].recipient, guest);
    assert.deepEqual(inbox[0].body.body, relay);
    await assert.rejects(signal({ ...relay, nonce: 'f'.repeat(32) }), /BAD_SIGNAL/);
    await assert.rejects(signal({ ...relay, addresses: ['relay=999.1.1.1:12'] }));
    await assert.rejects(signal({ ...relay, addresses: ['relay=127.0.0.1:0'] }));
    await assert.rejects(signal({ ...relay, addresses: ['relay=127.0.0.1:12','relay=127.0.0.1:13'] }), /BAD_SIGNAL/);
    await assert.rejects(signal(relay, 999), /BAD_RECIPIENT/);
    await identify(guest); await assert.rejects(signal(relay, 1), /NOT_OWNER/);
    await identify(host);
    assert.deepEqual(await signal({ addresses: ['127.0.0.1:12'], nonce: generation.replaceAll('-','') }), { legacy: true });
    const permissions = (await db.query(`select has_function_privilege('anon','public.bt4_turn_admission(uuid)','execute') as anon,
      has_function_privilege('authenticated','public.bt4_turn_admission(uuid)','execute') as player,
      has_table_privilege('authenticated','bt4_private.turn_grants','select') as read_grants`)).rows[0];
    assert.deepEqual(permissions, { anon: false, player: true, read_grants: false });
  } finally { await db.close(); }
});
