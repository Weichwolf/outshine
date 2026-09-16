Type: bug
State: active
Parent: 2094
Area: test, khronos, oracle
Tags: GPU, reproducibility, Blender
Depends: 2094

# GPU oracle requires the declared Blender and import capabilities

## Problem

`make lint` rejects three absent immutable reference PNGs. The local Blender 5.2.1
METAL renderer produces different bytes from their declared Blender 5.2.0 pins for
`DirectionalLight`, `PointLightIntensityTest` and `SheenWoodLeatherSofa`. Its glTF
addon also refuses `KHR_node_visibility` (`CubeVisibility`, `LightVisibility`) and
raises `KeyError: animations` for `AnimationPointerUVs`. Replacing pins with current
output would turn an oracle-version difference into an unreviewed expectation change.

## Decision

Run the declared Blender 5.2.0 release on a verified GPU, retain its complete
backend/device/version provenance and populate only images whose bytes match the
existing pins. If its importer still lacks a required extension, implement a narrow
manifest-driven import adapter or choose an independent GPU oracle that supports the
same declared glTF feature. Preserve input, camera, render recipe and pin; compare the
adapter/oracle result byte-for-byte before cache publication.

## Proof

- The selected oracle reports version, METAL backend and GPU device; CPU fallback is
  refusal.
- All 265 declared reference pins validate dimensions and digest without mutation.
- The six unsupported/divergent cases render through the same recipe and have their
  expected existing bytes. A deliberately changed byte is rejected by the cache gate.
- `make lint` reaches its post-cache analysis and repository guards with no skipped
  oracle case.
