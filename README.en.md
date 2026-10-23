[Español](README.md) · **English**

<div align="center">
   <img src="docs/presentation/cover.jpg" width="960" alt="Dragon Ball Z Budokai Tenkaichi 4 Tag Team presentation">
  <h1>DBZ BT4 Tag Team Recomp</h1>
  <p>Native Windows mod based on Tenkaichi3Decomp.</p>
  <p><strong>Work in progress.</strong></p>
   <img src="docs/presentation/icon.png" width="144" alt="BT4 Tag Team icon">
</div>

Simultaneous team battles, room-code lobbies, proximity voice and an adjustable
Tag Team HUD. This repository contains the source and review documentation.
The original game, ISO, extracted assets and player saves are not included.

## Project status

The recorded preview is `BT4-v0.1.38.1-r16-rc1`. See the
[R16 review](docs/review/R16-rc1.md) for tested scenarios and remaining issues.
Download availability is listed on the [Releases page](https://github.com/NauTaro/BT4-Tag-Team-Recomp/releases).
A draft release is not a published download. The source checkout is not a playable installation.

## Install or update

The complete Windows launcher ZIP supports both a first installation from the
user's own BT4 B14 Rev2 ISO and an existing extracted installation. Existing
users copy the program files over their installation with the game closed.
The archive does not contain `bt3_settings.txt`, `gamedata` or `saves`, so those
are not replaced. A new ISO extraction is needed only for a first installation
or a different/incompatible game-data profile, not every code update.

See `port/release/PORTABLE_LAYOUT.json` for the runtime layout. Documentation
and corresponding source are separate from the game folder in the ZIP.

## Source and upstream

Base: [z3xox/Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp),
tag `v0.1.38.1`, commit `11059a6d3e814cf36e6ca1f6307839f61c56f1cc`.
Upstream history is retained. This is an independent mod, not an official
upstream release. See [the original README](README_UPSTREAM.md),
[the upstream lock](UPSTREAM_LOCK.json), [port documentation](docs/port/README.md)
and [review build helpers](tools/review/README.md).

## Contributing and checks

See [CONTRIBUTING.md](CONTRIBUTING.md) for the repository map, build limitations
and bug-report guidance. Lightweight checks require Python 3.12 and no game data:

```sh
python -m unittest discover -s tools/review/tests -v
```

GitHub Actions runs these checks on Linux and Windows. They validate the portable
layout tool; they do not build the game or certify gameplay, voice or netplay.

## Credits and licensing

Original decompilation and PC port: Tenkaichi3Decomp contributors.
BT4 content belongs to its respective authors. Game data must be supplied
by the player. Retained licenses are [AGPL](LICENSE), [MIT](LICENSE-MIT)
and the third-party notices shipped with the runtime and voice libraries.

Artwork and icon retain their respective rights.
