Type: defect
State: open
Parent: 2108
Area: base, engine, include
Tags: architecture, audit, lifecycle
Depends:

# Logging context is owned, scoped and safe to retire

## Beleg und Grenze

Log.cpp hält prozessweite Sink_/Level_ und rohe thread_local Zeiger. Engine::logsTo
setzt denselben globalen Sink für alle Instanzen. Emit verwirft auch einen gesetzten
ThreadSink_, wenn Sink_ null ist. LogThreadSinkScope und LogUnitScope setzen beim
Verlassen auf null statt den vorigen Kontext wiederherzustellen: verschachtelte Scopes
verlieren den äußeren Kontext. Das ist direkt am Code belegt, kein nur vermuteter Race.
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
- [ ] Verschachtelte Unit-/Sink-Scopes restaurieren den äußeren Kontext, auch bei frühem Return.
- [ ] Thread-Sink funktioniert ohne globalen Sink; Shutdown wartet nur gemäß begrenztem Vertrag.
- [ ] Negativkontrolle des bisherigen Scope-Destruktors verletzt das Kontextoracle.
- [ ] Thread-/Lifetime-Tests und Lint; Framekosten mit aktivem Logging getrennt messen.
