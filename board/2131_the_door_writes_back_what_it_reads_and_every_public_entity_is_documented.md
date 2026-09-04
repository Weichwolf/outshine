Type: bug
State: open
Area: include, scenario
Tags: measured, gate, door
Supersedes: 2107

# The door writes back what it reads, and every public entity is documented

**Benchmark** -- Unreal: a `UPROPERTY` is serialised BOTH ways by the same reflection data, so a
field that loads is a field that saves, and every public API carries its doc comment because the
tooling refuses otherwise. RAGE: `parCodeGen` metadata declares each field once and the reader
and writer are generated from it. **Both agree**: a declaration that can be read and not written
back does not exist in the grammar, and a public entity without its documentation is not public.

## Where it stands, measured 2026-09-04, `make lint`

```
  76 children the grammar declares, 12 the writer writes back, 64 it cannot    target 0
  702 undocumented public entities in include/                                  target 0
```

Both are lint guards and both are RED, and neither had an item -- board:2093 holds the
clang-tidy count only. `Engine::writeScenario` exists so that read -> write -> read is a
counter-control a client can run; with 64 children the writer drops, the control proves the
writer and nothing else. And `Unacted()` in `EngineHeld.h` lists every section it CARRIES
without acting on, which is CLAUDE.md's loud failure made quiet: `layers`, `providers`,
`compositors`, `placements`, `kinds`, `instances`, `regions`, `doors`, `tables` (board:2107), and
`tables` and `buses` and `sounds` counted TWICE in that list.

## What will be true

- [ ] Every child the grammar declares is written back by `writeScenario`, or the row leaves the
      grammar -- per child, with the decision on the line
- [ ] Every section `Unacted()` carries is either acted on or refused at `declare`; the list of
      what is carried silently shrinks to nothing and the duplicate rows go
- [ ] `make doc` reports 0 undocumented public entities, and the Doxygen line on each is the
      unit and the promise, not the name restated
- [ ] Proving case: `roundtrip` over every place reads back byte-identical, which the writer gap
      makes impossible today
- [ ] Negative control: drop one child from the writer and the lint guard goes RED at 1

## What will show I was wrong

If a child cannot be written back because the engine holds it in a form the grammar cannot
spell -- a derived value -- then it should never have been a declaration, and the row goes rather
than the writer growing a special case.
