Type: bug
Area: ground
Tags: naming, measured
Parent: 1806

# A tile the field has settled is not a tile it decoded

Found by making `OsmField` testable (`board:1806`). Its two "is this tile in the field"
questions live in two containers and answer differently:

| door | reads | written by |
|---|---|---|
| `OsmField::Decoded(x, y)` (`OsmField.cpp:63`) | `Done_` | `Build`, after `AddTile` returns true |
| `OsmField::TileIndex(x, y)` (`:67`) | `Tiles_` | the accept path, only when bytes arrived |

`AddTile` returns **true** for `Reply::Absent` and `Reply::Undeclared` (`:86`) -- the tile was
asked for and settled, and it carries nothing. `Build` then records it in `Done_`. So:

> **A tile with no data at all reports `Decoded() == true`.**

And the converse, exposed the moment the pool was taken out of the door: a tile accepted
directly from bytes lands in `Tiles_` and never in `Done_`.

```
NOTE features the field took from it = 1 features
NOTE the field's heap = 668 bytes
NOTE is the tile decoded = 0
NOTE its index = 0
```

One feature decoded, 668 bytes of it on the heap, indexed at 0 -- and `Decoded()` says no.

`Done_` means SETTLED: asked, answered, not to be asked again. That is a useful set and it is
the right one for `Build`'s loop. It is not decodedness, and the name says it is.
`src/clients/Sim.cpp:173` reads it as decodedness:

```cpp
if (!W_.Vectors().Decoded(region.X(), region.Y())) return nullptr;
```

which is the one live caller, and it is asking "are there features here" of a set that answers
"did I stop asking".

## What will be true

- [x] The settled set is named for what it holds, and the accept path settles what it accepts,
      so the two doors cannot disagree about the same tile.
- [x] Whatever `Sim.cpp:173` actually needs -- decodedness or settledness -- it asks for by
      name, and the choice is recorded.
- [x] Proving test: `test/unit/ground/AVectorTileBecomesAFieldTheGroundCanRead` asserts that a
      tile accepted from bytes is both indexed and settled. Negative control: the settle
      removed from the accept path -> the twin names the disagreement.

## Comments

- 2026-08-24 -- repaid. `Done_` is `Settled_`, `Decoded(x, y)` is `Settled(x, y)`, and the
  accept path settles what it accepts through one idempotent `Settle(x, y)` that both `Build`
  and `Accept` call. The two doors cannot disagree about a tile any more.
- **What `Sim.cpp:173` needed was settledness**, and it now asks for it by name. It guards a
  region hand-out: *"has this tile been asked for and answered"* is the right question there --
  a region over a tile that legitimately holds no vector data must not be re-asked forever.
  Under the old name it read as *"are there features here"*, which is a different question and
  would have been the wrong guard.
- **Proving test**: `test/unit/ground/AVectorTileBecomesAFieldTheGroundCanRead` -- a tile
  accepted from 79 hand-encoded bytes is indexed at 0, settled, and 668 bytes on the heap.
- **Negative control**, run: `Settle(tx, ty)` removed from `Accept` ->

  ```
  14 tests: 12 PASS  2 FAIL
  FAIL **AND A TILE THE FIELD ACCEPTED IS BOTH INDEXED AND SETTLED**
  ```
