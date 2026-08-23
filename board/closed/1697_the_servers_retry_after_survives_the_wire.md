Type: issue
Area: data
Tags: transport, boundary, backoff

**The server's Retry-After survives the wire**

1691 closed with its second demand explicitly deferred: "Retry-After remains unread at the
Wire — carried forward here honestly: the header needs a Wire field and a CurlTransport read,
a later slice of this item's area." A closing note is not a carrier — nothing open tracked
that slice until this item. The state today: `src/data/Transport.h` `Wire` transports status
and body only; a 429 with `Retry-After: 120` is answered by the house ladder capped at
kRetryCapMs 4000 (`src/data/SourceSet.cpp:12`), so the engine knocks again 116 seconds before
the server asked — the polite backoff exists, the server's own number still dies at the
boundary.

Demanded (1691's original wording): `Wire` carries an optional server-declared wait;
`CurlTransport` reads the header (seconds and HTTP-date forms); `SourceSet::Collect` lets the
declared wait override the derived one when it is longer; a fake-transport unit case proves
the override on the faked clock, sleeping never.

---

Progress -- the pipeline believes the server: Wire carries RetryAfterS (a third Answered
overload, 0 = unsaid), the source hands it through Fetched::MeantAfter, and SourceSet takes
the MAX of the server's ask and its own doubling backoff -- a server that says when to come
back is believed, and never hammered sooner. Proven: a 429 with Retry-After 10 s holds
through the five-second mark where the backoff alone would have fired, and goes out after
the asked-for wait. Remaining before close: CurlTransport reads the actual Retry-After
HEADER off the HTTP response (tools/host) -- the one seam still dropping it.

---

Closed -- the last seam reads the header: CurlTransport asks curl for CURLINFO_RETRY_AFTER
(curl parses both delta-seconds and HTTP-date into seconds), carries it through Transfer and
hands it to Wire::Answered's third argument; 0 stays "unsaid". The honoring is proven in
ARetryWaitsOnTheTransportsClock (a 429 with Retry-After 10 s outwaits the doubling backoff
and fires after the ask); the header path itself is compiled by every driver suite via
`test/run.sh --audit-link` and exercised the day a live host says it -- a faked curl would
prove curl, not this tree.
