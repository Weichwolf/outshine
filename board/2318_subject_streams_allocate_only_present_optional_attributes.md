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

## Current evidence

4b9afb1de: format 1219 files PASS; six focused GPU tests PASS. New test: 54 checks,
zero failures; all four optional-attribute combinations and independent-piece retention.
Full lint/tidy/API PASS: 258/258 tidy units, zero findings; 32 repository guards PASS.
Graz/Wien/Olympiaturm Playable client renders PASS, PNGs personally opened: each unchanged
at 0/921600 differing pixels versus 973b697a5. Flat/repeated facades, empty sky, flat Wien
water and sparse Olympiaturm coverage remain; no fresh local webcam reference available.
The building-arena metric and subject bytes share one owner but different capture times;
never subtract these stale/current snapshots as if they represented separate categories.
Full Places suite: 38/44 PASS, two Olympiaturm TIMEOUTs and four Graz/Wien UNPREPARED;
pipeline exit 1. All ten current PNGs personally opened; Refined selections use terminal
ROW logs, timestamps and per-place provenance JSON. Husum/Hockenheim also match valid
973b697a5 images at 0/921600 differing pixels. Known terrain/material gaps remain (2169).
Five old Refined archive selections mismatch their 973b697a5 logs; their old identity
claims are withdrawn. Current validated Darmstadt/Feldkirch/Koerbersee/Malcesine/Rosenheim
images are 9218e7cc/7bdb269c/cb8319ab/2735cb40/d986fe9f; older copies are historical only.
Restoring unconditional reservation yields actual exit 1, four failures in 54 checks;
/tmp/outshine-2318-negative-control-results.log and /tmp/outshine-2318-negative-control/.
Runtime byte comparison pending. Graz/Wien stop before Ground publication in this gate;
faster case termination does not prove faster completed scenes. No smaller LOD bound.
Logs: /tmp/outshine-repair-4b9afb1de-{focused,full-lint,full-places}.log.
