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
