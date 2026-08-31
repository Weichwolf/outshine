Type: feature
State: open
Area: test, generators, process
Tags: layering, plugins, measured, benchmark

# A corpus links the SMALLEST library that can answer it

**Benchmark** — Unreal: a module declares its dependencies in `Build.cs` and a test target links the
modules it names, so a test of PCG does not pull in the renderer. RAGE: the game layer links
`rage::` and not the other way round. **Both agree**, and both enforce it by making the wrong link
FAIL rather than by asking people to be careful.

## The owner's rule, and it is the right one

> The Khronos tests may use `outshine-client`; everything else links only the generator library.

That is not a convenience. It is the only way to PROVE where a capability lives: a score that can be
computed while linking `libgenerators.a` alone has shown that the derivation is in the generators.
One that needs the engine has shown the opposite, and no amount of prose about tiers can substitute.

## What is measured today

The build already gets the hard half right. `libgenerators.a` is DERIVED -- `linkreach.sh` walks
`liboutshine.a` and closes the generator archive over what the linker actually reaches -- and
`run.sh` says why in its own words: **"a claim that they WOULD link is weaker than an archive that
does"**. `TheGeneratorsLinkWithoutTheEngine` passes.

**And the corpora ignore all of it.** `test/run.sh` keeps a hand-written include set AND a
hand-written source list PER SUITE:

    run.sh:216-225    -Iinclude -Isrc/base/math -Isrc/base/geo ... per suite, by hand
    run.sh:279-284    the SOURCES per suite, by hand -- and
                      `harness/khronos/validator` lists src/engine/Picturing.cpp among them

The Makefile's own head warns against exactly this: **"THE LAYERING IS THE BUILD AND IT IS DECLARED
ONCE, in `src/<tier>/reaches` ... so this file keeps NO second map -- one went stale, left three
layers out of the archive and broke `make` at HEAD (board:1584)."** The Makefile keeps no second
map. `run.sh` keeps one, and it is large.

## THE SPLIT, and the one move that matters

Libraries follow the `reaches` graph rather than a new map:

| archive | tiers | why it is one thing |
|---|---|---|
| `libbase.a` | `src/base` | math, geo, format, spatial, io -- reaches nothing |
| `libworld.a` | `src/content` `src/world` | tiles, OSM fields, elevation, materials, sky, weather. DATA, no verbs |
| **`libgenerators.a`** | `src/generators` **+ `src/base/curve` and `src/generators/path`** | polyline -> `ReferenceLine` -> `Ribbon` -> a finished `Geometry` |
| `librender.a` | `src/render` `src/import` | the device side |
| `liboutshine.a` | `src/engine` `src/scene` `src/scenario` `src/sim` `src/ui` `src/audio` `src/host` `src/compositor` | the motor |
| `outshine-client` | `src/client` | the one product |

**`src/base/curve` and `src/generators/path` splits at its own seam, and the measurement decided where.** The first version of
this item said the whole directory becomes a generator. Two greps refuted it.

**It includes NOTHING but the standard library and its own headers** -- no tier at all -- so what
lives there is not generator work by dependency, and the question has to be asked file by file. The
test is this tree's own: does it MAKE one concrete thing, or is it a TYPE others share?

| file | what it is | where |
|---|---|---|
| `Angle.h` | `kTurn`, `Wrapped` | a primitive |
| `ReferenceLine` | the curve itself: `Lay`, `Rise`, `At`, `Placed` | a shared TYPE |
| `Carriageway` | `Stand` / `StandAt` ON that curve | a query on the type |
| `SpeedProfile` | `Envelope` over that curve | a plan, and a type |
| **`Fit`** | polyline **->** `ReferenceLine` | it MAKES one |
| **`Alignment`** | polyline **->** `Aligned` / `Laid` | it MAKES one |
| **`Ribbon`** | `ReferenceLine` **->** a mesh | it MAKES one |

And the consumers settle it:

    actor/mind/Course.h                   ReferenceLine.h            <- the TYPE only
    actor/mind/{Fly,Walk,Rail,Drive}.h    SpeedProfile.h             <- the TYPE only
    sim/Rigging.h                         SpeedProfile.h
    sim/CorridorLay.{h,cpp}               Fit, Alignment, Ribbon + the types
    sim/DriveAssembly.cpp                 Fit, Carriageway
    engine/Picturing.cpp                  Fit

**`actor/mind` needs no maker at all**, so the driving mind never reaches the generators -- which is
what the first version of this item would have forced, and it was wrong. The types go to `base`
where everything may see them; the three makers go to the generators; `src/sim/reaches` gains
`generators` for `CorridorLay`, and that is the whole cost.

**One naming debt goes with it and is NOT paid in the same move**: `Carriageway` is a subject noun
by board:2079's own list, and putting it in `base` puts a road in the most generic tier there is.
What the file actually holds is standing on a line at an offset, which is a law. Renaming it is its
own commit, because a blind rename over a word four types share is a trap this tree has already
paid for.

