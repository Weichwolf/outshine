Type: defect
State: active
Architecture: ready
Parent: 2230
Depends:
Priority: P0
Area: engine, rendering, vegetation
Tags: ownership, handles, world-candidate

# Impostor handles survive world publication

## Reproducer and measured cause

`prepare Hockenheimring 30` with vegetation enabled failed after about 5 s:
slot 4, generation 1 was updated in a published world containing only four
slots. Trace: a candidate copied four published pieces, then vegetation
created pieces while `SceneRenderer` still had an active candidate editor.
`World.GroundBuild` was already null, so checking that pointer alone missed
the active renderer candidate. The old claim that Refined candidates use
`PieceSources::Omit` is false for this path: `GroundWorldCandidate` defaults
to `Copy`. The renderer's `CandidateEditorScope` selects candidate resources
unless a `PublishedWorldScope` overrides it.

## Binding ownership decision

Engine vegetation updates run in the renderer's published-world scope.
Ground builds skip vegetation updates; if a renderer candidate remains after
the ground pointer clears, `VegetationStreaming` may poll/cache CPU results
but defers creation of new GPU pieces until that candidate is gone.
The active world owns its existing handles; candidate copying preserves them.
Never repoint a handle to an unrelated slot. Keep vegetation optional in
scenario tests; Hockenheim's road proof disables it declaratively.

## Acceptance

- Empty, Playable and Refined world transitions with vegetation enabled keep
  impostor updates valid. A stale handle is rejected in a negative control.
- Repeated candidates do not grow piece/source bytes without bound; cancellation
  leaves the prior scene and prototype rows intact.
- A public-client scenario with vegetation enabled progresses past the failing
  transition. The 30 s Hockenheim reproduction now passes the former 5 s
  failure and times out only on pending vegetation; repeat with a longer bound
  or a smaller vegetation fixture before closing this WI. Run focused
  ownership tests, format and lint; inspect a PNG when refined capture is ready.
