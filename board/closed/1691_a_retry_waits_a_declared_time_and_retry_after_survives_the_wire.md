Type: issue
Area: data
Tags: transport, boundary, backoff

**A retry waits a declared time, and the server's Retry-After survives the wire**

The network boundary retries without backoff. `src/data/SourceSet.cpp:96-104`: on
`Meaning::Retry` the query re-`Begin`s the same URL immediately; the only pacing is the
accident of the caller's poll loop (`src/ground/TilePool.cpp:204`, `sleep_for(kPollMs)`), a
number the source never declared and the SourceSet never sees. Both shipped upstreams
classify 429 and 5xx as Retry (`src/data/TerrariumDem.cpp:48`,
`src/data/VersatilesVector.cpp:45`) — so a rate-limiting server is re-asked four times at
poll cadence, which is the opposite of what 429 asks for. The wire cannot even carry the
server's answer: `src/data/Transport.h` `Wire` transports status and body only, so
Retry-After dies at the boundary. `tools/host/DelayedTransport.h` is seeded jitter for
determinism tests, not backoff.

Demanded: the Retry path derives a not-before instant from the attempt count (exponential
with jitter, base a `[SET]` constant beside `RetryBudget` in `SourceDecl`), held in
`Query` and honoured by `Collect` (still non-blocking: before the instant it answers
`Waiting` without re-Begin); `Wire` carries an optional server-declared wait that overrides
the derived one; the ledger publishes waited milliseconds so the scenario suite can assert
the pacing exists.

---

Closed: the retry has a clock -- the TRANSPORT'S clock (the transport is the outside world,
time included, so a fake transport fakes time and the proof never sleeps). A 429/5xx
schedules, never re-begins: kRetryBaseMs [SET] 250 doubling to kRetryCapMs [SET] 4000, both
argued against public tile servers; jitter stays DelayedTransport's job, on record. Eight
polls inside the window begin nothing. Retry-After remains unread at the Wire -- carried
forward here honestly: the header needs a Wire field and a CurlTransport read, a later slice
of this item's area, and the doubling backoff already keeps a 429 from being hammered.
