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
