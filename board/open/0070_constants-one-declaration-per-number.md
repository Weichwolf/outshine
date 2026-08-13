Type: feature
Area: core
Tags: perf, instrument

**I.23 Constants: one declaration per number**

*Added 2026-08-12 on the owner's ruling: **no magic numbers, one const header**. The tension with
`CLAUDE.md`'s* every number carries its origin — derived, measured or `[SET]` — with unit and frame of
reference *is real and is resolved here rather than around: a single header of hundreds of constants is
the classic place where that discipline dies. Measured baseline this round over `src/`, comments and
string literals stripped and the eight trivial values excluded: **1 484 non-trivial numeric literals in
266 files**, against **193 `constexpr` declarations** and **105 `[SET]` tokens that nothing counts**.
Densest: `generators/draw/BuildingShape.cpp` 101 · `clients/SubjectBench.cpp` 97 · `core/ClusterDag.h`
81 · `render/Renderer.cpp` 67 · `render/stages/ModelDraw.cpp` 64.*

**One header cannot be literally one file**, because the layering is the build: a generator translation
unit compiles with `-Isrc/core -Isrc/generators` and nothing else, so a header holding a `render/`
constant beside a `generators/` one would give a generator a name for a render concept and dissolve the
gate. So the rule is **one const header per layer**, each including the layer below it, and the property
that is actually wanted — *one place to look, and no number in two of them* — is held by a check rather
than by a filename.

**"No magic numbers" is not "every constant in one file".** The magic number is the **unnamed literal at
a use site**. A named `static constexpr` member beside its single consumer is not one, and moving it into
a shared header would make it worse: it would be a number two layers can see that only one needs.

- [ ] `core/Const.h` as the one spelling a reader looks in for `core`, with one file per **subject** below it — units, Earth and the tile scheme, sky and ephemeris — and never one file per consumer. A subject has an owner who can say whether a number is right; `RendererConstants.h` has none
- [ ] One const header per layer, each including the one below: `core` · `generators` · `world` · `render` · `clients`. Five places in the program, and the layering already forbids the sixth
- [ ] `core/Units.h` folds in as the first subject file — it is already the shape: exact ratios rather than truncated decimals, derivation as the initialiser, one comment per deviation
- [ ] **A derived constant is written as its derivation**, never as its value: `kDeg2Rad = kPi / 180.0`, `kKtToMs = kNmToM / 3600.0`. The derivation then cannot drift from the number, because it *is* the number. `core/Units.h:11 kRad2Deg = 57.29577951308232` and `:22 kMsToKt = 1.9438444924406` are the two that are not, and one of them says so
- [ ] **An origin that is not a derivation is spelled, not commented**: `Const::Set(v)` for a decision and `Const::Measured(v)` for an instrument's reading, two `constexpr` identity functions in the const namespace. Then *every number carries its origin* is a property a 20-line test decides — a bare floating literal in a `const/` header is a failure — instead of 105 free-text tokens nothing reads. There is no `Derived` spelling because derivation is visible in the initialiser
- [ ] A constant's **name ends in its unit token**, from a declared suffix table with no ambiguity in it. The tree has one today: `Ms` is metres-per-second in `clients/Walker.h:17` and `core/Units.h:22`, and milliseconds in `clients/FrameTelemetry.h:33` (the bug tasks in `board/`). `MPerS` and `Ms` resolve it and `core/Units.h:15 kMPerDeg` shows the spelling already exists in the same file
- [ ] A constant with **exactly one consumer does not enter a const header** — it stays a `static constexpr` member beside its consumer, which is what `SubjectBench::kFovDeg` and `Renderer::kNearM` already are. This is the anti-junk-drawer rule and it makes the header shrink under use rather than grow
- [ ] A constant with **zero consumers fails the check** — a dead constant is dead code, and zero-consumer is the mechanically decidable symptom of a drawer. `CLAUDE.md` carries the dead-code rule; the Guidelines do not
- [ ] The const headers are **not included by any JSON reader**, which is the line that stops a tuning value being smuggled in as a constant (§ I.24)
- [ ] The literal ratchet as a test, not a ban — population: floating literals and integers outside `{0, ±1, 2, 3, 4}`, in `src/`, excluding `const/` headers and `static_assert` arguments; per-file counts published; **the test fails when a file's count rises**, exactly as the hardening ledger's counts do. Baseline is the 1 484 above and it is a first reading, not a target
- [ ] The ratchet states its own weakness in its own output: a count is gameable by folding two literals into one expression, so the per-file count is published for a human to read the diff against, and the number is never presented as a proof
- [ ] Constants per file and consumers per constant published by the same test, so *"is the header a drawer"* is a number
