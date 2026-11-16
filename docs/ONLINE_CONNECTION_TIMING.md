# Online connection settings

This preview measures the gameplay route before each duel. It uses 16 correlated
ping replies to calculate median RTT, the 90th percentile and unanswered probes.
Both updated clients agree on the same input delay and prediction window before
character selection. The settings stay fixed during the fight.

Ranked and Tenkaichi Budokai Online Quick Match select this policy automatically.
Private duels and tournaments offer **Rollback: Automatic**, paired with automatic
input delay. Manual values remain available. Rooms with an older participant
use the previous four-frame fallback instead of the extended negotiation.

The choice depends on the measured connection, rather than country names. A
nearby opponent can have an unstable route; a distant opponent can have stable RTT.

## Examples from the policy at 100% simulation speed

| Median / p90 RTT | Route | Missing probes | Input delay | Maximum rollback |
| --- | --- | --- | --- | --- |
| 10 / 12 ms | Direct | 0/16 | 1 frame | 2 frames |
| 50 / 70 ms | Direct | 0/16 | 1 frame | 4 frames |
| 100 / 120 ms | Direct | 0/16 | 1 frame | 6 frames |
| 150 / 180 ms | Direct | 0/16 | 1 frame | 8 frames |
| 200 / 220 ms | Direct | 0/16 | 1 frame | 8 frames |
| 280 / 320 ms | Relay | 2/16 | 6 frames | 8 frames |
| 400 / 440 ms | Relay | 2/16 | 10 frames | 8 frames |

These are calculated examples, not country-to-country measurements. Game speed
also changes the number of simulation frames covered by an RTT. The native local
UDP test with 90 ms imposed in each direction measured 184/186 ms and both clients
selected one frame of delay and eight frames of rollback at its configured speed.

A larger window can avoid waiting for a late input, but can also produce larger
visible corrections and cost more CPU when predictions are wrong. It does not
remove propagation delay, packet loss or a slow opponent's simulation.

## Connection paths

| Mode | Gameplay path | Timing |
| --- | --- | --- |
| Ranked / TBO Quick Match | Direct UDP, with negotiated relay fallback | New pre-match adaptive delay and rollback |
| Private duel / tournament | Same native duel transport | Automatic or host-selected manual settings |
| Tag Team | Its existing paced, authoritative input transport; direct or relay route | Existing RTT-based input lead and late-input feedback |
| Spectator | Selected match's stream | Follows that match; no extra fighter input |

Tag Team uses a different simulation transport. This change does not add the
duel rollback engine to the ten-fighter authoritative transport.

## Design references

The design uses the combination of input delay and prediction described in the
[GGPO integration guide](https://github.com/pond3r/ggpo/blob/master/doc/DeveloperGuide.md).
Riot's [2XKO networking article](https://2xko.riotgames.com/en-us/news/dev/how-2xko-handles-online-play/)
also explains its use of input delay, rollback and clock alignment. This project
does not claim to reproduce either game's tuning or network conditions.

## Validation boundary

Unit checks cover probe correlation, duplicate replies, percentiles and timing
selection. Two native clients agreed on the new settings on a delayed loopback
route. These checks do not establish latency or frame pacing between real players
in different countries. Further Windows network-client tests are paused at the
user's request because Windows displayed firewall prompts. Do not run those
clients or change firewall settings without a new instruction from the user.
