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
