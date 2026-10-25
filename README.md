<div align="center">
  <h1>DBZ BT4 Tag Team Recomp</h1>
  <p>Dragon Ball Z: Budokai Tenkaichi 4 on Windows, with battles of up to 5v5.</p>
  <img src="docs/presentation/bt4-intro-hq.gif" width="960" alt="BT4 Tag Team animated artwork">
  <p><strong>BT4 B14 Rev2 · R25 RC2</strong></p>
  <p><a href="#about-the-mod">About the mod</a> · <a href="#install-or-update">Installation</a> · <a href="#support-and-contact">Donate &amp; contact</a></p>
</div>

## About the mod

BT4 Tag Team Recomp adds simultaneous team battles to BT4 B14 Rev2.
Up to ten fighters can share the arena, with up to five on each team.
Teams can include players and CPU fighters.

You can join online rooms with a code, switch targets during a fight and track
your teammates through the HUD. Voice chat uses proximity: the farther away
another fighter is, the quieter their voice becomes.

The game runs natively on Windows, using the PC port by
[Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp).

<p align="center">
  <img src="docs/presentation/tag-team-battle.jpg" width="960" alt="5v5 battle with ally health bars, enemy targets and radar">
  <br><em>A 5v5 match with ally health bars, target selection and radar.</em>
</p>

## What's different from BT3?

| | BT3 on PS2 | BT4 Tag Team Recomp |
| --- | --- | --- |
| Team battles | One active fighter per team, with character switching. | Up to five fighters per team fighting at once. |
| Content | Original BT3 roster and game data. | Uses BT4 B14 Rev2 content. |
| Online | No online room system. | Rooms with codes and team selection. |
| Voice chat | None. | Built-in proximity voice. |
| HUD | Shows the two active fighters. | Adds ally information, targets and radar. |
| Platform | PlayStation 2. | Native Windows port with video, audio and control settings. |

## Game language

`Install.exe` has its own **Spanish / English** interface and lets you choose
**Spanish**, **English** or **both** for the complete game, including voices.

- **One language:** use your corresponding BT4 B14 Rev2 ISO. The game opens
  directly in that language, without the startup language selector.
- **Both languages:** setup installs the ISO language and downloads the missing
  text and voice package from this same private repository. Progress appears
  as “Installing language pack”.
- **Private downloads:** sign in with a GitHub account that has read access to
  this repository. Existing installed language files can be reused.

## Install or update

Download the [current Windows ZIP](https://github.com/NauTaro/tenkaichi3decomp-fork-DBZ-BT4-Decomp/releases/tag/bt4-r25-rc2).
Extract all contents directly into the game directory. `Install.exe` and
`Tenkaichi3Decomp.exe` must be next to each other.

- **First install:** run `Install.exe`, choose setup and game languages, and
  select your own compatible Spanish or English BT4 B14 Rev2 ISO.
- **Update:** close the game, copy all ZIP contents over your installation and
  replace files. Keep `gamedata`, `saves`, `bt3_settings.txt` and `online_name.txt`.
  An existing installation does not need its ISO extracted again.
- **Online compatibility:** Spanish and English share the supported BT4 gameplay
  profile. The cosmetic R label is not the room compatibility check.

<p align="center">
  <img src="docs/presentation/installer-en.png" width="960" alt="Installer with interface and game language choices">
</p>

## Custom maps

Compatible `.unk` stage models go in `stages`, with an optional PNG carrying the
same filename stem. The original selector displays the thumbnail, preview and
native map-name lettering. Custom maps are available offline.
See [the map guide](docs/port/adding_stages.md).

<p align="center">
  <img src="docs/presentation/map-selector.png" width="960" alt="Kaio Planet with a clean real-stage thumbnail and native BT4 lettering">
</p>

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md) for the source layout, build requirements
and contribution guidelines, or browse the [documentation](docs/README.md).

## Support and contact

Support development through PayPal:

<p align="center">
  <a href="https://www.paypal.com/paypalme/NauTaro">
    <img src="https://img.shields.io/badge/Donate_with-PayPal-0070BA?style=for-the-badge&amp;logo=paypal&amp;logoColor=white" alt="Donate to NauTaro on PayPal">
  </a>
  <br><a href="https://www.paypal.com/paypalme/NauTaro">PayPal: @NauTaro</a>
</p>

For questions, feedback or bug reports, message me on Discord or email me:

- **Discord:** `@nautaro`
- **Email:** [mellamannau22@gmail.com](mailto:mellamannau22@gmail.com)

## Credits

The original decompilation and PC port are by the **Tenkaichi3Decomp contributors**.
This mod is based on `v0.1.38.1`; the exact upstream version is recorded in
[UPSTREAM_LOCK.json](UPSTREAM_LOCK.json). See the
[upstream README](README_UPSTREAM.md) and [port documentation](docs/port/README.md).

BT4 content, artwork and icons belong to their respective authors.
See the [media credits](docs/presentation/CREDITS.md), [AGPL](LICENSE),
[MIT](LICENSE-MIT) and the third-party notices included with the libraries.
