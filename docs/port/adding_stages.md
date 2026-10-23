# Custom stages

Create a `stages` folder next to the game executable and add stage model files
in `.unk` format. Restart the game to refresh the stage list.
`BT3_STAGES` can select another folder.

## Names and ordering

File names become menu names, with underscores replaced by spaces. An optional
`maps.txt` manifest sets display names and ordering:

```text
arena.unk|Arena Name
another-arena.unk|Another Arena
```

Unlisted files follow in alphabetical order. The runtime supports up to 26 added
stages. Added stages use a shared sound bank and a reused preview thumbnail.

## Import tool

From the repository root:

```sh
python port/tools/add_stage.py add "/path/Arena.unk" "Arena Name"
python port/tools/add_stage.py list
python port/tools/add_stage.py remove "Arena Name"
python port/tools/add_stage.py rebuild
```

The tool copies the model, updates the manifest and generates a menu-name strip.

| Option | Purpose |
| --- | --- |
| `--data <dir>` | Select the data directory |
| `--font <ttf>` | Select the font for generated name strips |
| `--no-strip` | Update the manifest without regenerating name strips |

## Runtime implementation

`port/src/plat_extras.c` handles folder discovery; `plat_stages.c` serves custom
stage resources. Added models have separate file IDs so they do not replace
disc files. The loader manages the stage buffer and rejects models that exceed
its supported allocation.

Menu labels use generated strips, the game's lettering where available, or
plain text fallback. Added resources are subject to the runtime's online
compatibility rules and may be unavailable during network sessions.
