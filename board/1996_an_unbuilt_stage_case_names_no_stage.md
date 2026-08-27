Type: bug
State: active
Area: render
Tags: proof, catalogue

# the unbuilt-stage case derives its rows and cannot rot again

**Benchmark** — Unreal: RDG passes are registered by the code that owns them, so a pass with no
lambda cannot be named at all. RAGE: the draw list is built by the renderer, and there is no
declared row that nothing executes. **Taking neither**, because neither faces the question: both
make the state unrepresentable, this tree lets the CATALOGUE declare a row before an executor
exists, and that gap is deliberate -- the catalogue is the plan's vocabulary and it may run ahead
of the implementation. What it owes in exchange is a case that says which rows are still empty.

`test/outshine/door/ScoreWhatAnUnbuiltStageDoes.cpp` hardcodes `Stage::Terrain`, `Buildings` and
`Water`. Those rows were deleted with board:1990 -- they declared resource edges and executed
nothing -- and the case has not compiled since. It was written to go RED the day a row grew an
executor; it went red the day the rows went away, which is the same sentence with a different
verb, and a case that only fails by failing to BUILD tells the reader nothing.

Six rows still have no executor: `AmbientOcclusion`, `AutoExposure`, `Irradiance`, `Moon`,
`Stars`, `Sun`. The claim is alive. The list is what rotted.

**The repair**: derive the rows. Walk `Stage::kCount`, keep every row `Renderer::Executable`
refuses, and assert of each that a plan naming it still COMPILES -- an unbuilt row is a plan the
compiler accepts and the device declines, which is the property worth holding. Then the case
tracks the catalogue instead of remembering it.

**The measurement that shows I am wrong**: the case must print the derived rows by name, and it
must REFUSE when the derived set is empty -- a case with nothing to check passes vacuously, and a
vacuous pass is the green negative control this tree has already paid for once. Negative control:
seat one of the six with a no-op executor and the printed list shrinks by that name.
