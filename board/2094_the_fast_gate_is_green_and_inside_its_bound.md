Type: bug
State: active
Parent: 2188
Area: test, gate
Tags: measured, gate
Depends: 2093, 2131, 2152

# Gates report complete evidence or fail

## Problem

A green zero is valid only when every declared translation unit, public header and
reference input was checked. Missing oracle bytes, skipped sources, tool failures and
incomplete output must remain red. Tests may not regenerate or replace immutable
reference pins.

## Decision

The tidy runner compares every `src/*.cpp` against `compile_commands.json`, records
one terminal status per unit and rejects missing, duplicate or failed runs. Formatting
uses the owned-file inventory. Documentation requires a fresh Doxygen XML result for
every public header. The reference gate validates every declared SHA-256 digest,
dimensions and animation frame time in the external cache.

The reference cache is deliberately outside Git. Populate it only through the
manifest's pinned local source and Cycles on a verified GPU; record the backend/device
in provenance. Do not alter manifests, loosen the check or silently render on CPU.

## Current evidence

- Tidy: 189/189 units, 0 findings (2026-09-16). The last direct-include finding in
  `Advancing.cpp` was repaired in `4c6ac6a99` and the complete run repeated.
- Documentation: 24/24 public headers, 0 diagnostics (2026-09-16). This proves
  coverage, not the API contract audit in 2188/2093.
- `make lint` is red at `test-reference-cache`: the local cache contains 6 of the
  declared 265 distinct image pins; 259 are missing. The first uncovered pin is
  `68c1bce3…`. Pin `8077bf23…` and pin `39fa55fa…` were independently recreated from
  the local Khronos asset-generator clone with Cycles METAL on Apple A18 Pro GPU and
  matched their declared bytes exactly.
- The gates before the cache check pass: formatter, comment scanner, compile graph,
  tidy-runner fixtures and documentation/reference-store fixture suites.

## Proof

- A real tidy warning, skipped unit, malformed database, missing tool, timeout,
  signal or duplicate configuration fails the tidy tests.
- Missing/corrupt reference bytes and incomplete animation records fail the reference
  tests. Normal tests never create cache entries.
- A full `make lint` is green only when every gate reports terminal success; its
  concise transcript and detailed reports remain under `${TMPDIR:-/tmp}`.

## Remaining work

1. Supply the complete external reference cache from reproducible GPU provenance.
2. Resolve the separate `make test` failures in their owning WIs; neither gate may
   hide a failure by changing bounds, inputs or assertions.
3. Complete the API-contract and shader-artifact audits required by 2188/2093/2152.
