Type: bug
State: active
Architecture: ready
Priority: P0
Parent: 2145
Depends:
Area: world, render
Tags: water, coastline, webcam

# Polygon materials preserve the source shoreline

## Delivery
Water, land and constructed areas use the same polygon boundaries in material
classification, generated surfaces and terrain stamps. Water does not spill onto
land through independent smoothing of its material footprint.

## Existing defect and implementation
`world/ground/ClassBuilder::BuildFeatureEdges` applies Catmull-Rom to closed polygons;
`WaterSurfaceBuilder` and basin stamps use their straight source segments. The
classifier therefore invents different shores and island boundaries.
Keep polygon segments and winding unchanged, including interior rings. Preserve
the existing open-line path and road geometry. Ownership remains in the immutable
classification job; no new products, source cache or renderer API are needed.

## Acceptance and remaining water work
Analytic shore and island probes agree with the source polygon at subcell positions.
Open-line behavior remains unchanged. Flensburg and Husum images must improve or
preserve actual water/land contacts within the Place budget.
Independent marine coverage, connected levels and excessive terrain deformation
remain in 2145; this change does not prove their repair.
