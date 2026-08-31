Type: feature
State: open
Area: test
Tags: corpora, infrastructure, measured

# A deck is scored against a MEASURED one, not against an eye

**Benchmark** — Unreal's automation compares screenshots bit for bit; RAGE's replay plays a drive
back frame for frame. **Both agree that a picture is judged against a RECORDING**, and this tree
already lives by that for the places. What it has no recording of is a STRUCTURE: the bridge deck
heights board:2075 infers are judged by eye today, and an eye cannot tell 42 m from 53 m at a
kilometre.

## A PLACE IS A CAMERA; A CORPUS IS AN EXTENT, AND ONLY THE SECOND BOUNDS THE SCORE

The seven places say where a picture is TAKEN. They say nothing about how many decks may be
JUDGED, and reading them as a limit would throw away almost the whole oracle: **Hamburg's CityGML
set alone carries 2 170 `brid:Bridge` objects**, and this tree stands one camera in it. The
comparison needs no camera at all -- infrastructure is raised from a declared window headless,
which is this tree's fast path rather than a degraded one -- so the case scores EVERY bridge inside
the corpus's own extent and reports a DISTRIBUTION, the same way a frame time is reported.

    what bounds the score        the corpus extent, and the licence that let it be fetched
    what does NOT bound it       where a camera happens to stand

## THERE IS GROUND TRUTH, credential-free, over extents far larger than any camera

Researched, and it corrected the assumption that German open data carries no bridges.

| place | the oracle | licence |
|---|---|---|
| **Koehlbrand** (Hamburg) | CityGML 2.0 `brid:Bridge` with `lod3MultiSurface`, **2 170 objects**, and a 3D-Tiles mesh carrying the bridge's 135 m pylons | dl-de/**by**-2-0 |
| **Kaiserberg** (Duisburg) | LiDAR `LAZ`, ASPRS **class 17 = bridge**: **112 406 points** in the Kaiserberg tile, Z **32.92 - 40.18 m NHN**, +-0.15 m, flown 2025. Geobasis NRW defines class 17 as the points belonging to the CARRIAGEWAY -- deck without piers or parapets, the segmentation already done | dl-de/**zero**-2-0 |
| **Zurich** | swissSURFACE3D class 17; canton ZH LiDAR at 16 points/m2 (CC0); swissTLM3D `KUNSTBAUTE` for the axis with Z | swisstopo free / CC0 |
| **Heidelberg** | no raw LiDAR, but **DOM1 minus DGM1 inside the Basis-DLM mask `BWF=1800`** -- the DGM excludes bridges by definition, so the difference IS the deck | dl-de/by-2-0 |

**So `streets: and the most one stands over what it crosses` -- 6.70 m today -- has a number to be
wrong against.** Kaiserberg's decks are measured at 32.92 to 40.18 m NHN.

## AND MEASURED ROAD GEOMETRY IN OPENDRIVE

| corpus | licence | account |
|---|---|---|
| **A9 motorway** (`tum-gis/opendrive-testfeld-a9`) | GeoNutzV | no |
| Braunschweig Schwarzer Berg | CC-BY-4.0 | no |
| Testfeld Niedersachsen | CC-BY-**NC**-SA-4.0 | no -- and NC, so read but do not ship |
| `asam-ev/qc-opendrive` `tests/data/` | -- | no, and it holds **181 paired valid/invalid cases** |
| CARLA towns (`carla-simulator/opendrive-test-files`) | MIT | no -- but RoadRunner-generated, so FICTIONAL and a snapshot at best |
| SUMO `tests/netconvert/import/OSM/` | EPL-2.0 | no -- and it tests exactly this conversion |
| ASAM OpenDRIVE spec and examples | free | **e-mail registration**, so not re-derivable |

## What will be true

- [ ] EVERY bridge in one corpus extent is scored against its measured height -- not one per
      camera place -- and the case states its grade: **TRUTH**, because a LiDAR class-17 return is
      a measurement carried further than ours
- [ ] The score is a DISTRIBUTION over the decks it judged -- p50/p95 of the height error and the
      count -- because a single worst deck says nothing about whether the rule is right
- [ ] The oracle is prepared by the tree's own preparer into the system temp root, never committed,
      like every other corpus here
- [ ] `streets: and the most one stands over what it crosses` is replaced by, or joined by, a number
      that compares deck by deck rather than reporting a maximum
- [ ] The window the corpus is scored over is DECLARED, so growing the score is adding a window
      rather than adding a camera
- [ ] ODbL's share-alike is checked BEFORE any OSM-derived geometry is committed. Nothing derived
      from OSM is in this repository today and that should stay deliberate rather than accidental

## What this does NOT cover

Scoring the whole world. Germany and Switzerland publish open 3D bridge data and most of the Earth
does not, so the corpus proves the RULE over the extents that can judge it -- thousands of decks,
not four -- and everywhere else the standard stays the owner's: plausible rather than truthful.

It also does not cover the PICTURE. A deck at the measured height can still look wrong, and that is
what `make shots` and its digests are for. The two are different oracles and neither substitutes for
the other.
