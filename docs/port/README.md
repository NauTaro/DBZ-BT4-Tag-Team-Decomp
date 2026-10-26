# PC port

DBZ BT4 Tag Team Decomp runs the game natively on Windows. The runtime adapts
the original game code for host memory, deterministic arithmetic, rendering,
audio, input and online play.

## Architecture

| Component | Location |
| --- | --- |
| Game logic | `src/`, `include/` |
| Platform compatibility | `port/include/port_compat.h`, `port/src/` |
| Rendering and interface | `port/src/gs/` |
| Windows installer | `port/setup/` |
| Compiler and source conversion tools | `port/tools/` |
| Runtime libraries and notices | `port/voice-runtime/`, `port/third_party/` |
| Tag Team service | `cloudflare/` |
| Matchmaking | `port/matchmaking/` |

The game retains a four-byte pointer domain and deterministic floating-point
behavior. Platform changes must preserve game structure layouts, address
translation and network simulation behavior.

## Build requirements

The Windows build uses a MinGW-w64 toolchain, Clang/libclang, Python, SDL3 and
the voice dependencies. Compiler selection and dependency paths are defined in
`port/tools/toolchain.py`. The `BT3_CC=win64` variant selects Windows;
`BT3_SDL3` selects the SDL3 development directory.

The BT4 build helpers are documented in [tools/review/README.md](../../tools/review/README.md).
They require the external toolchain and generated build inputs described there.
Game data and the external toolchain are not included in the source archive.

`install.py` contains the inherited upstream setup workflow for original BT3 data.
For a BT4 installation, use the Windows package and a compatible **BT4 B14 Rev2 ISO**.
See the [BT4 profile](../../BT4_PROFILE.md) for runtime and protocol identity.

## Development checks

Run the portable layout tests from the repository root:

```sh
python -m unittest discover -s tools/review/tests -v
```

Simulation changes require deterministic state comparisons and network checks.
Rendering, audio and input changes require validation on the affected platform.
The [decompilation guide](../decomp_guide.md) describes original-binary comparisons.

## Customization and platform reference

- [Stages](adding_stages.md)
- [Music](adding_songs.md)
- [Menus](menus.md)
- [Fonts](menu_font.md)
- [Translations](translations.md)
- [Inherited macOS architecture](macos_plan.md)

The upstream source includes other platform variants; the BT4 package targets Windows.
