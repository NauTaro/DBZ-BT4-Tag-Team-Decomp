# Online session recovery, R5 1.6

The incident investigation observed HTTP 401 responses from the production
session refresh endpoint while new authenticated sessions could read standings,
presence, create rooms and join them. This establishes a rejected-session
failure path; it does not identify the cause of every individual user report.

The previous client returned immediately when its saved Cloudflare refresh
token was rejected. It did not try its retained legacy account proof, so that
installation could keep retrying the same token without reaching any online
menu operation. The recovery regression test fails against that implementation.

The client now uses the existing authenticated legacy account proof after an
explicit `AUTH_REQUIRED` response. It does not fall back to creating an anonymous
replacement account. Temporary HTTP or network failures retain saved credentials.
New session caches retain the authenticated account ID, which must match before
recovery can replace the Cloudflare session. An obsolete secondary token no
longer prevents checking a valid primary session.

WebSocket authentication failure and HTTPS API authentication rejection also
invalidate the cached access token's local deadline, allowing a real renewal.
The update does not change the server admission state, ranking records, combat
protocol, room ownership, TURN routing or simulation rules.

## Validation

- Nine deterministic C++ authentication scenarios compiled from the production
  functions passed, including original-account recovery, transient failures,
  account mismatch rejection, fresh installation and legacy outage. Run
  `python port/tests/test_cloudflare_session_recovery.py` with a C++17 compiler.
- The same suite reproduces a failure against the previous release's functions.
- The native Windows signaling code was compiled and exercised against the live
  authority using isolated test sessions. A deliberately rejected Cloudflare
  token recovered through the retained legacy session. The authenticated account
  ID was saved with the replacement token.
- Both native WebSocket and forced HTTPS paths then read standings and presence,
  and created/joined private duel, Tag Team and lobby rooms in both hosting
  directions. Test rooms used isolated compatibility strings and were closed.
- 41 existing authority, migration and Tag Team transport tests passed.
- The public Windows client linked successfully with the changed signaling code.

Recovery requires a valid saved proof for the existing account. If every local
credential has been removed or invalidated, the client cannot prove ownership
from a public nickname alone. The change does not bypass authentication or allow
another person to claim that nickname or its ranking history.
