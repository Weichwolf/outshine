Type: feature
State: open
Parent: 1953
Depends: 1955, 1964
Area: content

# A resource loads by mapping bytes and not by parsing

**Benchmark** — Unreal: import -> DDC -> cooked platform data, still parsed at load. RAGE: **map the bytes, fix the pointers, no parse**. **Taking RAGE** — a load that parses cannot keep up with a camera, and cell streaming is unaffordable without it.

**MEASURED BEFORE BUILDING, AND THE MEASUREMENT SAYS WAIT.** The store holds what was FETCHED, not
what is USED: a cache entry opens `89 50 4E 47` -- a PNG, 97496 bytes, decoded on every load. So
the defect this item names is real and present.

What it costs, measured over a drive:

    tiles decoded                        4518 tiles
    seconds spent fetching AND decoding  0.0114 s

Eleven milliseconds for four and a half thousand tiles, fetching included -- about 2.5 microseconds
each, and the fetch is most of it. At this scale parsing is not what keeps a camera waiting, and
building the baker now would be optimising a term measured NOT to bind. That is the same discipline
board:1926 applied to its indirect draw: read the number first, and let it decide.

**AND TARGET'S OWN REASON DOES NOT YET APPLY HERE, WHICH IS THE SHARPER FINDING.** TARGET says RAGE
wins because *a load that parses cannot keep up with a camera*. That is true where the source is
LOCAL and fast -- a shipped game reading its own disc, which is the case RAGE was built for. Here
the source is the NETWORK, so the fetch dominates and the parse hides behind it. Zero-parse becomes
the binding term only once the bytes are already local, which is board:1964's remaining half:
pinning the tiles by URL and hash so a drive runs offline and deterministically.

PARKED, with the reason named as CLAUDE.md requires: correct, measured, and not yet the term that
binds. It becomes urgent when any of three readings change -- tiles pinned locally (board:1964), a
world wide enough to exceed the tile budget (board:1955, whose eviction path has never once run),
or a format costlier than PNG. Whoever picks it up next argues with the numbers rather than
rediscovering them.

**RAGE wins this row clearly.** A resource is stored in the layout it will be USED in: the loader
maps the bytes, fixes the pointers, and the object is live -- no parse, no per-item allocation, no
construction pass. Unreal's DDC and cooked assets get part of the way and still parse.

This is not a performance nicety, it is what makes cell streaming affordable: a cell that must
parse its content cannot arrive at the speed a camera moves, and no amount of threading fixes a
per-item cost that scales with content.

The content store is already hash-addressed, so the addressing half is done. What is missing is
that what the hash names is a BLOB in final layout rather than a document to be read.

- [ ] a stored part is in final layout and loading it performs no per-item work
- [ ] loading a part allocates once and parses nothing, proven by a case counting both
- [ ] the glTF reader becomes a BAKER that writes the blob, and the frame path never sees glTF