Moving them:

- lets the OSM corpus validator link `libgenerators.a` and nothing else, which is the owner's rule
  and the proof that the derivation lives there
- **ends the twin board:1499 measures**: the world path and the sim path stop deriving the same road
  twice, because there is one library that lays it
- costs `src/sim/reaches` one word. `generators` reaches `base content world`; `sim` reaching
  `generators` closes no cycle

## How a corpus declares what it links

Three kinds, and the kind is a property of the QUESTION rather than a preference:

| kind | links | because |
|---|---|---|
| render corpora -- Khronos glTF, `outshine/places` | `outshine-client` | the question is what a picture looks like, and a picture needs the device |
| derivation corpora -- OSM/OpenDRIVE, `outshine/geo` | `libgenerators.a` | the question is what we DERIVE, and if the answer needs the engine the answer is in the wrong place |
| language corpora -- test262, WPT | the reader's own tier | the question is what a string means |

## What will be true

- [ ] `src/base/curve` and `src/generators/path` is split: `Angle`, `ReferenceLine`, `Carriageway`, `SpeedProfile` become a
      base-tier curve that reaches nothing, and `Fit`, `Alignment`, `Ribbon` become generators.
      `src/sim/reaches` gains `generators`; `actor` gains nothing
- [ ] The per-suite SOURCE list in `run.sh` is derived from `reaches` the way the archive already
      is, so the second map goes
- [ ] The OSM corpus validator links `libgenerators.a` and nothing else, and its link FAILS if the
      derivation is not there. That failure IS the claim
- [ ] Negative control: moving one derivation back into `src/engine` breaks the validator's link.
      A validator that still links proves nothing about where the code lives
- [ ] `include/Generate.h` is what a generator builds against -- see below

## AND THE DOOR NEEDS TWO THINGS BEFORE A ROAD CAN STAND IN IT

`include/Generate.h` already declares the contract and it is the right shape: `Ask` in,
`Generates::make` once, a finished `Geometry` out, `Makers` as the registry, `writeGlb` so a result
can leave as a standard file. **Exactly one generator implements it** -- `src/generators/Structures.h`
-- and the rest are reached by internal machinery (`GeneratorSet`, `Yield`, `Occupy`, `Proposes`),
which is a SECOND interface beside the public one.

**And the one generator that DOES stand in it proves both gaps rather than disproving them.**
`Structures::make` opens:

    const double lat = ask.NorthM;
    const double lon = ask.EastM;
    ...
    plan.CornerAslM = {0.0, 0.0, 0.0, 0.0};
    plan.BaseAslM   = 0.0;

It reads two fields whose names end in `M` as DEGREES, and it builds at sea level because it has no
ground to ask. So the georeference is already being smuggled through the door in fields named for
metres, and the height is simply absent. A name is a promise, and this one is broken inside the
public header rather than behind it.

Two gaps explain why infrastructure never stood in the door:

    struct Ask { double EastM, NorthM, ExtentM; uint64_t Seed; };

1. **It cannot say where on Earth.** A window with no origin, and no ground beneath it. An OSM
   generator needs a georeference and a height; today it takes them from `Ground &` internally,
   which is past the door. The fix is an ABSTRACT sampler in the `Ask` -- the generator asks, the
   caller supplies -- so the tier never links the engine's terrain
2. **It carries no DECLARATION.** The goal says an inferred number lives in a table with its
   origin; with nothing in the `Ask` to carry one, the rule ends up in the generator's body, which
   is what the goal forbids in as many words

`Ships` then enumerates each generator, and its `static_assert` already refuses a blank or repeated
name.


## FOLDED IN

**2088 — `Picturing` dissolves into three.** Unreal: `UWorld` holds, `FScene` is what the renderer
sees, PCG generates outside both. RAGE: `CGameWorld`, `fwSceneGraph`, map pipeline outside. Both
agree: derive, hold, hand over. `Picturing.cpp` is 2 574 lines doing all three, which is why it
needed a name neither reference owns. The derivation moves first — it makes the other two small, and
this item's validator cannot link `libgenerators.a` alone until it has. Negative control: the places
suite draws the same pictures, digest for digest.

**2080 — the entry check cannot see a generated table.** `entries_vs_shaders.py` reads literals;
`FragmentArms.h` replaced them with a table, so 24 entries are defined and 4 named. The vertex arms
already had this treatment and the comment records it. Fix: exclude them and NAME what holds them
instead — the static_asserts. Also `test/lint.sh:74` runs the claims with `|| true`, so `make lint`
prints `14 FAIL` and exits 0.

**2081 — the map guard and the map disagree about what a map is for.** `CLAUDE.md` cites 6 resolvable
paths and the claim demands 20; the page was deliberately rewritten to be AIM and not state. The
resolution half passes. Position: the count goes, the resolution stays — but the owner set both, so
it is a decision, not a repair.
