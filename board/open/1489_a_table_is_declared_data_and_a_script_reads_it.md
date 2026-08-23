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

---

Progress -- four of five boxes stand: src/scenario/Tables.{h,cpp} is TableBook, the one
home of every number a game balances. Rows keyed by their first column at stand-up
(unordered index -- no per-read scan); a cell is typed by its DECLARED column
(<column type="number">, the third type refuses), '13' reads as number and text by column,
never by spelling; a lookup that finds nothing answers NULL, never zero -- a missing damage
row and a damage of zero are different facts; two rows under one key refuse ("a lookup with
two answers has none"); kMostRows [SET] 4096. Proving test:
unit/scenario/ATableAnswersByItsFirstColumnAndItsColumnsType.cpp. Remaining box: reachable
from a script's and a surface's HOST -- that is board:1448's capability surface, and this
book is what it will hand out.

---

Review 2026-08-23: two sharpenings before the host door opens. (a) The lookup path
allocates and scans: `Tables.cpp:65,68` build a `std::string` per query to feed the
`unordered_map`, and `:70-73` finds the column by linear string compare -- once scripts
read damage per tick this is alloc-and-search on the tick path. Demand transparent-hash
heterogeneous lookup (`find(string_view)`) and a column handle resolved once, not per
read. (b) `Tables.cpp:22` bounds the Types copy by `min(Types, Columns)` -- tolerance for
a mismatch the one producer (`ScenarioRead.cpp`, one Type pushed per column) can never
emit; dead tolerance is a second truth, so either refuse the mismatch loudly or make the
invariant structural (one vector of {name, numeric} in `Table`).
