Type: task
Parent: 1480
Area: core
Tags: scope

**A table is declared data and a script reads it**

`Tables` are read and carried and nothing answers them. **This is where damage, loot, prices, recipes,
XP curves and dialogue conditions live** -- every number a game balances -- and it is the row that keeps
those numbers out of the engine.

## What must be true

- [ ] **A row is looked up by its first column** and the lookup is O(1) after stand-up
- [ ] **A cell is typed by its COLUMN and not by its spelling**, declared once per column, so `"13"`
  reads as a number where the column says number and as text where it says text
- [ ] **A lookup that finds nothing answers so**, and never a zero -- a missing damage row and a damage
  of zero are different facts
- [ ] **A table is reachable from a script's host** and from a surface's, because a HUD showing a price
  and a script charging it must read one number
- [ ] **A row count is bounded and the bound is declared**
