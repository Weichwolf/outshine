Type: defect
State: active
Parent: 2108
Area: base, engine, include
Tags: architecture, audit, lifecycle
Depends:

# Logging context is owned, scoped and safe to retire

## Beleg und Grenze

Log.cpp hält prozessweite Sink_/Level_ und einen geliehenen thread_local Sink-Zeiger.
Engine::logsTo setzt denselben globalen Sink für alle Instanzen. Unit-Labels liegen in
einem eigenen thread_local 32-Byte-Puffer. Globale Registrierung bleibt umzubauen.
Logging.h verlangt bereits quieszente Registrierung und eine ausreichend lange Sink-Lebensdauer;
vertragswidriges gleichzeitiges Austauschen wird nicht als nachgewiesener Engine-Race ausgegeben.

## Entscheidung

Benchmark: expliziter Engine-Kontext, verschachtelbares RAII und begrenzte Diagnosepfade.
Sink-Registrierung an Engine/Host-Kontext binden, Worker erhalten diesen Kontext ausdrücklich.
Scopes sichern/restaurieren den vorigen Zustand. Registrierung/Abmeldung und Drain brauchen
nachprüfbare Lebensdauer; Debug-Ausgabe darf nicht unbegrenzt im Frame allokieren oder blockieren.
Prozessweiter Sink höchstens ausdrücklich vom Host gewählter Adapter, kein versteckter Default.

## Abnahme

- [ ] Zwei Engines mit verschiedenen Sinks; keine fremden Meldungen und kein Dangling-Sink.
- [x] Verschachtelte Unit-/Sink-Scopes restaurieren den äußeren Kontext, auch bei frühem Return.
- [x] Thread-Sink funktioniert ohne globalen Sink; getrennte Thread-Aufzeichnungen geprüft.
- [ ] Shutdown wartet nur gemäß begrenztem Vertrag.
- [x] Bisheriger Code verletzt das Kontextoracle ohne Buildfehler.
- [ ] Thread-/Lifetime-Tests und Lint; Framekosten mit aktivem Logging getrennt messen.

## Lokale Scopes: implementierter Vertrag
Vorhandene thread_local Unit ist bereits ein besitzender 32-Byte-Puffer, kein roher
Labelzeiger. Scopes sichern/restaurieren diesen Puffer beziehungsweise den geliehenen
Thread-Sink ohne Allokation und sind nicht kopier-/verschiebbar. Labels als StringView
übernehmen, maximal 31 Bytes plus Terminator. Sink vor Nullprüfung auswählen.
Unabhängige Callback-Aufzeichnung prüft Verschachtelung, frühen Return, Kopie eines
veränderten Labels, Trunkierung und getrennte Threads. Altcode scheitert, korrigierter Regressionstest grün.
Keine Bildänderung erwartet. Globale Registrierung und Engine-Isolation bleiben offen.

Nachweis: LoggingScopesRestoreThreadContext prüft fünf Verträge, alle grün;
Altcode scheitert ohne Buildfehler. Finales make lint: 182 tidy-Befunde, 330
Dokumentationsdiagnosen, 32 Repository-Tests grün; drei bekannte rote Gruppen.
Keine visuelle Änderung, kein neuer Render und keine TSan-/Shutdown-Abnahme behauptet.
