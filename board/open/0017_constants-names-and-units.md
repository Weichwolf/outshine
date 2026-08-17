Type: bug
Area: core
Tags: perf, instrument

**Constants, names and units**

*Found 2026-08-12 in the design round for the const header. Every line here is a number that exists in
the tree more than once, or under a name that says the wrong unit.*

- **`core/Sha256.h`'s justification for 256 bits carries one right number and one wrong by
  seventeen orders of magnitude.** The header (`:11-14`) argues *"At 10^12 distinct keys the birthday
  bound is 10^24 / 2^257 ~ 1e-53 … a 64-bit truncation would be 1e-13 at the same load, which is
  not"*. The first is right to an order (n²/2·2²⁵⁶ = 4.3 × 10⁻⁵⁴). The second is **2.7 × 10⁴** —
  n²/2·2⁶⁴ at n = 10¹², i.e. a collision is *certain*, not one in 10¹³; the birthday threshold for
  64 bits is 2³² ≈ 4 × 10⁹, three orders below the stated load. The conclusion the number supports is
  unaffected and the derivation beneath it is false, which is the one thing `CLAUDE.md`'s *every
  number carries its origin* forbids. Right: the number, or the sentence goes. The six digests
  themselves are correct — all six vectors in `test/outshine/unit/core/Sha256MatchesTheStandard.cpp` reproduce
  against an independent SHA-256 (checked 2026-08-12).
- **`TilePool` holds a worker thread for up to 30 s where it held one for 3 s, asleep in a 1 ms
  loop.** `world/TilePool.cpp` `kPollMs = 1`, `kPollAttempts = 30000`. The previous shape blocked the
  worker inside one synchronous transfer for 60 × 50 ms; the new one sleeps a millisecond at a time
  and re-takes `CurlTransport::Mutex_` on every wake, against the transport's own 8 threads
  (`test/outshine/host/CurlTransport.cpp:14 kDefaultThreads = 8`) — up to 14 threads on 2 performance and 4
  efficiency cores (`CP.40`, `CP.41`). **Not attributable and the reason is stated**: the only
  comparison available is `verify-still` at 110–119 s against 103 s for the proxy version *in front
  of a warm container*, which is not a baseline — different upstreams, different cache state. The
  number that would decide it does not exist yet and is one field away: `Ledger::FetchMs` is now the
  sum of `SourceSet::Collect` calls rather than wire time, so wall-per-request split into wire and
  poll, p50/p95/p99 over a cold traversal, is a *not yet measured* and not a limit. Right: the
  transport declares a wait, so a thread with nothing to do blocks (`board/` § I.22).

- **The suffix `Ms` names two different units in the same tree.** `core/Units.h:22`
  `kMsToKt` is metres per second to knots. *Two of the three sites named here, `clients/Walker.h` and
  `clients/FrameTelemetry.h`, went with the browser-era clients on 2026-08-12; the rule they
  illustrated is unchanged.* Reading any one of them
  correctly requires reading its comment, against `CLAUDE.md`'s *a name that needs a comment is the
  wrong name*. `core/Units.h:15` `kMPerDeg` shows the unambiguous spelling already exists in the same
  file. Right: one declared suffix table, `MPerS` for velocity and `Ms` for milliseconds, applied
  everywhere; the ambiguity is decidable by grep and there are three sites.
