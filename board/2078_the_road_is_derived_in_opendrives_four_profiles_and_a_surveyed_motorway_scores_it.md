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

**The first thing this corpus attacks is a constant of ours.** `kCrossfall = 0.025` in
`RoadMesh.cpp` is SET and nothing measures it; the A9's superelevation has a median of **+1.84 per
cent** over 606 records. That median MIXES the straight road's roof camber with curve banking, so it
does not refute 2.5 per cent on its own -- separating the two is the work, and superelevation on a
curve is not a taste at all but `e = v^2 / (127 R) - f`, a rule that carries its own reason and
takes the design speed of the class, exactly as the goal demands.

## What will be true

- [x] The two .xodr are fetched by `test/scripts/fetch_opendrive_a9.py` into the system temp root
      and pinned by `sha256` into `test/opendrive/a9/manifest.json` -- nothing in the tree.
      **Done**: Nord `473c6db8...`, Sued `28318b98...`, GeoNutzV_130319.pdf `18ff7cc4...`, at commit
      `e75ee549`
- [ ] The derivation moves out of `Picturing.cpp` and behind the generators' door as a generator
      that emits the four profiles. The 2 401 / 237 split above is the number that must move
- [ ] The oracle is flattened ONCE into a station table, so the score does not re-read 10 MB of XML
- [ ] The client hands out our own stations for a declared window, headless and deterministic --
      two runs, byte-identical rows
- [ ] `test/scripts/score_opendrive_a9.py` prints per road: the agreeing fraction, the worst
      station, and HELD or APART -- then `N held` and the bar
- [ ] The held count is a baseline that may only RISE, and it is quoted in the item that spends it
- [ ] The work does not stop at "it runs": each quantity is developed against the corpus until its
      DECLARED threshold is undercut, and the threshold is written down with its origin BEFORE the
      first score is read. A threshold moved to make a score pass is the falsification this tree
      names in `CLAUDE.md`
- [ ] Negative control: scoring the A9 against the SUED section's roads goes red. A control that
      passes proves nothing
- [ ] Measurement that shows this is wrong: the score rises while the picture gets worse, or a road
      holds while laying no carriageway at all -- which is what the union denominator exists to stop

## What this does NOT cover

Lane-level geometry, markings, signals and junction CONNECTIONS. The oracle holds all four (1 129
lanes, 544 signals, 21 junctions with lane-to-lane mapping) and this tree sweeps ONE carriageway per
way. They are their own items and the driving simulation will want every one of them.

It also does not cover anywhere that is not this motorway. A9 Nord and Sued are 85 roads of German
Autobahn: no urban junction, no residential street, no track. A rule that holds here has been shown
to hold on the easiest geometry a road network has.
