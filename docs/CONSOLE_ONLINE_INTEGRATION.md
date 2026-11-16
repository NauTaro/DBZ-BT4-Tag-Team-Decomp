# Console port integration with BT4decomp Online

This handoff describes the client-facing Cloudflare API in this source tree.
It contains public connection settings and protocol references.

## Public connection settings

| Purpose | Address |
| --- | --- |
| HTTPS origin | `https://bt4-online-v5.mellamannau22.workers.dev` |
| Health | `GET /v5/health` |
| New installation identity | `POST /v5/auth/start` |
| Session refresh | `POST /v5/auth/refresh` |
| Authenticated requests | `POST /v5/api` |
| Signaling WebSocket | `wss://bt4-online-v5.mellamannau22.workers.dev/v5/socket` |
| Source repository | `https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp` |

These addresses were checked against the client and Worker source on
2026-11-15. A health request from the development tool returned HTTP 403.
Live access and gameplay compatibility from a console have yet to be validated.
Use the public release tag matching the PC version being tested. The working
tree also contains upcoming changes.

## Authentication

Each installation receives its own identity and tokens. This API requires no
shared client API key.

Start a new installation with HTTPS, JSON content type and an empty JSON object.

```http
POST /v5/auth/start
Content-Type: application/json

{}
```

The response contains `access_token`, `refresh_token`, `expires_in` and
`user.id`. The current access token lifetime is 900 seconds. Save the refresh
token in persistent application storage, retain the same identity between
launches, and refresh shortly before expiry.

```http
POST /v5/auth/refresh
Content-Type: application/json

{"refresh_token":"<this installation's stored refresh token>"}
```

Save both replacement tokens after a successful refresh. Treat timeouts as
retryable failures and retain the current stored identity. The PC implementation
includes recovery for a refresh response lost in transit.

The server's `AUTH_SECRET`, `ADMIN_SECRET`, TURN provider secrets, and
Cloudflare deployment tokens remain server-side. Developers need the public
addresses above and their installation's issued tokens.

## Room and queue requests

Use `Authorization: Bearer <access_token>` for `POST /v5/api`. The body is
`{"op":"<operation>","args":{...}}`.

Register a nickname first.

```json
{"op":"nickname_set","args":{"name":"ConsoleTester"}}
```

Create a duel using values derived from the port's compatible engine and game
data. Replace the placeholders with the values for the matched public release.

```json
{
  "op":"create",
  "args":{
    "mode":"duel",
    "wire":"bt4-duel-<NET_VERSION expressed as an unsigned decimal integer>",
    "content":"<16-character lowercase hexadecimal gameplay fingerprint>",
    "rules":"S",
    "capacity":2,
    "listed":false,
    "_client":{"version":"<compatible release version>","capability":2}
  }
}
```

The response supplies the room code, generation, self peer ID, host peer ID and
member list. Retain the generation and reject events belonging to older rooms.

Join using `op: "join"` with the room's `code`, matching `wire` and
`content`, `role: "player"`, and the release's `_client` metadata.
Use `role: "spectator"` for a native duel spectator. Spectating inside a
tournament additionally follows the native lobby protocol.

Other operations include `get`, `lease`, `leave`, `queue`, `cancel`,
`counts`, `signal` and `get_signals`. Ranked admission uses
`ranked_queue`, `mode: "duel"`, `rules: "S"` and `ruleset: "solo"`.
The native client adds Ranked-specific wire capabilities when advertised by
the server. Reuse that logic from the matching release.

The PC room owner renews its lease every minute. A connected room requires
normal heartbeat and lease traffic throughout its lifetime. Renewing a lobby
and reporting a Ranked match's liveness are separate operations.

## Signaling

Open the WebSocket, then authenticate with this message.

```json
{"op":"authenticate","access_token":"<this installation's access token>"}
```

