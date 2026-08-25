Type: task
State: active
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

---

Sharpened (review 2026-08-24): `src/scenario/Tables.{h,cpp}` was TOUCHED this hour
(e014531d, "three bool verdicts become one expected factory") and the sharpening filed
against it on 2026-08-23 was not paid — the commit converted `Stand` to `std::expected` and
walked past the lookup two functions below it:

```cpp
const auto held = Held_.find(std::string(table));        // src/scenario/Tables.cpp:65
const auto keyed = stood.ByKey.find(std::string(row));   // src/scenario/Tables.cpp:68
for (size_t at = 0; at < stood.Columns.size(); ++at) {
  if (stood.Columns[at] != column) { continue; }         // :71 — linear string compare
```

Unchanged since the sharpening: two heap allocations and one linear column scan per read, on
a path the item's own body describes as "damage, loot, prices, recipes" — per tick. The
`string_view` parameters at src/scenario/Tables.h:22-25 make the door LOOK converted, which
is the failure mode board:1621 names in its own words: *a `string_view` parameter that
allocates in the first line of the body is worse than the `const std::string&` it replaced.*

(c) — new, and the reason to do (a) and (c) in one sitting rather than bolting a transparent
hash onto the current storage: **the storage itself is wrong for a rectangle.**

```cpp
struct Stood {
  std::vector<std::string> Columns;
  std::vector<bool> Numeric;                  // src/scenario/Tables.h:38 — a proxy bitset
  std::vector<std::vector<Cell>> Rows;        // src/scenario/Tables.h:39 — a pointer chase
  std::unordered_map<std::string, size_t> ByKey;
};
struct Cell {
  std::string Spelling;                       // src/scenario/Tables.h:33
  double Value = 0.0;
};
```

A table is `rows × columns`, bounded at `kMostRows` = 4096 (:17). Today each row is a
separate heap block, no two cells of a column are adjacent, and a purely numeric column
still carries a 32-byte `std::string` per cell it never uses. One flat `std::vector<Cell>`
with a 2-D view (board:1769 owns the `mdspan` decision), a `uint8_t` type column, and the
`Spelling` split off into a side array for text columns, turns a per-tick damage lookup into
`base[row * stride + column]` on one cache line. Do it with the lookup, not after it — both
touch the same four lines.
