#!/usr/bin/env python3
"""Stage first-party PE files and put verified Authenticode files back in the release ZIP."""
import argparse
import pathlib
import shutil
import struct
import tempfile
import zipfile


BINARIES = ("BT4decomp.exe", "BT4decomp-setup.exe")
PREFIX = "BT4decomp/"


def pe_fields(data):
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('Expected a Windows executable')
    header = struct.unpack_from('<I', data, 60)[0]
    if header+24+152 > len(data) or data[header:header+4] != b'PE\0\0':
        raise ValueError('Invalid PE header')
    optional = header+24
    magic = struct.unpack_from('<H', data, optional)[0]
    if magic not in (0x10b, 0x20b):
        raise ValueError('Unsupported PE optional header')
    security = optional+(112 if magic == 0x20b else 96)+4*8
    return optional+64, security


def same_program(unsigned, signed):
    """Only checksum, certificate directory, alignment and signature may change.

    Trust validation is performed by Windows before this integrity check. A
    certificate-shaped byte array alone is not a trusted signature.
    """
    checksum, security = pe_fields(unsigned)
    if pe_fields(signed) != (checksum, security) or any(unsigned[security:security+8]):
        raise ValueError('The original build must be unsigned with matching headers')
    offset, size = struct.unpack_from('<II', signed, security)
    if size < 8 or offset < len(unsigned) or offset-len(unsigned) > 7 or offset%8 or offset+size != len(signed):
        raise ValueError('Missing or unexpected appended Authenticode certificate')
    if any(signed[len(unsigned):offset]):
        raise ValueError('Unexpected data before certificate')
    before, after = bytearray(unsigned), bytearray(signed[:len(unsigned)])
    for position, length in ((checksum,4),(security,8)):
        before[position:position+length]=b'\0'*length
        after[position:position+length]=b'\0'*length
    if before != after:
        raise ValueError('Signing changed executable code, resources or program data')


def stage(archive, directory):
    directory.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as source:
        if len(source.namelist()) != len(set(source.namelist())):
            raise ValueError('Duplicate archive entries')
        if source.read(PREFIX + "Install.exe") != source.read(PREFIX + BINARIES[1]):
            raise ValueError("Install.exe must match the built setup executable")
        for name in BINARIES:
            with source.open(PREFIX + name) as executable, (directory / name).open("wb") as output:
                shutil.copyfileobj(executable, output)


def replace(archive, directory):
    replacements = {PREFIX + name: directory / name for name in BINARIES}
    replacements[PREFIX + "Install.exe"] = replacements[PREFIX + BINARIES[1]]
    if not all(path.is_file() for path in replacements.values()):
        raise ValueError("Both verified executables must be present")
    if {p.name for p in directory.iterdir() if p.is_file()} != set(BINARIES):
        raise ValueError('Unexpected files in the signed artifact')
    with zipfile.ZipFile(archive) as source:
        for name in BINARIES:
            same_program(source.read(PREFIX+name), (directory/name).read_bytes())
    with tempfile.NamedTemporaryFile(dir=archive.parent, suffix=".zip", delete=False) as file:
        temporary = pathlib.Path(file.name)
    try:
        with zipfile.ZipFile(archive) as source, zipfile.ZipFile(temporary, "w") as destination:
            entries = source.namelist()
            if len(entries) != len(set(entries)) or not replacements.keys() <= set(entries):
                raise ValueError("Unexpected Windows ZIP layout")
            for entry in source.infolist():
                replacement = replacements.get(entry.filename)
                stream = replacement.open("rb") if replacement else source.open(entry)
                with stream as data, destination.open(entry, "w") as output:
                    shutil.copyfileobj(data, output)
        temporary.replace(archive)
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("stage", "replace"))
    parser.add_argument("archive", type=pathlib.Path)
    parser.add_argument("directory", type=pathlib.Path)
    args = parser.parse_args()
    (stage if args.operation == "stage" else replace)(args.archive, args.directory)
