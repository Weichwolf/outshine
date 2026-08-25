Type: task
State: open
Area: test
Tags: corpora, invariants

# The corpora survey names three, and each names a gap it closes

`test/CORPORA.md` is the survey: for every capability TARGET demands, which ESTABLISHED corpus
asserts that problem class, and is it reachable. Two structural findings came out of it that the
brief did not ask for and that decide how the list is read:

**Effort has two halves.** FETCH is a pinned URL and a hash — cheap for almost everything worth
having. REACH is priced by board:1879: `test/` may only go through `include/`, and the door is
two headers. A corpus whose fetch is trivial and whose reach is not is not a cheap row.

**Four grades separate what a corpus HOLDS**: SPEC (a standards body states the answer), TRUTH (a
measurement or computation carried to more digits than we hold), SNAPSHOT (another
implementation, frozen — agreement, never correctness), INPUT (nothing is supplied; it proves
survival only). Most of what graphics calls a test corpus is INPUT.

## The three, in order

| | corpus | what it closes |
|---|---|---|
| 1 | glTF-Validator, 372 golden report pairs, Apache-2.0 | glTF is the only content surface. The 151 render cases prove what we do with a CORRECT asset and nothing proves what we do with a broken one — and refusal-with-a-reason is first class here (`std::expected`). Rides on `prepare.py` with no new job kind |
| 2 | GeographicLib GeodTest + TMcoords, CC0, DOI-pinned | *One world space*. `src/core/Geodesy.h` and `Mercator.h` carry Munich–Hamburg, every tile key and every building placement, and NOTHING proves one function of it. 50-digit reference, so the corpus cannot be the thing that is wrong |
| 3 | Kider's measured sky via `ebruneton/clear-sky-models`, BSD | Three medium stages are green in CURRENT *and* TARGET with no oracle at all — Cycles models no atmosphere. See the box below |

JTS TestBuilder lost third place to a measurement, not a preference: 129 files of SPEC-grade
expected WKT, dual EPL/EDL — and `grep` over `src/ground` and `src/generators` finds no buffer,
no overlay, no union, no point-in-polygon. A corpus for operations the engine does not perform
proves nothing.

## The magic number the survey walked into

```
src/render/stages/ParticipatingMedium.h:18
  float RayleighScatteringPerKm[3] = {0.005802f, 0.013558f, 0.033100f};
```

`grep` finds those digits in no board item. Under *every number carries its origin* they are
undeclared constants on the frame path, and they are exactly what corpus 3 would give an origin.

## What will be true

- [ ] The three are fetched, prepared and run, each as a SCENARIO with its oracle.
- [ ] `RayleighScatteringPerKm` cites where it comes from, or is derived from a corpus that does.

## Comments

- 2026-08-25 — `test/CORPORA.md` landed at `bf41233e` citing this number before the item existed;
  the item is written to the survey rather than the other way round.
