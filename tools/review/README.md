# Build and layout tools

## Windows build helpers

`build_bt4_native.py` builds selected game and runtime sources;
`build_bt4_setup.py` builds the installation launcher.

Set `BT4_SOURCE_ROOT` to the checkout. The helpers expect the MinGW UCRT toolchain
at `work/build-tools/ucrt64/bin` relative to the invocation directory and SDL3
under `port/build/win/SDL3-3.4.12/x86_64-w64-mingw32`.
Generated build inputs and external dependencies must already be available.

See the [PC port guide](../../docs/port/README.md) for the runtime structure
and toolchain configuration.

## Portable layout checker

With Python 3.12, run from the repository root:

```sh
python tools/review/check_portable_layout.py "path/to/installation"
python -m unittest discover -s tools/review/tests -v
```

The checker reads `port/release/PORTABLE_LAYOUT.json`. It checks root names and
file/directory types case-insensitively and reports unexpected entries without
deleting files. It does not inspect nested content or verify game-data integrity.

Exit codes: `0` for an accepted layout, `1` for unexpected entries, and `2` for
an unreadable directory or policy.

## Runtime preparation

`prepare_update_runtime.py` is a development utility requiring an external
`audit_mod` module, runtime assets and upstream archives. It writes test settings
to its target directory; use a separate disposable build directory.
