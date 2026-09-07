Type: bug
State: active
Area: render
Tags: measured

# Piece draws use written indices and release reserved capacity

SubjectResidency::TakeIndices rounds to 4096-index pages. SubjectDraw::PlacePiece stores
that allocation Range, but Retable uses its Count as the draw's IndexCount and ReleasePiece
subtracts its Count/3 from PieceTriangles_. A three-index mesh therefore draws reserved
padding and releases 1365 counted triangles after adding only one.

Measured by PieceInstancesShareTheirGeometry in build/shared-piece-probe.log:
1 resident triangle before ReleasePiece, 4294965932 afterwards (uint32 underflow).
The test retains the zero-after-release oracle. Its original depth-background expectation
was separately corrected against the renderer's explicit Reverse-Z clear value 0.

Unreal/RAGE distinguish buffer allocation capacity from each draw's used index count.
Keep both here: retain the allocation Range for freeing, store the actual mesh index
count for drawing and triangle arithmetic. No shrinking of the allocation to hide padding.

Proof: one triangle, shared placements, release and slot reuse. Actual count remains one
regardless of placement count and returns to zero after release. The old capacity-as-count
implementation is the measured red control. Inspect GPU output as well as arithmetic.
