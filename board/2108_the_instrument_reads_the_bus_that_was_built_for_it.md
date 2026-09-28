Type: debt
State: open
Architecture: planned
Priority: P2
Area: diagnostics, engine, world, client
Tags: architecture, owner, measured
Supersedes: 2113
Depends:

# Measurements have a consumer, units and bounded cost

## Revidierter Befund

Der Audit vom 2026-09-04 ist kein aktueller Implementierungsstatus.
WorldPlacement.cpp liest Yield::Notes und publiziert die Namen/Werte über DiagnosticLedger.
Die Behauptung „ZERO readers“ und der daraus abgeleitete Blocker für 2111 sind erledigt.
base/io/Telemetry.h/.cpp besitzt weiterhin keinen externen Source-/Sink-/Tick-Consumer.
Ein allgemeiner TelemetryBus ist deshalb keine Voraussetzung für Profiling oder Wald.
2208 besitzt host-eigenes Logging; dessen Gesamtabschluss blockiert numerische Traces nicht.

## Architektur und Empfehlung

Domain-Owner liefern gebündelte native Werte; Engine aggregiert an der Messgrenze;
Client hält begrenzte Framefolgen und berechnet p50/p95/p99 außerhalb des Framepfads.
Einheit, Messintervall, Quelle und Vollständigkeit gehören zum Messvertrag.
Host-/Fence-Wartezeit ist keine GPU-Passzeit. 2092 besitzt die konkrete Kostenmessung,
2314 ihre spätere Verwendung im gemeinsamen Budget. Keine doppelte Statistik-Registry.

Vor weiterer Infrastruktur genau den fehlenden Consumer und seinen Fehlernutzen benennen.
Vorhandene Werte benutzen. Keine ungenutzten Getter oder Zähler allein für Vollständigkeit.
Ein kurzlebiger Sample-Borrow darf nicht im Client gespeichert werden; Traces besitzen
Kopien ihres begrenzten Datensatzes. Fehler-/Ereignislogs bleiben getrennt von Messreihen.
Den ungenutzten TelemetryBus erst nach vollständiger Caller-/Public-Header-Prüfung in
einem kleinen WI entfernen oder mit einem tatsächlich nötigen Consumer ersetzen.
Nicht DiagnosticLedger pauschal nach base verschieben, um Domain-Owner zu koppeln.

## Abnahme

- [ ] Jeder neue Messwert hat Leser, Einheit und dokumentierten Samplingzeitpunkt.
- [ ] Bekannte analytische Folge ergibt unabhängige Quantile; fehlende/abgebrochene
      Frames bleiben unvollständig statt still aus der Auswertung zu verschwinden.
- [ ] Begrenzte Speicherung und gemessene Observer-Kosten; hot-path-Allokationen offen.
- [ ] Keine zweite Messbus-Implementierung für dieselben Daten. Logging-Verträge unter 2208.
- [ ] Fokussierte Tests und vollständiges Lint für einen aktivierten Consumer-Schritt;
      Dokumentation allein erfüllt diesen WI nicht.
