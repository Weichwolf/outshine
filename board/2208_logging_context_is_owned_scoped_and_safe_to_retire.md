Type: defect
State: active
Parent: 2108
Area: base, engine, include
Tags: architecture, audit, lifecycle
Depends:

# Logging context is owned, scoped and safe to retire

## Beleg und Grenze

Log.cpp hält den Prozess-Level und einen geliehenen thread_local Sink-Zeiger. Jede Engine besitzt
ihren `Diagnostics`-Zeiger; ihr Scope setzt und restauriert den Thread-Sink für den jeweiligen
Aufruf. Unit-Labels liegen in einem eigenen thread_local 32-Byte-Puffer. GroundStack übergibt
den Sink ausdrücklich an TilePool-Worker. `LogSink::Write` ist `noexcept`; `Logging.h` verlangt
quieszente Registrierung und ausreichende Sink-Lebensdauer. Gleichzeitiges Austauschen ist kein
nachgewiesener Race-freier Vertrag.

## Entscheidung

Benchmark: expliziter Engine-Kontext, verschachtelbares RAII und begrenzte Diagnosepfade.
Sink-Registrierung an Engine/Host-Kontext binden, Worker erhalten diesen Kontext ausdrücklich.
Scopes sichern/restaurieren den vorigen Zustand. Registrierung/Abmeldung und Drain brauchen
nachprüfbare Lebensdauer; Debug-Ausgabe darf nicht unbegrenzt im Frame allokieren oder blockieren.
Prozessweiter Sink höchstens ausdrücklich vom Host gewählter Adapter, kein versteckter Default.

## Abnahme

- [x] Zwei Engines mit getrennten Sinks empfangen keine fremden Meldungen; eine abgemeldete Route
      erhält keinen späteren Callback.
- [x] Verschachtelte Unit-/Sink-Scopes restaurieren den äußeren Kontext, auch bei frühem Return.
- [x] Thread-Sink funktioniert ohne globalen Sink; getrennte Thread-Aufzeichnungen geprüft.
- [x] TilePool-Shutdown weckt und joint Worker/Carrier, unterbricht die ausstehende Abfrage und
      kehrt innerhalb einer Sekunde zurück.
- [x] Bisheriger Code verletzt das Kontextoracle ohne Buildfehler.
- [x] Worker-Ereignis, Sink-Isolation und Shutdown-Lebensdauer sind geprüft.
- [ ] Vollständiger Lint-Gate und Framekosten mit aktivem Logging getrennt messen.

## Lokale Scopes: implementierter Vertrag
Vorhandene thread_local Unit ist bereits ein besitzender 32-Byte-Puffer, kein roher
Labelzeiger. Scopes sichern/restaurieren diesen Puffer beziehungsweise den geliehenen
Thread-Sink ohne Allokation und sind nicht kopier-/verschiebbar. Labels als StringView
übernehmen, maximal 31 Bytes plus Terminator. Sink vor Nullprüfung auswählen.
Unabhängige Callback-Aufzeichnung prüft Verschachtelung, frühen Return, Kopie eines
veränderten Labels, Trunkierung und getrennte Threads. Altcode scheitert, korrigierter Regressionstest grün.
Keine Bildänderung erwartet. Globale Registrierung und Engine-Isolation bleiben offen.

`LogSinkScope` restauriert nun ebenfalls seinen vorherigen globalen Sink; der
Kontexttest prüft äußere, innere und danach wiederhergestellte Ausgabe.

Nachweis: LoggingScopesRestoreThreadContext prüft die lokalen Kontextverträge;
Altcode scheitert ohne Buildfehler. Finales make lint: 189/189 Analyse-Einheiten,
0 Befunde. Keine visuelle Änderung, kein neuer Render und keine TSan-/Shutdown-
Abnahme behauptet.

## Engine-Anbindung und Ausgabeserialisierung
Framing.cpp::logsTo setzt den Engine-eigenen geliehenen Sink. Client Main und PlaceCamera nutzen
diesen Pfad. Kontext muss über GroundStack::Open nach TilePool sowie über TerrainLoader und Tasks
weitergegeben werden; bloßer ThreadScope um einen Engine-Aufruf erreicht Worker nicht.
GroundStack::Close und Tasks-Join müssen vor Freigabe des geliehenen Sinks abschließen.
TextLogSink::Write zerlegt ein Ereignis in mehrere print-Aufrufe und erfüllt dadurch
die Callback-Nebenläufigkeit nicht auf Ereignisebene. Pro Sink einen Mutex über die
gesamte Ausgabe einschließlich Flush halten. Synchroner blockierender Diagnoseadapter,
keine Zusage begrenzter Framekosten. Unabhängiger Mehrthreadtest prüft vollständige
Zeilen und exakte Ereignismenge; Negativkontrolle am bisherigen Code. Zwei separate
Sinks am selben FILE sind damit nicht koordiniert und bleiben Host-Verantwortung.

Textausgabe umgesetzt: Mutex schützt vollständiges Ereignis samt Flush. Vier Threads
mit je 64 Ereignissen und 32 Feldern ergeben exakt 256 unvermischt lesbare Zeilen.
Altcode verletzt dieses Oracle ohne Buildfehler; beide Logging-Regressionen grün.
Der öffentliche Zwei-Engine-Test löst ohne SDL-Video je eine Renderdiagnose aus und prüft
Routentrennung sowie Abmeldung. Worker-Lebensdauer und begrenzte Diagnosekosten bleiben offen.

GroundStack zerstört GroundStream vor TilePool; TilePool setzt Stop, weckt beide Arbeitsgruppen
und joint sie vor Rückkehr. Die Pending-Fetch-Regression prüft Ticket-Abbruch, keine vollständige
Retry-Wartezeit und die Ein-Sekunden-Grenze. Ein Worker-Ereignis mit instanzgebundenem Sink und
eine Messung aktiver Diagnosekosten bleiben offen. `WorkerDiagnosticsStayWithConfiguredSink`
erzwingt eine Carrier-Ablehnung, beobachtet genau ein `tile_refused`-Ereignis im konfigurierten
Sink und bestätigt nach Pool-Zerstörung unveränderte Ereignismenge.
