# BT4 game-data profile

DBZ BT4 Tag Team Recomp targets **BT4 B14 Rev2** and uses the native PC runtime
from Tenkaichi3Decomp `v0.1.38.1`. The upstream revision is recorded in
[UPSTREAM_LOCK.json](UPSTREAM_LOCK.json).

## Game data

The launcher extracts data from the player's compatible ISO. Game data, saves
and settings remain in the installation and are excluded from this repository.
The BT4 resource loader supports four archive ranges, expanded character resources
and the corresponding menu textures. Resource verification is implemented in
`port/src/plat_verify.c` and `port/src/plat_verify_tab.h`.

## Version compatibility

The R16 RC1 build identity is defined in `port/include/bt4_build_identity.h` and
`port/release/BT4_VERSION.txt`. Duel networking uses protocol `0x4254340E`;
Tag Team uses `0x54540007`. Players in the same session must use the same revision.

## Runtime layout

`port/release/PORTABLE_LAYOUT.json` defines the installation layout. Updates
replace program files while retaining `gamedata`, `saves`, `net_session`,
`bt3_settings.txt` and `online_name.txt`.

See the [PC port guide](docs/port/README.md) for development requirements.
