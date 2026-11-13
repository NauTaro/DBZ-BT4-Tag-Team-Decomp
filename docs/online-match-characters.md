# Online match selection metadata

The Worker uses SQLite-backed Durable Object storage. This change does not move
the database or change combat transport, rating rules, or result digests.

## Client contract

`port/src/gs/match_characters.inc` snapshots the native selected roster at combat
start, before transformations or substitutions. `net.c` supplies the synchronized
session seed and player names; `tag_team_net.c` supplies the fixed room seats and
teams. Character values are internal engine roster IDs, not display names,
costume IDs, portrait IDs, or the player's current transformed model.

For a single fighter per side, the optional fields are:

```json
{"char_a":187,"char_b":310}
```

Tag Team uses arrays in the fixed team order, independent of the local camera:

```json
{"chars_a":[187,188],"chars_b":[310,311]}
```

The same arrays support native relay teams (up to seven fighters per side),
larger Tag teams (up to five per side), duplicates, and CPU slots. FFA stores all
selected fighters in `a`, an empty `b`, and `free_for_all:true`; it does not infer
a winning player from the native two-team result bits.

Ranked attaches these fields to `ranked_heartbeat` as soon as the match starts,
and to the existing `ranked_report` and `ranked_forfeit` payloads. The existing
`BT4-RANKED-V1` digest input is unchanged. An opponent's later forfeit can therefore
settle the match without losing the starting selection. Old clients can omit the
fields, including when paired with a new client.

Other online modes use the authenticated `match_report` command through the same
signaling worker. A start report is queued immediately, then the same record is
updated with a confirmed result. Retries are bounded, asynchronous, and do not
hold combat waiting for statistics. Room generation, synchronized session seed
and round counter distinguish rematches and tournament bouts. Tag uses generation,
epoch and seed. Started records survive room departure. A completed casual result
requires agreement from all human participants; absent reports do not invent wins.

## Storage and validation

`cloudflare/migrations/20261113_match_characters.sql` adds nullable
`ranked_matches.chars`. `OnlineCore` automatically checks `PRAGMA table_info`
before applying the ALTER, so deployment also upgrades existing storage. Fresh
databases include the column. Existing matches keep NULL; no historical choices
are fabricated.

The column contains JSON `{"a":[187],"b":[310]}`. The Worker accepts only integer
IDs from 0 through 511, matching the engine's selected-fighter range in
`Tt_ChooseSeat`. It rejects malformed, mixed scalar/array, incomplete and oversized
payloads. Ownership is resolved against authenticated room membership. Repeated
reports do not replace the first recorded selection or apply ratings twice.
Character choices remain client-reported metadata, not server-simulated proof.

Live Ranked mirrors are stored under `online_legacy.source='cloudflare'`,
`table_name='ranked_matches'`, `row_key=match_id`. Matching imported legacy rows
retain their other fields. Both carry `chars`, `player_a`, `player_b`, `started_a`,
`started_b`, `completed`, `winner` and the unchanged `result_digest`.

Casual records use `source='cloudflare'`, `table_name='online_matches'`, and the
per-battle key. Their JSON body contains `chars`, `players_a`, `players_b` (user IDs
parallel to the character arrays, NULL for CPU), mode, start timestamps,
`character_reports`, and `result_reports`. Agreed finishes also contain `completed`,
`outcome` (0=A, 1=B, 2=draw), and `result_digest`. These records never modify ratings.

There is no statistics panel or additional user-facing setup in this update.

## Checks

```sh
node --test cloudflare/tests/online_characters.test.mjs cloudflare/tests/online_core.test.mjs cloudflare/tests/online_migration.test.mjs
```

The native `online_discovery_ranked_test.cpp` exercises actual report construction,
start/forfeit fields, immutable Tag selections, retries and distinct rematch keys.
