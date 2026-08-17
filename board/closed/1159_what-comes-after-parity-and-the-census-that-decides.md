Type: issue
Area: render
Tags: instrument, scope

**What comes after parity, and the census that decides it**

**The decision is which pillar is worked next, and for how long parity stays the frontier.** It is a scope
call across the whole vision, so it is not mine to take — but it is measured here rather than argued, and
a recommendation stands at the end.

## The census, by instrument and not by impression

| | measured | instrument |
|---|---|---|
| library | **30 798 lines, 259 files, 9 directories** | `find src -name '*.h' -o -name '*.cpp'` |
| board | **1 156 items — 1 130 open, 9 active, 17 closed** | the directories |
| **ready to start** | **1 012** — open items whose every `Depends:` is closed | the board's own ready query |
| of those, content inventory | **891 are `Area: generators`** — species forms, roof types, vehicle classes | `Area:` header |
| ids cited from `src/` or `test/` | **29 distinct**, of 1 156 | `git grep -ho 'board:[0-9]\{4\}' -- src/ test/` |
| **the vision's own 33 features, `0040`–`0072`** | **0 citations. 143 ticked boxes, 400 unticked.** | the same query, per id |

**The citation band is the finding.** Every cited id is in one of two groups: documentation hygiene
(`0019` `0031` `0037` `0038`) and **the glTF → corpus → oracle → parity band** (`0073` `0076` `0078`
`0079` `0082`–`0089` `0095` `0105`, then `1119`–`1138`). **The caveat was checked and it clears**: the
citation convention is *not* new — it is used on ids as old as `0019` — so zero citations on `0040`–`0072`
is a map of where the work went, not an artefact of when the convention started.

## What no suite can even spell, which is stronger than any grep

`test/run.sh`'s include sets are the layering, and they are a compile-time fact:

```
render) -Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/* -Isrc/clients
frame)  -Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/* -Isrc/clients
```

**No suite that brings up a device carries `-Isrc/world` or `-Isrc/generators`.** So **12 498 lines** —
`src/world` 6 587 and `src/generators` 5 911 — **have no spelling in any test that has a GPU**. This is
not *nothing greps them*; it is *they cannot be named*.

- `src/world` — 6 587 lines, 37 files: the tile pool, terrain grid, OSM fields, street/building/water
  fields — **carries one unit test, and that test asserts an include set** (`AGeneratorHasNoSpellingInTheStreamer`).
  **No test loads a tile, meshes a terrain, reads an OSM field or streams anything in.**
- `src/generators` — 68 files, **3 unit tests**, two of which execute (determinism, closed bark mesh).
- Four declared worlds exist — `test/outshine/mods/{ardeche,badwater,demo,preikestolen}/mod.json`, with lat/lon,
  eye height, UTC, wind, cloud and jitter — and **`git grep 'mods/'` finds no runner in `test/run.sh` or
  the `Makefile`.** The reader is unit-tested; **no declared world has ever been run.**

## Clause by clause. Where it is zero, it says zero

| clause | what exists, measured | home |
|---|---|---|
| world loaded from OSM | `src/world` 37 files incl. `OsmVector` `OsmField` `TerrainTiles` `TilePool`; **0 behavioural tests** | `0052` `0012` |
| providers · content store | `src/data` 25 files, `ContentStore` `WebTileSource` `TerrariumDem` `Transport`; **5 unit tests, host transports exist** — the most complete non-render pillar | `0069` `0063` |
| Ground as a field | `Ground.h` `GroundTable` `GroundSample` exist and are read by the compositors | `0054` |
| the three edges | **holding, and proven by the build**: four `unit/compile/*/…IsNotReachable` tests assert the negative directions | `0082` |
| **screen-space error as the one currency** | **absent.** `DrawSource::Draw(ground, placed, sink)` takes **no budget** and returns **no capability**; there is no `(kind, params, seed, rung)` key — `ContentKey` exists only in `src/data` for *fetched upstream bytes*; `ModelLadder` is used by **one** file | `0055` `0053` |
| the capability reply both ways | **absent** — no achieved error, no shortfall, no over-delivery anywhere | `0055` `0041` |
| the compiled render plan | **real and strong**: catalogue, prune, `static_assert`ed edges, alias, spliced attachment indices — **and 18 of 20 rows cannot execute** | `0030` `0096`–`0102` |
| Cycles as the oracle | **the tree's strongest asset**: 35 cases, five quantity passes, a picture bound of named terms, three-leg normal comparison, index passes | `0073`–`0095` |
| 720p60 as a distribution | **one** measurement: the shadow-ray price, p50/p95/p99 over 240 timed frames per arm, sanitiser-free — **on a subject on a card** | `0058` |
| one physics system | **zero.** `gravity` has **0** word-boundary occurrences in `src/` or `test/`; the only `physics` hits are comments saying it does not exist yet | `0059` |
| epoch-and-decay dial | **zero.** `decay` has **0** occurrences; every `epoch` hit is `CivilTime.h`'s **astronomical** epoch — a false friend, and the reason this census used word boundaries | `0050` + content items |
| LLM actors · the RPG | **zero.** `llm` `quest` `npc` `dialog`: 0 each. `actor`: **one** hit, in `core/io/Log.h` | `0060` |
| declarative scenarios | reader complete and unit-tested (6 tests); **suite has no members and no runner** | `0072` `0058` |

