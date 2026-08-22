Type: issue
Area: core
Tags: scene

**The scene door refuses by construction what today only convention prevents**

Review of `include/outshine/{Register,Store,Column}.h`, `src/scene/Store.cpp`,
`src/clients/Assembly.cpp` after board:1583 slices 1-5. The store's intrusive machinery is
correct -- `ErasePair`'s unlink-both/relink-moved (`src/scene/Store.cpp:162-177`) survives the
swap, `ARemovalKeepsEveryIndexTrue` proves it. Four places rely on convention where the house
style demands construction:

1. **Acyclic without Exclusive is representable and silently unchecked.** `Store::Link`'s cycle
   walk (`src/scene/Store.cpp:203-211`) follows `TargetOf` -- the FIRST target only. Today both
   acyclic relations (IsA, ChildOf) are also Exclusive, so the walk is complete; the moment a
   rule row declares `Acyclic` without `Exclusive`, cycles through a second target pass without
   refusal. One `static_assert` beside `kRules` in `include/outshine/Register.h` (Acyclic
   implies Exclusive) makes the invalid table unspellable.

2. **A parameter is shadowed by a bool of the same name.** `src/clients/Assembly.cpp:15`:
   `const bool drives` shadows `Column<Drive> &drives`; the column is used forty lines later.
   Legal, misread-prone -- and `-Wshadow` is not in `WARN` (`test/run.sh:15`). Rename the local,
   and weigh `-Wshadow` for the warning set.

3. **`Give` accepts duplicates into eight seats.** `src/scene/Store.cpp:105-114` appends without
   looking; eight identical tags exhaust the entity's capacity and change no answer `Has` gives.
   Refuse the duplicate (or fold it) with a text, like every other refusal.

4. **A column's capacity is a second spelling of the store's.** `Column::Open(of, capacity)`
   (`include/outshine/Column.h`) lets the two diverge; a `Put` beyond the column then fails with
   a bare `false` and no refusal text. The store knows its size -- publish it and let the column
   derive, so the mismatch is unrepresentable.
