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

## THERE IS GROUND TRUTH, credential-free, for the places this tree already stands at

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

- [ ] One place's bridge decks are scored against a measured height, and the case states its grade:
      **TRUTH**, because a LiDAR class-17 return is a measurement carried further than ours
- [ ] The oracle is prepared by the tree's own preparer into the system temp root, never committed,
      like every other corpus here
- [ ] `streets: and the most one stands over what it crosses` is replaced by, or joined by, a number
      that compares deck by deck rather than reporting a maximum
- [ ] ODbL's share-alike is checked BEFORE any OSM-derived geometry is committed. Nothing derived
      from OSM is in this repository today and that should stay deliberate rather than accidental

## What this does NOT cover

Scoring the whole world. These four places have open 3D bridge data because Germany and Switzerland
publish it; most of the Earth does not. The corpus proves the RULE on the places that can judge it,
and everywhere else the standard stays the owner's: plausible rather than truthful.
