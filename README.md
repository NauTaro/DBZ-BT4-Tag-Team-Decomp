<div align="center">
  <img src="docs/presentation/icon.png" width="144" alt="BT4 Tag Team icon">
  <h1>BT4 Tag Team Recomp</h1>
  <p>Native Windows mod based on Tenkaichi3Decomp.</p>
  <p><strong>R16-rc1 · Review preview</strong></p>
  <img src="docs/presentation/cover.jpg" width="960" alt="Dragon Ball Z Budokai Tenkaichi 4 Tag Team presentation">
</div>

Simultaneous team battles, room-code lobbies, proximity voice and an adjustable
Tag Team HUD. This repository contains the source and review documentation.
The original game, ISO, extracted assets and player saves are not included.

## Preview status

R16-rc1 is a test revision. The reported multicolored effect in Hit's rush
was not reproduced in the reviewed captures and remains open. This release
does not certify that defect as fixed. Performance across all hardware and
independent household networks has not been certified.

## Changes in this revision

- Tag Team always uses Default Attacks; the attack-style selector is removed.
- Shared attack preparation is retained. Rush impact cameras are restricted
  to the attacker and victim, while outsiders resume play.
- Cinematic midpoint calculations use the actual actors. Grabs cancel stale
  cuts; ordinary projectile impacts no longer transfer the victim's camera.
- Transformation resources also reload for additional fighters, with deferred
  retirement of the previous model buffers.
- Windows voice capture and playback use independent WinMM buffers. The exact
  previously reported SDL access violation was not reproduced.
- The supplied icon is embedded in the game executable and used by its window.

## Validation

Native checks covered 18 transformation steps, camera geometry and policy,
the local result menu, two clients joining an Internet room by code and
rematching, and regular 1v1 direct/lobby sessions. Voice transport and proximity
were checked with synthetic audio; physical-device and toggle checks are
described in [the review notes](docs/review/R16-rc1.md).

These checks cover selected scenarios, not every fighter, move, map or device.
Both players must use the same revision. Duel protocol: `0x4254340E`.
Tag Team protocol: `0x54540007`, with an R16-rc1 build marker.

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
`UPSTREAM_LOCK.json`, `port/README.md` and the build scripts under `port/`.
Review build helpers are under `tools/review/`.

## Credits and licensing

Original decompilation and PC port: Tenkaichi3Decomp contributors.
BT4 content belongs to its respective authors. Game data must be supplied
by the player. Retained licenses are [AGPL](LICENSE), [MIT](LICENSE-MIT)
and the third-party notices shipped with the runtime and voice libraries.

Presentation artwork: `Gemini_Generated_Image_46di9146di9146di.jpg`, selected
by the maintainer from Drive. The game icon is
the supplied `icon_game.ico`; artwork and icon retain their respective rights.
