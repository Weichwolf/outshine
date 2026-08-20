Type: feature
Area: clients
Tags: scope, perf

**A scenario is parked, and another stands up in its place**

**The owner's design, and it is better than the one it replaces.** An interior and an exterior are not
two `Regions` inside one scenario: **they are two SCENARIOS**, and a door parks one and stands the other
up. Walking back through the door resumes what was parked, where it was.

**So the engine holds 1..N scenarios and exactly ONE is live** -- the same shape as everything else
here, and the thing that makes `create -> load -> run -> destroy` into a game rather than a demo.

## What parking keeps and what it releases, which is the whole design

| | |
|---|---|
| **kept** | the declaration, every instance's attributes, the clock's offset, what the player left on the floor -- the STATE, which is small |
| **released** | device buffers, meshes, images, the standing `Live` -- the RESIDENCY, which is large |
| **not decided by the engine** | how many stay parked. `CLAUDE.md`: *everything that grows states its bound*, so the bound is declared and reaching it evicts the least recently live one, or refuses -- and which is a decision this item makes with a measurement |

**Looked up rather than recalled**: this is Bethesda's cell buffer -- an interior stays in memory a
while after you leave so re-entering is instant -- and Unreal's level streaming, where a sublevel is
loaded, unloaded or kept by distance and by a request. **Take the MECHANISM**: a bounded set of resident
declarations with exactly one active, and eviction by a stated rule. *The assumption that comes with it:
state is small and residency is large, which is only true if a scenario's state really is attributes and
not geometry.*

## What must be true

- [x] **`Engine` holds 1..N scenarios and exactly one is live**, and which is answerable. `Park()`,
  `Resume(name)` and `Parked()` are on the public handle; a parked scenario keeps its declaration and
  nothing stands while it is parked. **`Resume` PARKS NOTHING BY ITSELF** -- a door is two explicit
  calls, because a scenario that vanished on somebody else's call is one nobody can reason about
- [ ] **Parking is O(state) and never O(residency)**: it drops what a stand-up can rebuild and keeps
  what a stand-up cannot
- [ ] **Resuming a parked scenario returns it to the frame it was parked at**, and the clock advances
  or does not -- *declared*, because an interior where time froze and one where it did not are both
  games somebody made
- [ ] **The number of parked scenarios is bounded and the bound is declared**, with eviction by a
  stated rule
- [ ] **The cost of a transition is MEASURED** in the scenario suite: park, stand up, resume, over a
  declared run, at p50/p95/p99 -- because a door that costs 400 ms is a loading screen and one that
  costs 16 is a doorway
- [ ] **Memory over a hundred transitions returns to where it started**, which is `board:1463`'s
  question asked of a whole scenario rather than of a frame

## What this may not do

**It may not make parking a save.** A save is `State`, it survives a process, and it is `board:1493`. A
park is a live thing kept warm, and confusing the two would make every door touch a disk.

## The declaration follows, and `Regions` means something narrower now

**An interior and an exterior are two SCENARIOS**, so `Regions` inside one scenario is not the
interior/exterior split -- it is the streaming decomposition WITHIN one, and `Doors` names which
scenario a transition parks into rather than which region. That is a change to what those two rows
mean and it is written here rather than left to be inferred.

## What proves what is built

**`test/render/outshine/client/AClientRunsAScenarioInFourLines.cpp`** walks a door both ways: parks the
exterior under its own name, stands the interior up in its place, refuses `Resume("nowhere")`, parks the
interior, resumes the exterior and finds it carrying the two kinds it declared. 42 checks.

**What is NOT built is the whole of the rest of this item**: parking still keeps the declaration and
drops the `Live`, which releases the residency by construction -- but nothing bounds how many park,
nothing evicts, the clock does not resume where it left, and the transition has never been timed.

