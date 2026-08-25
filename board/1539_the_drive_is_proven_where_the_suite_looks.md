Type: bug
State: open
Area: harness
Tags: scope

**The drive is proven where the suite LOOKS**

**One claim stands in two places and they have drifted.**

| Where | What it says |
|---|---|
| `tools/driver/APlannerFindsTheRoadFromMunichToHamburg.cpp` | drives it -- 774.852 km, never off the carriageway, 191x real time |
| `test/render/outshine/drive/TheCarDrivesFromMunichToHamburg.cpp` | **refuses**, citing `board:1503`: *"no network is woven over the streamed ways yet"* |

The refusal is **false as written** -- the network is woven and the route is planned. What it really
records is that this case calls a **different, stale entrance**: the free two-argument
`Plan(from, to)` at `src/world/Wayfinding.h:100`, whose only caller in the tree is this case.
`Network::Plan(from, to, tightestM, withinM)` at `:59` is the live one.

**And `tools/` is not run with the library** -- `CLAUDE.md` says so on purpose. So the goal's proof
currently lives exactly where the default suite does not look, and the place the suite DOES look
reports the opposite.

## What must be true

- [ ] **The drive's assembly is library code**, not tool code -- `Journey` composes corridor, pilot,
      physics and world, which is engine capability by every line of the layer table
- [ ] **`test/render/outshine/drive/` exercises it** and is the single proof, UNPREPARED when the
      fetched tiles are not there rather than falsely red
- [ ] **The stale `Plan(from, to)` disappears in the same round its replacement is wired** -- *what is
      replaced disappears in the same round*, and a refusal message that names a board id and is no
      longer true is the worst kind of dead path: it reads as a status report
- [ ] **`tools/driver/` keeps its own entry point and stops carrying the proof**

## Comments

**Found by running the full suite and reading the trailer rather than the tail.** 1735 tests, 9 FAIL,
and this was among them; the tail showed passing cases and said nothing about it.

**The population moved too, and that is a separate caution**: khronos reported 49 criteria of 49 with
49 within bound, against 181 and 180 previously -- because 411 cases were UNPREPARED after 1178 were
pruned at a 27.9 GB corpus peak. **49 within of 49 is not the same measurement as 180 of 181**, and
quoting the new one as "held" would be the changed-selection defect exactly.
