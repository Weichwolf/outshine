Type: bug
Area: sim
Tags: scope, layering

**The library speaks no case, and the sim layer includes no client**

The fold of Journey into `src/sim` (board:1581, move 1) moved the FILE but not the boundary:
what stands in the library is still the test case it was in `tools/driver/parts`, verbatim.
Every point below is a hard rule broken NOW, independent of the systems decomposition that
board:1581 plans as move 2.

## Findings, each with its rule

| where | what | rule broken |
|---|---|---|
| `src/sim/Journey.cpp:275,499,510` | board numbers inside engine strings ("board:1527's finding", "board:1528") | code never names work items (CLAUDE.md); same class pre-exists at `src/world/Wayfinding.cpp:326` |
| `src/sim/Journey.cpp:273-277` | `say.Claim(true, ...)` -- a claim that cannot fail, pure narration | the Sink claims belong to the CASES; systems publish numbers, cases judge (board:1581's own table) |
| `src/sim/Journey.cpp:258` | `keeping.Directory = "/tmp/outshine-drive-cache"` hardcoded | the cache location is the CALLER's declaration; `Sim` already takes `ContentStore::Config` through `SetContentStore`; TMPDIR is ignored |
| `src/sim/Journey.cpp:262,318,322` | `"src/assets/sky"`, `"src/assets/world/*.json"` relative paths in library code | cwd-dependent library behaviour; `Sim::Assets` is the standing pattern for declaring these |
| `src/sim/Journey.cpp:43` | `constexpr double kF31WidthM = 1.811` feeds `Reap`, the lane budget (`:657`), the room clamp (`:678`) and `Ride` | a content noun in the engine, and a magic number: `Scenario::Vehicle` declares mass, track, drag -- but no width. The width belongs in the declaration and travels via `Column<Vehicle>` |
| `src/sim/Journey.cpp:15-16` + `test/run.sh:230` | `#include "Rigging.h"`, `#include "ScenarioRead.h"` -- src/sim reaches UP into `src/clients`, and the runner spells `-Isrc/clients` for the sim layer | peers never call each other, and a library layer never includes the facade layer above it; `Clients::Stand` and the scenario reader must move to (or below) sim before the include dies |
| `src/sim/Journey.cpp:50-73` | `struct Unused_Drove` -- dead, and named so | delete on the day you replace |
| `src/sim/Journey.cpp:206-215` | the vararg `Line(const char*, ...)` overload is dead: every call site passes a single string, which the exact-match overload always wins | dead code carrying vsnprintf UB-bait |
| `src/sim/Journey.cpp:987-1131` | `Ride`'s body indented one level deep -- the fossil of the loop it was cut from | the fold left the scar |

## Done when

- [ ] no string in `src/` names a board item (Wayfinding.cpp:326 included)
- [ ] Journey's cache directory, asset paths and vehicle width arrive as declarations, not constants
- [ ] `test/run.sh:230` spells no `-Isrc/clients` for `src/sim`, and the compile proves it
- [ ] `Unused_Drove`, the vararg `Line` and the fossil indentation are gone
- [ ] Munich and Kyoto stay green through each step

board:1581's decomposition (move 2) supersedes the narration wholesale -- but nothing above
waits for it.

---

**Closed, all boxes.** No string in src/ names a work item; the cache directory, the assets root
and the vehicle width arrive as declarations (Provision through Lay, widthM through the grammar,
the scenario read FIRST); no sim include line spells src/clients -- ScenarioRead lives in the
scenario layer (whose compile subjects caught my first, too-wide cut), Sim::Stand lives in sim
with its test in the mirror; the dead struct, the vararg Line (two live callers made explicit)
and the fossil indentation are gone. Proofs: the fast gate 118/118, Munich 38/0, Kyoto 35/0,
the road edge 7/0 over the final build. The duplicate-symbol lesson is in the runner: a file
group never sits beside its own directory group.

---

**Sharpened by review (2026-08-22).** Residue of the move: `src/scenario/ScenarioRead.h` still guards itself `OUTSHINE_CLIENTS_SCENARIOREAD_H`; and the file-beside-directory ambiguity survives for `src/sim/Rigging.cpp` vs `src/sim` (same object path, two include sets) — filed as board:1603.

---

## The rule this item wrote is broken again, 2026-08-24 (hourly review)

This item's own table states it: *"the Sink claims belong to the CASES; systems publish
numbers, cases judge"*. It was applied to `Journey.cpp` and to nothing else. Measured at
2b2c2f69: **34 `Sink::Claim` sites in `src/` carrying 5 812 characters of English**, the
largest of them added this session at `src/sim/DriveAssembly.cpp:206` -- a five-line
architecture essay compiled into the library, asserting that a fetched OSM extract contains at
least one grade separation. Carried forward as **board:1821** (`Regresses: 1598`) rather than
reopening this one, because every box it closed on is about a file that no longer exists.
