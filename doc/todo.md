# Now

| | |
|---|---|
| **Working on** | The eye in every telemetry row, plus per-pass admission counters |
| **Scope** | `doc/requirements.md`: **1379 features, 216 ticked, 1163 open** · `doc/bugs.md`: **47 defects** |
| **Last accepted** | The ledger stops lying — `heapKB` falls 35 times in 138 rows where the break never falls |

**Bugs come before requirements.** A defect in `doc/bugs.md` outranks any open line in
`doc/requirements.md`, and a round that touches a file with a recorded defect in it fixes that defect
in the same round. Nothing is ticked while the thing it names is broken.

## Now, in order

1. **The eye in every telemetry row**, plus per-pass admission counters. A run whose subject is motion
   cannot answer "did the camera move" from its own record — that is what turned one grep into three
   rounds. "Nothing was requested" and "everything requested was already resident" must stop producing
   the same row. **Done when** a 500 m walk is decidable from its CSV alone.
2. **The 500 m walk gate.** 500 m of travel raises `tilesTotal` and `poolHttpGets`; it fails in CI, not
   in a browser. **Done when** it is a declared run that goes red on today's binary.
3. **Nothing streams during play** — the defect the gate above catches. From t=31 s to t=77 s of the
   owner's session, `poolHttpGets` 310 flat and `tilesBuilt` 0. **Done when** the gate goes green.
4. **Eviction, before more streaming.** Fields grow monotonically and their unit of removal does not
   exist; real headroom is ~29 MiB against 545 KiB of building heap per tile, so roughly fifty tiles.
   A working streamer without a working evictor turns a world that stops into a world that crashes.
   **Done when** `tilesEvicted` rises under a bound and a 2 km walk holds its budget.
5. **The silent-success class**, all three sites at once (`bugs.md`, first section). `RoofSurface::Cover`
   is drawn in a shipped frame as a roof with no covering. **Done when** the trim of an absent covering
   does not compile.

## Then, from `requirements.md`

Band 0 in order (0.1 ledger · 0.2 request and priority · 0.3 budget and eviction · 0.4 arrival ·
0.5 exhaustion · 0.6 instruments · 0.7 headroom), then the picture work: **more than one prototype
resident** — one `SetPrototype` slot is what stopped fifteen finished shrub species from being drawn —
the grass stratum as a field, overdraw, the water level, one rank per stand, occlusion between 1 m and
20 m, the night.

## Standing debt

**147 of 210 ticked lines name no file**, against the rule that a ticked line names what implements it.
Band III is worst at 43 of 45, Band II at 43 of 49. Not a round of its own: each is filled in as its
band is touched, and a line that cannot be given a file was never true.
