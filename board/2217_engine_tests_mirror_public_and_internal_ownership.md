Type: task
State: active
Parent: 2188
Area: tests
Tags: architecture, iteration
Depends:

# Engine tests mirror public and internal ownership

## Problem
`test/outshine/conventions` mixes API contracts, import internals, numerical
algorithms, generators and rendering. Directory names do not identify ownership.
The runner also grants implementation include paths to public API tests.

## Decision
Mirror public headers under `test/outshine/include/` and implementation components
under `test/outshine/src/`, with a directory per tested header/component.
Keep full place scenarios under `test/outshine/integration/places/`.
This follows the project's existing include/src boundary; no external framework
or replacement test oracle is needed. Existing independent cases remain intact.
Keep MVT sanitizer arms, device validation and process-allocation instrumentation.
Resolve build profiles by component, not by a second list of individual tests.
Update runner selection, includes, claims and live documentation references.
Public test translation units receive no implementation include directories.

## Acceptance
- Every existing C++ case appears exactly once under its new responsibility.
- Existing specialized execution arms remain discoverable and execute.
- Moved tests build and run; unavailable integration inputs are reported explicitly.
- No conventions bucket or stale executable test paths remain.
- `make format` and `make lint`, including clang-tidy, run after migration.
- No engine or image behavior changes are intended; assertions remain unchanged.

## Bei der vollständigen Ausführung gefunden
Sechs Fixtures benutzen CameraPlacement ohne outshine-Namespace; Qualifizierung
korrigieren, Assertions unverändert. Der Trigger-Dwell-Fall verlangt StepS=0-Fallback,
was dem dokumentierten positiven StepS-Vertrag und dem bestehenden Negativtest
DeclarationRejectsInvalidSimulationTiming widerspricht. Mit explizitem Zeitschritt
die Zeitgrenze von beiden Seiten prüfen. Schach-Wiederholung bleibt WI 2179.

## Harness-Kosten
Die gespiegelten Ordner vervielfachen identische Quellgruppensätze. Der bisherige
Link-Audit wiederholt deren komplette Prüfung pro Verzeichnis; der Claim läuft
nach 120 s ins unveränderte Limit. Exakt gleiche, vollständig aufgelöste Gruppen
innerhalb eines Audit-Laufs einmal prüfen; unterschiedliche Sätze bleiben getrennt.
Die vorhandenen Duplicate-, Missing- und Ghost-Negativkontrollen müssen weiter
fehlschlagen. Keine Cache-Wiederverwendung zwischen Läufen oder Toleranzänderung.
