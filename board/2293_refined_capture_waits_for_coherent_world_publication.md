Type: defect
State: done
Architecture: ready
Parent: 2191
Depends: 2294
Priority: P0
Area: engine, client, render
Tags: publication, capture, hockenheim

# Static capture waits for coherent refined world publication

## Evidence

Repeated Hockenheim still captures at lap station 1373.068 m have identical
camera, surface ID, normal and depth but foreground RGB alternates between
(4,10,26) and (91,88,83). The dark frames retain 147955 shadow-atlas texels
above clear and no building footprints; a gray frame retains 189062 texels and
2267 footprints. All tested still captures report `settled(Refined)=false`.
`CaptureScenarioView` calls `preload`, which currently returns at Playable.
Render-plan redeclaration is not an established cause: repeated runs with the
same plan produce both colours. Compare plans only at equal refined readiness.

## Contract and ownership

`Engine::preload(patienceS, WorldQuality)` extends the synchronous public wait
without changing the existing Playable overloads. Refined succeeds only when
`settled(Refined)` is true after coherent world publication. Internal preload
ground work and timeout diagnostics use the requested quality. Invalid budgets
fail before work, no-ground scenes complete, and timeout preserves partial
progress. `src/client/ScenarioCapture` requests Refined for a static diagnostic
capture, then renders and reads the same published world. A paced motion capture
retains its frame-by-frame readiness trace and does not block on every frame.

## Falsifiable acceptance

- A pinned small world with delayed inputs reaches Playable first; Refined
  preload does not return success until `settled(Refined)`. A too-short budget
  reports outstanding refined work. Existing preload behaviour remains Playable.
- Two fresh Hockenheim still captures at 74.850 s both report Refined and have
  matching surface/color/shadow metrics within a declared tolerance. Compare a
  retained-output plan with the default plan at equal readiness. If colour
  still differs, open a separate render-plan defect with that evidence.
- `--probe-pixel` reports the quality at which its frame was captured. Focused
  tests, `make format` and `LINT_JOBS=2 make lint` pass.

## Result

`preload(patienceS, WorldQuality)` waits for the requested publication quality;
the original overloads retain Playable. Invalid deadlines and groundless scenes
are covered; incomplete offline input gives an explicit Refined timeout. Two
fresh static Hockenheim probes at 74.850 s reached Refined with identical
1280x720 PNGs and pixel (91,88,83). The first-declaration diagnostic-output
plan and client-added outputs also match pixel-for-pixel. Final preload took
697/698 ms from a complete warm cache. A paced Refined frame still differs;
that is WI 2295, not a reason to weaken the quality wait.
