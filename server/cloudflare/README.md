# Tag Team service

Cloudflare Worker for Tag Team rooms, gameplay sessions and voice signaling.
Room metadata uses D1; active sessions use Durable Objects.

## Deployment

Authenticate Wrangler with the owning Cloudflare account, then run from this directory:

```sh
npx --yes wrangler@4.46.0 d1 execute bt4-tag-rooms --remote --file schema.sql
npx --yes wrangler@4.46.0 deploy
```

`wrangler.json` defines the D1 and Durable Object bindings. Deployments to another
account require that account's own database and resource identifiers. Keep
authentication credentials outside the repository.

## Protocol

`kernel.wasm` implements the same room rules as the client's C kernel, including
loading barriers, inputs, results, slot movement and host control. Rebuild the
module and validate clients and service together when changing the protocol.

Gameplay input uses WebSocket transport. Rooms support up to ten connections
and expire after three hours. Cloudflare plan quotas apply.

## Voice

`/v3/voice/tag/CODE` requires a temporary capability issued to a gameplay member.
`/v3/voice/duel/HASH` supports two participants using a capability shared through
the game channel. Proximity attenuation is calculated by the client from fighter
positions; voice messages do not pass through the combat kernel.

The `tag-v1` and `voice-v1` migrations create the session objects. Command `20`
is reserved for voice capability; command `21` identifies the results notification.
