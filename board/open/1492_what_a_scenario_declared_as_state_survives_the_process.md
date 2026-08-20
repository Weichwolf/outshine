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
