# Conservative affine bounds in native GPU selection

Owner: #1066, parent #16. The existing shader scaled each local cluster sphere
by the longest column of its placement matrix. That bounds orthogonal scaled
axes, but not a general affine map. For example, `x'=x+y+2.1, y'=0.25y, z'=z`
gives a column-length estimate of 1.030776 while a unit local sphere reaches
1.414214 along world X. A cluster centred at world X=2.1 is incorrectly rejected
by an orthographic view ending at X=1, despite containing a visible triangle.

The native regression draws the same vertices and placement twice: first without
clusters, then with a valid enclosing local sphere. The original shader loses the
18 visible control pixels. The corrected shader retains all 18, with identical
linear image and depth. Three affine cases use local scales 1, 1e20 and 1e-20,
with reciprocal placement scales; a fourth case verifies conservative fallback
for a non-affine homogeneous placement. The original shader fails three image/
depth comparisons among 17 checks; all independent controls and setup pass.

## Bound and failure contract

The existing CPU method in #819/#1063 bounds the largest singular value using
the maximum absolute row sum of the Gram matrix `A^T A`. The GPU now uses the
same inequality: `lambda_max(A^T A) <= max_i sum_j abs((A^T A)[i,j])`.
Before forming that matrix, divide A by its largest absolute coefficient and
restore that scale afterwards. This avoids overflow or underflow in the Gram
products for the compensating extreme-scale cases above. A zero linear map
has zero scale. Float32 scale, transformed-centre and plane-test calculations
include magnitude guards; the shader guard is 16 times Float32 epsilon.

Self and parent spheres use the same guarded transformation. Projective,
nonfinite, subnormal-sensitive or unrepresentable bounds retain the full job;
unknown bounds do not establish rejection or a qualified LOD decision. Existing
occlusion and LOD selection then consume the qualified enlarged world spheres.
No new binding, buffer, pass, cache format or quality profile is introduced.

The underlying cluster-selection reference remains the inspected local
[SIGGRAPH 2015 GPU-driven rendering slides](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
slides 11/13. The actual numerical bound and its source-vertex coverage contract
are the existing native CPU method and conservative clipping tests, not a claim
that those slides prove Float32 rounding on every backend.

## Iris verification and cost

Linux/i5-1135G7/Iris Xe, Clang22/libc++: the new regression and existing camera-
movement, temporal-jitter and depth-history consumers pass 135 checks. Test
analysis uses complete actual CDB flags. glslang rebuilds the original shader
byte-identically to the existing artifact; both old/new SPIR-V validate. Independent
reflection agrees on descriptor layouts, buffer member types/offsets/access and
64x1x1 dispatch shape. SPIR-V grows from 17,888 to 25,632 bytes; driver code/register
storage is not measured and is not claimed unchanged.

The existing explicit Vulkan work probe repeats the same offline Tokyo profile:
1280x720/60, 60 frames, 360 degrees and 30 m translation. Both runs validate all
61 traces/1,209 samples and uniquely match every measured submission duration to
its benchmark GPU sample. No query or capture is omitted. Both execute 50,368
culling invocations per frame. Median culling interval is 0.136406 ms original
versus 0.148333 ms corrected. Opaque index requests over the window are
265,910,106 versus 265,910,670; vertex invocations are 151,607,066 versus 151,607,387.
This is a correctness fix with modest observed additional work, not a speedup.

Both complete 60 CPU/GPU samples and preserve the exact opened Tokyo PNG,
SHA256 `57e427d31a806e0e20433fc9701b49c27941826faace2a866212115236ffa412`.
Instrumented GPU medians are 20.620/20.482 ms and p99 29.233/28.787 ms. Query
overhead is uncalibrated, intervals can overlap, and recorded host load remains
part of the evidence. All 60 frames remain unrefined (#1045); this establishes
neither the frame budget nor complete-world or Metal/A18 admission.

Originals, exact commands, per-source/archive/binary hashes, shader reflection,
native negative controls and raw diagnostic pixels remain under
`/tmp/outshine-engine-affine-gpu-cull-6ec91f3ad/`. Reports include
`checks-terminal.json`, `regressions-terminal.json`, `current-terminal.json`,
`shader-contract.json`, `source-manifest.json`, `pixels.json` and
`work-comparison.json`. The measured C++ composition corresponds to native72cbca35e:
the verified #1065 archive plus its only intervening product delta, recompiled
FrameIO.cpp (#1061). The final topic is rebased onto native6d1c02510; involved
render/base/world/generator/engine implementation, scene headers and the regression
remain unchanged. Its focused compile/link/17-check run also passes after rebase.
The unrelated glTF removal is not given a new broad gate or full engine-build claim.
Master owns integration, bundled checks and the Metal/A18 countercheck.
