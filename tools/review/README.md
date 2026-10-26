# Build and layout tools

The build helpers default to this checkout. `BT4_SOURCE_ROOT` can override it;
`BT4_TOOLCHAIN` can point to a MinGW UCRT64 bin directory. Use matching Clang
and libclang versions, and provide SDL3 through `BT3_SDL3` or the pinned build
directory. Generated shader headers and external dependencies are required.

```sh
python tools/review/build_bt4_native.py
python tools/review/build_bt4_setup.py
python -m unittest discover -s tools/review/tests -v
```

The setup outputs are `port/build/setup_bt4/BT4-Setup.exe` and `Install.exe`.
The portable package places the former at `tbo/setup/BT4-Setup.exe` and the
latter in the installation root. SDL3 and the official GitHub CLI are required
alongside the setup helper. Account credentials are never package inputs.
Optional language package names, sizes and hashes are pinned in
`port/setup/language_packages.h`. Do not replace a pinned package in place.

`check_portable_layout.py` reads `port/release/PORTABLE_LAYOUT.json` and reports
unexpected root entries without deleting anything. It does not verify game
data. Use disposable runtime folders for build validation; do not overwrite
player data or settings as part of a source build.

The installer binds native data tables to its verified ISO during installation.
Do not ship raw generated data tables without the matching verified program
fingerprint. See the PC port guide and current private release for distribution.
