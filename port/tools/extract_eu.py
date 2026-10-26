#!/usr/bin/env python3
"""Takes what the game needs for the European languages from a European disc image (SLES-54945).

Usage:
    python3 port/tools/extract_eu.py SLES-54945.iso [--data gamedata]

The European disc has the game's menus, its battle HUD and their text five times, once per language (English,
Spanish, German, French, Italian). This copies those archive entries, 60 of them, as they are into
gamedata/pzs3eu1/<entry>.bin (the layout the setup uses for the USA disc's archives). The game makes its
Spanish, German, French and Italian catalogs and its menu art from them at the next start (ui_text_extract.c,
menu_labels.c). The setup window does the same in a release folder. Like the rest of gamedata, these files are the
player's own disc data: never commit them.
"""
import argparse
import pathlib
import struct
import sys

SECTOR = 2048
# The USA disc's menu archives with text and painted lettering (ui_text_extract.c kUtMenuFiles, menu_labels.c
# kMlFiles): on the European disc archive 448 + k is entry 451 + k + 31 * language. The battle HUD (USA 5) is
# entries 5 to 9, one per language. Languages in the discs' order: English, Spanish, German, French, Italian.
USA_FILES = (448, 449, 450, 451, 452, 453, 459, 460, 461, 463, 472)
EU_FIRST, EU_STRIDE, HUD_FIRST, LANGUAGES = 451, 31, 5, 5


def entries():
    """The European entries the game reads."""
    out = [HUD_FIRST + bank for bank in range(LANGUAGES)]
    for usa in USA_FILES:
        out += [EU_FIRST + (usa - USA_FILES[0]) + bank * EU_STRIDE for bank in range(LANGUAGES)]
    return sorted(out)


def u32(data, at=0):
    return struct.unpack_from("<I", data, at)[0]


def iso_entries(disc, extent, size):
    disc.seek(extent * SECTOR)
    data = disc.read(size)
    if len(data) != size:
        raise ValueError("the disc image is cut short")
    pos = 0
    while pos < size:
        length = data[pos]
        if not length:
            pos = (pos // SECTOR + 1) * SECTOR
            continue
        record = data[pos:pos + length]
        if len(record) < 34 or 33 + record[32] > len(record):
            raise ValueError("the disc image's directory is damaged")
        name = record[33:33 + record[32]].decode("ascii", "replace").split(";", 1)[0]
        yield name, u32(record, 2), u32(record, 10), bool(record[25] & 2)
        pos += length


def iso_file(disc, path):
    """(offset, size) of a file of the image, or None."""
    disc.seek(16 * SECTOR)
    pvd = disc.read(SECTOR)
    if pvd[:7] != b"\x01CD001\x01":
        raise ValueError("this file is not a disc image (.iso)")
    root = pvd[156:156 + pvd[156]]
    extent, size, folder = u32(root, 2), u32(root, 10), True
    for part in path.split("/"):
        found = next(((e, s, f) for n, e, s, f in iso_entries(disc, extent, size) if n.upper() == part.upper()), None)
        if found is None:
            return None
        extent, size, folder = found
    return None if folder else (extent * SECTOR, size)


def extract(iso, data, say=print):
    """Copies the entries into <data>/pzs3eu1. Returns how many; raises ValueError with a sentence for the player."""
    out = pathlib.Path(data) / "pzs3eu1"
    with open(iso, "rb") as disc:
        if iso_file(disc, "SLES_549.45") is None:
            raise ValueError("this is not a disc image of the European release of Budokai Tenkaichi 3 (no SLES_549.45 on it)")
        afs = iso_file(disc, "DATA/PZS3EU1.AFS")
        if afs is None:
            raise ValueError("the disc image has no DATA/PZS3EU1.AFS")
        base, size = afs
        disc.seek(base)
        head = disc.read(8)
        count = u32(head, 4) if head[:4] == b"AFS\0" else 0
        if not 0 < count < 100000:
            raise ValueError("the archive on the disc image is damaged")
        table = disc.read(count * 8)
        out.mkdir(parents=True, exist_ok=True)
        want = entries()
        for k, index in enumerate(want, 1):
            if index >= count:
                raise ValueError("the archive on the disc image has fewer entries than the European release")
            offset, length = struct.unpack_from("<II", table, index * 8)
            if offset + length > size:
                raise ValueError("the archive on the disc image is damaged")
            disc.seek(base + offset)
            blob = disc.read(length)
            if len(blob) != length:
                raise ValueError("the disc image is cut short")
            part = out / f"{index:05d}.part"
            part.write_bytes(blob)
            part.replace(out / f"{index:05d}.bin")
            if k % 10 == 0:
                say(f"{k} of {len(want)} entries")
    return len(want)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("iso", help="the European disc image (SLES-54945)")
    ap.add_argument("--data", default="gamedata", help="the game data folder (default: gamedata)")
    args = ap.parse_args()
    try:
        n = extract(args.iso, args.data)
    except (OSError, ValueError) as e:
        sys.exit(f"extract_eu: {e}")
    print(f"{n} entries in {pathlib.Path(args.data) / 'pzs3eu1'}; the game makes the languages from them at its next start")


if __name__ == "__main__":
    main()
