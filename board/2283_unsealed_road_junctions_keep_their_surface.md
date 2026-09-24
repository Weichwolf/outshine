Type: defect
State: active
Architecture: ready
Parent: 2175
Depends:
Priority: P0
Area: generators, road, materials
Tags: osm, junction, visual-acceptance

# Unsealed road junctions keep their surface

## Problem and evidence

`Corridors::PaveLane` colors each ribbon from its `StreetField::Way::CoverRow`,
but both `RaisesTheJunctionBodies` and `AdvanceRoadBodies` color every junction
from `GroundMaterials["asphalt"]`. Thus connected `kind=track`/`path` segments
receive an asphalt-colored fill despite their unsealed source rules. This is a
generator defect, not a place-specific material choice. The synchronous and
resumable paths duplicate the error.

## Contract

`StreetField` owns the semantic way and cover row; `VegetationTemplates` owns
the mapped linear-sRGB ground color. `Corridors` must derive each junction's
color from its incident way covers, using the same resolver as ribbons. A
homogeneous unsealed junction must match its arms exactly. At a mixed junction,
blend incident colors by positive road half-width; never invent an asphalt
island from an unrelated global material. Keep the native geometry/material
contract and identical one-shot/resumable products. Invalid cover rows use the
same explicit fallback as ribbons. No OSM tag or place-name branch in meshing.

## Acceptance

- A pinned three-arm `kind=track` fixture produces no asphalt-colored junction
  vertices; replacing the source kind with `residential` changes the junction
  color by the semantic rule. Preserve source IDs, node joins and triangle
  topology. The negative control must fail with the current hardcoded color.
- Mixed sealed/unsealed arms interpolate from their actual cover colors;
  shuffled incident-way order leaves the chosen junction color unchanged.
- One-shot and resumable corridor products remain byte-identical for the same
  inputs. Run the focused corridor suite, `make format` and `make lint`.
