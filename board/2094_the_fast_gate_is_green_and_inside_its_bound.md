Type: bug
State: active
Architecture: planned
Priority: P0
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

## Runner preflight failure

Tool-version failure previously left `units` empty despite known database entries.
The timeout fixture exposed this when Python startup exceeded its 0.1-second budget.
Record each missing unit as incomplete with the abort diagnostic and unknown duration;
never invent a successful process exit or change timeout thresholds. A missing-tool
fixture with two declared sources deterministically reproduces the missing records.
Existing per-unit records remain intact; malformed databases still cannot declare units.

## Current evidence

- On 2026-09-27, `LINT_JOBS=2 make lint` checked 247/247 units with zero tidy
  findings and all 32 tests. Documentation and shader-artifact gates passed.
- All 340 declared Khronos frame references are present and verified in the
  local cache. The 176 referenced cases now declare current Blender 5.2.2 on
  METAL/Apple A18 Pro GPU. 337 images remained byte-identical; three changed
  after verified rerender and visual inspection (2226).
- `CubeVisibility` and `LightVisibility` remain unsupported by the local
  Blender importer (`KHR_node_visibility`); `AnimationPointerUVs` raises
  `KeyError: animations`. They have no image pins, and no fallback is implied.

## Proof

- A real tidy warning, skipped unit, malformed database, missing tool, timeout,
  signal or duplicate configuration fails the tidy tests.
- Missing/corrupt reference bytes and incomplete animation records fail the reference
  tests. Normal tests never create cache entries.
- A full `make lint` is green only when every gate reports terminal success; its
  concise transcript and detailed reports remain under `${TMPDIR:-/tmp}`.

## Remaining work

1. Repair or explicitly replace the unsupported Blender importer path for the
   three unpinned cases (2226). Khronos client/image mismatches are separate
   render defects; successful reference-cache validation does not accept them.
2. Resolve the separate `make test` failures in their owning WIs; neither gate may
   hide a failure by changing bounds, inputs or assertions.
3. Complete the API-contract and shader-artifact audits required by 2188/2093/2152.

## Belegter Board-Guard-Befund 2026-09-28

ABoardIdIsIssuedOnce vergleicht historische Additions seit seiner Geburt nur mit IDs
VOR dieser Grenze. Doppelte neue IDs innerhalb desselben Fensters und zwei aktuelle
Dateien mit gleicher ID, einschließlich untracked Dateien, werden nicht zurückgewiesen.
Eine versehentlich zweite 2140-Datei passierte lint-docs; vor Commit entfernt.
Kleiner eigenständiger Schritt: aktuellen Boardbestand und historische Erstvergabe
beide prüfen; Scratchpfade pro Prozess/Worktree statt geteilter /tmp/outshine-ids-*.
Unabhängige temporäre Git-Fixtures müssen alte Wiedervergabe, zwei neue gleiche IDs,
aktuellen untracked Zwilling und zulässige Umbenennung einer bestehenden ID trennen.
Diesen Befund nicht durch Historie-Umschreiben oder Änderung gültiger WI-IDs beheben.

## Linker-Warnungen

618371b14 besteht lint/fokussierte Suites, deren Buildlog meldet jedoch doppelte SDL3-
Libraries und /opt/homebrew/lib-rpaths. Kein warnungsfreier Buildclaim. Doppelte Flag-
Owner im Harness/Library-Pkgconfig lokalisieren und am Produzenten deduplizieren;
Warnungen weder unterdrücken noch aus dem Log filtern. Relevanten Linkschritt erneut prüfen.
