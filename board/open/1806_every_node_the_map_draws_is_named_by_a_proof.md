Type: bug
Area: test, ground, data, clients
Tags: architecture, measured, mirror

# Every node the map draws is named by a proof

`CLAUDE.md` states what the unit mirror is:

> *`test/unit` — mirrors `src/`, IS the layering proof* · *every src file has its unit twin in
> the mirrored path*

Walked over the CURRENT class-structure diagram -- 76 nodes -- and asked of each one whether
anything under `test/` names it at all:

| node | lines in `src/` | files in `test/` naming it |
|---|---|---|
| `OsmField` | 297 | **0** |
| `BuildingField` | 342 | **0** |
| `WaterField` | 315 | **0** |
| `RegionForge` | 175 | **0** |
| `StreetField` | 108 | **0** |
| `Ephemeris` | 87 | **0** |
| `WebTileSource` | 86 | **0** |

**1 410 lines the map draws and no proof mentions.** Not "a twin that proves little" -- a
`grep -rlw` over all of `test/` returns nothing for any of the seven.

Three of the four remaining zero-hits are diagram artefacts rather than classes (`TD` is
mermaid's direction token, `LayoutUi` and `SceneStore` are display labels for `Layout` and the
scene store), and they are the reason this must be a walk with a stated node list rather than a
grep somebody runs once.

## Why these seven and not others

`OsmField` is the entry point of the whole ground layer -- `RoadHarvest`, `StreetField`,
`WaterField`, `BuildingField`, `ClassField` and `World` all read it. It is the most-depended-on
unproven file in the tree. `Ephemeris` decides where the sun is, and the sky stages are green
on the strength of it. `RegionForge` and `WebTileSource` sit on the content path.

Their absence from `test/` is also why `board:1805` could stand: a layer nothing proves and
nothing calls looks exactly like a layer that works.

## What will be true

- [x] Each of the seven has a unit twin in the mirrored path that names it and proves something
      about it -- not a smoke test that constructs and destructs.
- [x] A claim walks the CURRENT class diagram's node list and asserts every node is named by at
      least one source under `test/`, so the mirror's completeness is a rule rather than a habit.
- [x] The claim's node list is DERIVED from `CLAUDE.md` rather than copied beside it, and the
      three display labels are excluded by name with the reason stated.
- [x] Negative control: one twin deleted -> the claim names that node and goes red.

## Repaid, and the walk found an eighth (2026-08-24)

Eight, not seven. The hand sweep that filed this item missed `Frustum` -- it has no header of
its own, it lives in `Camera.h` -- and the claim that walks the map found it the first time it
ran. That is the difference between a grep somebody runs once and a rule.

| node | what its twin proves | where |
|---|---|---|
| `Ephemeris` | the obliquity of the ecliptic, **23.4354 deg**, recovered from a year of noon elevations at the equator; polar day and polar night; the moon over a full cycle in 30 days | `unit/core/TheSunIsWhereTheObliquityPutsIt` |
| `WebTileSource` | coverage is the declaration -- 8 rows of zoom band and tile grid; a request at z15 is served by its z12 ancestor, three levels, three shifts; asking about coverage spells no url | `unit/data/AWebTileSourceServesAnAncestorOrRefuses` |
| `OsmField` | a 79-byte hand-encoded vector tile decodes to 48.136767 / 11.535645; tags resolve by name; an absent tag returns the caller's default; every point inside the tile's own box | `unit/ground/AVectorTileBecomesAFieldTheGroundCanRead` |
| `StreetField` | residential declared 5.5 m becomes a 2.75 m half width; an undeclared kind is dropped and a tunnel is counted; a field ingested twice holds the road once | `unit/ground/AStreetFieldTakesTheWaysItsClassWidens` |
| `WaterField` | over a ridge the ground climbs 343.323 m and the water climbs 0.000; a lake takes the fifth percentile of its own shore; ground nobody has produces no lake | `unit/ground/WaterOnlyRunsDownhillAndALakeHasOneLevel` |
| `BuildingField` | a declared height wins and is recorded as declared; the same footprint carries 9.0 m alone and 11.9 m on a street; no ground, no building | `unit/ground/ABuildingTakesItsHeightFromOsmOrFromItsFootprint` |
| `RegionForge` | idle before its first order, names the region while it grows, hands the grown region back exactly once, idle again after | `unit/clients/ARegionForgeGrowsOnItsOwnThreadAndHandsBackOnce` |
| `Frustum` | keeps what is ahead, drops what is behind, and over **185 860** points inside the clip volume never drops a box containing one | `unit/core/AFrustumKeepsWhatTheCameraCanSee` |

## Three doors narrowed to make them provable

Not one of the ground fields was untestable by oversight. Each had a door too wide to reach
without a network:

| door | was | is |
|---|---|---|
| `OsmField::Build` | `TilePool &` -- threads, a content store, fetched bytes | `Build` fetches and calls `Accept(tx, ty, std::span<const uint8_t>)`, a pure function of the bytes |
| `WaterField::Ingest` | `const GroundStream &` | `const GroundQuery &`, the way `board:1624` narrowed `LayCorridor` |
| `BuildingField::Build` | `const GroundStream &` | `const GroundQuery &` |

The MVT encoder those twins need lives once, in `test/harness/shared/VectorTileMaker.h`.

## What the twins found while being written

- **board:1807**, closed: `OsmField::Decoded()` read the SETTLED set, so a tile with no data
  answered yes and a tile accepted straight from bytes answered no with 668 bytes of features
  in it.
- **A claim of mine that was wrong about the code**: the building twin first asserted that a
  bigger plan area carries more storeys. It does not -- a 120-unit block and a 900-unit block
  both came out at 9.000 m. What decides is the frontage, and the twin says so now.
- **A derivation of mine that was wrong about the sky**: the ephemeris twin first took the
  year's global maximum and minimum elevation and called the difference the tilt. That is the
  equinox measured twice; 1.82 deg came out where 23.44 was wanted.

- **Proving test**: `test/harness/claims/EveryNodeTheMapDrawsIsNamedByAProof` -- 74 nodes
  parsed out of the map's own mermaid fence, three of the diagram's own words excluded by name,
  **0 unproven**.
- **Negative control**, run: one twin moved aside ->

  ```
  FOUND Frustum is drawn and unproven
  26 tests: 25 PASS  1 FAIL
  ```
- Gate **256/256**, 67 106 ms of run -- sixteen cases more than the round began with.
