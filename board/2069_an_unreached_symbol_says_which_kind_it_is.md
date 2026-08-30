Type: feature
State: open
Area: engine
Tags: measured, gate

# An unreached symbol says WHICH KIND it is, because two kinds are counted as one

**Benchmark** — Unreal: `UnusedHeaderTest` and the build's IWYU pass report unreferenced code, and
a `UFUNCTION` reachable only from Blueprint is EXEMPT by declaration rather than by argument -- the
reflection marker says "a caller exists that the linker cannot see". RAGE: `__forceinline`
libraries and the data-driven registries are declared as entry points so the map file does not
report them. **Both agree**: the tool cannot tell dead code from an unwired entry point, so the
CODE says which it is.

## Measured -- one night, three of them, and all three were defects

`test/unreached-baseline` records 184 symbols nothing in the archive calls. It is shrink-only and
that policy is right: old code is repaired at the pace it is touched. What the number cannot do is
tell the two kinds apart, and this tree's commonest defect (CLAUDE.md) lives entirely in the
second one.

| symbol | what the count said | what it WAS |
|---|---|---|
| `ClassStructure::Words()` / `Bytes()` | nothing calls it | the packed, pointer-free GPU form of the classification, built for a consumer never written (board:2064) |
| `VegetationTemplates::Rows()` / `RowBytes()` | nothing calls it | the per-template ground albedo WITH the sward already mixed in. `Picturing` was reading a different table with an index that did not belong to it, and every picture this tree ever made came out desert-brown (board:2068) |
| `Stage::AutoExposure` | not in this count at all -- it is a catalogue row, not a symbol | a declared render stage with no executor. Found only because `stage_without_a_body` was added to log it |

The third is the point: the count did not even SEE it. A capability can be unreached in more than
one alphabet -- a symbol, a catalogue row, a resource, a declared field -- and there is one guard
for one of them.

## What will be true

- [ ] The unreached list separates **DEAD** from **UNWIRED**, and the source says which: a symbol
      kept deliberately without a caller carries a declaration to that effect, so everything else
      in the list is a question somebody has to answer. The count then means something a reader
      can act on instead of a number that may only fall.
- [ ] The same question is asked of the OTHER alphabets, because being unreached is not a property
      of symbols: a catalogue row with no executor, a `Resource` no stage writes, a public field no
      reader reads. `stage_without_a_body` already does this for one of them and found five.
- [ ] Negative control: a function added with no caller and no declaration appears in the list;
      the same function declared as deliberately callerless does not; and a catalogue row whose
      executor is deleted is reported by the second guard.
- [ ] Measurement that shows this is wrong: how many of today's 184 are UNWIRED rather than dead.
      If the answer is nearly zero, the two kinds did not need separating and this item is
      withdrawn. Three were found in one evening by hand, so the estimate to beat is not zero.
