Type: defect
State: active
Area: engine
Tags: measurement, diagnostics

# A measure's name is its identity, and a clash is LOUD

**Benchmark** — Unreal: `STAT_` / `TRACE_CPUPROFILER_EVENT_SCOPE` names are declared once and the
declaration is what the viewer keys on; two stats cannot share a name because the declaration is a
symbol. RAGE: `PF_PAGE` / `PF_TIMER` counters are likewise declared, and the page they print on is
generated from the declaration. **They agree** -- a counter's name is a KEY, not a caption -- so
the matter is closed and this item is about the tree not doing it.

## What was measured

`Core::Ledger::Places` overwrites by name. Ten names in `src/` are published from more than one
call site:

    its most             3 sites   the shadow atlas's deepest texel, the step's worst
                                   millisecond, the picture's worst millisecond
    its least            2 sites   the step's best millisecond, the picture's best
    its highest          2 sites   two different rings' tallest vertex
    the ring's lowest    2 sites   the same two rings' lowest
    the eye, east/up/south         two BRANCHES of one function, only one runs -- legitimate
    how far along it the body has come   a seed of 0.0 then the real value -- legitimate

The first four are real: both sites execute in ONE advance-and-render, so the later write destroys
the earlier number and nothing says so. **`shots --measures Shibuya` prints one row named
`its most` reading 16.744 ms, and there is no way to tell from the page whether that is the
simulation step's worst frame or the picture's.** That number is what an item about the frame
budget's tail would be filed on.

**It has already cost a case its meaning.** `test/outshine/door/ScoreWhatTheShadowCasts.cpp` and
`ScoreWhatMovingTheEyeDoesToAShadow.cpp` both read `Measured(engine, "its most")` under the
printf `THE ATLAS HOLDS depths from %.3f to %.3f` -- they want the atlas's deepest texel and they
are handed a millisecond. Both are GREEN.

## What will be true

- [ ] every quantity `Places` publishes has its OWN name, and the four clashes above are gone
- [ ] a second `Places` call with one name inside ONE round is REFUSED and counted, and the count
      is published as `measures published twice in one round`
- [ ] the two door cases read the atlas depth they name, and their printed range is a depth again
- [ ] the negative control: restoring one clash makes the count read one and the claim go RED

## Why a runtime round rather than a grep

A grep over `Places("` finds all ten and cannot tell the four defects from the six legitimate
pairs -- a seed overwritten by its real value, and two branches of one function where only one
runs, are both CORRECT uses of the overwrite. Telling them apart needs to know whether both sites
executed in the same round, which only the ledger knows. An allow-list would encode today's six
and rot; the round counter encodes the RULE.

## What this does NOT cover

The 719 undocumented door entities and the names published from `test/` are outside this. So is
whether the measure page should be DECLARED rather than accumulated -- that is the larger answer
this tree's own invariant asks for, and it is not this item.
