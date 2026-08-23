Type: task
Parent: 1480
Area: scenario
Tags: scope

**What a scenario declared as state survives the process**

`State` is a list of what to persist and nothing writes it. **A save is not a park** (`board:1485`): a
park keeps a scenario warm in memory, a save survives the process and the machine.

## What must be true

- [ ] **Only what the scenario DECLARED persists**, so a save is a function of the declaration and a
  scenario that declares nothing saves nothing
- [ ] **A save names the scenario and its version**, and a save from a version this engine does not
  know is a refusal quoting both
- [ ] **A save is deterministic**: two saves of one state are the same bytes, or nothing can diff them
- [ ] **Loading a save is standing a scenario up and then applying the state**, never a second stand-up
  path -- one arrival route, which is the rule the compositor already follows for parts
- [ ] **The size is bounded and the bound is declared**, and reaching it refuses naming what would not
  fit

---

Closed -- the five demands stand:

- ONLY WHAT THE SCENARIO DECLARED persists: Engine::Save walks the <persist what=
  "instance.trait"> rows against the resolved traits column; a scenario declaring nothing
  refuses ("an empty promise"); a row naming nothing assembled refuses ("would load as a
  lie").
- NAMED AND VERSIONED: line one is "outshine-save 1 <name> <version>"; a foreign save
  refuses QUOTING BOTH sides.
- DETERMINISTIC: rows sorted, %.17g -- two saves of one state are one byte sequence, proven
  by equality.
- ONE ARRIVAL ROUTE: Restore requires the normal stand-up first and applies through the same
  traits column (Put by handle); it never stands anything up itself.
- BOUNDED: kMostSaveBytes [SET] 1 MiB with its reasoning; overflow refuses naming the size.

This also pays 1665's debt: the park bound's refusal now has the savefile beneath it that
Bethesda's eviction assumed. Proving test:
test/render/outshine/client/ASaveIsAFunctionOfTheDeclaration.cpp. Gate green.
