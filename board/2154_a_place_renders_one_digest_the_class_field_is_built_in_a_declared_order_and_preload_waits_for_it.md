Type: bug
State: active
Area: world, engine
Tags: determinism, owner, audit

# A place renders ONE digest: the class field is built in a declared order and preload waits for it

**Benchmark** -- Unreal's automation compares screenshots bit for bit and calls a wandering one
a streaming bug; RAGE's replay plays a drive back frame for frame. CLAUDE.md's fourth invariant:
anything assembled from work on more than one thread is combined in a DECLARED order. Measured
2026-09-05 at HEAD (15406d39, before any change of this round): OldTown rendered 372d51ce,
7c1f32bd, 372d51ce in three runs, and 372d51ce, fa1af887, 7c1f32bd, 99f3ea05 over the day --
1 197 pixels, 273 by more than 1 of 255, worst 14 of 255, on the FAR GROUND (the hills behind
the town, looked at through a magnified crop), never on a building. The regression gate is
blind while this stands: a digest that moves on its own cannot attribute a change.

## Where it stands

- the one measure that differed between a 372d51ce run and a 7c1f32bd run, timings aside:
  `class field: the version the colours used` 3 against 4. The class builder is a worker with
  ONE pending job (the latest wins), so the number of builds before the picture is timing;
  both runs said `it calls itself complete` and held the same feature counts (7 457 / 7 415)
- `ClassField.cpp` sorted the job's features by rank alone with a STABLE sort, so equal ranks
  painted in the order their tiles landed -- completion order deciding overlaps. Repaired in
  this round: the sort key is (rank, the feature's own bounds, template, form, width, rings),
  never the landing. Nine runs after it read 372d51ce; the four before it did not all -- the
  repair is right and is NOT proved sufficient, because a fourth build was not caught since
- `Engine::settled()` (Engine.cpp:130) waits for tiles, rims and the stack's ingest and NOT
  for `Classes().Complete()`, so preload may return while the class builder still holds a job
  and the relay (`renamed` at Laying.cpp:297) lands inside the shot's frames, or after them
- a measure now digests the class structure's packed words (`class field: the structure's
  digest`), so the next run that reads version 4 says whether its CONTENT differs from
  version 3's -- the same features in a different job, or a job that missed a landing

## Measured 2026-09-05, after the sort: the class field is NOT the cause

With the structure's packed words digested, a version-4 run and a version-3 run of OldTown
read the SAME digest (877494161 / 342430996) and both rendered 372d51ce; the fourth build
changes nothing a shader reads. Yet c803c350 appeared once more in the same hour. The two
measures that differed between a 372d51ce run and a 7c1f32bd run, timings and the version
aside: `asks that repeated a posted job` (3 669 against 3 419) and `generators: and flora
wanted ground another body already held` (138 against 215). The suspect moves to the tile
pool: a repeated ask is a tile re-posted after an eviction or a miss, and a far sheet whose
finer rung had not landed by the shot's frame is drawn at a coarser one -- the far ground's
colours, exactly where the pixels moved. The next measurement: per run, the rung of every
sheet at the shot's frame, digested; a wandering run reads a different digest there or it
does not, and either answer names the next suspect.

## Measured 2026-09-05 evening: BOTH written controls are GREEN, and the cause is the VECTOR TILES

The two repairs above were never proved, because both controls were run on a WARM cache. Put
back one at a time and run ten times each, OldTown reads 372d51ce ten times either way: the
rank-only stable sort does not bring the wandering back, and neither does `settled()` without
the class-field wait. A control that passes proves nothing, so both claims are struck.

What DOES bring it back is the one condition the morning had and the evening did not: a run that
FETCHES. With one cache entry in seven deleted (7 738 -> 6 633, so about 1 100 tiles must come
over the wire during preload) OldTown reads c803c350 on the fetching run and 372d51ce on the five
warm runs after it. Every condition that does NOT reproduce it, for the record: a warm cache (10
runs), a cold cache (the first two runs refuse to preload, the next three read 372d51ce), ten
busy-loops on the cores (6 runs), and either repair reverted (10 runs each).

The measures of a fetching run against a warm one, timings and heap aside, name it:

| measure | warm | fetching |
|---|---|---|
| `generators: vector tiles that settled` | 49 | **9** |
| `world: the bytes its fields hold` | 37 608 095 | 37 436 819 |
| `world: of that, the land classes` | 30 673 971 | 30 502 503 |
| `world: the buildings` / `water` / `streets` | 1 240 208 / 52 880 / 393 872 | 1 240 272 / 52 944 / 393 936 |
| `class field: the version the colours used` | 3 | 5 |

`ground: the sheets' digest` is IDENTICAL in both, so the lattice's rungs were never the cause and
the hypothesis above is struck too. The two runs simply stand DIFFERENT WORLDS: nine of the
forty-nine vector tiles had settled, and the buildings, streets and water were built from that.
`Engine::settled()` waited for the terrain tiles, the rims, the stack's ingest and the class field
-- and the class field waits for its OWN two `OsmField` tiers -- but the generators' vector field
is a THIRD `OsmField` that nothing waited for.

**The repair, one line, the same rule the class field already applies to itself**: `settled()`
also requires `Stack.Vectors()->PendingTiles() == 0`. Measured with one entry in seven deleted
again: four fetching runs read 372d51ce. **The negative control is RED**: the condition removed
and the cache thinned, the fetching run reads c803c350.

## The solution

- preload is settled only when the class field is complete and the ground was laid at that
  version: `settled()` takes `Classes().Complete() && World.LaidClasses == version`
- the builder's job is a snapshot of the landed tiles in DECLARED order (tile id), and a
  landing after the job was taken marks it stale so that one more build follows -- the
  `Stale` flag's hole, if the version-4 digest differs, is here
- a case: OldTown rendered N times through the client reads one digest; its negative control is
  the vector condition removed WITH A THINNED CACHE, which reads two. A warm run is no control

## What will be true

- [x] the features paint in a declared order
- [x] the class structure's digest is the same across every run that says complete
- [x] `settled()` waits for the class field complete and the ground laid at its version
- [x] `settled()` waits for the GENERATORS' vector field too, which is what the wandering was
- [x] ten runs of OldTown, Heidelberg and Kaiserberg read one digest each (372d51ce, b732280b,
      01849439), and three full runs of all nine places read one digest each
- [x] Negative control, RED: the vector condition removed and one cache entry in seven deleted,
      the fetching run reads c803c350 instead of 372d51ce
- [x] Struck as GREEN and therefore proving nothing: the rank-only stable sort put back (10 runs,
      one digest) and `settled()` without the class-field wait (10 runs, one digest). Both
      changes are still right -- the sort is a declared order and the wait is the invariant --
      but neither is the cause and neither may be quoted as proof
- [ ] a fetching run is what the gate must exercise: `make shots` on a warm cache cannot see this
      class of defect at all, so the thinned-cache run belongs in the gate or in a case
