Type: defect
State: active
Architecture: ready
Parent: 2228
Depends:
Priority: P0
Area: render
Tags: memory, realtime

# Allocate optional vertex streams only for their consumers

## Problem and evidence

973b697a5 Graz: SubjectResidency requests 4.75 GB; sampled GPU-fence waits dominate.
RoomForStreams unconditionally allocates Tangent and Uv1 through the end of the main
mesh range, even when Shape().HasTangent/HasUv1 are false. Independent pieces already
reserve tangents on demand. Main meshes validate their streams against their layout.
Instrumented GPU growth: Tangent 577585152 bytes + Uv1 288792576 bytes = 866377728 bytes.
These are requested capacities, not measured physical residency or proven frame-time cost.
Logs: /tmp/outshine-batch-readiness-973b697a5-Graz-gpu-streams.log.

## Decision and implementation

Owner: SubjectDraw::{RoomForStreams,PlacePiece}; existing SubjectResidency::Grow/Cross.
Reserve optional UV/colour/tangent/UV1 only through each actual consumer's addressed range.
Use Grow BEFORE partial uploads: it flushes pending data, grows geometrically and copies
existing contents. Cross replaces storage transactionally but does NOT copy prior bytes.
A shared private RoomForOptionalStreams helper handles present attributes for the main
mesh and for incoming pieces. Never size their attributes from unrelated VertexRoom.
Keep required position/normal/index, emitted and velocity contracts. Never release an
optional buffer when the main mesh omits it: independent pieces may still own its data.
No shader/layout/index/source/visibility/quality changes and no second allocator.
GPU readback must prove older main/piece bytes survive later consumer growth, plus
absent-consumer capacity stability and independent data after main-mesh removal.

## Acceptance

GPU test under test/outshine/src/render/stages/SubjectDraw: main mesh after an independent
piece reserves no tangent/UV1 buffers when absent; present streams allocate and upload;
removal preserves independently owned tangent/colour data. An untextured, untinted piece
reserves neither UV nor colour storage; main-mesh absence must not grow these streams. Check actual buffer capacities/data,
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
Instrumented Graz: same addressed main range, 4754565080 - 3888187352 = 866377728 fewer
requested bytes, exactly absent Tangent 577585152 + UV1 288792576. Not physical residency.
Probe still reaches its 150 s deadline (exit 124); no completed-scene speedup proved.
Next repair is active: colour capacity still grows 787480576 -> 1574961152 for new
building pieces with no colours, while the main colour range stays fixed. Remove this
remaining arena-wide optional reservation and apply the same rule to UV storage.
Logs /tmp/outshine-batch-readiness-4b9afb1de-Graz-gpu-streams.log. No smaller LOD bound.
Logs: /tmp/outshine-repair-4b9afb1de-{focused,full-lint,full-places}.log.

d35b9a1ed removes eager optional reservation entirely; actual present uploads now own
UV/colour/tangent/UV1 capacity through the existing transactional Cross path. Required
streams remain unchanged. Format 1219 PASS; six focused GPU tests PASS, 84 checks.
Restored 4b9afb1de arena-wide UV/colour reservation yields exit 1 and ten real failures,
including growth without new consumers; GPU readback preserves present and independent data.
d35b9a1ed gate aborted after full lint PASS: Cross growth loses prior UV/colour contents.
The old 84 checks missed this because they read earlier values BEFORE later growth.
Six new earlier-byte checks FAIL on d35b9a1ed (90 checks, exit 1), proving the regression.
Do not accept d35b9a1ed or its incomplete Places run. Repair above is active; retain tests.
Evidence /tmp/outshine-2318-prior-stream-data/{build,run}.log. Full gates must rerun.
Logs /tmp/outshine-repair-d35b9a1ed-*.log and /tmp/outshine-2318-consumers-negative-control*.