Wait for `{"event":"bt4.ready",...}`. Send commands with a unique integer
`serial`, a `consumer` from 0 through 5, `op`, and `args`.
For example, native room discovery uses consumer 0.

```json
{"serial":1,"consumer":0,"op":"get","args":{}}
```

Replies include the same serial and consumer. Handle `bt4.room`,
`bt4.signal` and `bt4.closed` events. Authenticate a renewed connection
when the token expires and reconcile the room descriptor after reconnecting.

The PC implementation also supports authenticated HTTPS requests when
WebSocket upgrades fail. That path polls `get` and `get_signals` and
tracks its cursor. Preserve the same fallback behavior on the console.

## Gameplay transports

Native duel, team battle and tournament matches use UDP gameplay packets.
The room service exchanges peer candidates, including TURN candidates. It
does not run or stream the native 1 vs 1 simulation.

Request `turn_credentials` through the authenticated API with the current
room generation. Credentials are issued for the requesting member. The
implementation obtains and refreshes relay credentials through this operation.

Native UDP signaling uses channel `duel`, kind `udp`, and a body containing
`addresses` and a per-session hexadecimal `nonce`. Follow
`match_supabase.cpp` for candidate announcements and the retained descriptor
fallback.

Tag Team uses the matched release's Tag transport. The current source has
WebRTC data channels for direct or TURN-assisted play and the existing
session transport. Room control and gameplay input channels have distinct
ordering and reliability requirements.

## Crossplay compatibility

Successful room admission is the first check. The port must also:

1. Match the public PC gameplay version, data fingerprint and game rules.
2. Reproduce the same simulation, RNG, PS2 floating-point behavior and input
   ordering on the console's architecture.
3. Read and write the existing wire formats with explicit byte order and
   fixed-width integers. Native PC packets currently use little-endian fields.
4. Implement the native UDP handshake, shared seed, frame/input history,
   synchronization checks and disconnect handling.
5. Implement the matched release's rollback or delay path and save-state
   requirements.
6. For Tag Team, preserve room authority, actor ownership, shared options,
   barriers, input delivery and native simulation hashes.
7. For Ranked, preserve server admission, start heartbeats, terminal result
   confirmation, character metadata and result digest behavior.

Different renderers, resolutions and UI languages can coexist when gameplay
remains compatible. A working room code alone does not validate deterministic
crossplay. Do not substitute a claimed version or copied fingerprint for the
actual compatible implementation.

## Source references

| Component | Repository path |
| --- | --- |
| Cloudflare client authentication and signaling | `port/src/gs/cloudflare_signal.inc` |
| Native room discovery, candidates and result reporting | `port/src/gs/match_supabase.cpp` |
| Worker request and WebSocket handling | `cloudflare/online_worker.mjs` |
| Room rules and compatibility checks | `cloudflare/online_core.mjs` |
| Token issuance and refresh | `cloudflare/online_auth.mjs` |
| Native UDP handshake, input history and rollback | `port/src/gs/net.c` |
| Native tournament and private lobby state | `port/src/gs/net_lobby.c` |
| Native TURN support | `port/src/gs/net_relay.c` |
| Tag Team transport | `port/src/gs/tag_team_p2p.cpp` and `tag_team_net.c` |
| Tag wire format | `port/include/tag_team_wire.h` |
| Gameplay and version identity | `port/include/bt4_build_identity.h` and `bt4_version.h` |
| Platform compatibility | `port/include/port_compat.h` |

## Console validation checklist

Test a console hosting a PC player and a PC hosting a console player. Then test
direct and forced TURN routes, reconnecting signaling, room-code joining,
spectating and return to the same private lobby.

Compare native simulation checksums through character selection, combat,
transformations, special moves, KO and result handling. Exercise different
characters and stages. Test Tag Team separately with matching room settings
and actor ownership.

Use private test rooms while validating each console build. Enable its public
matchmaking and Ranked path after compatibility and authoritative result
reporting have been demonstrated.
