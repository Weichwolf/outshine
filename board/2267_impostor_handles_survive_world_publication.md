Type: defect
State: open
Architecture: needs decision
Parent: 2230
Depends: 2224
Priority: P2
Area: engine, rendering, vegetation
Tags: ownership, handles, world-candidate

# Impostor handles survive world publication

## Reproducer and cause to verify

Basel Badischer with vegetation enabled passed the structure-material stage,
then client preload failed: `the piece handle names no live resource in this
world`. The same temporary scenario with vegetation disabled passed that point
and baked over 40,000 buildings. `VegetationStreaming` retains
`ImpostorInstances`; `ImpostorInstances::MoveTo` only changes its renderer
pointer, while each `View` retains a `PieceHandle`. Refined candidates use
`PieceSources::Omit`, so those handles may name pieces absent in the published
candidate. Verify the failing handle's owner/generation at the publication
boundary before implementation.

## Decision required

Give persistent impostor prototypes an explicit candidate residency policy:
either copy their piece sources and preserve valid handles, or create candidate
pieces from retained atlas/source data and atomically replace handles on
publication. Do not copy obsolete structure tiles merely to keep impostors,
and do not repoint a handle to an unrelated resource. Candidate failure leaves
active impostors usable; retirement releases each GPU product after last use.
Keep vegetation optional in scenario tests, as its visual development follows
ground, architecture, light and atmosphere work.

## Acceptance

- Empty, Playable and Refined world transitions with vegetation enabled keep
  impostor updates valid. A stale handle is rejected in a negative control.
- Repeated candidates do not grow piece/source bytes without bound; cancellation
  leaves the prior scene and prototype rows intact.
- A public-client scenario with vegetation enabled progresses past the failing
  transition. Run focused ownership tests, format and lint; inspect its PNG
  when refined capture is independently ready.
