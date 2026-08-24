Type: issue
Area: sim, core
Tags: layering, scope, prose
Regresses: 1598

# The library publishes numbers and the case judges them

`board:1598` closed on a rule this tree wrote for itself:

> *`say.Claim(true, ...)` -- a claim that cannot fail, pure narration | **the Sink claims belong
> to the CASES; systems publish numbers, cases judge** (board:1581's own table)*

The rule was applied to `Journey.cpp` and to nothing else. Measured at 2b2c2f69:

```
34 Sink::Claim sites in src/, carrying 5 812 characters of English
210 Sink calls in src/ in total
```

The library both evaluates the criterion and narrates it, and the narration is compiled into
the engine's rodata. This session added the largest one yet, at `src/sim/DriveAssembly.cpp:206`:

```cpp
say.Claim(found > 0,
      "**THE GRADE SEPARATIONS ARE FOUND WHERE THE SOURCE OMITS THEM.** The vector tiles "
      "carry two tag keys -- kind and rail -- so no bridge, tunnel or layer reaches this "
      "engine. What does reach it is OSM's own convention: two ways crossing AT GRADE share "
      "a node and two crossing grade-separated do not, and that survives the tiling because "
      "it is geometry rather than a tag");
```

Five lines of architecture essay inside a library translation unit. The owner directive that
bans `//` in `src/` bans this prose for the same reason -- it is explanation standing beside
code rather than in the board and the commit -- and it evades
`harness/claims/TheSourceCarriesNoCommentary` only because it is a string literal.

## And the claim itself is the engine judging its input

`found > 0` asserts that a fetched OSM extract contains at least one grade separation. Route a
drive through a region that has none and the ENGINE reports a failed claim about the WORLD.
That is a case's judgement, made in the library, about data the library does not own.

## The same block publishes one number twice

```cpp
src/sim/DriveAssembly.cpp:199   say.Number("junctions among them", (double)roads.JunctionCount(), "nodes");
src/sim/DriveAssembly.cpp:205   say.Number("junctions among the nodes", (double)roads.JunctionCount(), "nodes");
```

Two labels, one O(nodes) walk run twice, five lines apart, in a log a human reads as two facts.

## What will be true

- [x] `Sink` loses `Claim` and `Near`, or `src/` loses every call to them: the library publishes
      `Number` and `Say`; the case reads the numbers and asserts. A refusal the library owns is
      `std::expected` with its reason, not a boolean plus an essay.
- [x] No string literal in `src/` exceeds what a label needs -- the walk that bans comments in
      `src/` gains the same bar for narration in string form, with the label length it measures
      published.
- [x] `JunctionCount()` is published once.
- [x] Proving test: `harness/claims/TheSourceCarriesNoCommentary` extended to string literals in
      `src/`. Negative control: the paragraph above restored -> red, at its line.

**Closed.** `src/` holds no `Claim` and no `Near`. The 34 sites divided three ways:

| what it was | how many | where it went |
|---|---|---|
| a refusal wearing a claim's clothes -- `Claim(x, essay)` then `if (!x) return false` | 17 | one refusal with a short reason |
| a criterion evaluated in the library and narrated there | 15 | a published count, judged by the case |
| a load inside the predicate that returned nothing | 2 | it returns now |

The last two were the sharpest: `materials.Load(...)` and `widths.Load(...)` sat inside the
claim's own argument, so a missing width table produced a red claim and a drive that carried
on with an empty table.

`Corridor::Made` and `DriveProduct::Harvest` publish what the case needs to judge -- resolved
stations, holes, laneless and gradeless kinds, the narrowest half carriageway, the worst
gradient against the climb limit, features decoded, ways harvested, ways refused as not a
carriageway, the widest way still refused, nodes, junctions, crossings, route length against
the great circle. Every one of those was a number that existed only inside the string that
judged it.

`JunctionCount()` was published twice under two labels five lines apart, running the same
O(nodes) walk twice. Once now.

The label bound is the guard against the same prose returning through `Number`: 100 characters,
against a longest live label of 83 -- `[SET]`, and the measurement is in this item.

One consequence is worth stating because it looks like a loss: the driver case's trailer went
from **48 checks to 27**. The test sink counted every library `Claim` as a check, so 32 of
those 48 were the library judging itself and the case tallying along. What replaced them is 13
judgements the case makes on published numbers; the other 19 were refusals, and a refusal is
proven by the drive completing at all, not by a claim beside it.

`ACorridorIsLaidOverASyntheticRoute` read `quiet.Refused()` -- the count of FAILED CLAIMS
INSIDE THE LIBRARY -- to decide whether a 40 % climb was refused. `LayCorridor` returns false
now and the case reads the return value.

Proving test: `harness/claims/TheSourceCarriesNoCommentary`, 7 checks. Negative control: the
grade-separation paragraph restored as a `Claim` in `src/sim/DriveAssembly.cpp` -> FAIL, "FOUND
src/sim/DriveAssembly.cpp:204 .Claim( -- the library judges its own output". Gate 260/260,
driver 5/5 with the drive unchanged: 742.636 km, least clearance 0.160301892 m.
