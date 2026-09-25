Type: defect
State: done
Architecture: ready
Parent: 2175
Depends:
Priority: P0
Area: generators, road, materials
Tags: osm, junction, visual-acceptance

# Unsealed road junctions keep their surface

## Problem and evidence

Previously ribbons used `StreetField::Way::CoverRow` while junctions used
`GroundMaterials["asphalt"]`, leaving asphalt patches on unsealed tracks.
Both synchronous and resumable paths now consume the same junction cover.

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

`TrackJunctionKeepsUnsealedCover` checks track, residential, mixed covers and
shuffled input order. It compares exact native vertices, normals, colors,
indices, materials and earthworks between one-shot and 1-way/1-node slices.
The old asphalt constant fails the track and mixed-color checks. Focused test,
`make format` and `LINT_JOBS=2 make lint` pass.
