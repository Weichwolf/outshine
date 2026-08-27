Type: feature
State: active
Parent: 1953
Depends: 1955
Area: content

# A resource loads by mapping bytes and not by parsing

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

What would change the answer, and each is a measurement rather than an opinion: a world wide enough
to exceed the tile budget (board:1955 -- the eviction path has never run), a tile format costlier
than PNG, or a camera that outruns the fetch. Until one of those reads differently this item is
CORRECT and NOT URGENT, and it says so with its numbers rather than waiting to be rediscovered.

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
