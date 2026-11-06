# Optional 1v1 and native lobby TURN fallback

This change fixes dropped out-of-order endpoint signals, retries rejected endpoint
announcements, and propagates changed peer addresses into the native handshake.
Those client fixes work without deploying a service. Restrictive NATs may still
need the optional relay described here.

The owner requests temporary credentials only after a direct connection remains
unanswered (four seconds for a duel, eight seconds for a lobby member). Supabase
continues to own signaling. Gameplay goes through the existing native TURN adapter,
not through PostgreSQL or Realtime messages. Tag Team and voice are unchanged.

## Activate after reviewing the migration and deploying updated clients

1. Create a Cloudflare Realtime TURN key in the operator's own account. Review its
   usage and billing terms. This repository does not supply a shared public key.
2. Apply `supabase/migrations/202611060001_duel_turn.sql` after the existing
   signaling migrations. The live project's route-gate migrations stay intact.
3. Store `BT4_TURN_KEY_ID` and `BT4_TURN_KEY_TOKEN` as Supabase Edge Function secrets.
   Never put them in client code, SQL, logs, commits, or chat.
4. Deploy `bt4-turn` using this directory's `index.ts` and `handler.mjs`, together
   with `supabase/config.toml`. The handler forwards the caller's JWT to PostgREST;
   PostgREST validates it and `bt4_turn_admission` checks current room ownership,
   a second admitted participant and issuance quotas before any provider request.
5. Install updated game executables on **both participants**. Older clients can
   still use direct connections, but do not adopt late endpoint changes and cannot
   be relied on to use this fallback. Reinstalling ISO data or Setup is unnecessary.
6. Reproduce with the affected pair. Look for `TURN fallback allocated` and a
   successful match/lobby entry. Check actual gameplay, disconnect, and retry.
   Local tests are not evidence of a successful cross-ISP connection.

Issuance is limited to one grant per room generation, ten per authenticated game
identity per UTC day and 100 project-wide per UTC day. A failed provider request
consumes its reservation, conservatively preventing retry storms. These controls
limit credential issuance, **not transferred bytes or financial exposure**; monitor
provider usage before enabling this in production. Credentials expire in three
hours and are never saved to SQL. Limitations of anonymous identities remain.

The existing native adapter uses IPv4 and TURN over **UDP**. It addresses NAT
mapping problems but does not provide TURN-over-TCP/TLS when all outbound UDP is
blocked. Do not claim this fixes every restrictive network. If the service is not
configured or allocation fails, direct connections remain available and the log
records the failure without credentials. No provider account or deployment was
created by this source change.

The migration preserves ordinary signaling by delegating it to the previous
function. Only room owners can publish a `relay=ip:port` endpoint. The relay path
must be deployed before testing the feature; a client update alone does not enable
hosted TURN.

Validation: `node --test supabase/tests/turn.test.mjs`, the discovery C++ regression
fixture, and the production C handoff fixture in `tools/review/tests`.
