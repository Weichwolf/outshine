Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2234
Depends:
Area: engine, sky

# Preserve exact atmosphere evaluation across world replacements

## Evidence

Native macOS sample of Malcesine at 054e6b387: 11676/15041 main-thread samples
are GroundAtmosphere::Evaluate from MeteredLux during PublishStructureTiles ->
PreparesWorldReplacement -> Prepare -> Build. Profile is attribution, not a
frame-time benchmark. Unprofiled simulation p99 was 597.81 ms.
GroundAtmosphere already caches exact Medium and float cosine. Each RuntimeScene
candidate starts with an empty cache, repeating expensive unchanged integration.

## Decision

Prepare accepts an optional prior GroundAtmosphere value and copies it before
Build. Open and both replacement preparation paths supply the previous scene's
cache. Each candidate owns its copy: rejection cannot alter the published owner.
No global cache, shared mutable state, parameter quantization or skipped lighting.
Changed Medium/sun retains existing exact invalidation. Dynamic-light integration
cost and bounded structure publication remain separate work.

## Acceptance

Exercise multiple air states so the integration count exceeds one; then prepare,
reject and publish replacements, with/without native geometry. Counts and lux stay
unchanged for identical state. Changed air/sun must reintegrate and change light.
A cache reset in replacement preparation must fail, not pass because both old/new
instances happen to report one integration. Retain existing atmosphere tests.
make format; focused RuntimeScene/atmosphere suites; make lint.
Repeat Malcesine without profiler; open PNG and compare against 8dd84aa7. Report
simulation/draw distributions through full refinement, not only settled frames.