## The recommendation

**1. Do not set an `N of 35`, and that is a judgement rather than an evasion.** `board:1146` measured the
parity verdict's domain: **4.17 % of rendered pixels, 1 328 002 of 31 852 800, over 8 of 35 cases**, with
**27 cases comparing `declaredRadiance × baseColour(u,v)` on both sides**. A target of *N within the
bound* would therefore be a target on a texture-sampling suite. **The rule instead: finish what is in
flight — `1136` `1137` `1144` `1150` `1153` and their children — and open no new parity item unless it
blocks a world case.**

**2. The next pillar is `board:0058`'s unticked subject clause, and it is already written**: *a declared
world scene with terrain, vegetation and buildings, at 720p, over the orbit that already exists.* It needs
activating, not authoring. It is first because it is **the cheapest falsifier of the entire stack** — one
case turns 12 498 uncited lines into either evidence or a defect list, gives the fourth constraint its
first real number, and gives the scenario suite its first member.

**3. Then, in this order, with the reason each sits where it does:**

| | why here |
|---|---|
| **the request · budget · capability spine** (`0055`, `0040`–`0046`) | every pillar above it is *defined in its currency*. Second and not first, because building it before a world case measures anything is designing against an unmeasured cost — `Per.1`, `Per.6`, and this tree's own *no claims about performance without measurements* |
| **the content stages can execute** (`0030`) | a world case needs terrain, buildings and water rows that run; 18 of 20 cannot |
| **streaming · residency · the memory ledger** (`0048` `0052` `0040`–`0046`) | priced by the world case rather than argued before it |
| **physics** (`0059`) | zero lines. Its first question — what does a walk collide with — is answered by the world's own surface, so it stands after the world runs |
| **actors · the RPG** (`0060`) | zero lines, and correctly last: it needs a world to act in |
| **epoch-and-decay · audio · verbs** (`0050` `0062` `0061`) | zero system lines; each is dressing on geometry that must first exist |

**4. What carries, said explicitly so nothing is torn down.** The glTF reader, the corpus preparer, the
oracle relationship, the compiled render plan and the picture bound are the tree's real asset and the
reason its numbers can be argued at all. **`src/world` is unproven, not condemned** — 6 587 lines that
nothing has run is a different statement from 6 587 lines that do not work, and the world case is what
tells the two apart.

**5. The risk of this recommendation, named rather than discovered.** Activating a world case may show
`src/world` does not run at all, and the round becomes a repair marathon. **That is a finding and not a
failure** — and continuing on parity cannot discover it, because no suite with a device can spell those
lines.

**Filed and worked around, never waited on.** The in-flight parity items are ready today and blocked by
nothing here.

## DECIDED: parity stops being the frontier, and the world runs next

**DECIDED by the orchestrator, on authority the owner delegated.** *`board:1129` records "by the owner"
and that was literally true there; this was not, and the two records must stay distinguishable.*

- **Finish what is in flight — `1136` `1137` `1144` `1150` `1153` — and open no new parity item unless it
  blocks a world case.**
- **No `N of 35`.** `board:1146` is the reason: 27 of 35 cases never enter the BRDF, so a target on that
  count is a target on a texture-sampling suite.
- **The first item after the in-flight set is `board:0058`'s unticked subject clause**, activated rather
  than authored. Its first task is `board:1162`, which is **narrower than the clause on purpose** — see
  the attribution argument there.
- **The order after it**: the request · budget · capability spine (`0055`, `0040`–`0046`) → the content
  stages can execute (`0030`) → streaming, residency, ledger → physics (`0059`) → actors and the RPG
  (`0060`) → epoch-decay, audio, verbs. **The spine is second and not first, deliberately**: building it
  before a world case measures anything is designing against an unmeasured cost (`Per.1`, `Per.6`).
- **The named risk is part of the decision and not a caveat on it.** The world case may show `src/world`
  does not run at all and the round becomes a repair marathon. **That is a finding.** 6 587 lines nothing
  has run is a different statement from 6 587 lines that do not work, and no device-bearing suite can
  currently tell them apart.

**Where the work goes**: `board:1161` (the compile group that lets any suite spell the world) then
`board:1162` (the first case), under `board:0082` and `board:0058` respectively.
