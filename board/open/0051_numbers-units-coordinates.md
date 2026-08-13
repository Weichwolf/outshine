Type: feature
Area: core
Tags: scope

**I.5 Numbers, units, coordinates**

- [x] `float64` ECEF as the truth, `float32` camera-relative, one late conversion (`core/Geodesy.h`)
- [x] Geodetic ↔ ECEF, tile addressing, slippy scheme
- [x] Metres as the only length unit at an interface (`core/Units.h`)
- [x] `uv` in metres, never 0..1, for every mesh that carries one
- [x] Ephemeris for sun and moon at true altitude and azimuth
- [x] Civil time with a declared instant per scene
- [ ] Calendar with a day-of-year that anything seasonal reads
- [x] Keyframe evaluator that knows none of its consumers (`core/Keyframes.h`)
- [x] Determinism: seed derived from the region key, so placement is a property of place (`test/unit/generators/SameRegionSamePlacement.cpp`)
- [ ] Determinism across tile arrival order — a pinned binary does not reproduce its still today
- [ ] `FB_TAU` read from the environment removed — the picture must not depend on an undeclared variable
