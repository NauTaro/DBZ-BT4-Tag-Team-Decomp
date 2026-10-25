# BT4 room and voice service

`worker.mjs`, `session.mjs`, `voice.mjs` and `kernel.wasm` are the server sources
used by the current Tag Team client. The C kernel shares the native room state
machine in `port/src/tag_team.c` and `tag_team_wire.c`.

Copy `wrangler.example.json` to a private deployment configuration and set your
own account, D1 database and Durable Object bindings. Run the idempotent schema
before deploying. Account credentials and TURN tokens belong in Worker secrets,
never in this repository.

```sh
python cloudflare/build_kernel.py
npx wrangler d1 execute YOUR_DATABASE --remote --file cloudflare/schema.sql
npx wrangler deploy --config path/to/your/private-wrangler.json
```

Input travels over authenticated WebSockets rather than one D1 write per
frame. The room supports ten seats, load barriers, detailed sync checks and
paced input for clients that advertise support. Missing input affects that
seat in the committed input history; late input cannot overwrite a past frame.

Voice signaling is separate from the fight kernel. TURN credentials are issued
temporarily using Worker secrets. Proximity attenuation is computed on each
client from fighter positions and applies across both teams. This deployment
uses direct voice connections with TURN fallback; it does not use an SFU.

Recompile the kernel and validate client/server protocol behavior together
after changing the shared room state machine. Deployment is not a side effect
of a source push or repository checks.
