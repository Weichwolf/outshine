Type: feature
Area: corpus
Tags: instrument oracle

**A million buildings are proved functionally correct without being looked at**

**The owner's question, and it is the right one:** *you cannot look at a million renders. 99 % has to
be guaranteed by mathematics.* This item is where that answer is built, and it is deliberately kept
open and returned to -- the tiers below are what has been reached so far, not a finished plan.

**The reframing that makes it tractable:** a render is an oracle for APPEARANCE. What a planet of OSM
data needs is an oracle for FUNCTION -- a road that can be driven, a shell that can be entered, a
tunnel that is a tunnel, a bridge with something under it. Almost all of that is computable from the
geometry with no picture at all.

## The tiers, ordered by cost per instance

| | Cost per part | What it catches | Where it stands |
|---|---|---|---|
| **1. unspellable** | zero | the whole class, forever | `board:1499` -- one reference line, so a seam has no spelling |
| **2. metamorphic** | O(n), no oracle | tile dependence, order dependence, axis swaps | not built |
| **3. invariants** | O(n), no oracle | manifold, support, interpenetration, clearance, gradient | not built |
| **4. physical** | a headless drive | everything the above missed that a vehicle can feel | `board:1504` |
| **5. held-out tags** | free, planet-wide | whether an INFERENCE is right | not built |
| **6. eyes** | a person | appearance, plausibility | the tail and one per class, ~50 a round |

### 2. Metamorphic relations are the largest untapped lever

**You do not need to know the right answer if you know how the answer must CHANGE.** Each of these
runs on every part, needs no oracle, and is a `static` property of the generator rather than of the
data:

- **shift the tile grid by half a tile: the geometry inside must not change.** This is the single most
  valuable one for a streamed world -- it catches every defect where a part learned which tile it was
  in, which is the same class as the seam
- **reorder the input ways: identical output.** Catches order dependence, which is what makes a bug
  reproduce only on one machine
- **rotate the input by 90 degrees: the output rotates with it.** Catches axis-order and
  latitude-longitude swaps, which are silent and everywhere
- **loosen the budget: the part must be no finer.** Monotonicity of the ladder, and it is checkable
  against the published capability rather than against a picture

### 5. OSM is its own labelled set, and that is the answer to "how do you know the inference is right"

**Where a bridge, a tunnel, a layer or a level IS tagged, we have ground truth we did not have to
make.** Hold the tag out, run the reconstruction that is meant to work without it, and compare. That
gives precision and recall for the reveal over the whole tagged population -- millions of examples,
zero human effort, and a number that moves when the reveal improves.

**The first thing to measure is how big that population is**, per class and per region, because the
claim *the reveal works where OSM says nothing* is only as strong as the set it was validated on.

### What survives all of this, and must be said

**A systematic error passes every invariant.** Every building 10 % too tall is manifold, watertight,
supported, navigable and metamorphically stable -- and wrong. Nothing above catches it. What does:

- a **distribution** compared against a published one (storey heights, road widths, lane counts),
  which is a statistic and not a picture
- the small human sample of tier 6, which is exactly what the sample is FOR

**So the honest split is: mathematics proves the geometry is well formed and the inference is faithful;
a person decides whether the model of the world was right in the first place.** Conflating those two is
what makes a suite that is green and a world that is wrong.

## What must be true

- [ ] Every generated part answers a fixed set of geometric invariants, computed from itself, with no
      render and no oracle
- [ ] The tile-shift relation runs over the corpus and its violations are a named class
- [ ] Held-out tag validation reports precision and recall per structure class, with the population
      quoted beside it
- [ ] Findings are CLASSIFIED and the verdict is a rate -- kilometres per finding, parts per finding --
      never a count
- [ ] The tail of every metric is rendered and looked at, and one representative per cause class
- [ ] A distribution check against a published population, so a systematic error has somewhere to show

## Comments

Tier 4 is why the physics is being hardened first: **a vehicle is a general-purpose oracle for
functional correctness that nobody has to write cases for.** A crack, a hole, a step, an unrevealed
tunnel and a gradient nothing could climb all arrive as the same reading -- a contact past its limit,
or free fall -- and `board:1516`'s pilot means the same instrument drives a road, walks a footway and
runs a rail.

## The fleet is not the optimiser, and that is a correction

**Asked whether driving cars is the right way to OPTIMISE the OSM generator, the answer is no, and the
first version of this reasoning oversold it.** A driving fleet is a downstream instrument with five
structural weaknesses:

