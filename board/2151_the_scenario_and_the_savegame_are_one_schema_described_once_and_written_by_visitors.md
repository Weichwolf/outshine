Type: debt
State: open
Area: scenario, engine
Tags: architecture, determinism, owner

# The scenario and the savegame are ONE schema, described once, written by visitors

**Benchmark** -- Unreal: ONE description per type (`UPROPERTY` reflection) and `FArchive` is the
one basis; binary packages, text (`FJsonObjectConverter`), `USaveGame`, replication and the
property DIFF all read the same table. A level (`.umap`) and a save are the same machinery, and
they stay two files: the map is what the author declared, the save is what happened. RAGE:
`parser` -- `.psc` metadata compiled to `parStructure` tables, XML (`.meta`) and PSO binary
(`.ymt`, and the savegames ARE PSO) from one table; map data and savegame stay apart the same
way. **Both agree** on all three: one description, N writers; declared and state in one schema;
declared and state in two files. Both replay STATE streams (RAGE's replay packets, Unreal's
`DemoNetDriver` checkpoints + deltas) because their simulations are not deterministic;
outshine's is, by invariant, so an EVENT log is admissible here and is the smaller record.
Decided with the owner 2026-09-05.

## Where it stands, measured 2026-09-05

```
  include/scenario/Scenario.h     50 structs, 132 scalar fields
  src/scenario/ScenarioRead.cpp   a hand-written XML walk, 476 string literals
  src/scenario/ScenarioWrite.cpp  a second hand-written walk, 86 string literals
  lint                            "the scenario grammar declares a child its writer cannot
                                  write back, and the count GREW" -- the drift is already
                                  measured, and "NOT COVERED: attributes" beside it
  a savegame                      none; the engine holds no snapshot and no event log a
                                  client can take out or hand back
```

Two walks over one schema are two sources for one rule; the guard above is the proof they
drift. C++23 has no reflection (that is C++26's P2996), so the references' table has to be
written by hand ONCE and read by every writer.

## The solution

- one `Visit(visitor)` per document struct naming each member once with its element name; the
  XML reader, the XML writer, the diff and the binary reader/writer are VISITORS over that one
  description, and `ScenarioRead`/`ScenarioWrite` collapse into it. A member the table does
  not name is a member no format can carry, which is the guard's claim made structural
- **a savegame is three parts in one schema**: the SCENARIO it was played from (its id and
  digest, never a copy), a SNAPSHOT of the state at time t (every body's pose, the sim clock,
  the minds' memories, the seeded generators' seeds), and the EVENT log since the last snapshot
  (inputs, provider answers as they landed, minds' answers) -- Unreal's checkpoint + delta
  shape. Loading is declare(scenario) + restore(snapshot) + replay(events), and the
  determinism invariant is what makes the third part possible at all
- two writers from the one description: XML for the door (the author reads and diffs it) and
  binary for size and speed; a scenario may be handed in as either
- the door's round trip is the case: read → write → read gives the same document, XML and
  binary give the same document, and a save taken at t and restored renders the same bytes as
  the run that never stopped

## What will be true

- [ ] `ScenarioRead.cpp` and `ScenarioWrite.cpp` are gone; one description, visitors beside it
- [ ] the "child its writer cannot write back" guard reads 0 and is deleted, because the
      structure now holds what it counted
- [ ] a save at frame N, restored, renders frame N+1 bit-identical to the run that never
      stopped, at one place, and the nine references unmoved
- [ ] Negative control: a member added to a struct and not to its `Visit` fails to compile or
      fails the round-trip case, never passes silently
