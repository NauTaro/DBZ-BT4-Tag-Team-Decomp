# Review build helpers

These scripts record the local build workflow. Set BT4_SOURCE_ROOT to this
checkout. The native helper expects a MinGW UCRT toolchain at
`work/build-tools/ucrt64/bin` relative to the invocation directory and SDL under
`port/build/win/SDL3-3.4.12/x86_64-w64-mingw32`. Consult the upstream port build
documentation for a clean toolchain installation. Local QA runtimes and game
data are intentionally excluded. Build environments outside the original
Windows workstation have not been verified for this preview.
