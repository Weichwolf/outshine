Type: feature
State: active
Area: world, generators, test
Tags: infrastructure, osm, opendrive, corpora, measured, benchmark

# The road is derived in OPENDRIVE's four profiles, and a surveyed motorway scores it

**Benchmark** — RAGE and Unreal both AUTHOR their roads, so neither faces this and neither can
settle it. **CARLA is the admissible third body** on this tree's own rule: open, readable, the
reference for the driving simulation outshine is also going to be, and it meshes ASAM OpenDRIVE
rather than inventing a road model. **OpenDRIVE is therefore taken as the STRUCTURE** -- not as a
file format this tree reads or writes, but as the decomposition a road has:

    planView          the reference line          line | arc | SPIRAL | poly3 | paramPoly3
    elevationProfile  z along the arc length      cubic in s
    lateralProfile    superelevation / crossfall  cubic in s
    lanes             laneSection -> lane width   cubic in s, per lane, with a TYPE

Those are this tree's own four steps -- plan, profile, grade, sweep -- under the names a driving
simulator already owns. **No XML enters the engine.** The engine keeps deriving from OSM; the .xodr
lives in `test/`, in Python, as the ORACLE'S format.

## The corpus, fetched and verified rather than cited

`tum-gis/opendrive-testfeld-a9` -- two sections of the A9 Autobahn, surveyed by **3D Mapping
Solutions** and published by the MDM portal, mirrored by TUM because the Mobilithek migration broke
the original download. **GeoNutzV**, and the licence PDF ships in the repository.

    2017-04-04_Testfeld_A9_Nord.xodr    10 808 489 B
    2017-04-04_Testfeld_A9_Sued.xodr     9 766 787 B

Measured in the Nord file:

    OpenDRIVE 1.4         geoReference +proj=tmerc +lon_0=9 +k=0.9996 +x_0=500000 +datum=WGS84
    roads              85     every one type="motorway"
    planView geometry 907     paramPoly3
    elevation         528     420.95 .. 570.07 m, p50 526.75
    superelevation    606     -4.18 .. +5.62 deg, p50 +1.84 per cent
    laneSection       213
    lane            1 129     width a: 0.50 .. 23.10 m, p50 3.07 (mixed types, needs filtering)
    junction           21
    object          1 389     signal 544
    first geometry at 48.79 N, 11.46 E

**It is a real motorway that is ALSO IN OSM**, which is the whole point: the same kilometres exist
as the input we derive from and as a measurement someone else made. That is what no synthetic
corpus can be. `carla-simulator/opendrive-test-files` is RoadRunner-generated and therefore
FICTIONAL -- a snapshot of a generator, never a fact about the world -- and SUMO's OSM import tests
prove agreement with SUMO. Neither grades a reconstruction.

**And `asam-ev/qc-opendrive` (MPL-2.0) is a separate, later row worth its own fetch**: 181 PAIRED
valid/invalid cases, which is a corpus that ships its own NEGATIVE CONTROLS -- the thing this tree
most often lacks. It grades rule conformance rather than reconstruction, and it costs an XML reader,
so it is not this item.

## THE ENGINE MAY NOT KNOW A ROAD OR A HOUSE, and the violation is one file

The rule is the owner's and it is sharper than "the derivation is misplaced": **outshine's engine
must not know a street, a building, or any other thing a world happens to contain.** Its vocabulary
is body, mesh, material, instance, tile. A generator knows roads -- that is what generators are FOR,
and `CLAUDE.md` already carves out the exception in as many words: a generator's whole job is to
make one concrete thing. It hands the engine a FINISHED result.

