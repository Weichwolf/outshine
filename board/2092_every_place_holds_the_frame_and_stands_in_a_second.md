Type: bug
State: active
Architecture: ready
Priority: P0
Parent: 2169
Area: engine, client, render
Tags: webcam, measured
Depends: 2124, 2123, 2154

# Every place meets the frame budget while the world streams

## Aktuelle Messung

`build/webcam-board-audit.log`, 12ceb790, `make shots`, 1280×720. Alle neun Places haben
0/120 Frames über 16,67 ms; p99 reicht von 2,95 ms (DarmstadtWest) bis 8,82 ms (Koerbersee).
120 Standframes je Ort messen residenten Render, keine fahrende Open World. Vollständige
Tabelle/Hashes in 2169. Daraus folgt kein nachgewiesener Restetat für alle fehlenden Effekte.

## Instrument und Abnahme

Hockenheim 220 s warm/offline wiederholt: 13,200 Frames, p99 13.314/13.040 ms,
40/35 Gesamtframes spät. Einzelne `render()`-Aufrufe dauern 79.52/94.27 ms;
8/5 Render-Aufrufe überschreiten 16.67 ms. `advance()` ist an den größten
Ausreißern teils unter 2 ms. Pass-Messungen halten nur den letzten Frame.

`src/render/SceneRenderer.{h,cpp}` misst erfolgreiche Host-Frames nach Prepare,
Acquire, Upload, Swapchain, Cull, Encode, Fence-Wait, Submit und Abschluss;
`src/engine/FrameMeasurements.cpp` publiziert Worst je Phase ohne öffentliche API.
Ein weiterer 220-s-Lauf misst p99 12.741 ms, 32 späte Frames und maximal
26.944 ms Render, davon 26.312 ms Fence-Wait; Encode höchstens 0.897 ms,
Upload 0.017 ms. Das beweist einen Host-Wait auf GPU-Fortschritt, keine
GPU-Passzeit oder seinen Produzenten. Vor einer Änderung von Frames-in-flight
GPU-Arbeit/Uploads gegen den Stall korrelieren; keine Latenz durch bloßes
Triple-Buffering verstecken. Negativkontrolle: fehlgeschlagenes Submit/Wait
publiziert keine Erfolgsprobe. Device-Suite, 220-s-Trace, `make format`,
`make lint` sind die Abnahme dieses Messschritts.

Kontrollierte 100-s-Ablationen: ohne `lightVisibility` bleibt bei 95.7167 s
ein 30.68-ms-Renderstall; bei 640×360 verschiebt er sich um einen Frame und
dauert 25.78 ms. Schatten und Pixelzahl allein erklären ihn nicht. Ein Plan
ohne `subjects` ist bei Weltgeometrie ungültig, also keine nutzbare Ablation.
Der Motion-TSV koppelt den zuletzt eingereichten Fence-Wait an Upload- und
Crossing-Zähler der aktiven Residency. Diese Zähler können bei einem
Contentwechsel zurücksetzen; ihre Werte sind um ein Frame gegenüber der
aktuellen Renderzeit versetzt. In einem 100-s-Warm/Offline-Lauf dauert
`render()` bei 95.7167 s 22.279 ms, davon 21.671 ms Fence-Wait. Die aktive
Residency meldet in diesem Abschnitt vier zusätzliche Upload-Versuche pro
Frame, aber keine staged Crossings. Bei 62.1 s tritt ein weiterer
24.889-ms-Renderausreißer auf. Der Zusammenhang mit Upload-Arbeit ist damit
beobachtbar, die verursachende GPU-Passzeit noch nicht belegt.

Nächster Eingriff: `SubjectDraw::PlacePiece` lädt jedes neue Stück derzeit mit
`Cross(..., false)` und damit einem eigenen Copy-Submit hoch. Die Residency
besitzt bereits Staging, `FlushCrossings` vor Cull und `CommitCrossings` nach
erfolgreichem Frame-Submit. Neue Stücke auf `Cross(..., true)` umstellen,
ohne den unmittelbaren Mesh-Pfad oder Texturen zu ändern. Besitzer des
Transfers bleibt die Residency; fehlgeschlagene Render-Submits behalten
staged Daten für den Retry. Gegenprobe: vor/nach gleichem 100-s-Warm/Offline-
Hockenheim-Trace Upload-Versuche, Crossings, p99/Max und späte Frames, plus
gleiches PNG und unveränderte 267-Edge-Route. Falls nur der Zähler sinkt,
der Fence-Stall aber bleibt, ist die Hypothese widerlegt; dann GPU-Passzeit
und explizite Byte-/Queue-Last messen statt Frames-in-flight zu erhöhen.
Der 100-s-Vergleich reduziert positive Upload-Versuchs-Deltas von 1680 auf
318, während 4132 staged Crossings im Frame-Pass aufgezeichnet werden.
Finales PNG: 0/921600 Pixel Unterschied; 267-Edge-Route und 538/538
Cache-Hits bleiben. p99 12.702→12.738 ms, späte Frames 15→16;
maximaler `render()`-Aufruf 24.889→74.051 ms (verschiedene Laufvarianz).
Der längste neue Aufruf enthält 71.911 ms Fence-Wait. Batching spart
Submits, löst den Stall aber nicht. Nächster Beleg: GPU-Passzeiten und
Upload-Bytes desselben Frames, nicht ein weiteres Submit-Zähler-Tuning.

- Stillvergleich behalten; zusätzlich deklarierte Geh-/Fahr-/Flugroute mit Tilegrenzwechsel,
  dichter Stadt, bewaldetem Hang, Tunnelportal und mehrstöckigem Verkehrsknoten. Warm-/Cold-
  Cache und deterministischer Datenreplay getrennt; Zeit/Wetter/Seeds/Build im Manifest.
- End-to-end Frame p50/p95/p99, Maximum und Anzahl über Budget; CPU Simulation/Bake/Upload,
  GPU pro Pass, IO-Wartezeit, Heap/VRAM und Queue-Rückstand getrennt messen. CPU-/GPU-p99
  nicht addieren; GPU-Passzeiten aus demselben Frame und End-to-end-Stalls berücksichtigen.
- Mindestens 120 Frame-Regressionsfenster über jeden Übergang, zusätzlich zusammenhängende
  Routen und 2143s Dauertest. Nicht nur nach Preload stehen bleiben und Echtzeit behaupten.
- Ziel aus 60 Hz: 1000/60 = 16,666… ms pro Frame. 0/120 im Regressionfenster und p99 darunter;
  Worst-/Langzeit-Hitches weiter ausweisen. Qualitätsänderungen nur unter Gesamtbudget abnehmen.
- Zeit bis erste brauchbare Welt und bis vollständige Zielqualität separat; bestehendes
  Ziel <1 s bis Stehen bleibt offen. Cache-/Netzannahmen nennen, Providerwartezeit nicht löschen.
- Negativkontrolle: synchronen Rebuild gezielt einfügen, Hitch-Messung muss anschlagen;
  fehlenden Draw/dauerhaft coarse Welt nicht als Performanceerfolg akzeptieren.

Wahl: bewegte Welt als Benchmark wie Unreal/RAGE, portabler SDL-Passnachweis. Die visuelle
Abnahme aus 2169 ist gleichrangig; schnell und geometrisch leer ist kein fertiges Ergebnis.
