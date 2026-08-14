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

## REFUTED ON ITS LARGEST CASE, and the owner's rule replaces the boundary

**The owner, verbatim:** *the species list is one of the things i trust in the backlog. every species
requires a task and a test with rendering as proof.*

**So the rule above is not wrong and is not sufficient.** Its two halves stand: the species table exists,
is read by `src/generators/TreeSpecies.h`, and the required row count is derivable. What it does not
settle is the thing the owner put at the centre — **a row nothing has ever rendered is not a served
requirement.** `src/assets/world/species/hazel.json` existing is not evidence that a hazel renders.

**THE BOUNDARY, RESTATED FROM WHAT THE OWNER SAID RATHER THAN FROM A TITLE'S SHAPE:**

> **A content kind is a work item, because it carries a proof. Only a VALUE inside a kind that is already
> proven is a row.**

The datum was never the question. `form`, `crown`, `lai` and their `_origin`s are values **inside** the
species kind; the species itself is a kind, and a kind is served when a render shows it. That is why the
first rule mis-drew the line: it asked *does an existing capability read this*, and reading is not proof.

**What that does to the three classes.**

| | items | verdict under the owner's rule |
|---|---|---|
| **A — instance enumerations** | 436 | **rewrite, all of them. The delete list is EMPTY** |
| **B — mixed features** | 212 | **rewrite or keep, per item; nothing swept** |
| **C — capability features** | 299 | **keep**, unchanged |

**Delete is empty and that is the correct outcome rather than a salvaged one.** Every one of the 436 is a
content kind: a species, a building use, a vehicle class. Each needs a task and a render proof, which is a
**stronger** requirement than any of them carries today — the rewrite adds an acceptance where there was a
binomial and a common name. [MEASURED] **436 items, 436 distinct titles, zero duplicates**, so not one is
redundant with another either.

**The last delete candidate died on inspection.** `0137 Forms belonging to other biomes, named so they are
not mistaken for oversights` reads as a list of exclusions — `Krummholz`, `Alpine dwarf shrub heath`,
`Cushion` — but `vegetation.json`'s templates already name `alpine_aster`, `mountain_avens` and
`chamois_cress`, and `core/AlpineLimit.h` exists. **The alpine band is declared, so its forms are
required and the feature's own framing is out of date.**

**One item is mis-typed rather than unwanted**: `1085 Military — setting-dependent; a post-scarcity world
may have no place for it, and that is the owner's call` says in its own body that it is a decision only
the owner can make. **That is a `Type: issue`**, and changing a type is a rewrite, not a deletion.

**Four measurements this round of how badly shape-based predicates do here**, kept because the next round
will be tempted by one: two of mine — a word-count-and-echo test that called `Pantograph contact with the
catenary` inventory, and a binomial-title test that called `Hull planing` a species — and two of the
orchestrator's failed species extractions, one keying on the wrong field and returning 14 land-cover
classes, one walking the wrong depth and returning 0. **None of the four contradicted a conclusion; each
measured a different population.** The rule is now semantic and its unit is the feature, and even that is
only a first pass over a list a person then reads.
