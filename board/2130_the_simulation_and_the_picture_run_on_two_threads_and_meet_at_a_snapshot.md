Type: debt
State: open
Area: engine, render, audio
Tags: architecture, performance, determinism
Parent: 2188
Depends: 2124, 2190, 2191

# Simulation and rendering will exchange owned snapshots within SDL thread rules

## Befund und Entscheidung

Engine::advance führt Updates, Audio-Snapshot und Draws seriell am aufrufenden Thread aus.
Simulation ohne Renderziel muss über öffentliche API prüfbar werden; Ausgabe darf
die Simulations-/Audio-Ereigniszeit nicht bestimmen.
EngineHeld.h enthält gemeinsam erreichbaren Zustand; Audio liest Sources mit atomarem
Told-Index. Ein atomarer Index allein beweist keine sichere Wiederverwendung des Puffers.
Vor Parallelisierung jeden Producer/Consumer inklusive Audio und Shutdown inventarisieren.
Auch Log::Sink_/Level_ und LogSink-Callbacks prüfen: Registrierung ist derzeit nicht
synchronisiert; TextLogSink schreibt eine Zeile in mehreren Calls. Dokumentierte
Quieszenz allein ist noch kein vollständiger Laufzeit-/Shutdown-Nachweis.

**Benchmark**: Filament trennt Frontend und Driver durch einen CommandStream.
https://github.com/google/filament/blob/main/filament/src/details/Engine.cpp
Unreal veröffentlicht Game-/Render-Thread-Trennung. Eine bestimmte RAGE-Kernbelegung
ist nicht belegt. Frühere feste 2P/4E-Zuordnung und die Behauptung, zwei Puffer reichten
immer, sind verworfen: Scheduling und Pufferbesitz benötigen einen Nachweis.
https://wiki.libsdl.org/SDL3/SDL_WaitAndAcquireGPUSwapchainTexture
SDL-Präsentation bleibt am Fenster-Erzeugerthread; CommandBuffer wechseln ihren
Acquire-Thread nicht. Simulation kann auf Worker laufen, Video auf dem Main-Thread.

Immutable Snapshots/Deltas mit Generation, Frameursprung, Simulationszeit und Besitz.
Begrenzte Queue mit expliziter Backpressure; kein Überschreiben noch gelesener Puffer.
Rendern des letzten vollständigen Snapshots; Ressourcenänderungen atomar mit ihm.
Audio-Callback ohne Allokation/Blockierung, konsistenter Snapshot und sicherer Rückgabe.
Physikereignisse mit Simulationszeit auf die Audio-Samplezeit abbilden; Geräte-/
Pufferlatenz und Präsentationszeit messen. Kein pauschaler Ein-Frame-Vorlauf für Ton;
Scheduler-Jitter und Blockgrößenwechsel dürfen Ereigniszeitpunkte nicht verschieben.
Fixed-Step-Simulation mit begrenztem Nachholen; Renderinterpolation und Zeitverzug
explizit messen. Keine Hardware-Kernreservierung ohne Messung und Plattformgarantie.

## Abnahme

- [ ] Race-/Besitzprüfung bei absichtlich langsamem Consumer und schnellem Producer.
- [ ] Fensterthread/CommandBuffer-Verträge aus 2190 halten bei paralleler Simulation.
- [ ] Queuegröße, Snapshotalter, End-to-end-Framezeit sowie Sim/Render getrennt messen;
      isolierte p99 dürfen weder addiert noch als Beweis für Gewinn ausgegeben werden.
- [ ] Gleiche Inputs/Seeds erzeugen bei verändertem Scheduling gleiche Simzustände;
      PNG-Vergleich mit festem Snapshot, nicht zufällig anderem Simulationszeitpunkt.
- [ ] Audio bleibt störungsfrei; Shutdown wartet kontrolliert außerhalb des Framepfads.
- [ ] Negativkontrolle überschreibt einen gehaltenen Snapshot und verletzt das Oracle.
- [ ] Bewegung/PNG-Abnahme gemäß 2092/2169, keine bloße Threadzahl als Erfolg.
