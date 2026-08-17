Type: bug
Area: generators
Tags: scope

**Silent success — a call that answers and nobody reads**

The most expensive class in this tree, all with one shape. **The rule that closes the class:** a
function whose failure changes what the caller may do next returns that failure as a value the caller
cannot spend without deciding, and the value that says "no" carries no usable payload — which is
`core/GroundSample.h` and `core/WaterDepth.h`, written out by hand twice already. `std::optional` is
**not** that shape: `*opt` and `opt.value()` read the payload without anyone having looked at the
state, and `[[nodiscard]]` on the function is satisfied by an assignment. `Try(T *out)` is.

- **`RoofSurface::Cover` returns `void`, and `EarClip` bails silently** (`generators/draw/RoofSurface.cpp:32`, called at 209). When triangulation fails, `Covering` loops over an empty vector and draws nothing, then `BuildingMesh.cpp:383-387` draws `Gables`, `Eaves` and `Chimney` unconditionally — **a roof drawn as its own trim, floating in the sky with no covering**, visible in the shipped frame at (930,240)–(1190,370) of `after/street.png`. Right: `[[nodiscard]] bool Cover(...)`, and eaves, gable and chimney unspellable without a closed covering.
- **Six `Try` answers thrown away behind a `(void)`, and the invariant that saves them lives in
  another layer.** Each site writes `T x = 0;`, calls a `[[nodiscard]] bool Try*(T *out)` behind a
  `(void)` cast, and reads `x` afterwards. **Checked first for the harmless explanation, and it holds
  at every one of the six — no wrong value is reachable in the tree today**, which is why this is
  filed as a shape and not as a wrong number:
  `generators/GroundPatch.cpp:31` is guarded by its own class — `GroundPatch::Complete` refuses the
  whole patch unless *every* posting is `State::Resolved` (`GroundPatch.cpp:19-20`) and the
  constructor is private, so at line 31 `TryAslM` provably answers true. That one is sound, and it is
  the model: the invariant is three lines away, in the same file, behind the only door.
  `generators/Buildings.cpp:54`, `generators/Water.cpp:20` and `generators/Water.cpp:53` are guarded
  by something much weaker — a single-producer invariant in **another directory**. Every `Structure`
  and `Water` feature that reaches a generator is minted at `clients/Sim.cpp:218` and `:226`, and both
  set `f.Top`. Nothing in `generators/` can see that rule, no type carries it, and
  `test/outshine/unit/generators/SameRegionSamePlacement.cpp:410` already constructs a `Structure` with
  `FeatureLevel::None()` — so the rule is one ingest site away from being false. If it ever is: a
  top-less feature enters the `highest`/`deepest` comparison at 0.0 m ASL, beating every declared top
  below sea level and losing to every one above, and at `Water.cpp:53` becoming a water level whose
  `WaterDepth::Between(0.0, ground)` reports `LevelBelowGround` for every dry-land outline — a missing
  datum counted as a disagreement between two models.
  *The fourth site, the deleted world entry point, went with the client on 2026-08-12; the three
  under `generators/` are what remains and they are the ones the factory closes.*
  Right, and **not** by swapping `Try(T *out)` for `std::optional` — this file's own opening argument
  rules that out and it still stands: `*opt` reads the payload with nobody having looked at the state.
  The answer is one rung up, at `C.41`/`C.42`: a `FeatureField::Feature` whose `Kind` is `Structure`
  or `Water` **cannot be constructed without a top**. It is an aggregate today with
  `FeatureLevel Top = None()`, minted by hand at three places, and the rule that keeps it right lives
  in a fourth directory. A named factory that takes the top as an argument deletes
  `Buildings.cpp:54`, `Water.cpp:20` and `Water.cpp:53` outright — no branch, no cast, no zero —
  and costs nothing at runtime. `GroundPatch` already has exactly this shape and that is why it is
  the one of the six with nothing to fix.
