# Ranked abandonment policy

The policy defaults off in a fresh database and is enabled in production.
See ONLINE_BASELINE_20261119.md and RANKED_RULES.md for the current ordinary
loss rule and deployment baseline. This patch does not reset scores or add
the deferred hybrid win bonuses.

## Player-facing behavior

An authenticated surrender after both players have started, or a sustained
one-sided disappearance confirmed by the server, counts as a defeat for the
departing player. Their penalty is 100 points, with a minimum balance of zero.
Their streak ends. The normal loss is not charged in addition.

The remaining player receives the normal victory reward under the match's
existing scoring policy. The reward is not funded by the quitter's balance.
Repeated suspicious results can still put the reward under review; a confirmed
abandonment still incurs its penalty. A reviewed reward can therefore be zero,
but is not a negative delta for the remaining player.

New Ranked searches require the updated client's `abandonment_version: 2`.
Activation retires pending Ranked searches so an old queued client cannot be
paired under the new policy. Admitted rooms and matches are retained.
This applies only to Ranked matches admitted after server activation. Existing
matches retain their admitted policy. Quick Match, private rooms and Tag Team
are not changed. Completed results cannot be replaced by later abandonment
reports. Previous-month matches cannot deduct points from the new month.

## Evidence and limits

The server accepts an explicit surrender only from that player's authenticated
session. The client records which player confirmed an exit item in the pause
menu. Merely having a pause menu open during a connection failure is not an exit.
The recorded exit must survive input confirmation before it is sent.

For a missing opponent, the remaining client must keep sending authenticated
heartbeats claiming the transport loss for at least 45 seconds, with no gap over
20 seconds. The opponent must also have been absent from the server for at least
45 seconds. Their return clears the witness chain. Both clients still sending
heartbeats, an interrupted witness chain, or conflicting result reports do not
establish a unilateral abandonment. No penalty is applied before both clients
have marked the match as started.

Health bars, local win predictions and accusations are not used to decide who
quit. The server can observe absence, not human intent. A sufficiently long
accidental outage or crash can also count as abandonment. Malicious clients and
all network failure patterns cannot be ruled out by these observations alone.

## Implementation

- `cloudflare/ranked_abandonment.mjs`: admission snapshot, liveness evidence,
  confirmed quitter and independent winner reward for legacy Elo matches.
- `cloudflare/online_core.mjs`: settlement in the existing synchronous storage
  transaction. Both histories, the penalty audit and streak changes commit
  together; retries return the saved result.
- `cloudflare/migrations/20261117_ranked_abandonment.sql`: additive, repeatable
  tables. Historical matches, accounts and scores are retained.
- `port/src/gs/match_supabase.cpp`: current Cloudflare client adapter. A pending
  normal result no longer prevents its watch from reporting a later missing
  opponent. Generic teardown does not become an explicit surrender.
- `src/battle/hud_0.c` and `src/battle/btl_seq.c`: confirmed pause-exit identity,
  replacing inference from who had the pause menu open.
- `src/menu/port_tbo.c` and `port/src/gs/online_language.c`: EN/ES/PT rules show
  the separate penalty only when the server advertises the policy.

Activation, after release validation, uses the existing authenticated admin
route: `POST /v5/admin/ranked-abandonment` with `{"enabled":true}`. The response
states `applies_to: "new_matches"`. Disabling future admissions uses false;
already admitted matches keep their policy. Operational activation records and
encrypted score backups are retained outside the public repository.

## Validation

Tests cover both player identities, fixed penalties at rating boundaries,
normal result preservation, interrupted/recovered connections, repeated
reports, suspicious rewards, atomic rollback after a failed write, expired
matches, month changes, migration reruns and authenticated activation.

Native regression tests exercise the actual client adapter and the extracted
pause-exit helpers with both local player positions. These use local fixtures
and mocked service replies. They do not constitute a live two-player internet
test, and cannot establish that every failure scenario is covered.
