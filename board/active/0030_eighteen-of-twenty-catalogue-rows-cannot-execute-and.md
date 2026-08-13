Type: bug
Area: render
Tags: khronos, instrument

**Eighteen of twenty catalogue rows cannot execute, and the catalogue is read as a capability statement — **Band 1****

**Enumerated, not sampled.** `Renderer::Executable` (`src/render/Renderer.cpp:102-127`) is an exhaustive
switch over the twenty rows of `src/render/plan/RenderCatalogue.h`. **Two return `true` — `Subjects` and
`Tonemap`. Eighteen return `false`**: `Transmittance` · `MultiScatter` · `SkyView` · `Irradiance` ·
`AutoExposure` · `ShadowMap` · `Sky` · `Sun` · `Moon` · `Stars` · `BenchGround` · `Terrain` ·
`Buildings` · `Water` · `Models` · `Occlusion` · `TemporalResolve` · `Present`. *The switch is the
container and it is closed, so this is a count and not an estimate.*

**The harmless explanation, sought, and it holds in part — which changes the finding rather than
clearing it.** *"Nothing notices"* is **false**: `Renderer::Init` (`:136-140`) walks the compiled plan,
refuses the first non-executable stage, and logs `stage_not_executed` **with the stage named**. The
refusal exists and it is a good one.

**What is actually wrong is the LAYER the refusal sits in, and § I.27's claim is what it contradicts.**
That section's acceptance for the whole design is *"the impossible plan is largely unspellable rather
than refused"*. **A stage with no implementation is the plainest impossible plan there is**, and for it
the truth is neither: `RenderPlan::Compile` **accepts** the plan, and the refusal arrives later, at
device bring-up, on a machine with a GPU. The word *largely* is carrying this entire class.

**And the reason the refusal has never fired is the part worth having.** All **34** render cases declare
the same single content stage — `test/render/Parity.cpp:1322`, `Content = {Stage::Subjects}` — so **no
run in this tree has ever named one of the eighteen.** The mechanism is not weak; **its population is
empty.** A refusal that cannot fire is not evidence that the thing it refuses does not happen.

**The class, and it is the mirror of one `CLAUDE.md` already carries.** *A grep proves a string absent,
never a capability.* **This is the inverse: a catalogue row proves a capability PRESENT when the
implementation is gone.** Same shape as the citation checker that enumerated *files* to answer a
question about *paths* — an instrument whose domain is narrower than what it is read as asserting. **A
row is currently evidence that a name was written down, and it is read as evidence that a picture can be
made.**

**The interval, stated as a fact.** The implementations were deleted at `0161f88` and the rows kept;
`39c9cd6` is **33 commits later**, same day, and in between the suite was green and reported *criteria
met* on every one. **A defect that survives 33 commits under a green suite is evidence about the suite**,
and the evidence is the line above: the suite exercises two rows of twenty.

**Consequence to state without softening, because it is quoted as a headline:** *"31 of 34 Khronos
criteria"* is a number about a renderer that **draws glTF subjects and tone-maps them**. It is a true
number and it is about a much smaller renderer than the catalogue's twenty rows suggest.

**Can `Executable` be a compile-time property? Not in the catalogue, and the reason is the property that
makes the catalogue worth having.** The plan layer compiles with `-Isrc/core -Isrc/render/plan` and
**cannot see `src/render/stages/`** — that exclusion is what makes a plan checkable before a device
exists, and it is proved by the build rather than asserted. **A row naming its implementation would
require the plan layer to see the implementation and would destroy exactly that.**

**It CAN be compile-time one layer up, where both are visible, and that is the end state.** A `constexpr`
table in the renderer mapping **every** `Stage` to its encoder, with a completeness `static_assert`:
deleting an implementation then breaks the initialiser and **the port could not have left the rows**.
*The cost is that the port would have had to delete eighteen rows too — which is correct, because § I.27
already rules that adding a row is an engine change with its assertions re-proved, and a scenario naming
a stage that does not exist is already an unknown-name refusal at `StageByName`.*

**Owed now, because something missing is a task and this one is cheap: a test.**
**`EveryCatalogueRowCanExecute`** — one loop over `kStages`, asserting `Renderer::Executable(row)` for
each. It goes red the moment a row outlives its implementation, it needs no device, and it would have
failed at `0161f88`. **Fixed when** that test exists and is green, and **finished when** the dispatch
table makes it unnecessary.


## Comments

**Two shapes were considered and one is refused.** `Renderer::Executable` is **private**, and making it
public to read from a test would give a **checker** where this design wants a **compile error** — the
same distinction `CLAUDE.md` draws between a rule a checker counts and a rule the type system carries.

**The form that closes it: a `constexpr` table from every `Stage` to its encoder, in the renderer, with a
completeness `static_assert`.** Both are visible there; in the plan layer they are not, and that
exclusion is what lets a plan be checked before a device exists — so **the assertion must not go in the
catalogue**. With the table, deleting an implementation breaks the initialiser: the port that removed
eighteen encoders would have had to delete eighteen rows, which is the correct outcome.

**And the cheap half is still worth having beside it**: print `N of 20 rows executable` in the suite's
own output, so nobody can quote *31 of 34 Khronos criteria* without the population beside it. That is a
number, not a gate.

**Not started here** because half of it is worse than none: a public flag plus a test that reads it would
close the item while leaving the defect spellable.
