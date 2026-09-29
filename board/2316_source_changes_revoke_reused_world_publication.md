Type: defect
State: active
Architecture: ready
Parent: 2188
Depends:
Priority: P0
Area: engine, world
Tags: source, lifetime, routes, regression

# Source changes revoke reused world publication

## Problem and evidence

0d5962a2a Places gate: HockenheimPublishedRouteHasWorldPose fails its source-change
assertion; validated repetition passes. Temporary public-engine diagnostic at 36e376880
reproduces two failures in eight runs. Both declare/assemble succeed; the reuse branch
retains GroundPublished and exposes the old route AND road contact. Six full-declaration
paths reset publication correctly. /tmp/outshine-route-source-probe/results.log.

ReuseDeclaration compares picture/stand but not Providers/Routes. HasGeneratedContent
covers explicit generators/assets, not a streaming ground world. RouteQueries accepts
GroundPublished.Current without distinguishing a newly declared source. Automatic sun
setup samples system seconds in the fixture, so picture equality may hide the defect
by forcing a full reset; freeze declared time and verify this with the unchanged engine.

## Ownership and correction

- engine/Declaring.cpp owns declaration reuse. Changed SourceProvider or RouteDeclaration
  vectors must take the existing complete preparation/reset path. Compare full values,
  not hash/IDs alone. Keep reuse for unchanged sources and view/UI-only changes.
- Reuse does not invent a second invalidation mechanism. Existing GroundPublished reset,
  world source reconfiguration, candidate cancellation and renderer ownership remain owners.
- Tests under integration/places/HockenheimPublishedRouteHasWorldPose use explicit fixed
  time so lighting cannot accidentally mask a source change. Preserve all route/contact,
  station and source rejection requirements. Separate declaration/setup failures from
  stale-publication failures when diagnosing; never lower quality or remove assertions.
- Include changed route identity with unchanged provider and unchanged-source reuse
  controls. A failed declaration must not revoke the previously valid state.

## Acceptance

- Original code with fixed time actually fails the source-change contract; patched code
  passes. Restore either missing source or route guard and its independent control fails.
- Query metadata, pose and contact refuse stale publication after successful declaration
  and assembly; newly built source routes become available through ordinary advancement.
- No camera, source data, timeout or test expectation changes to obtain green.
- make format; focused HockenheimPublishedRouteHasWorldPose, ScenarioRoundTrip/Outshine
  declaration cases; full lint/tidy/API; all Places and personally opened client PNGs.
- Finish the independent c8c670ea9 surface-proof parameter gate in the combined repaired
  snapshot. CPU surface proofs still grant no smaller runtime building LOD error.
