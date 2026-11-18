# Online admission and connection recovery

The shared-IP ingress allowance is 600 authentication attempts and 600 WebSocket
handshakes per minute. Successful session renewal additionally uses a separate
30-per-minute budget for each account. These are admission budgets, not a limit
of 600 players. Operation budgets, authenticated access and existing bans remain
in place. These settings do not change gameplay packets, relay regions or ping.

A valid desktop refresh credential is renewed in place for 30 days of inactivity.
Access tokens still expire after 15 minutes. The server stores only the refresh
hash. A lost renewal response does not invalidate the locally saved credential
after two minutes. An unknown, expired or mismatched credential cannot recover
an account. Previously lost credentials are not reconstructed from nicknames.

The client temporarily uses HTTPS after an unsuccessful WebSocket connection,
backs off repeated identity failures and waits a minute after a rate-limit
response. It does not drain queued profile requests with an unrenewed identity.
The nickname lookup retries after a temporary failure instead of requiring a
game reinstall or a menu restart. Saves and account identifiers are retained.

Local tests exercise 300 accounts sharing an IP, per-account retry isolation,
invalid and expired credentials, lost renewal replies, blocked upgrades,
socket authentication failure and automatic nickname recovery. These fixtures
cannot establish reachability from every ISP, proxy or country.
