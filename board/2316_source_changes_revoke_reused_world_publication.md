Type: defect
State: done
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

## Implemented and focused evidence

Fixed-clock original engine fails 3/3 executions, always on reused publication:
/tmp/outshine-route-fixed-probe/results.log. Source comparisons now route changes
through existing complete preparation/reset. No separate invalidation state added.
The fixture fixes time, keeps all old requirements, checks immediate metadata/pose/
contact refusal, valid-source reuse, rejected-declaration preservation, successful
source rebuild, and changed relation under the same route name.

Format 1213 PASS; ten focused tests PASS. Correct control exits 0; original reuse,
omitted provider guard and omitted route guard each actually exit 1 (not build failures).
Targeted clang-tidy on Declaring/StructureSurfaceRefinement has zero user findings.
Logs /tmp/outshine-route-source-{format,focused,control-results,targeted-tidy}.log.
4b54277f2: fourteen focused tests and all surface/route controls PASS. Full lint exits 0,
including 257 clang-tidy units without findings and all repository/API guards. Both route
source cases PASS in the completed Places suite: 38/44 PASS, zero FAIL, two Olympiaturm
timeouts and four Graz/Wien UNPREPARED. These existing detail/cost failures remain open.
All ten current client PNGs personally opened and pixel-identical to 0d5962a2a; standalone
Hockenheim exits 0. Source-publication defect closed; overall visual/runtime goal stays open;
/tmp/outshine-repair-4b54277f2-{gate-results,full-lint,full-places}.log.
