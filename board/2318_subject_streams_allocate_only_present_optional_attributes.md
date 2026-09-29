Type: defect
State: active
Architecture: ready
Parent: 2228
Depends:
Priority: P0
Area: render
Tags: memory, realtime

# Allocate optional subject attributes only when present

## Problem and evidence

973b697a5 Graz: SubjectResidency requests 4.75 GB; sampled GPU-fence waits dominate.
RoomForStreams unconditionally allocates Tangent and Uv1 through the end of the main
mesh range, even when Shape().HasTangent/HasUv1 are false. Independent pieces already
reserve tangents on demand. Main meshes validate their streams against their layout.
Instrumented GPU growth: Tangent 577585152 bytes + Uv1 288792576 bytes = 866377728 bytes.
These are requested capacities, not measured physical residency or proven frame-time cost.
Logs: /tmp/outshine-batch-readiness-973b697a5-Graz-gpu-streams.log.

## Decision and implementation

Owner: render/stages/SubjectDraw::RoomForStreams. Guard optional main-mesh tangent/UV1
capacity by the existing Shaping flags. Keep existing buffers and independent piece
reservations; never release attributes still referenced by pieces. No shader/layout,
index, source, visibility or quality changes. Keep emitted and velocity contracts.
Absent optional streams require no allocation; present streams cover their full addressed
range and retain existing failure propagation through BeginMesh/PlacePiece.

## Acceptance

GPU test under test/outshine/src/render/stages/SubjectDraw: main mesh after an independent
piece reserves no tangent/UV1 buffers when absent; present streams allocate and upload;
removal preserves independently owned tangent data. Check actual buffer capacities/data,
not an implementation helper. Restoring unconditional reservations must fail the test.
make format; make suite SUITE='outshine/src/render/stages/SubjectDraw outshine/src/render/stages/SubjectResidency';
LINT_JOBS=2 make lint; all Places via client, all ten PNGs personally opened and compared.
Compare requested buffer capacities in Graz; do not claim repaired readiness without its gate.
