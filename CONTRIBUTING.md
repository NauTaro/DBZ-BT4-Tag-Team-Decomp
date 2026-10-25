# Contributing

## Source layout

| Path | Contents |
| --- | --- |
| `src/`, `include/` | Game code and headers |
| `port/src/`, `port/include/` | Native runtime, rendering, networking and voice |
| `port/setup/` | Installation launcher |
| `port/release/` | Version identity, icons and portable layout |
| `tools/review/` | Build helpers and portable layout checks |
| `cloudflare/` | Tag Team room and voice service |
| `port/matchmaking/` | Matchmaking service |
| `docs/` | Architecture and development guides |

## Building and checking changes

See the [PC port guide](docs/port/README.md) and
[build helper documentation](tools/review/README.md) for dependencies and toolchain configuration.
Game data must come from your own compatible disc image.

Run the portable layout checks from the repository root with Python 3.12:

```sh
python -m unittest discover -s tools/review/tests -v
```

These checks run without game data. Gameplay, networking and audio changes also
need validation in the affected game modes and supported environments.

For decompiled game functions, follow the [decompilation guide](docs/decomp_guide.md).

## Reporting an issue

Use the [issue templates](https://github.com/NauTaro/tenkaichi3decomp-fork-DBZ-BT4-Decomp/issues/new/choose).
Include the version from `BT4_VERSION.txt`, system details, reproduction steps,
and expected and actual behavior. Combat reports should identify the characters,
stage, move and player count; voice reports should identify the audio devices.

## Pull requests

Keep changes focused and describe the resulting behavior and validation.
Document effects on game data, saves, determinism or protocol compatibility when relevant.
Retain existing credits and license notices.

Keep ISOs, extracted game data, player saves, credentials, logs and build outputs
out of commits. Include only relevant diagnostic excerpts in issues, with personal
information removed.
