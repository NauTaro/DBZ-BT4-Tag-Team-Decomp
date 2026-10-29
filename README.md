<div align="center">
  <h1>DBZ BT4 Tag Team Decomp</h1>
  <p><strong>Dragon Ball Z: Budokai Tenkaichi 4 B14 Rev2 on a native PC runtime</strong></p>
  <p>BT4 gameplay and content on the <a href="https://github.com/z3xox/Tenkaichi3Decomp">Tenkaichi3Decomp v0.1.39.1</a> PC port, with online Tag Team, spectator rooms, proximity voice chat, and Spanish or English installation.</p>
  <p>
    <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-1f6feb?style=flat-square" alt="Windows, Linux and macOS">
    <img src="https://img.shields.io/badge/upstream-v0.1.39.1-7c3aed?style=flat-square" alt="Tenkaichi3Decomp v0.1.39.1">
    <img src="https://img.shields.io/badge/languages-English%20%7C%20Espa%C3%B1ol-f0883e?style=flat-square" alt="English and Spanish">
  </p>
  <img src="docs/presentation/bt4-intro-hq.gif" width="900" alt="DBZ BT4 presentation artwork">
  <p><strong>BT4 B14 Rev2 · R26 RC1 · Launcher 0.1.39.3</strong></p>
  <p><a href="#online-play">Online play</a> · <a href="#graphics-and-frame-pacing">Graphics</a> · <a href="#installation">Installation</a> · <a href="#automatic-updates">Automatic updates</a></p>
</div>

## Project

BT4decomp adapts the BT4 B14 Rev2 game data and gameplay for the native PC runtime developed by the Tenkaichi3Decomp contributors. It is not a console emulator. The repository does not contain game disc data; setup reads a compatible ISO supplied by the player.

The current integration is based on Tenkaichi3Decomp v0.1.39.1. The launcher build identifies itself as `BT4-v0.1.39.3-r26-rc1`.

## Online play

### 1 vs 1 and Tag Team

Play online 1 vs 1 or in simultaneous Tag Team battles. Tag Team supports up to five fighter slots per team, with human players and CPU fighters. Private rooms use shareable codes; Quick Match searches for an opponent automatically.

The two modes use different network paths. 1 vs 1 uses the upstream peer rollback path. Tag Team sends inputs through the BT4 relay and follows a server-paced frame schedule. Its input lead starts from estimated one-way relay time and adjusts when the server reports late frames. Tag Team's ping is the round-trip time to the relay, so it is not directly comparable to 1 vs 1 peer ping. Actual delay still depends on route quality and packet timing. A single human playing against CPU consumes inputs locally without relay pacing; its overlay does not show relay RTT as opponent ping.

### Lobbies and tournaments

Open an online lobby or host by address. Tournaments support two to eight entrants, automatic byes, a live bracket and the upstream rejoin window. The lobby, bracket and Tag Team menus are available in English and Spanish. If the two assigned fighters cannot establish a direct connection, match packets fall back through the lobby owner after two seconds. All lobby members must use launcher 0.1.39.2 or later with the same lobby protocol.

### Spectators and proximity voice

Spectator rooms let viewers join by code or address without taking a fighter slot. The spectator camera can follow either side or show a split view.

Proximity voice chat supports both teams. Windows can use direct WebRTC media; the native Linux and macOS packages use the TLS Opus relay unless a compatible libdatachannel library is installed. The client adjusts voice volume by in-game distance; microphone, output device, voice activation, threshold, and volume controls are available in Audio settings.

## Graphics and frame pacing

The Windows renderer offers Vulkan, Direct3D 12, and OpenGL backends. Video settings include render scale, aspect ratio, effects, and frame presentation options. Performance depends on the graphics driver, scene, and selected settings; use the in-game FPS and network statistics when reporting an issue.

Smooth-motion presentation is separate from combat simulation and network inputs. If visual pacing artifacts occur, include the selected renderer, motion setting, resolution scale, and a `bt3_log.txt` in the issue report.

## HUD and custom stages

Hide or restore the combat HUD from the pause menu or Video settings. The change affects the local display, including health bars, damage and technique labels, names, radar, and battle overlays.

Custom stages use the `stages` folder. Place the compatible stage model there and add a PNG with the same filename stem for its thumbnail and preview. See the [custom stage guide](docs/port/adding_stages.md).

## Installation

Packages are available for Windows x64, Linux x86-64 and macOS Apple Silicon (arm64). Intel Macs and Linux ARM devices are not targets of these builds.

Download the package for your system from [Releases](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases) and extract the complete ZIP into the folder where the game will live. Keep the setup application and game executable together.

1. Run `BT4decomp-setup.exe` on Windows, or `./BT4decomp-setup` on Linux and macOS.
2. Choose the setup language and the game language or languages.
3. Select a compatible Spanish or English BT4 B14 Rev2 ISO and finish setup.
4. Start `BT4decomp.exe` on Windows, or `./BT4decomp` on Linux and macOS.

For source builds on Linux or Apple Silicon, run `python3 install.py --iso /path/to/BT4.iso`. The script verifies the BT4 disc profile, builds the native runtime with blank data tables, and delegates extraction to the same setup application. See [native platform setup](docs/port/native_platforms.md) for dependencies and macOS download handling.

The setup interface is available in English and Spanish. A single-language installation starts directly in that language. A bilingual installation downloads the second language package during setup; that package is hosted in this repository. Setup requests GitHub authorization when required for access.

## Automatic updates

The launcher checks this repository's latest signed release, downloads the matching Windows package in the background, and verifies its SHA-256 digest and Ed25519 signature before installation. It applies updates from the main menu, outside online sessions and battles. Player saves, settings, installed game data, and user content are preserved by the updater.

Launchers built before the signed updater was introduced need one initial package installation. Subsequent signed releases can install from the launcher without extracting a ZIP manually.

## Development and feedback

See the [contribution guide](CONTRIBUTING.md) for the source layout, build tools, and validation process. The [documentation index](docs/README.md) links to runtime and customization references.

For a useful bug report, include the launcher version, game mode, renderer, render scale, reproduction steps, and the relevant `bt3_log.txt` or `bt3_crash.txt`. Submit reports through the [issue forms](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/issues/new/choose).

## Credits and licenses

The original decompilation and PC runtime are by the Tenkaichi3Decomp contributors. This repository adapts that work for BT4 B14 Rev2. The exact upstream source revision is recorded in [UPSTREAM_LOCK.json](UPSTREAM_LOCK.json); [README_UPSTREAM.md](README_UPSTREAM.md) is retained for attribution and reference.

BT4 content, artwork, and icons remain the property of their respective authors. See [presentation credits](docs/presentation/CREDITS.md), the [AGPL license](LICENSE), the [MIT license](LICENSE-MIT), and third-party notices.
