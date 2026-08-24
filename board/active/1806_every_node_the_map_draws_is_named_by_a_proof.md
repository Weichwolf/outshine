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

- [ ] Each of the seven has a unit twin in the mirrored path that names it and proves something
      about it -- not a smoke test that constructs and destructs.
- [ ] A claim walks the CURRENT class diagram's node list and asserts every node is named by at
      least one source under `test/`, so the mirror's completeness is a rule rather than a habit.
- [ ] The claim's node list is DERIVED from `CLAUDE.md` rather than copied beside it, and the
      three display labels are excluded by name with the reason stated.
- [ ] Negative control: one twin deleted -> the claim names that node and goes red.
