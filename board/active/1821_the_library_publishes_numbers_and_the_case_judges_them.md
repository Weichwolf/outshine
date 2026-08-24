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

- [ ] `Sink` loses `Claim` and `Near`, or `src/` loses every call to them: the library publishes
      `Number` and `Say`; the case reads the numbers and asserts. A refusal the library owns is
      `std::expected` with its reason, not a boolean plus an essay.
- [ ] No string literal in `src/` exceeds what a label needs -- the walk that bans comments in
      `src/` gains the same bar for narration in string form, with the label length it measures
      published.
- [ ] `JunctionCount()` is published once.
- [ ] Proving test: `harness/claims/TheSourceCarriesNoCommentary` extended to string literals in
      `src/`. Negative control: the paragraph above restored -> red, at its line.
