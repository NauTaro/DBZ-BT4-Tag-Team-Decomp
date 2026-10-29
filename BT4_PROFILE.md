# BT4 runtime and compatibility profile

The Windows package targets BT4 B14 Rev2 with Spanish (LATINO) and
English data tables. The underlying BT4decomp engine provenance is in
[UPSTREAM_LOCK.json](UPSTREAM_LOCK.json). Source and runtime licenses remain
in the repository and release package.

## Data and language installation

`port/setup/native.cpp` verifies and extracts the chosen compatible ISO.
`language_install.inc` adds the alternate language when both are requested.
`language_packages.h` pins the exact language release assets, sizes and SHA-256
values. Downloads use the official GitHub CLI and the user's authorization.
No account credential is part of the package. Per-entry CRC checks and a hidden
fight run before an alternate installation is marked complete.

`tbo/languages.ini` records Spanish (1), English (2) or both (3). Runtime profile
selection uses `game_profile.c`; only a two-language installation presents the
startup selector. Text and voices switch together.

## Online compatibility and timing

The compatibility family is `BT4-B14-Rev2-sim2`. The base-data check reads the
three verified program resources once per process, independently of the
displayed revision label. 1 vs 1, lobbies, spectators and Tag Team share this identity. The check covers those resources; it does not audit
the full roster or establish compatibility with other BT4 modifications.

Tag Team includes up to ten actor identities and gameplay state in detailed
sync samples. The Worker and client share the room kernel. When supported by
all connected clients, the server commits input at 34.2 simulation steps/second
(114% of the original 30 Hz simulation). Missing input is neutral for that seat;
late packets do not rewrite committed history. The Tag Team FPS meter reports
measured ping. Rendering uses the selected resolution scale.

## Maps and presentation

BT4 owns native stage IDs 0 through 98. Offline custom stages append at 99,
with up to 26 slots. A same-name PNG supplies native thumbnail and preview
packets; the map name uses glyphs from the selected ISO's original atlas.

## Validation scope

In local validation, four native mixed-language clients reached 1,200 common simulation frames on
the production service without hash mismatches. A normal ES/EN room-code fight
also passed. Both private language downloads, both directions of alternate
language installation and six profile-selection combinations were checked.
These tests used local clients. They did not reproduce the reported four-PC
disconnection and do not cover every story event or roster combination.

## Launcher 0.1.39.2 validation

This hotfix retains upstream 0.1.39.1. Lobby protocol 4 adds a per-match relay
through the lobby owner when the assigned fighters cannot connect directly.
Forwarding checks the current match generation, assigned seats, member ID,
source address and recipient. Direct connections remain the first choice.

Native local tests reached combat in a public 1 vs 1 lobby forced through TURN,
and in a five-entrant tournament with the owner spectating and direct traffic
between fighters blocked. The equivalent tournament without the fallback
reproduced the connection failure. Two native Tag Team clients completed the
result-to-character-selection transition and entered a second fight. Keyboard
tests covered the default and rebound Back key; menu captures covered English
and Spanish. A damaged base program was rejected. These checks do not establish
compatibility with every router, modification or remote PC.
