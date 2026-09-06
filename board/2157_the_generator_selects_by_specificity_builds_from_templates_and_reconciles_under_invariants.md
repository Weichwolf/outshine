# The generator SELECTS by specificity, BUILDS from templates, and RECONCILES under invariants

**State:** open · **Waits on:** nothing · **Raised:** 2026-09-06, by the question "why not a template engine?"

## The proposal, and what is right about it

Build the OSM generator like a template engine: a tree decides WHAT a thing is, and the HOW sits
in templates. That separation is right and the bed is already half-way there -- `classify()` is
the tree, and `roofs.py`, `features.py`, `region.py` and the epoch tables are the templates. The
half that is missing is what makes it hold up at scale.

## Where one big tree breaks, and what replaces it

A building is not ONE choice. Mass, roof shape, storey height, bay, window proportion, wall
material, eaves oversail, elements, ground relation, LOD rungs -- each is partly independent, and
each is decided by a different mix of epoch, use, region, footprint and height. A single tree that
must reach one leaf per building is the cross product of all of them, and the way that fails is
already written into this tree: `Style.__init__` decides the precedence between the epoch and the
region in PROSE ("where both speak, the EPOCH wins"), inside a constructor, per field. That is a
flag, and the next case adds a second one.

**A CASCADE OVER SLOTS, resolved by SPECIFICITY -- CSS, not an if-chain.** A rule declares three
things: a PREDICATE over the subject (tags, epoch, region, geometry, coordinate), the SLOTS it
sets, and its SPECIFICITY. Every rule that matches contributes; per slot the most specific wins.
Then:

  - adding a variant is adding ONE rule, and it never touches the rules already there
  - the precedence between an epoch and a region is a NUMBER on the rule and not a sentence in a
    constructor
  - every value carries the rule that set it, which is what the sheet has to print anyway
  - there is no cross product, because slots are decided independently

## What a template engine has no analogue for, and it is the important part

A template engine emits TEXT, and text has no invariants -- any string is a valid string. A
generator emits GEOMETRY, and geometry has to be closed, consistently wound, welded, and it has to
MEET the things beside it. Every defect found on 2026-09-06 by looking at the pictures lives
exactly there and nowhere else: the comb where the roof's rings meet, the grade where the deck
meets its class, the junction plane where a leg meets the through road, the C1 corner where two
ways meet, the terrain fan wound away from the sky. None of them is a selection defect and none is
a template defect.

So the pipeline is THREE stages, not two:

| stage | what it owns | how it is proved |
|---|---|---|
| **SELECT** | the cascade decides every slot | the rule that set each value is named |
| **BUILD** | a registry of constructors per slot produces its part | each part checked on its own |
| **RECONCILE** | the parts are joined into one body, under the invariants | the checks that go RED |

## And the templates' VALUES are data, not code

`EPOCHS`, `ELEMENTS`, the region table, the span table, `GRADE_OF` -- these are numbers and names,
and this tree's own rule is DECLARED, NOT CODED. They belong in JSON that a scenario can override,
so a Fallout can re-dress a real place by declaring a table rather than by editing a generator. The
CODE holds the verbs -- build a roof of this shape, draw this element, mesh this mass -- and those
stay registries of functions, which they already are.

## What Unreal does, what RAGE does

Unreal's **PCG** graph is exactly SELECT + BUILD: filters and spawners over attributes, no
reconcile stage, because Unreal's world is authored and a human fixes what does not meet. RAGE has
no generator at all -- a studio places every prop. So neither faces the third stage, and the item
says so: the reconcile stage is this engine's own, and it is there because nobody authors here.
The readable body that does face it is **CGAL** (straight skeleton, Nef polyhedra, mesh repair),
cited for technique and never for structure.

## The measurement that shows I was wrong

`test/lab/README.md` gains a row per slot: how many rules decide it, and how many cases reach the
DEFAULT rule rather than a specific one. A slot whose default is reached by most of the world is a
slot with no rules yet, and that number is the generator's own honest coverage. The bar is that
every case on the ladder names a specific rule for every slot a viewer can read at 200 m, and the
negative control is a rule removed -- the sheet must then say `default` where it said the rule's
name, and the picture must get worse.