| | |
|---|---|
| **long causal chain** | crash → physics → pilot → corridor → solve → generator → provider → OSM. Six candidate owners, and the fingerprint INFERS between them. An invariant on the output names the owner with certainty |
| **coverage follows roads, not branches** | a car drives kilometres; the generator's rare paths -- tunnel portal, multi-level junction, untagged bridge, roundabout -- are sampled in whatever proportion routes happen to cross them. That is where the defects are and where the sample is thinnest |
| **it cannot see most of the world** | buildings, footways, waterways, power lines and rails are all invisible to a car driving past them, and buildings were the original question |
| **it spends a simulation on arithmetic** | a 40 % gradient is answerable from the corridor before anything drives it |
| **optimising against it fits it** | with "the car crashed" as the only signal, smoothing everything until nothing crashes is a local optimum that looks defensible and is wrong |

**The counter-argument, and it is real:** functional correctness for a road IS drivability, so the car
is not merely an instrument, it is the definition of the requirement. **The resolution is that the car
defines the ACCEPTANCE CRITERION and need not be the SEARCH procedure.** Define correctness by
drivability; find violations with arithmetic wherever arithmetic can; drive only for what it cannot.

**So the fleet's product is not a fix -- it is a new O(n) check.** Every finding it produces ends its
life as an invariant, or it will be found again forever. Its own finding rate must FALL over time
because checks moved upstream, and never because roads were smoothed until it stopped complaining.

## Optimise towards what -- the objective that is still missing

There is no positive target for the OSM generator today, only the absence of crashes, and an absence
target has a trivial wrong solution. The target must be **the corridor reproduces the surveyed
geometry within a stated tolerance wherever ground truth exists** -- held-out tags, and better
elevation data where a region has it.

Which puts the TIGHT loop somewhere else entirely: the elevation solve and the structure reveal run
over a whole region with no vehicle at all, in seconds, and validate against OSM's own tags. That is
the loop the generator is iterated in. **The fleet is the slow outer loop that says what the tight one
cannot see.**

### The owner is right that drivability is a very good indicator, and here is the precise form

**It is a SYSTEM signal.** It cannot go green by repairing one subsystem: the elevation solve, the
junction blend, the gradient limit, the structure reveal, tile continuity and curvature continuity
must all hold at once. That is exactly what a top-line number should be, and it is very hard to
satisfy accidentally.

**And the "smooth everything until nothing crashes" objection above only bites if drivability is the
ONLY signal.** With the tolerance-against-ground-truth check running beside it, smoothing costs
geometric fidelity immediately, and that local optimum is closed. The two instruments seal each
other.

**The asymmetry that remains is the useful one: drivability is a STRONG FALSIFIER and a WEAK
CONFIRMER.** A crash means something is definitely wrong -- high information, and the owner named
correctly. A clean run means the road is DRIVABLE, not that it is RIGHT: a bridge reconstructed as a
smooth embankment drives perfectly and is wrong. That is precisely the case held-out tags see and
driving cannot.

**So: two headline numbers every round, never one.** Kilometres per finding over a stratified
population, and geometric agreement where ground truth exists. Neither subsumes the other, and quoting
either alone is the defect this pairing exists to prevent.

### And the smoothing objection is weaker than the owner's answer to it: THE DEFECT IS CONSERVED

*"Smoothing would produce unrealistic gradients."* -- and that closes the local optimum by arithmetic
alone, without needing the fidelity check beside it. **A step is a bounded vertical displacement that
has to be absorbed somewhere.** Spreading a 5 m step over a length L gives an average grade of 5/L:

| L | grade | what happens |
|---|---|---|
| 20 m | 25 % | the drivetrain refuses |
| 50 m | 10 % | drivable -- and the road has left the terrain by about 2.5 m |

**The limit is derived and not a matter of taste.** On the F31, 3699.1 N of drive against 15788.7 N of
weight gives `sin(theta) = F / mg = 0.2343`, so 13.55 deg = 24.1 %. The friction ceiling
`tan(theta) <= mu = 0.95` sits at 43.5 deg, so the drivetrain binds first. Smoothing to avoid one
crash therefore produces a different one, and the argument above was weaker than it needed to be.

**Which makes the gradient limit the TRIGGER and not only the check.** A step that cannot be spread
without an impossible grade IS the evidence that a structure stands there -- the owner's original
statement, *too much gradient because a tunnel was not recognised*, is the reveal's whole mechanism
stated as a measurement.

**One honest exception**: where the step was noise in the elevation data rather than terrain,
smoothing is CORRECT and the fidelity check punishes it. Noisy ground truth is its own case and needs
naming before a fidelity number is believed.
