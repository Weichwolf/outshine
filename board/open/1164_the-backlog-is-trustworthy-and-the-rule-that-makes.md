Type: feature
Area: harness
Tags: scope, instrument

**The backlog is trustworthy, and the rule that makes an item one**

The owner's standard: **`board/` must contain only work items required to reach the vision within the
four constraints, and every item still standing must be true of the tree today.** That is stronger than
*delete what is untrue* — a survivor needs a verdict as much as a casualty does.

## The rule, and it is a rule rather than a list

**An item is a work item when its absence is a MISSING CAPABILITY. It is a table row when its absence is
a MISSING VALUE that an existing capability would read.**

The vision already decides which side a thing falls on: *every piece of content from a generator behind
one interface*, with configuration externalised as declared data. So *the generator kind exists, reads its
declared table, and the table covers what a declared scenario names* is the requirement; **which rows the
table holds is data, and data does not need a markdown file each.**

**And the required row count is DERIVED, never listed.** [MEASURED] `src/assets/world/vegetation.json`
declares **13 templates naming 43 species**; `src/assets/world/species/` holds **31 rows**; **22 species
named by a declared template have no row.** *That* is the required work — 22, derived from the
declaration — against **754 board items whose title is a botanical name.** A candidate pool is not a
backlog.

## What cannot be derived, and two attempts prove it

**No textual predicate separates a species from a behaviour, and both attempts at one mis-swept.** A
word-count-and-echo test classified `1103 Pantograph contact with the catenary`, `1107 Buoyancy from
displaced volume against the core's water level` and `1111 Hull planing at speed` as inventory; a
binomial-shaped title test then caught *Hull planing* and *Wheels, suspension* as species. **The
difference is semantic and the shape does not carry it** — which is this repository's own *the number was
right and about something else*, committed twice in one round while looking for it.

**What IS decidable is the parent feature**, because the feature titles state which kind they enumerate —
*Species needing the X form*, *Building types by use*, *Road vehicle classes* enumerate **instances**;
*Growth forms — what the generator must be able to shape*, *Wind and interaction*, *Wheels, suspension,
tyres*, *Damage* enumerate **capabilities**. So the classification is **two-stage: the feature decides the
class, and body content decides the exceptions** — anything inside an instance feature carrying a marker,
a backtick citation or a number is held back for a read rather than swept.

## The three classes over the 946 `Area: generators` tasks, and they partition it exactly

| | items | features | verdict |
|---|---|---|---|
| **A — instance enumerations** | **436**, of which **8** held back for a read → **428** | 16 | **delete**, once the replacement below stands |
| **B — mixed features** | **212** | 14 | **per-item read**, not this round; the feature does not decide these |
| **C — capability features** | **298** | 15 | **keep**, and verify as C is worked |

436 + 212 + 298 = 946. **Class A is edge-free and citation-free** — [MEASURED] 0 `Depends:` edges into it,
0 `Regresses:`/`Supersedes:`, 0 items parented to one, **0 cited from `src/` or `test/`** — so removing it
reds no invariant and dangles no marker.

- [ ] **THE REPLACEMENT STANDS BEFORE THE DELETION, and this is the load-bearing sequencing clause.**
  A deleted line is scope given up. Class A goes only where its requirement survives in derived form:
  the species half is `board:1165`; the building-use and vehicle-class halves are **already upstream
  attributes** — `board:0637` marks building use `TILE`, so the taxonomy comes from the served data and
  not from a list of sixty-five files
- [ ] **The 8 held-back items are read, one by one, before any batch runs.** They are the ones the rule
  cannot decide, which is precisely why they are named rather than counted
- [ ] **Class B is worked feature by feature, not swept.** *V.9 Watercraft* holds both *Classes: dinghy,
  motorboat, cabin cruiser…* and *Buoyancy from displaced volume against the core's water level*; one is a
  row and the other is a physics requirement with its own falsifier
- [ ] **Class C is verified as it is worked, never in a pass.** An item asserting a fact about the tree
  has that fact **re-measured**; an item asserting a gap has the gap **re-checked**, because a gap closes
  silently — `board:1160` is that in one direction and an unticked box whose work is done is the other
- [ ] **A survivor that could not be verified says so**, carrying the instrument that would settle it and
  what it costs. *Not yet measured* is a cost; *not measurable* is a different sentence; **and neither may
  read as checked because it survived**

## What this must not become

**A sweep.** The board is kept true at the point of use, and a pass that has to be remembered is a pass
that will not happen. This campaign is the one exception and it is bounded by the counts above — after it,
the rule is applied when an item is written and when it is groomed, and never again as a project.

**And it must not become a licence to delete a requirement because it is short.** *Rider lean as the
steering input, not a yaw torque* is nine words and is a physics requirement with a falsifier. The test is
never length.

**Done when** every `Area: generators` item is in exactly one of the three classes with its verdict
executed or its read scheduled, no surviving item asserts a fact about the tree that has not been
re-measured or explicitly marked unverified, and the rule above is what a new item is written against.
