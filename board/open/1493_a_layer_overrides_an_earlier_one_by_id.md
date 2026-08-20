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
