Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2224
Depends:
Area: render

# World publication invalidates cached visibility

## Evidence and decision

Repeated identical-camera world replacement in GroundRoughnessMatchesNativeMaterial
renders black after the third publication. SubjectCullStage caches by SubjectDraw
address, local Generation(), and camera. Move assignment into the published owner
preserves its address; a replacement may reuse the same local generation while
owning fresh indirect buffers. These identifiers do not prove cached GPU validity.

SceneRenderer::PublishesWorldCandidate must invalidate SubjectCullStage's cached
result after replacing WorldContent. Add an explicit Invalidate operation; retain
normal static-frame reuse. Failed candidates must not invalidate published content.
No camera jitter workaround, global generation counter or unconditional per-frame cull.

## Acceptance

Repeated publications at one camera produce nonempty matching native/ground images
for low/high roughness (WI 2250). Each new world recomputes visibility; subsequent
unchanged frames reuse it. StaticOcclusionPreservesDrawInput remains green.
make format; focused RuntimeScene suites; make lint.
