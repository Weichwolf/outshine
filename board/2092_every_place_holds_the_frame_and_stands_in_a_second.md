Type: bug
State: open
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
