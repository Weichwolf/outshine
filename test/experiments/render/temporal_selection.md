# Selection survives raster jitter

Owner: #16. Native integration: #531. Diagnosis: #15.

`FrameContext::Mvp` includes the temporal raster jitter. Comparing it as the
selection-cache key repeats GPU cull, scan and index compaction on a stationary
view. The geometry, pose, physical projection and requested error have not changed.

`SubjectCullStage::SelectionProjection` separates the stable selection matrix
from the current raster/depth matrix. Its side guard contains the whole jitter
sequence. Source generations, translation, matrix, guard and pixel-error scale
invalidate the cache. Submission rejection leaves recorded selection dirty.

For positive clip `w`, raster `x' = x + j_x w`, `|j_x| ≤ 1/W`, and similarly
`|j_y| ≤ 1/H`: a raster-visible point satisfies `|x| ≤ (1 + 1/W)w` and
`|y| ≤ (1 + 1/H)w`. Use these expanded side planes; near/far planes remain
unchanged. W/H are the actual picture dimensions, including letterboxing.
This applies to perspective and orthographic projection. It is a clip-space
containment argument, not a general floating-point error proof.

Derive the existing screen-error scale from the unexpanded projection. The guard
adds possible edge coverage; it does not authorize a larger LOD error. Raster,
motion vectors and temporal resolve retain the original jitter. A guarded
selection does not authorize testing against jittered depth history. Existing
unjittered Hi-Z refinement and its movement/retry invalidation remain available.

The native culling probe compares actual GPU arguments, slots and ordered index
ranges with independent references. A cluster outside the original frustum enters
the guarded selection; opposite jitter extremes reuse it. A changed pixel scale
requires a fresh sweep. The runtime fixture enables actual TAA for two periods,
reads changing depth, verifies selection reuse, then exercises movement, rejected
submission and FOV change. Restoring the jitter-dependent cache comparison must
fail these reuse checks. Existing disocclusion tests remain unchanged.

Compare fresh offline processes with identical full scene demand and no extra
world frames. A stationary diagnostic selects one pose repeatedly but declares
the original complete turn demand; it is not the 360-degree Place admission.
Separately compare every current Place's full turn and final image. Count actual
work and report complete GPU windows, including the first frame. Less culling
does not prove a general frame-budget pass or reduce ground-raster work.

Reference: [Temporal Antialiasing Survey, CGF 2020](../../../doc/references/presentation/cgf/2020-temporal-antialiasing-survey.pdf).
