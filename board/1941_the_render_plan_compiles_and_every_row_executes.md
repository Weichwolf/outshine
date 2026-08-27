Type: feature
State: open
Progress: render-plan
Area: render
Tags: benchmark, target

# The render plan is compiled from a declared graph, and every catalogue row executes

**Benchmark** — Unreal: RDG compiles a graph of passes with declared resources and dependencies, and culls passes nothing reads. RAGE: fixed passes. **Taking Unreal** — a declared graph is what lets a catalogue row be dropped without leaving a reader behind.

Unreal's RDG compiles a pass graph from declared resource reads and writes, and a pass nothing
reads is culled. outshine has the same shape -- `RenderCatalogue.h` declares stages with their
reads, writes and contributions, and `RenderPlan::Compile` refuses a content stage nothing
consumes. The gap is that three declared rows execute NOTHING.

- [ ] a plan is COMPILED from declared resource edges, and a content stage nothing reads
      refuses -- the code does this and NO case asserts it
- [ ] a stage reads only resources it declares -- an undeclared read is a silent switch-off.
      `ScoreWhatTheShadowCasts` only passes because `Subjects` declares `LutSampler`, but it
      does not SAY so and would stay green if the rule were dropped elsewhere
- [ ] the executor order is the catalogue's order and not the declaration's, so a producer
      cannot run after its consumer -- true today, asserted nowhere
- [ ] `Stage::Terrain`, `Stage::Buildings` and `Stage::Water` EXECUTE -- three rows a scenario
      can select and no executor runs (board:1805)
- [ ] a resource a plan stops writing is cleared or declared stale, never left standing
      (board:1922)
- [ ] the subject stage does ONE thing; the other five responsibilities move out (board:1867)
- [ ] a second executor table -- a software rasteriser -- compiles the same plan (board:1636)
