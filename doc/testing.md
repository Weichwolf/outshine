# Testing — declarative expectations, differential nets, mirrored trees

> Owner, 2026-08-05: *„Outshine muss komplett über ein Harness mit Tests in JSON (oder was dir besser
> gefällt) testbar sein — ich mag auch diese HTML/CSS-Tests, wo zwei Versionen den gleichen Output
> bringen müssen, falls das für Outshine was bringt."* ·
> *„`test/*`, `doc/*` und `src/*` müssen die gleiche Verzeichnisstruktur aufweisen — tree muss identisch
> sein."*

Spec-first. `## State` is empty until it is built.

## Spec

### 0. Why, and the measurement that forced it

On 2026-08-05 a critic did not read the test net — it **mutated the model** and ran the whole net
against each mutation: seven harnesses, 296 missions.

| Deliberate destruction of `f16.xml` | The net said |
|---|---|
| transonic drag rise deleted | all green, `payerne-full` matched baseline to the tenth of a second |
| inertia tensor `ixx` +100 %, `izz` −37 % | all green |
| pitch damping `Cmq` × 0.2 | all green |
| nosewheel steering 80° → 5° | **byte-identical** |
| tyre friction −60 % | all green |
| spring rate × 5 | CRASH — from a *mission*, not a harness |

**Five of six were invisible.** The cause is not laziness; it is *where the expectation lives*.
`FBTestAirEnvelope` returned `notMeasured == 0 ? 0 : 1` — it counted whether a number **came into
existence**, never whether it was **right**. Fixing that one line turned the gate red immediately:
**7 anchors outside their band**, present since they were written, never seen.

> **An expectation buried in a program can silently fail to gate. An expectation in a table cannot —
> a missing row is visible.**

That is the whole argument for the owner's first instruction, and it is measured, not asserted.

### 1. Two nets, and they catch different things

The tree already has both. Only one of them works today, and it is worth saying which.

