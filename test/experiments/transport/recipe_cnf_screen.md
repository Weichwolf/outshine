# Local recipe selection and the ETH preprint

Owner [#1835](https://github.com/Weichwolf/outshine/issues/1835). The concrete
adoption/reserve decision and acceptance live there. This reference records the
reproducible size analysis; it implements no counting algorithm or SAT engine.

Primary source: OpenAI, *The Exponential Time Hypothesis is False*, revision
25 October 2026, pinned upstream commit `4eaee87548250b2682affda7d05d947c05554bae`.
The PDF, source/license pointers and optical inspection pages are recorded in
`doc/references/procedural/2026-exponential-time-hypothesis-is-false.reference.json`.
The upstream repository describes these manuscripts as model-produced research
data at different verification stages. This screen does not independently
validate or refute the mathematical theorem. It checks the literal published
construction against a concrete current generator request.

SIGGRAPH first: the existing Chen et al. 2008 *Interactive Procedural Street
Modeling* was re-opened, including its pipeline figure. Its directional tensor
field and the preprint's algebraic tensor network are different objects. Neither
a graph label nor an asymptotic counting result supplies valid 3D contacts.

## Actual bounded request

The current elevated-rail recipe names three complete original sources. The
existing source reader and `station_choices` produce 94 nominal support stations.
`rail_supports_plan.VARIANTS` supplies five longitudinal offsets: 0, -2, +2, -4,
+4 m. The current planner evaluates these lazily, with old-traffic rejection
before body construction and complete actual contact/clearance checks afterward.
The delivered physical result and costs are owned by #1771/#1776, not this screen.

Let `x[i,a]` choose station i's option a, including one explicit omission. There
are 564 choice bits. Require exactly one per station: one disjunction of six
options and 15 pair exclusions. Replacing the long disjunction by uniquely
defined binary OR gates yields 282 auxiliary bits. For `z = a OR b`, use

    (NOT a OR z) AND (NOT b OR z) AND (a OR b OR NOT z).

The last disjunction has width three. The auxiliaries are uniquely determined,
so this replacement preserves counts of option assignments, not merely SAT.
The checks enumerate the small gate cases and verify exact unique extension.

The resulting partial domain instance has 846 variables, 2,352 clauses and
5,078 literal occurrences. Two unit clauses reject offsets outside the existing
station domain. All source IDs and nominal positions are retained. This is an
exact encoding of the choice domain only. It is deliberately not called a
physically valid layout, and the output report says `physical_feasibility_known`
is false. The independent choice-domain count also factorizes directly; a large
general counting engine is unnecessary for that isolated stage.

## What a complete reduction would additionally need

For each of up to 470 physical candidates, run the unchanged contact/body/old-
traffic admission and both-precision audit. Failed options become negative unit
clauses. Pair exclusions must use the actual admitted contact footprints and
0.2 m foreign clearance, rather than visual XY overlap or invented source nodes.
At most 109,275 cross-station candidate pairs exist before spatial rejection.
The size bound is an upper count, not a measured number of required tests.

Complete source-span rules are additional hard factors. For each potentially
selected station a, if its source end is over 24 m away, some selected successor
b must satisfy `0 < position[b]-position[a] <= 24`; include the source start in
this rule. Long disjunctions need further count-preserving auxiliaries. Every
physical candidate must still carry its source, frame and product identities.
These omitted predicates are not inferred or bypassed by the generated CNF.

Coverage preference is an objective: maximize non-omitted stations, then apply
the declared recipe preference. Exact counting alone returns neither that
optimizer nor a time-bounded valid partial world. Threshold circuits and witness
self-reduction introduce extra variables, clauses and counting queries. An
existing feasible partial result/omission ledger must survive interruption.
The current accepted 94-of-94 station result already attains the trivial upper
bound 94 for this one fixed coverage objective; no global geometry, shift-cost
or world optimality follows from that observation.

Soft MaxSAT approximation is insufficient for hard geometry. If H hard clauses
are each copied W times and S soft clauses remain, the general gap bound between
hard-invalid assignments and a valid optimum is only `W-S`. A conservative
sufficient condition is `W>S` and `epsilon < (W-S)/(W*H+S)` for the additive
`epsilon*(W*H+S)` guarantee. This bound shrinks with instance size; a theorem for
fixed epsilon supplies no such uniform hard-feasibility guarantee.
Hard constraints must remain hard in a suitable exact/anytime SAT/MIP/CSP model.

Continuous height/grade fitting remains separate. Binary-encoding a bounded
Float32 height lattice multiplies variables by its per-height bit width and adds
arithmetic/comparison circuits; it does not replace GEOS or LP/QP geometry work.
A polynomial-size reduction alone does not preserve a subexponential bound in
the original recipe size. Practical SAT/MIP may still help a future measured
discrete bottleneck; this screen makes no claim against those solver families.

## Published parameter check

For this CNF the interpolation schedule has lambda=10, nu=4, r=0 and Q=0.
The sharper fixed-density tensor construction uses s=n+m=3,198, lambda=12 and
root u=floor(lambda/131072)=0. Its corresponding bipartite encoding has 15,236
wires. Both published fast branches fail necessary size checks here. The source
handles smaller regimes by direct summation; no fast counting run is benchmarked.

The first interpolation r>=2 threshold alone needs lambda>=521284. That is
only one necessary condition: Q>=6R still fails there. Likewise u first becoming
positive at lambda=131072 is not a useful crossover or sufficient eligibility.
The unit checks use exact integer arithmetic to prevent either threshold from
being presented as a practical speedup.

The tensor construction also uses finite-ring exact arithmetic and increasing
precision. The reported root ring entry would carry 40,656 coefficient payload
bits for this encoding; no such tensor is allocated by the screen. Polynomial
space is not an 8-GB peak, and these constants do not establish practical cost.
No claim about an alternative tuned implementation follows from their failure.

## Reproduction and boundaries

Existing .venv and existing NumPy/Shapely references only; no SAT package, Lean
build, native implementation or new library dependency. Outputs are fresh host-
temp directories checked by the common `experiment_output.create_output` helper.

```sh
export PYTHONPATH=lib/src:test/experiments/transport:test/experiments/render:test/experiments/buildings
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -B -m unittest test_recipe_cnf_screen
python -B test/experiments/transport/recipe_cnf_screen.py --output "$fresh_temp"
```

The screen records the recipe/source binding, exact partial DIMACS, station map,
bit/wire counts, source hashes and commit. Its preparation CPU time includes
input/domain work and is explicitly not counting-solver runtime. Physical
candidate construction, solution quality, allocations/peaks and native/device
costs of a new solver remain unmeasured because no solver is selected. The
conditional implementation/benchmark gate in #1835 is therefore not entered.
Generator products, source geometry and the accepted support plan are unchanged.
