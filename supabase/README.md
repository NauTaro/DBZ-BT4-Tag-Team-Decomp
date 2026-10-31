# BT4 P2P signaling

This service registers rooms, matches compatible clients, and delivers directed
ICE/UDP endpoint negotiation messages. It does not execute the fight, coordinate
simulation frames, or relay gameplay and voice packets.

Apply migrations to a dedicated Supabase Free project. Enable anonymous sign-ins
for game identities. Disable public Realtime channels. The native client needs
the project URL and **publishable** API key, never a service-role key, database
password, or Supabase management token.

The client-callable endpoint is `POST /rest/v1/rpc/bt4_api` with an authenticated
anonymous-user JWT. Requests use `{ "p_op": "create", "p_args": {...} }`.
Supported operations: `create`, `join`, `get`, `lease`, `leave`, `list`, `queue`,
`cancel`, and `signal`. Business errors use a bounded JSON `error` field.

Each player subscribes to their private `bt4:inbox:<auth-user-uuid>` channel before
creating, joining, or entering a queue. Server-originated events are `bt4.room`,
`bt4.signal`, and `bt4.closed`. Clients cannot broadcast to inboxes themselves.
Reconnect by joining the inbox first and calling `get` once to reconcile state.
Do not poll room state every frame.

The host refreshes its five-minute advertisement lease at most once per minute
while accepting players. A running P2P room must not depend on its advertisement
remaining in the database. Queue entries expire after two minutes. List pages
contain at most 20 entries and never include peer addresses or credentials.

The included Node reference client and integration tests exercise the hosted
contract. They are developer tools and are not required by players.

Anonymous refresh tokens stay in the per-user BT4decomp network preferences, with an exclusive process slot. Concurrent instances use separate identities. Voice renegotiation carries monotonic revisions; manual-address sessions rendezvous through a hash of the native shared key, never through a public room listing.
