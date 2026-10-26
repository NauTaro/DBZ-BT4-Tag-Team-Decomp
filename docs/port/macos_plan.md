# macOS architecture reference

The inherited upstream runtime includes an experimental native Apple Silicon
variant, selected with `BT3_CC=mac64`. DBZ BT4 Tag Team release packages target Windows.

## Memory model

The game stores four-byte pointers. On x86-64, the host runtime preserves those
layouts with `__ptr32 __uptr` and low-address mappings. Apple Silicon requires
address translation because the low 4 GB of the process address space are unmapped.

`port/tools/ptr32.py` transforms pointer operations for the target architecture.
`port/src/port_addr.h` defines the address translation interface.
Game structure layouts and deterministic arithmetic must remain consistent
across platform variants.

## Toolchain

The macOS variant uses Homebrew LLVM and the macOS SDK. `BT3_LLVM` selects an
alternate LLVM binary directory; `port/tools/toolchain.py` defines compiler and
linker options. The runtime uses SDL3 with Metal and CoreAudio.

Platform work should be validated with native builds, game-state comparisons
and rollback checks before claiming cross-platform compatibility.
