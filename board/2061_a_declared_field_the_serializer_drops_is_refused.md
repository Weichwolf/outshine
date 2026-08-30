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

## What will be true

- [ ] A field added to `Scenario` and not to `ScenarioWrite` FAILS something. The cheapest
      honest form is a check that walks `include/Scenario.h`'s members and requires each to be
      named in the writer, in the reader's grammar, or in a written-down list of what is
      deliberately not serialised -- with the REASON beside it.
- [ ] `ScoreEachPlaceWrittenAndReadBack` states where it prints what it does NOT cover, because
      today it reads as a proof of the serializer and is a proof of self-consistency.
- [ ] Negative control: a member added to `Scenario` and to neither writer nor list goes RED;
      the same member named in the list goes green.
