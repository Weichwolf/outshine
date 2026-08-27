Type: feature
State: open
Progress: render-plan
Area: render
Tags: benchmark, target

# The render plan is compiled from a declared graph, and every catalogue row executes

**Benchmark** — Unreal: RDG compiles a graph of passes with declared resources and dependencies, and culls passes nothing reads. RAGE: fixed passes. **Taking Unreal** — a declared graph is what lets a catalogue row be dropped without leaving a reader behind.

Unreal's RDG compiles a pass graph from declared resource reads and writes, and a pass nothing
reads is culled. outshine has the same shape -- `RenderCatalogue.h` declares stages with their
reads, writes and contributions, and `Compiled::Compile` refuses a content stage nothing
consumes. The gap is that three declared rows execute NOTHING.

- [x] a plan is COMPILED from declared resource edges, and a content stage nothing reads
      refuses. Measured: a spec naming `subjects` and `overlay` compiles to FOUR stages --
      `subjects tonemap overlay present` -- because `Surface` is requested and the read edges
      pull the rest; and a plan whose only output is the meter refuses `render.content.sky:
      nothing this plan requests reads what it draws into`.
      proof: outshine/door/ScoreWhatAPlanIsCompiledFrom
- [ ] a stage reads only resources it declares -- an undeclared read is a silent switch-off.
      `ScoreWhatTheShadowCasts` only passes because `Subjects` declares `LutSampler`, but it
      does not SAY so and would stay green if the rule were dropped elsewhere
- [x] the executor order is the catalogue's order and not the declaration's, so a producer
      cannot run after its consumer. Two halves and both now stand: the catalogue's
      `TopologicalOrderHolds()` static_assert says the stage ENUMERATION is a linear extension
      of the edge graph, and the case says the compiled order follows the enumeration. Negative
      control: reversing `Order_` lands 2 unfed reads, the first at `present`, and takes 12 of
      the 30 door cases with it.
      proof: outshine/door/ScoreWhatAPlanIsCompiledFrom
- [x] `Stage::Terrain`, `Stage::Buildings` and `Stage::Water` are GONE, which is the opposite of
      what this predicate asked for and the right answer. It asked them to EXECUTE; thinking the
      pipeline backwards says they should not exist. A generator hands back a `Geometry`, one
      cooker cooks it, and ONE subject pass draws it -- Unreal draws Landscape as a primitive in
      the base pass and RAGE puts terrain on the same draw list as everything else. Neither has a
      terrain pass. `Stage::Models` went with them for the same reason. board:1991 carries the
      cooker half.
      proof: the door suite and --audit-layers, unchanged by their removal
- [ ] a resource a plan stops writing is cleared or declared stale, never left standing
      (board:1922). **MEASURED and larger than it looked**: in the minimal plan above, four
      reads land on a resource the plan does not hold at all -- `subjects` reads an unheld
      `shadowAtlas`, and `tonemap` reads an unheld `sceneLinear`, `aoBuffer` and `meter`.
      Nothing writes them, so what a shader samples there is whatever was last left in it. The
      case counts them beside its own checks so the number cannot drift unseen
- [ ] the subject stage does ONE thing; the other five responsibilities move out (board:1867)
- [ ] a second executor table -- a software rasteriser -- compiles the same plan (board:1636)
