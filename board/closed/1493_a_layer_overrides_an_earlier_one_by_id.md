Type: task
Parent: 1480
Area: scenario
Tags: scope

**A layer overrides an earlier one by id**

`Layers` is read and carried and nothing loads them. **This is what makes a scenario the size of a game
authorable and a mod a first-class thing** rather than a patch: RAGE registers content packs in an
ordered `dlclist.xml`, Bethesda's plugin chain is an ordered list where a later record overrides an
earlier one of the same id, and this is that.

## What must be true

- [ ] **A later layer overrides an earlier one by id**, per row, and what overrode what is PUBLISHABLE
  -- a declaration nobody can trace is a declaration nobody can debug
- [ ] **A layer that names an id nothing declares is ADDING**, not failing, because that is how a mod
  adds a thing
- [ ] **A layer's own layers are refused**, one level, because a graph of overrides is a thing nobody
  can predict
- [ ] **The order is the declaration's and never the filesystem's**
- [ ] **A change set selects which declarations are active**, so one scenario carries variants without
  a second copy of itself

---

Closed -- all five demands stand:

- OVERRIDES BY ID, PER ROW: MergeLayer replaces a whole row whose IDENTITY matches -- and
  identity is the grammar's own Required column (kind by name, instance by id, asset by uri,
  vehicle by name), the same attribute 1484 made every element carry; a layer is a scenario
  file read by the SAME reader (one grammar, no dialect), path resolved relative to the
  scenario that orders it. PUBLISHABLE: every override, addition and inactive skip lands on
  Carried ("layer 'winter' overrode kind 'mug'").
- UNKNOWN ID ADDS -- that is how a mod adds a thing.
- A LAYER'S OWN LAYERS REFUSE, one level, by name.
- THE ORDER IS THE DECLARATION'S: layers merge in declared order; the filesystem never
  decides (and an INACTIVE layer's file is never even opened -- proven with a layer whose
  file does not exist).
- A CHANGE SET SELECTS: <layer set="winter"/> stands only when the root's active="..." names
  its set; unset layers always stand -- one scenario, variants, no second copy.

Proving tests: test/unit/scenario/ALayerOverridesAnEarlierOneById.cpp (merge semantics,
refusals, set algebra) and test/render/outshine/client/AModIsALayerTheScenarioOrders.cpp
(the file door end to end through Engine::Read, trace on Carried). Gate green.
