# Custom stages

Copy compatible `.unk` map models into `stages` next to the game executable,
then restart. They appear at the end of the offline selector. Up to 26 added
maps are supported; custom maps are not offered online.

## Thumbnail and preview

Add a PNG with exactly the same filename stem:

```text
stages/Kaio Planet.unk
stages/Kaio Planet.png
```

The game uses it for the circular icon and full preview while retaining the
native cursor, transition and BT4 map-name lettering. Kaio Planet's included
PNG is a clean capture of the actual supplied stage, without HUD or fighters.
Recommended size: 512 x 448. Maximum: 2048 x 2048 and 8 MiB.
An absent, damaged or unsupported image uses the game's fallback artwork.

## Names and ordering

An optional `maps.txt` sets filenames, labels and ordering:

```text
Kaio Planet.unk|Kaio Planet
```

Use characters available in the native font. Other models follow in sorted
order. `BT3_STAGES` overrides the directory. `maps` is accepted as a legacy
folder alias when `stages` contains no models. Installing a model requires no
external image or font generator.
