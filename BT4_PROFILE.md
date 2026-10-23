# Local BT4 B14 Rev2 native profile

Based on Tenkaichi3Decomp ec3c85a98cffd9cd22c22ac14cd62fb584d23664 (0.1.37). This directory contains the modified C game and PC runtime sources. It is not a universal ISO interpreter.

Expected source disc: SLUS_219.78.DBZBT4B14REV2.iso, size 5396742144. The flattened SLUS_219.78 SHA-1 is 2ae9c636de479c01d2b4e9906c25b7ca807e80d9; DBZP.BIN SHA-1 is de91913f40bd675d70ee748aa16a42a96dff5919. Extracted pzs4us0 through pzs4us3 contain 50503, 3400, 32774 and 30000 entries respectively. Data remain local to the user's installation.

The modified resource loader uses BT4's four archive ranges, cumulative costume model numbering, separate per-character parameter/misc/lip packs and expanded animation/model buffers. The main menu uses BT4's alternate guide texture sections. port_bt4_label.h holds bounded patches for the 4-bit unselected and 8-bit selected online label sheets. make_bt4_label4.py reproduces those patches from the original bundled lettering and the two locally available discs' palettes and upload layouts.

BT4 includes malformed optional animation tracks, observed in character 13's motion 0x26 at nodes 64 and 66. BtlObjAnim_Load validates track extents and packed quaternion indices, and leaves those nodes at the model's rest pose. This avoids the unchecked decoder corrupting its native stack. Very large secondary-chain angles use the port's deterministic fmodf implementation before wrapping; ordinary angles retain the original calculation.

The networking transport is Tenkaichi Decomp's protocol-5 implementation, with NET_VERSION 0x42543405 so this simulation has its own handshake and matchmaking pool. The public matchmaking service accepted room creation and joining with this version during the local tests. Do not advertise compatibility with vanilla BT3 or another mod's simulation.

TBO's online gate uses Port_DataState from plat_verify.c and the BT4-specific CRC reference in plat_verify_tab.h. It does not treat BT4 as a matching USA fingerprint. The complete verification covers 116685 files (four archives and eight loose disc files), retaining the native cache mechanism and detecting missing, changed or overridden resources. The USA gameplay fingerprint routines remain in the source but are not a BT4 data definition.

Build helpers in the adjacent herramientas folder describe the local Windows compiler setup. Source functions are transformed through port/tools/undefined.py and compiled with the bundled 64-bit Windows toolchain, while preserving the game's four-byte pointer domain and deterministic float implementation. Linking also generates a .dat file: prepare_bt4_test_runtime.py binds its existing table checksum to the verified BT4 ELF and overlay. No ISO data or build-key secret is included in this source archive.

Tests cover a native demo, 84 roster demo battles, the interactive main menu, CRC verification and TBO room creation, and direct/public-room network pairs with rollback, simulated latency and packet loss. A public-room pair compared 5997 confirmed state hashes without differences. This was two local processes using the real public matchmaking service; a match between two separate home networks was not tested. Full story-mode coverage and all BT4-specific mechanics remain unverified.
