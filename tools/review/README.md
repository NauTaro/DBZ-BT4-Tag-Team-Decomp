# Review build helpers

These scripts record the local build workflow. Set BT4_SOURCE_ROOT to this
checkout. The native helper expects a MinGW UCRT toolchain at
`work/build-tools/ucrt64/bin` relative to the invocation directory and SDL under
`port/build/win/SDL3-3.4.12/x86_64-w64-mingw32`. Consult the upstream port build
documentation for a clean toolchain installation. Local QA runtimes and game
data are intentionally excluded. Build environments outside the original
Windows workstation have not been verified for this preview.

## Portable layout checker

From the repository root, with Python 3.12:

```sh
python tools/review/check_portable_layout.py "path/to/installation"
python -m unittest discover -s tools/review/tests -v
```

The checker loads `port/release/PORTABLE_LAYOUT.json` relative to its own checkout,
so an absolute script path also works from another working directory. It compares
root entry names and file/directory types, case-insensitively for the Windows
layout. It does not inspect nested content, require every allowed entry to exist,
or verify game-data integrity. An empty directory is a valid layout plan.

Output is JSON. Exit codes: `0` for an accepted layout, `1` for unexpected root
entries, and `2` for an unreadable directory or policy. Unknown entries are
reported for manual review; the checker never deletes files.

## Local build limitations

- `build_bt4_native.py` and `build_bt4_setup.py` require the external Windows
  toolchain and pre-existing build dependencies described above.
- `prepare_update_runtime.py` is a workstation-specific QA helper. It imports an
  external `audit_mod` module that is not included here, reads local runtime assets
  and upstream archives, and writes test settings into its target directory.
  Use only a separate disposable QA directory as its target; it is not an updater
  for a live player installation.
- The portable-layout tests run without those dependencies or game data. Passing
  them does not establish that a complete native build or gameplay test passes.
