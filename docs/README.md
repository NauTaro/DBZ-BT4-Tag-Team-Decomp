# Documentation

## Development

| Guide | Contents |
| --- | --- |
| [PC port](port/README.md) | Runtime architecture and build requirements |
| [BT4 profile](../BT4_PROFILE.md) | Game-data profile and network compatibility |
| [Decompilation](decomp_guide.md) | Matching C, symbol naming and compiler behavior |
| [Game overview](game_overview.md) | Original executable structure and subsystems |
| [Executable map](text_map.md) | Address ranges and library boundaries |
| [Netplay](netplay_notes.md) | Simulation, input and rollback considerations |

## Customization

| Guide | Contents |
| --- | --- |
| [Stages](port/adding_stages.md) | Stage manifests and importing |
| [Music](port/adding_songs.md) | Custom music and supported formats |
| [Menus](port/menus.md) | Menu layout and controls |
| [Fonts](port/menu_font.md) | Menu font configuration |
| [Translations](port/translations.md) | Translation files and text loading |

## Architecture reference

The [systems directory](systems/) documents the original game's battle, fighter,
combat, AI, graphics, audio, input, memory, file, menu and scripting systems.
[VU1 documentation](systems/vu1/README.md) covers the vector programs.

References to verified behavior describe matching C or a stated check; inferred
behavior is identified separately. Original BT3 implementation details do not
automatically describe every BT4 modification.

See [presentation credits](presentation/CREDITS.md) for repository media and
the root [license](../LICENSE) files for source licensing.