Measured across the tiers, by files naming `street`/`road` and `building`:

    src/render/**                        0        0     <- already holds the rule, entirely
    src/engine/Asset.cpp                 0        1
    src/engine/Engine.cpp                1        0
    src/engine/Advancing.cpp             1        1
    src/engine/EngineHeld.h              1        2
    src/engine/Picturing.cpp            90       69     <- the violation, and it is one file
    src/world/ground/**                 12 files with the OSM readers
    src/generators/**                   12       15     <- where it belongs

`src/render` proves the rule is reachable rather than aspirational: one tier down it already holds
at zero. **The number that must go to zero is the engine tier's**, and 159 of its 167 mentions are
in `Picturing.cpp` alone.

The OSM readers under `src/world/ground/` move with it. `src/generators/reaches` already names
`world`, so a generator may keep reading tiles and elevation from there -- what may not stay is the
SEMANTICS: a field that knows what `highway=motorway` means is a generator's knowledge, not a
world's, and the engine reaches `world`.

## THE MACHINERY ALREADY STANDS, AND I BUILT A SECOND ONE BESIDE IT

**This item was filed as "build an OSM -> OpenDRIVE generator" and that was wrong.** `board:1499`
has stood open for a long time saying, in as many words, "the mechanism is ASAM OpenDRIVE's: the
reference line's plan view is a sequence of line, arc and spiral; elevation along `s` and the roll
angle are sequences of cubic polynomials". It is not a plan. It is BUILT:

    src/base/curve/ and src/generators/path/          1 497 lines
      ReferenceLine.h        enum class Curve { Straight, Arc, Spiral }        <- planView
                             Placed{ CurvaturePerM, CurvatureRatePerM,
                                     Slope, SlopeRatePerM,                     <- elevationProfile
                                     BankRad, BankRatePerM }                   <- lateralProfile
                             Knot{ AlongM, Value, RatePerM }                   <- the cubics
                             Segment{ Shape, EntryCurvature, ExitCurvature }   <- the clothoid
      Ribbon.h               Sweep(line, Section{HalfWidth, Shoulder, Thickness}, from, to, step)
      Carriageway.h          Stand / StandAt over the line
      Alignment · Fit · SpeedProfile

    src/generators/Infrastructure.cpp    45 lines, Proposes() returns 0, emits nothing
    src/engine/Picturing.cpp          2 401 lines, calls NONE of the above

**A complete capability no declaration reaches is `CLAUDE.md`'s named commonest defect here, and
writing a second one is named as the worst outcome available.** That is exactly what happened:
`RoadStation`, `RoadProfile`, `RaiseRoad`, `RaiseJunction` are a raw polyline with a constant
crossfall -- no curvature, no spiral, no bank, no `s`. The one already standing is strictly better
and is the one both references would recognise.

**So this item is NOT the derivation.** `board:1499` owns that and always did. This item is the
ORACLE and the SCORE -- the surveyed motorway, and the number that says how far the derivation is
from it. The two do not overlap and neither is a duplicate of the other.

## AND `Picturing` IS NOT A WORD EITHER REFERENCE OWNS

Neither Unreal nor RAGE has such a class. Unreal assembles in `UWorld` and `ULevel`, hands the
renderer an `FScene`, and keeps GENERATION outside both, in PCG. RAGE has `CGameWorld`,
`fwSceneGraph` and `gtaDrawable`, with the map data a pipeline that feeds them. `CLAUDE.md`: **a
name is a promise**, and a word that means nothing in either reference spends a reader's knowledge
just as surely as a word that means something else -- they must learn a new noun for a thing both
references already name.

**It needs an invented name because it is not one thing.** 2 401 lines that DERIVE and then
ASSEMBLE. Once the derivation leaves for the generators, what remains is exactly what both
references do name -- the assembly of a world for the renderer -- and it should take that name
rather than keep one coined to cover a mixture.

## THE DERIVATION IS A GENERATOR, and today it is in the wrong tier

Measured, and it is the structural finding of this item:

    src/engine/Picturing.cpp        2 401 lines   plan, trim, ramps, decks, clearances, junctions
    src/generators/draw/RoadMesh.cpp   237 lines   the sweep, and nothing else

**Everything that GUESSES is in the engine and everything that only extrudes is in the library.**
That is backwards. `CLAUDE.md` puts the generators in their own tier with their own door -- a client
registers its own beside them and the tier links with NONE of the engine behind it -- and a
heuristic that turns a public map into a road is exactly what a generator is for. It takes data and
makes one concrete thing, which is the one place this tree allows a generator to be concrete.

So the derivation moves behind `include/Generate.h`: OSM ways and a terrain sampler IN, the four
profiles OUT, and the sweep reads the profiles instead of re-deriving them. The engine keeps
placing and drawing. **This is what makes the corpus reachable at all**: a score that had to boot a
renderer to ask what gradient we inferred would be a score nobody runs.

## THE SCORING, and it is the Khronos render corpus's shape

`test/scripts/render_corpus.py` already solved the hard half of this and its docstring records what
it cost: `PointLightIntensityTest` drew a COMPLETELY BLACK FRAME against an oracle of six lit boxes
and scored **57.6729 per cent** -- better than the 11.3325 per cent it scored when it drew the
subject in the wrong place. **A metric that rewards doing nothing is a metric being read without its
picture.** The fix there was the denominator: every pixel lit in EITHER image.

The same trap, translated exactly:

| Khronos | here |
|---|---|
| a case is one glTF asset | a case is one **`road`** -- 85 in A9 Nord |
| a pixel | a **station**, one sample per metre of arc length |
| the denominator is every pixel lit in EITHER picture | the denominator is every station where EITHER side has a carriageway |
| a pixel black in both agrees by construction, so it is not counted | a station where neither has road is not counted |
| within `kMostDelta` of 255 | within a tolerance DECLARED PER QUANTITY, with an origin |
| held when agreement >= `kLeastAgreeing` | a road is held at the same bar |
| the corpus number is the COUNT HELD | the corpus number is **roads held**, and it may only RISE |
| the worst pixel is REPORTED and never gated | the worst station is reported and never gated |

**Not scoring where we built nothing is the same defect as scoring a black frame.** If the A9 exists
in the oracle and this tree lays no carriageway there, those stations count against us; if we lay
one where the oracle has none, they count too.

**100 per cent is not the target and will not be reached** -- OSM does not carry lane markings, and
osm2cdr's own page says an export "is always an approximation through heuristics". The number that
means something is HOW MANY ROADS HOLD, and that it never falls.

## THE SCORE IS BUILT AND BOTH CONTROLS BEHAVE

`test/scripts/score_opendrive_a9.py` implements the shape above and was proven on the oracle against
ITSELF before anything of ours was fed to it:

    positive control   Nord against Nord    85 of 85 road(s) held
    negative control   Nord against Sued     0 of 85 road(s) held

**The negative control is a sharp one** and that is why it was chosen: two sections of the SAME
motorway, surveyed by the same company in the same year to the same standard, differing only in
where they are. A metric that cannot separate those separates nothing. `CLAUDE.md`'s trap -- a
control that passes proves nothing -- is what this answers.

The tolerances, each declared with an origin rather than tuned:

    width          1.875 m       half a lane of 3.75
    gradient       0.5 per cent  a difference, so datum-free
    superelevation 0.5 per cent  a difference, so datum-free
    a station matches within 8 m; beyond that the two are different roads
    a road is HELD at 90 per cent of the stations either side carries

**Absolute height is not scored and will not be until a DGM stands beside it.** The A9 is surveyed
on WGS84 and this tree's ground comes from its own tile source; a height difference would measure
the geoid rather than us, and fitting an offset would remove exactly the error being looked for.

## The quantities, each with its own tolerance and its own origin

The tolerance is a DECLARED number with a derivation, never a dial turned until the score looks
good. Each starts UNSET and is written down with its reason before the first score is believed.

| quantity | datum-free | compared against | tolerance's origin |
|---|---|---|---|
| carriageway present | yes | the oracle's lane extent | -- |
| lateral offset from the reference line | yes | `planView` | OSM node accuracy, which is the input's own error and not ours |
| carriageway width | yes | sum of `driving` lane widths | half a lane |
| **gradient** dz/ds | **yes** -- a difference | `elevationProfile` derivative | the class's `maxGradient`, already declared |
| **superelevation** | **yes** | `lateralProfile` | see below |
| absolute elevation | **NO** | `elevationProfile` | not scored until the datum is settled |

## THE FIRST NUMBER THE CORPUS PRODUCED, and it corrected the item that asked for it

The table is flattened once by `test/scripts/opendrive_oracle.py`: every road walked at one metre,
plan, elevation, superelevation and the summed width of the `driving`, `entry` and `exit` lanes.

    248 road(s), 117 813 station(s)      Nord 85 / 57 547, Sued 163 / 60 266, none refused
    station spacing   p01 0.9991  p50 1.0000  p99 1.0009  max 1.0017 m

**The spacing is the table checking itself**: `paramPoly3` with `pRange="arcLength"` should advance
one metre of curve per metre of `s`, and it does, to under two millimetres. A plan evaluation that
were wrong would show here first.

Then the question this item was filed on. **`kCrossfall = 0.025` is SET in `RoadMesh.cpp` and
nothing measures it.** The first version of this item quoted the A9's superelevation median as
**+1.84 per cent** and reasoned from it -- and that number is MEANINGLESS: it is the SIGNED median
over every station, where a left-hand curve cancels a right-hand one. The comparable quantity is the
MAGNITUDE, and it has to be taken where the camber is not competing with banking, so it is binned by
the radius computed from the heading change between consecutive stations:

    radius band        stations   p50 |superelevation|
    straight R>5000      16 597          2.76 per cent
    R 2000-5000           4 296          3.41
    R 1000-2000          22 465          3.15
    R 500-1000           11 213          4.05
    R<500                 2 806          5.61

**On the straight the A9 measures 2.76 per cent, and this tree sets 2.50.** So the constant is LOW
rather than high, which is the opposite of what the first reading said. It is also close enough that
the right repair is to give it an ORIGIN rather than a new value: 2.5 per cent is the German design
minimum for drainage and the built road exceeds it, which is what a built road does.

And the monotone rise across the bands is `e = v^2 / (127 R) - f` showing up in the measurement --
the rule the goal asks for, carrying its own reason, visible rather than asserted.

## What will be true

- [x] The two .xodr are fetched by `test/scripts/fetch_opendrive_a9.py` into the system temp root
      and pinned by `sha256` into `test/opendrive/a9/manifest.json` -- nothing in the tree.
      **Done**: Nord `473c6db8...`, Sued `28318b98...`, GeoNutzV_130319.pdf `18ff7cc4...`, at commit
      `e75ee549`
- [ ] **The second implementation is DELETED**, not kept beside the first: `RoadStation`,
      `RoadProfile`, `RaiseRoad` and `RaiseJunction` go, and what `Picturing.cpp` does today is
      done by `ReferenceLine` + `Ribbon::Sweep` through `Infrastructure`'s door. Deleting is
      board:1499's work; this item only has to be able to SCORE whichever one stands
- [ ] What remains after the derivation leaves is named for what Unreal and RAGE call it, and
      `Picturing` is retired rather than renamed in place
- [ ] **The engine tier names no street and no building.** 90 + 69 in `Picturing.cpp` and 1-2 each
      in four more files today; `src/render` already stands at zero, so the target is not a wish
- [ ] The oracle is flattened ONCE into a station table, so the score does not re-read 10 MB of XML
- [ ] The client hands out our own stations for a declared window, headless and deterministic --
      two runs, byte-identical rows
- [x] `test/scripts/score_opendrive_a9.py` prints per road: the agreeing fraction, the worst
      station, and HELD or APART -- then `N held` and the bar
- [x] Negative control: scoring Nord against Sued goes red -- 0 of 85 against 85 of 85 for the
      positive. Both were run before anything of ours was scored
- [ ] The held count is a baseline that may only RISE, and it is quoted in the item that spends it
- [ ] The work does not stop at "it runs": each quantity is developed against the corpus until its
      DECLARED threshold is undercut, and the threshold is written down with its origin BEFORE the
      first score is read. A threshold moved to make a score pass is the falsification this tree
      names in `CLAUDE.md`
- [ ] Measurement that shows this is wrong: the score rises while the picture gets worse, or a road
      holds while laying no carriageway at all -- which is what the union denominator exists to stop

## What this does NOT cover

Lane-level geometry, markings, signals and junction CONNECTIONS. The oracle holds all four (1 129
lanes, 544 signals, 21 junctions with lane-to-lane mapping) and this tree sweeps ONE carriageway per
way. They are their own items and the driving simulation will want every one of them.

It also does not cover anywhere that is not this motorway. A9 Nord and Sued are 85 roads of German
Autobahn: no urban junction, no residential street, no track. A rule that holds here has been shown
to hold on the easiest geometry a road network has.
