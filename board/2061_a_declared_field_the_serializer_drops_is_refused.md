Type: defect
State: open
Area: scenario
Tags: door, measured

# A field the serializer drops is REFUSED, not written off as agreement

**Benchmark** — Unreal: `FArchive` serialises through the SAME property reflection that
defines the type, so a field cannot exist and be unwritten; `UStruct::SerializeTaggedProperties`
walks the properties, not a hand-kept list. RAGE: `parStructure` is generated from the type's
metadata and a member without a tag does not compile into the structure. **They agree**, and
the matter is closed: neither hand-maintains a writer beside its type.

## What it looks like here

`ScenarioWrite.cpp` is a hand-written list of fields beside `Scenario`, and
`outshine/places/ScoreEachPlaceWrittenAndReadBack` compares `write -> read -> write`. That
comparison is BLIND BY CONSTRUCTION to a field both sides drop: the writer never emits it, the
reader never sees it, the second write matches the first, and the case is green while a
declared capability is silently gone.

## Measured

Found the day `Scenario::Clock` was first declared by the client (board:1868). `ScenarioWrite`
emitted no `<clock>` at all; `roundtrip` reported `0 place(s) apart` across all seven places
with the sun's hour missing from every one of them. The hole was found by grepping the writer,
not by the case that exists to find exactly this.

The clock is now written and the scenario grew by the 59 bytes the element spells. The DEFECT
is not the missing clock -- it is that nothing refuses the next one.

## THE BLINDNESS IS MEASURED, not argued

`roundtrip` was run with `<clock>` removed from the writer -- one `if (false &&` on
`ScenarioWrite.cpp`'s clock branch, the whole 59-byte element gone from every place:

    ZurichPlan   4479 -> 4420 byte(s)
    0 place(s) apart

Seven places each lost a section and the check that exists to find exactly this reported no
difference. The reasoning in this item is now a measurement.

## THE GUARD, and it goes red on the same edit

`test/scripts/grammar_vs_writer.py` is the mirror of `grammar_vs_reader.py`: every child the
GRAMMAR declares must be a child the WRITER emits. Both now read the grammar through
`test/scripts/scenario_grammar.py`, because a second copy of that parser would be the very drift
they guard against -- and its adjacent-literal join is the trap that once truncated it.

    72 child(ren) the grammar declares, 8 the writer writes back, 64 it cannot

**Eight of seventy-two.** A scenario may declare physics, audio, generators, compositors, events,
regions, volumes, surfaces, tables, instances -- and the engine can hand back none of them. The
roundtrip is green because the places only use those eight AND because it compares the writer with
itself; either alone would have hidden this.

64 carries a baseline that may only fall, on the tree's own discipline: a strict count over a grown
tree is red on day one and switched off in the first week.

**Negative control, on the same edit that left `roundtrip` green:** with `<clock>` spelled
`"<cloc" "k"` the checker reads `65 it cannot -- the baseline allows 64`, names `clock` in its list
and exits 1. Red where the identity was green.

And `roundtrip` now prints what it does NOT cover, with that measurement in it, so it can no longer
be read as a proof of the serializer.

## What will be true

- [ ] A field added to `Scenario` and not to `ScenarioWrite` FAILS something. HALF DONE at CHILD
      granularity: a `<child>` the grammar declares and the writer drops is refused. A field added
      to an existing child as an ATTRIBUTE still passes, and that is what the box still wants --
      the checker says so where it prints
- [x] `ScoreEachPlaceWrittenAndReadBack` states where it prints what it does NOT cover -- the
      `roundtrip` verb prints it, with the 59-byte measurement that proves it
- [x] Negative control: `<clock>` removed from the writer goes RED on the guard and stayed green on
      the identity, which is the whole point of the guard
