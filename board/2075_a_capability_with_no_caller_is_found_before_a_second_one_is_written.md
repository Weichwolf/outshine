Type: task
State: open
Area: process
Tags: measured, process

# A capability with NO CALLER is found before a second one is written

**Benchmark** — Unreal: `-unusedcode` and the module dependency graph make an unreferenced module a
build-time complaint, and a `UFUNCTION` nothing calls shows up in the reflection database. RAGE:
the parser generates its structures from the type, so a member no declaration reaches does not
compile in. **Neither ships a working subsystem nobody can reach**, and CLAUDE.md already names this
as this tree's commonest defect. This item is that the tree has no MEASURE of it.

## What was measured in ONE session

Eleven complete capabilities were found with no caller. Every one of them was found by accident --
while chasing something else -- and every one was WORKING code that a declaration could not reach.

| capability | what it does | how it was found |
|---|---|---|
| `Work::Graph` | steps, dependencies, hands, ready queue -- the tree's ONE scheduler | reading board:2056's own first box |
| `CookDag` | the cluster DAG Nanite's LOD half needs; `Shape.cpp` calls the FLAT `CookClusters` | asking why 23 207 of 23 207 clusters are rootless |
| `ClassStructure::Words()` / `Bytes()` | the packed, pointer-free class structure for the GPU | board:2064 needed it and it was already there |
| `GroundMaterials::SpecularScale` | the dial that stops a dry slope carrying a specular lobe | chasing a white wash the owner asked about |
| `World.Instances` | 1311 tree placements per rebuild, written and never read | asking why the ground wears its floor |
| `Generators::Forest` | a complete `Making` with a species table and an alpine limit | the same question |
| `RoadHarvest::Reap` | reads `bridge`, `tunnel` and `layer` and builds a `Path::Network` | asking what OSM tells us about structures |
| `WeatherProvider::Clouds` | `cloudCover`, `cloudLow/Mid/High`, `cloudBaseAglM` read from a scenario | the owner asked whether a haze layer was left over |
| `IrradianceBuffer` as `Handle` | declared with stride 0, so `Compiled.cpp:290` never binds it | board:2013 |
| `Live::Restand`'s zeroing | `StandMs_`/`SubmitMs_` cleared right after `Build` measured them | profiling the product path |
| `distM` / `runnerUp` | the class edge distance, computed and discarded at both call sites | board:2064 |

**A declaration that is accepted and does nothing is worse than a refusal** -- CLAUDE.md says so, and
`cloudCover` is the pure case: a scenario states the day's cloud, the reader parses it into
`Scenario::Sky`, and `grep -rn "\.Clouds(" src/` returns nothing. The engine agrees and does not act.

## Why the existing measure does not see them

`test/scripts/unreached.py` counts SYMBOLS nothing in the archive calls and holds a shrink-only
baseline, 151 today. It found none of the eleven, and the reason is structural rather than a bug:

- `Work::Graph` and `Generators::Forest` are CLASSES whose methods are virtual or whose
  constructors are called from their own tests
- `World.Instances` is a MEMBER, not a symbol -- it is written, so it is "used"
- `SpecularScale` is a FIELD assigned in one file and read in none
- `CookDag` is a free function that IS in the archive and IS linked, because nothing prunes it
- `distM` is a PARAMETER

So the measure sees linker-level deadness and every one of these is alive at the linker and dead at
the DECLARATION. That is the gap.

## What will be true

- [ ] A measure that can see a WRITE with no READ: a member or an out-parameter assigned in the tree
      and never read by anything but its own writer. That is the shape nine of the eleven have
- [ ] It carries a shrink-only baseline, because a strict count over a grown tree is red on day one
- [ ] Negative control: a member added and written but never read RAISES the count; the same member
      read once does not
- [ ] The eleven above are either wired or deleted, and each one says which in its own commit

## What this does NOT cover

Whether each capability SHOULD be wired. `Generators::Forest` wants a canopy albedo this tree does
not have an origin for (board:2068) and `Work::Graph` wants a phase worth parallelising that the
measurement did not find (board:2056). "Unreached" is a finding, not a verdict -- but a finding
nobody can see is not a finding at all.
