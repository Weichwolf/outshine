Type: defect
State: open
Architecture: ready
Priority: P0
Parent: 2234
Depends:
Area: ground, streaming

# Nested terrain fetch admission makes progress

## Reproduction

At 86eee1580, `DelayedFieldsDoNotBecomeRims` proves deferred admission remains
pending, but an experimental run with `TilePool::Config::OutstandingMost=1` and a
released 4x4 Terrarium source never resolved the field. The field job occupies the
sole admitted carrier while `StitchedGrid` needs source fetches; `PoolTerrain::Take`
sees the full pool and defers those fetches. Repeated caller polling produced about
1.35 million fetch attempts and 4.4 thousand dropped field jobs in five seconds.
The field job cannot complete without the fetch; retrying it does not free the
dependency chain. The production default is larger, but the bounded-queue contract
must work under a valid tight configuration and under temporary saturation.

## Decision

Treat source fetch as a dependency of a stitched-field job, not an independent
peer contending for the same final slot. Preserve a strict bound on live work and
decoded bytes. Choose a concrete admission model after auditing `TilePool::Field`,
`PoolTerrain::Take`, worker ownership and cancellation: reserve dependency capacity
at field admission, or let one field job yield its carrier while awaiting admitted
fetches. Do not enlarge `OutstandingMost` silently, execute network work on the
frame thread, spin/repost on Deferred, or turn a transient shortage into Absent.
Specify whether zero/one-slot configurations are supported or rejected explicitly
at construction; if supported, demonstrate eventual progress. A terminal source
failure still propagates as Absent/Refused according to the source contract.

## Acceptance

- A deterministic delayed-source test holds the fetch, requests a field with
  `OutstandingMost=1`, releases it, and reaches Ready with a watchdog. Verify the
  fetch runs off the caller and the request count remains bounded without hot spin.
- Run the same case at limits 2 and production default; pressure from unrelated
  fields cannot starve already-admitted dependencies. Cancellation and shutdown
  release every reservation exactly once; no stale field result is published.
- Keep decoded-cache and source-revision semantics from WI 2248. Preserve the
  pixel-complete Malcesine result and measured frame path from WI 2253.
- `make format`; focused TilePool/HeightSheets suites; `make lint` with zero tidy
  findings. Profile the tight case and record work/queue maxima, not only elapsed
  completion time.