**DIFFERENTIAL (the owner's HTML/CSS pattern) — strong, proven here.** Two versions must produce the
same output, byte for byte. This tree runs it in its strongest form already: telemetry SHA-256 across
two binaries, across `--threads 1/2/4`, and campaign-vs-standalone replay (100 of 100 missions
identical). **Every real defect found on 2026-08-05 was caught by a differential**, never by an
assertion. Its weakness is equally clear: it says *something moved*, never *this is wrong* — and it is
useless the day the reference itself is replaced.

**ASSERTIVE — weak here, and it is a fixable weakness.** „Corner speed is 320–340 kt because
[source]". It is the only net that can judge a *new* implementation, and the only one that answers the
owner's real question: *„in a game engine everything is always wrong — the question is whether it is
still believable."*

**Both, always.** Differential guards against regression; assertive guards against being confidently
wrong from the start. Neither substitutes for the other.

### 2. Tests are data

A test is a declaration, not a program. The runner is one program; the tests are many files.

```
{ "subject": "modules/f16",
  "claim":   "corner speed at 15000 ft, clean, 50 % fuel",
  "measure": { "harness": "corner-speed", "args": {"altFt": 15000, "fuel": 0.5} },
  "expect":  { "value": 330, "unit": "kt", "band": 0.03 },
  "source":  "[DOC modules/f16/aerodynamics-performance.md §3]",
  "tier":    "A" }
```

Four properties make this the right shape for an engine a machine builds with:

- **A missing expectation is visible.** An empty `expect` is a hole in a table, not an unwritten
  `if` inside a 600-line C++ file.
- **`source` is mandatory**, like every number in this tree. A band without a source is a defect.
- **`tier` carries believability.** `A` is the hard gate — the places a knowledgeable person checks
  ([`body-format.md`](body-format.md) §4). `B` is recorded and reported, never gating. This is how the
  owner's *„everything is wrong, is it believable"* becomes a machine verdict instead of taste.
- **Generatable.** A machine can write a hundred of these from a source table; it cannot write a
  hundred bespoke harnesses without them rotting.

Format: JSON for the declarations. Not because it is pleasant, but because the runner must never be the
place a test is *interpreted differently* than it is read.

### 3. The three trees and what each one answers

> Owner, 2026-08-05: *„`doc/*` was wollen wir, `src/*` was können wir, `test/*` was beweisen wir."*

One place per statement. A want without a can is a gap in `src/`; a can without a proof is a gap in
`test/`; a proof without a want is a test nobody ordered. **The triad is also the completeness check** —
walking the mirrored tree, any directory that has two of the three has a named hole.

### 3.1 The mirrored tree

**`src/<path>`, `doc/<path>` and `test/<path>` name the same thing.** Given any one of the three, the
other two are known without searching — which is the difference between a machine navigating a codebase
and a machine grepping one.

```
sim/src/sensors/FBVisualSystem.cpp
doc/sensors/visual.md
sim/test/sensors/visual.json
```

**Checkable, and it must be checked** — `tools/verify_layers.py` already counts and prints structural
facts (layers, registry readers, draw viewers). Tree congruence is the same kind of fact: *every source
directory has a doc directory and a test directory, and no test file is orphaned.* An engine that a
machine builds with must be able to answer *„what tests this?"* by path, not by search.

Three shapes of orphan, reported apart because the work each implies is different:

| | |
|---|---|
| `MISSING` | nothing at that path in one of the three trees |
| `LEAF` | `doc/<path>.md` where `src/<path>/` is a directory — one document that has not been split into the directory its subject already is |
| `EXTRA` | a directory in `doc/` or `test/` with no counterpart in `src/` |

### 4. Acceptance

| Contract | Anchor |
|---|---|
| Every gate's verdict is data, not control flow | no harness computes its own pass/fail; the runner compares `measure` against `expect` |
| A hole is visible | a subject with no `tier: A` declaration is reported by the runner, not silently absent |
| Both nets run | differential (byte-identity across binaries/threads/replay) **and** assertive, on every round |
| The trees are congruent | a tool prints the count of directories in each of the three and the list of orphans; the number is expected to be zero |
| Tier A is honest | the list exists **before** a physics or model change is accepted, per [`body-format.md`](body-format.md) §4 |

## State

**The tree exists, the congruence is machine-checked, and one harness is on the data shape.**

| | |
|---|---|
| `sim/test/` | the ten harnesses, each beside what it judges: `test/core/` (3 monitor proofs + the weather mirror), `test/fdm/`, `test/weapons/`, `test/modules/{f16,mig29,air,missile}/`. Namespace `FlightBox::Test`, rank 12 in `verify_layers.py` — a harness may reach anything, nothing may reach a harness |
| `make -C sim verify-trees` | `tools/verify_trees.py`, the triad gate. Prints the directory count of each tree and every orphan by name and kind. **20 orphans today** (9 `MISSING`, 8 `LEAF`, 3 `EXTRA`) against 24 before the move — red, and naming why |
| `make -C sim verify-tests` | `tools/fb_test.py`, the one runner. Walks `test/**/*.json`, runs each declared harness once, matches a declaration to the measurement line whose fields agree with its `args`, compares against the band |
| `test/modules/air/envelope.json` | **120 declarations, 59 gating (tier A), 61 recorded (tier B, no published figure)** — generated by `tools/gen_air_decks.py --tests` from the same anchor table that writes the decks and the harness's flight parameters |
| `make -C sim test-air` | builds the harness **and** judges it. **7 anchors outside their band**, unchanged across the migration: `su27` A1, `mig23`/`mig25`/`su7`/`su22` A3, `mig17` A5 + α |

**The measurement interface.** A harness prints one line per measurement and no verdict:

```
[measure] air-envelope row=f15c anchor=A1 value=2.468210 unit=M
```

The differential that accepted the migration: of the 120 measurements the rewritten harness emits,
**111 are bit-identical** to what the self-judging version produced. The other nine are measurements
that were never taken before — the take-off run of the seven rows that publish none, and the sea-level
Vmax of `mirf1` and `f5e` — because *„does the catalogue publish a figure"* is a property of the
expectation, not of the flight, and it has moved out of the C++ into the declaration file.

**What the runner also prints, because §4 asks for it:** 20 of 21 source directories carry no tier-A
declaration. That list is the honest size of the remaining work.

## Gaps

- **Seven anchors are outside their bands.** Reading them one by one — model defect or band too tight —
  is owed, and it must not be settled by widening a band. `A3` (service ceiling) is four of the seven,
  in both directions (−22.9 % to +28.1 %), which points at the measurement's schedule sweep rather than
  at four unrelated decks.
- **Nine harnesses still carry their own verdict** and are not in `fb_test.py`'s table: the three
  monitor proofs, the weather mirror, `two-fdm`, `gun`, `corner-speed`, `mig29-envelope`,
  `missile-airframe`. Each must be re-measured against its old output when it moves, not ported on
  faith.
- **`FBTestCornerSpeed` asserts no number at all**: `kCornerBandFrac = 0.97` measures against its own
  sweep maximum, so a physics with 5 °/s would pass it. It is the only F-16 performance harness.
- **20 of 21 source directories have no tier-A declaration**, and `doc/` is not yet a directory per
  source directory: eight subjects are a single `.md` leaf (`core`, `fdm`, `pilot`, `sensors`,
  `systems`, `weapons`, `world/terrain`, `modules/stores`), four have no doc node at all
  (`units`, `render/stages`, `modules/f16/displays`, `modules/missile`), and three doc directories have
  no source (`campaigns`, `render/clouds-legacy` + its `exhibits`).
- **`missions/payerne-takeoff.fbm` is red at HEAD** — touchdown 11.0 km off the runway on a 620 m
  hillside, and both judges stay silent.
- **The binary's exit code is no longer a verdict** for `air-envelope`, by design. Anything that read
  `build/fb-test-air-envelope`'s rc must read `make -C sim test-air` or `make -C sim verify-tests`
  instead.
