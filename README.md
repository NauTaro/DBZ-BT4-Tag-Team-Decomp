<div align="center">
  <h1>DBZ BT4 Tag Team Decomp</h1>
  <p><strong>A native Windows PC port of Dragon Ball Z: Budokai Tenkaichi 4 (B14 Rev2)</strong></p>
  <p>Built on the <a href="https://github.com/z3xox/Tenkaichi3Decomp">Tenkaichi3Decomp</a> codebase, with online Tag Team battles, spectator lobbies, proximity voice chat, bilingual setup, and a configurable GPU renderer.</p>
  <p>
    <img src="https://img.shields.io/badge/platform-Windows-1f6feb?style=flat-square" alt="Windows">
    <img src="https://img.shields.io/badge/upstream-v0.1.38.9-7c3aed?style=flat-square" alt="Upstream version 0.1.38.9">
    <img src="https://img.shields.io/badge/languages-English%20%7C%20Espa%C3%B1ol-f0883e?style=flat-square" alt="English and Spanish">
  </p>
  <img src="docs/presentation/bt4-intro-hq.gif" width="900" alt="DBZ BT4 presentation artwork">
  <p><strong>BT4 B14 Rev2 · R26 RC1</strong></p>
  <p><a href="#features">Features</a> · <a href="#installation">Installation</a> · <a href="#languages">Languages</a> · <a href="#development-and-feedback">Development &amp; feedback</a></p>
</div>

## Project

This project adapts the decompiled game code for a native Windows runtime. It is built on Tenkaichi3Decomp v0.1.38.9 and is not a console emulator. A compatible Spanish or English BT4 B14 Rev2 ISO is required during setup; game disc data is not included in this repository.

## Features

### Online battles and Tag Team

Play online 1v1 or in simultaneous Tag Team battles with up to five fighters per team. Room codes support arranged matches, while Quick Match searches for an opponent automatically. Teams can include human players and CPU fighters.

The Tag Team HUD tracks allies, enemy targets and arena positions. Both supported game languages use the same online gameplay profile, so Spanish and English players can join the same compatible rooms.

### Spectator lobbies

Hosts can allow up to six spectators to join by room code or direct address. Spectators run a synchronized copy of the match and can switch between Player 1, Player 2 and split-screen views without occupying a fighter slot.

<p align="center">
  <img src="docs/presentation/tag-team-gameplay-preview.gif" width="720" alt="Animated in-game battle preview">
  <br><em>In-game battle preview.</em>
</p>

### Proximity voice chat

Voice chat works across both teams. Volume fades with distance and reaches silence outside the hearing range. The Audio settings include microphone and output device selection, push-to-talk or voice activation, an activation threshold, microphone level, voice volume and a microphone indicator.

<p align="center">
  <img src="docs/presentation/voice-chat-settings.png" width="612" alt="In-game Audio settings for microphone, voice activation and proximity voice chat">
</p>

### Rendering and frame pacing

The Windows SDL3 GPU renderer supports Vulkan and Direct3D 12, with OpenGL available as a fallback. Video settings include internal render scale, aspect ratio, effects and smooth-motion presentation.

Smooth motion can display additional pictures between the game's original 30 simulation steps. It changes presentation cadence without changing combat timing or network inputs. A new installation starts at 1x internal resolution; saved resolution and motion preferences are retained when updating.

### HUD and custom stages

Hide or restore the combat HUD from the pause menu or Video settings. The toggle affects the local display, including health bars, damage and technique labels, names, radar and battle overlays.

Install a custom stage by placing its compatible `.unk` model in the `stages` folder. Add a PNG with the same filename stem for its thumbnail and preview. See the [custom stage guide](docs/port/adding_stages.md) for supported images and installation examples.

## Installation

Download the [Windows package](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases/tag/bt4-r26-rc1) and extract the complete ZIP into the folder where you want to keep the game. Keep `Install.exe` and `Tenkaichi3Decomp.exe` together.

1. Open `Install.exe`.
2. Choose the installer language and the game language or languages.
3. Select a compatible Spanish or English **BT4 B14 Rev2 ISO** and complete setup.
4. Start the game with `Tenkaichi3Decomp.exe`.

### Updating an existing installation

Close the game, extract the new ZIP over the existing installation, and replace files when prompted. Keep `gamedata`, `saves`, `bt3_settings.txt` and `online_name.txt`.

Updates reuse the installed game data. The ISO does not need to be extracted again.

## Languages

The installer interface is available in Spanish and English. Game language selection covers both text and voices.

| Installation | Result |
| --- | --- |
| Spanish | Installs from the Spanish ISO and starts directly in Spanish. |
| English | Installs from the English ISO and starts directly in English. |
| Spanish and English | Installs the selected ISO and downloads the other language package from this repository. A language selector appears at startup. |

For a two-language installation, setup displays an **Installing language pack** progress indicator. The repository is private; authorize a GitHub account with read access when setup requests it. Existing installed language files can be reused.

## Development and feedback

The [contribution guide](CONTRIBUTING.md) describes the source layout, build tools and validation requirements. The [documentation index](docs/README.md) links to runtime, customization and original game architecture references.

For a useful bug report, include the game revision, the selected renderer, graphics scale, game mode, reproduction steps and the relevant `bt3_log.txt` or `bt3_crash.txt`. Use the [issue forms](https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/issues/new/choose).

## Credits and licenses

The original decompilation and PC port are by the **Tenkaichi3Decomp contributors**. This project is based on upstream `v0.1.38.9`; its exact source revision is recorded in [UPSTREAM_LOCK.json](UPSTREAM_LOCK.json). The [upstream README](README_UPSTREAM.md) is retained for attribution and reference.

BT4 content, artwork and icons remain the property of their respective authors. See the [presentation credits](docs/presentation/CREDITS.md), [AGPL license](LICENSE), [MIT license](LICENSE-MIT) and third-party library notices.
