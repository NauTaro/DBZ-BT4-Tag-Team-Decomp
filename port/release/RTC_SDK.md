# Native RTC SDK

The Windows runtime and native builds use libdatachannel 0.24.6. The native build
locks the upstream commit and each compiled submodule archive in
`rtc-sources.lock.json`; every download is checked against its SHA-256 digest.
The Windows SDK's recorded upstream commit is the same
`6b1e2e620f1e37f0eafeee702eaea0043cb305fd`.

`build_rtc.py` builds the C API with SCTP DataChannels and media enabled, using
the locked libjuice, usrsctp, libSRTP and plog sources. Examples, upstream tests
and the separate WebSocket API are disabled. Game and voice WebSockets retain
the existing WinHTTP/libcurl transports. The SDK does not depend on enabling
voice capture or loading a microphone.

Linux builds install the SDK in the release Docker image. macOS builds compile
the same source lock for Apple Silicon and use Homebrew OpenSSL. OpenSSL remains
a shared dependency; `package_native.py` copies its library closure together with
SDL, curl, Opus and libdatachannel. The native package also includes copyright
notices, build arguments, source URLs and digests, and the compiled source subset.
Complete unmodified archives remain identified by the source lock. No game disc
data is part of this SDK or its tests.

`port/tests/native_rtc/run.py` compiles a host C program that loads the actual
library through SDL. It checks the required C exports, creates two loopback-only
peers, exchanges SDP and ICE candidates locally, and verifies 512-byte binary
messages over real SCTP DataChannels. It tests both unordered reliable delivery
and the `bt4-tag-input-v1` unordered unreliable mode with zero retransmissions.
Native CI runs it before packaging and
again against the packaged library, after library-path rewriting and signing.
The probe opens no audio device and contacts no external STUN server. Its timing
describes the local test, not Internet latency or gameplay performance.

Build provenance includes the exact SDK sources and options. Compiler, OpenSSL
and operating-system builds still differ between platforms; this is not a claim
of identical binary output across systems.
