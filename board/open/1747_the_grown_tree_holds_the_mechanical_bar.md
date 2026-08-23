Type: issue
Area: generators
Tags: origin, hygiene, hot-path-adjacent

# The grown tree holds the mechanical bar and its numbers name their origin

The generators layer is architecturally sound — seeded determinism (TreeRandom per
GrowOnce, Region::Seed per cell), bounded loops (kMaxNodes, kDemCacheCeiling), yield
telemetry with named notes, refusal at parse (TreeSpecies.cpp:139-154). What it lacks is
the bar the rest of the tree already passed (1693 data, 1729 gltf, 1739 stages):

1. **Origins.** A carpet of bare tuning constants where the house rule demands
   derived · measured · [SET] with population:
   - TreeGrower.cpp:106 (0.18 stool), :112-113 (0.35, 0.55/0.45 lean split), :122-124
     (0.34, 0.94, 0.5 hedge jitter), :151/:154 (0.30, 0.95, 0.40 leaf ring), :170
     (-0.05 outward test), :269 (×4.0 pull ramp), :288/:293 (0.25/0.4 roll jitter),
     :303-306 (0.55 fork spread, 0.74 fork radius), :309-311 (1.7, 1.1), :413 (1.6
     first-pass crown guess), :426 (0.005 DBH tolerance), kCapReach 2.4, kMinBranchSteps.
     kEscapeStop and kBendBack show the layer knows the form — the rest never followed.
   - TreePrototype.cpp:49-52 — MaterialRow's beta-lobe shape numbers (1.5, 0.80, 0.95,
     1.25, 0.55) carry no derivation, and out[0..19] is an index-mapped row whose field
     meanings live nowhere.
   - Forest.h:35 kCellM = 3.33 (why this cell?), Forest.h:14-18 Stem defaults
     (20 m / 0.15 m / 1000 kg — measured on what?), Forest.cpp:124 the 8-sigma headroom.
   - TerrainTiles.cpp:156 extent literal 4096.
2. **[[nodiscard]] ALWAYS.** 13 of the draw/ headers carry zero [[nodiscard]]
   (TreeGrower.h:17-20 Passes/DbhErrorRel/GrowHeight, TreeRandom.h Next/Unit/Signed,
   TreeMesh.h, TreeSkeleton.h, TreeFoliage.h, TreePrototype.h queries...), and
   constexpr/noexcept are absent where they trivially hold.

What will be true: every number in src/generators names its origin beside itself; every
value-returning query in the layer is [[nodiscard]]; MaterialRow's packing is a named
struct or named indices with one static_assert on the row width.
