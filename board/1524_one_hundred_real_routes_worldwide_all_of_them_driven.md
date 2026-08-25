Type: feature
State: open
Area: corpus
Tags: instrument scope

**One hundred real routes worldwide, and all of them are driven**

**The owner's ruling, and it is what the driver tool is FOR:** *the driver is the tool for building
the OSM generator correctly, and it must be tested worldwide on real routes. One hundred realistic
routes, all of which must pass.*

**A route is a declared member of a STRATUM and never an arbitrary line.** Adding one requires naming
what it is the hard case for; a route that is in the set because it was convenient teaches nothing and
dilutes every rate computed over the set.

## What a route declares

| | |
|---|---|
| **the stratum** | what it is the hard case for -- the single field that justifies its existence |
| **the endpoints** | two coordinates, and the route between them derived by the planner rather than stored |
| **the data pin** | the upstream extract's digest, because OSM changes under a run and a route that changes is not a test |
| **the vehicle** | which declared vehicle drives it, because a lorry and a motorbike disagree about the same road |
| **the expected refusals** | where the DATA is genuinely broken, a named refusal is a PASS -- see the criterion below |

## The strata, and why each is hard

| stratum | what breaks there | examples |
|---|---|---|
| **untagged structure** | the gradient and crossing reveals, `board:1518` | alpine galleries, Norwegian coastal tunnels, Japanese mountain expressways |
| **stacked junction** | vertical separation, layer inference | Los Angeles interchanges, Shanghai elevated, Birmingham |
| **hairpin geometry** | the pursuit law's tightest case, `board:1522` | Stelvio, Trollstigen, Tianmen |
| **very long straight** | the opposite -- numerical drift over distance | Nullarbor, Ruta 40, US midwest grid |
| **extreme elevation** | the solve's range and the drivetrain's gradient limit | Andes passes, Himalayan roads, below-sea-level basins |
| **high latitude** | Mercator distortion and metre-per-degree | Tromso, Ushuaia, Iceland |
| **antimeridian** | longitude wrap, which is a whole class of silent defect | Fiji, Chukotka, Taveuni |
| **sparse tagging** | inference where OSM says almost nothing | rural Sahel, Mongolia, Amazon roads |
| **dense urban** | junction density and short segments | Manhattan grid, Tokyo, medieval Prague |
| **discontinuous network** | ferries and gaps OSM asserts but no road spans | Norwegian fjords, Greek islands, Baltic crossings |

**Ten strata, ten routes each.** The set is balanced by construction, so a rate quoted over it is not a
rate about Germany's motorways.

## What must be true

- [ ] **Route 0 is the negative control** and drives in every campaign -- `board:1504`. If it produces
      a finding, nothing else in the run is believed
- [ ] **Munich to Hamburg is route 1**, because it is the first thing the owner asked to see and it is
      800 km of the best-tagged data on earth: if the engine cannot drive that, no stratum matters
- [ ] **A route passes when it is DRIVEN end to end within the floor** -- no contact past its limit, no
      wheel airborne, deviation inside `board:1504`'s published floor plus a declared margin
- [ ] **A named refusal is a PASS where the data is genuinely broken.** Functional correctness is the
      bar: a road that ends in a wall must be REFUSED loudly by the generator, and refusing correctly
      is success. Crashing on it is not
- [ ] **A failing route is a work item, never an exemption.** There is no skip list -- `board:1504`
- [ ] **Every route publishes kilometres per finding** and its findings are classified, so the set's
      verdict is a rate over a named population and not a count
- [ ] **A route re-runs identically** from its seed and its data pin, or a class found on it cannot be
      stood up again
- [ ] **The set grows to 100 before it is quoted as a set.** A partial set is quoted with its size and
      its strata, always

## Comments

**The exact endpoints are filled in as each route is built and verified, and the STRATA are the design
decision.** Writing 100 coordinate pairs today would be writing 100 unverified claims; writing ten
strata with named exemplars is the part that decides what the set measures.

**The set is not a pass/fail gate on the tree.** It is the instrument the generator is iterated
against, over hundreds of hours -- `board:1518` says the tight loop is held-out tags over a region in
seconds and this is the outer loop around it. All hundred passing is the DESTINATION, and the useful
number long before that is how many pass and which stratum the failures are in.
