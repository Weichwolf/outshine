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

## Selection prepared before the first frame

Cull/scan/compact also run during world-resource preparation. Their accepted
GPU products can serve the first unchanged view. Preparation creates no colour
frame, advances no camera publication serial and adds no measured world frames.
A changed pose/source/projection still requires selection in the real frame.

Accepted selection is independent of usable depth. Changing its key or invalidating
its resources clears depth eligibility. Only a successfully submitted real frame
which writes the depth pyramid marks that selection as having depth history.
Repeated preparation cannot promote uninitialized depth into an occlusion source.
An accepted submission remains ordered before later consumers; this does not
claim that failed device work produces valid data.

The real renderer fixture checks rejected/repeated preparation, actual pre-frame
indices, zero published/measured frames, first-frame reuse and later depth
refinement. The native culler additionally receives an intentionally occluding
pyramid: repeated accepted preparation must keep its geometry until explicit
depth submission. Removing that guard fails the retained-index checks. The
unmodified d2bd3b14b archive/headers fail three preparation-fixture requirements.

On the complete stationary Koerbersee/Tokyo worlds at 1280x720/60, with unchanged
360-degree declared demand, one A/B/B/A sequence per place reduces measured-frame
cull chains from one to zero; all PNGs match. Profiled Tokyo first frames are
25.776/36.196 ms before and 17.814/21.994 ms after. Koerbersee remains
23.614/23.363 versus 22.254/23.296 ms. These small samples do not establish a
general timing gain or device-budget acceptance. Ordinary complete turns retain
selection work as the view changes. All twelve final full-turn pictures remain
byte-identical.
