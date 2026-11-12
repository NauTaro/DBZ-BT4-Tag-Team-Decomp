# Cloudflare online authority

`online_worker.mjs` handles account sessions, reserved nicknames, room membership,
queues, signaling, presence and competitive records. SQLite-backed Durable Object
storage owns this state. Native duel, lobby/tournament and Tag Team simulation
remain on the players' machines. Combat inputs and voice are not sent through the
coordinator. Authenticated room members can request short-lived Cloudflare TURN
credentials when peer connectivity requires a relay.

The native client maintains one authenticated WebSocket. Heartbeat strings have
an automatic response so idle sockets can hibernate. A bounded HTTPS fallback uses
the same authority and retained room/signaling records if a network rejects the
WebSocket upgrade. Neither transport changes the combat protocol or physics.

## Deployment and migration

1. Deploy `wrangler.online.json`. Configure `AUTH_SECRET`, `ADMIN_SECRET` and
   `TURN_GATEWAY_SECRET` as Worker secrets, not repository variables or client data.
2. The `TURN_GATEWAY` service binding points to the existing, authenticated
   internal issuer. Its existing TURN key remains private. It calls Cloudflare's
   credential API with a three-hour lifetime. The old service's public routes and
   Durable Object namespaces are retained.
3. Export immutable user IDs, nickname reservations and Ranked data from both old
   projects. Do not export passwords, Auth sessions, emails or refresh tokens.
   Read profiles in one PostgreSQL statement for a consistent MVCC snapshot.
   Store private exports outside the repository.
4. While new admission is closed, import authenticated batches through
   `/v5/admin/import`. Each batch carries a SHA-256 digest and idempotency key.
   Secondary-project competitive profiles remain authoritative; both projects'
   user IDs and original records are retained with provenance.
5. Validate a separate coordinator namespace. Open production admission only
   after its imported inventory and the native client checks pass. Publication
   of the client is a separate step from deploying the service.
6. Keep the old service available for clients and battles that have not migrated.
   Restrict old admission only after verified update packages are available.
   Established rooms can still signal, renew and submit results.

Existing installations prove ownership once using their saved legacy Auth session.
The old Auth API verifies that proof. The new service maps the verified subject to
its existing immutable user ID, then issues its own access and refresh tokens.
Later renewals use Cloudflare. A failed migration does not silently create a new
account. Accounts created during the migration window can also prove ownership;
their nickname and competitive profile are fetched from the old authenticated API.
The old Auth service must remain available for installations that have not yet
completed this first exchange.

`/v5/admin/ranked` can pause only new competitive searches. Existing results and
liveness continue to work. `/v5/admin/sync` supports an optional drain of late
legacy results: apply the change since the imported baseline, rather than
overwriting points earned on the new service. These endpoints require the admin
secret and are never used by a game client.

## Retention and capacity

Room inactivity expires after 30 minutes. Pending signals, queues, presence,
expired sessions and rate-limit counters have bounded lifetimes. User identities,
nickname ownership and competitive history are not deleted by transient cleanup.
Top standings use a ten-minute cache; the weekly boundary is Sunday 23:59 in
Argentina and does not reset permanent ratings.

The deployment disables Worker request logging by default. A single coordinator
keeps admission and scoring transactional. Review paid-plan request, SQLite and
TURN usage before increasing capacity. A successful admission test is not a
measurement of combat latency: peer routing, distance and relay usage remain
separate factors.

## Validation

Run the Node 24 tests:

```
node --test cloudflare/tests/online_core.test.mjs cloudflare/tests/online_migration.test.mjs
```

The rollout audit also exercised 300 synthetic users through the deployed HTTPS
endpoint, both native Windows hosting directions, native WebSocket and HTTPS
fallback, temporary TURN credentials, nickname ownership, queue sizes, Ranked
abandonment and an existing legacy account exchange. Test identities and private
export files are not release assets.
