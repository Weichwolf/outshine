Type: debt
State: open
Area: world, compositor, generators, engine
Tags: measured, memory, performance, owner

# A place costs what its geometry is worth, and the arithmetic says what that is

**Benchmark** -- RAGE streams a heightfield and per-cell props into pools sized once, and a cell
that leaves gives its memory back; the transient cost of standing a cell is the cell, never the
world. Unreal's World Partition loads and UNLOADS cells, and `bAllowCPUAccess` is off so the render
mesh is not addressable from the CPU at all. **Both agree**: the resident cost is the RING, and the
transient cost of a tile is that TILE.

## The budget, computed rather than guessed

OldTown, the smallest of the nine: 349 766 triangles, 128 tiles resident, about 200 000 vertices
once corners are shared.

| what must exist | arithmetic | bytes |
|---|---|---|
| vertex data | 200 k x (12 position + 12 normal + 8 uv + 4 colour as RGBA8) | 7.2 MB |
| indices | 349 766 x 3 x 4 | 4.2 MB |
| elevation, resident | 128 x 257 x 257 x 2 (int16 is a centimetre over any relief) | 16.9 MB |
| land class raster | at the resolution the ground shader samples | ~8 MB |
| collision | ~15 000 footprint prisms x ~130 | 2.0 MB |
| the road graph | ways, widths, classes | ~5 MB |
| decode scratch | ONE tile at a time | ~2 MB |
| **resident** | | **≈ 45 MB** |

**Measured today: 393 MB live, 1 570 MB resident set.** Nine times the arithmetic, and thirty-five
times it once the allocator's high-water mark is counted.

**And the transient cost is the worse number.** One rebuild produces about 11 MB of geometry and
allocates 11.5 GB to do it -- a factor of ONE THOUSAND. A clean implementation allocates two to
three times what it produces, because it writes into buffers it sized once.

Where the churn goes, measured by tagging the passes:

| tag | churn |
|---|---|
| `world-ground`, the rest of it | 6 392 MB |
| `ground-yield` | 3 572 MB |
| `ground-classify` | 745 MB |
| `ground-patchwork` | 323 MB |
| `road-pave` | 225 MB |
| `road-fit` | 168 MB |
| `ground-model` | 99 MB |

## Why chasing the hot spots inside this design is the wrong move

The factor is a THOUSAND, not a third. That is not a slow loop, it is the wrong granularity: the
world is derived per WORLD and not per TILE. Every rebuild reclassifies every vertex of the whole
ring, re-derives every road of the whole ring, and grows one set of arrays for all of it -- so a
tile landing costs the world, and the arrays are reallocated as they grow past megabyte sizes.

Two repairs this round proved the point from both sides. Replacing the per-pass `unordered_map` in
`Refine` and `Cut` with an open-addressed map halved both passes -- 702 -> 454 ms and 1 095 ->
484 ms -- and moved the BYTE churn by nothing at all, because the bytes are not in the map. And
reserving the mesh vectors to exactly the count the split announced made the churn WORSE, 3 572 ->
3 804 MB, because reserving exactly turns geometric growth into linear growth and copies more.
Both are one-per-cent answers to a one-thousand-times question.

## What doing it right looks like, and it is the owner's own three stages

> "die osm rohdaten kommen in beliebiger reihenfolge rein -> osm generator baut super schnell und
> effizient die daten für den engine/renderer -> engine/renderer halten nur was notwendig ist"

- **raw is TRANSIENT.** A vector tile is decoded into a scratch that is reused for the next tile.
  Nothing of the raw survives the derivation
- **derived is PER TILE.** A tile that lands produces its own vertices and indices, into a buffer
  whose size is known before it is filled -- features times the most vertices a feature can carry.
  Nothing outside that tile is touched, so a tile costs a tile
- **resident is what the RENDERER needs**, plus what the physics touches: the elevation field, the
  class raster, the footprint prisms and the road graph. Not the render geometry, which lives on
  the GPU
- **a tile that leaves gives its memory back.** Today nothing is ever released: `OsmField::Settled_`
  is a vector that only grows and is searched LINEARLY once per ring tile per stand, and
  `Tiles_`, `Features_`, `Rings_`, `Points_`, `Prints_` and `Built_` have no removal path at all.
  Moving across the world therefore costs quadratic time and unbounded memory

## What will be true

- [ ] `heap: bytes LIVE right now` is under 80 MB with a place standing, at all nine places
- [ ] The churn of one rebuild is under 4x the geometry it produces, quoted per tag
- [ ] `maximum resident set size` for one place is under 250 MB
- [ ] Standing a tile touches only that tile: a case lands one tile into a standing world and
      measures that the work and the churn are proportional to the tile, not the ring
- [ ] Moving the eye across ten tiles and back leaves `heap: bytes LIVE` where it started
- [ ] `OsmField::Settled_` is not a linear scan and not unbounded
- [ ] Negative control: hold one released tile and require the live-bytes ceiling to go RED
