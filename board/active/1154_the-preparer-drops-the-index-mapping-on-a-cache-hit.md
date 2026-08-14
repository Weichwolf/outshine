Type: bug
Area: corpus
Tags: oracle, instrument

**The preparer drops the index mapping on a cache hit, so a cached pass names nothing**

`jobs.py` records the `pass_index` → material-name mapping on the **`miss`** path only: a cached render
returns `{"cache": "hit", ..., "provenance": None}`. So a case whose oracle was already in the store keeps
its index products and loses the only thing that makes them readable.

**[MEASURED] on the tree today**: `materials/normal-tangent` and `materials/normal-tangent-mirror` each
carry `oracle.materialIndex.raw` and `oracle.objectIndex.raw` at **14 745 644 B each — 29.5 MB per case —
with `provenance: null`**. Two passes, 29.5 MB, naming nothing.

**Why it is a bug and not a gap.** The products are produced, stored, placed and read; the reader
(`board:1138`) resolves the mapping from the `provenance.json` render entry whose `products` contain the
pass's own raw, and on these two cases there is no such entry. **The pipeline works and the artefact it
produces is incomplete in a way only a second run reveals** — the first run after a `prepare.py` cold
start is correct, and every run after it is not. That is the worst direction: the failure appears when
nothing changed.

**Where it belongs.** The mapping is a **property of the products**, not of the run that happened to
produce them, so it belongs **in the store beside them** and comes back on a hit with the bytes. The
content store is hash-named files with no sidecar by design, so the mapping is either its own keyed
product — one more entry in `PRODUCTS`, keyed like the rest — or a field of the render entry that the hit
path reconstructs rather than omits. **The first is the one that cannot drift**: a mapping stored under
its own key alongside the pass it describes is invalidated by the same `preparer` digest that invalidates
the pass.

**The caveat, sought and cleared.** *Could the reader simply derive the mapping itself?* No — it is
assigned inside Blender over `bpy.data.materials` **including the factory startup file's own materials**,
in name order, so index *n* is never material *n* and the ordering is not reconstructible from the glTF.
That is exactly why `board:1138` reads it rather than assuming it, and why losing it is fatal rather than
inconvenient.

**Done when** a cached oracle returns its index mapping with its products, a case placed entirely from the
store adjudicates surface identity, and the two tangent cases above stop carrying 29.5 MB that names
nothing.
