Type: debt
State: open
Area: include, engine, world, render, generators, base
Tags: architecture, owner
Parent: 2188
Depends:

# Names and module boundaries expose the engine architecture

## Ziel

include/ enthält ausschließlich die minimale dokumentierte öffentliche API.
src/ kapselt deren Implementierung in fachlich zusammenhängenden Modulen.
Klassen, Methoden, Dateien und Verzeichnisse benennen ihre tatsächliche Aufgabe;
keine autorenspezifischen Metaphern oder Sammeldateien für unabhängige Systeme.

## Befund und Entscheidung

EngineHeld/Live bündeln mehrere Besitzer und Phasen. Telling.cpp enthält Metriken,
Audio-Publikation, Renderer-Aufbau und Audio-Verdeckung. Blocks ist ausschließlich
Audio-Verdeckung, nicht Physikkollision. Restand bezeichnet mehrere verschiedene
Austauschoperationen; eine pauschale Übersetzung in Recenter wäre falsch.

Pro Consumer Zuständigkeit und Ownership prüfen, dann vollständig migrieren:
- Audio-BVH-Aufbau und Abfrage in AudioOcclusion.cpp, mit expliziten Methodennamen.
- Importkonvertierung, native Assets und Animationsinstanzen nach 2150 trennen.
- Öffentliche Typen und Header nach 2096 fachlich auffindbar schneiden.
- Simulation, Streaming, Rendering und Audio über dokumentierte Übergaben koppeln.
- Include-Pfade, Namespaces und Build-Tiers müssen dieselben Grenzen ausdrücken.

Keine pauschale Eins-zu-eins-Rename-Tabelle. Gemischte Verantwortungen aufteilen;
kein kompletter ECS oder zusätzliche Modulhierarchie ohne konkreten Consumer.
Aufrufer, Builddeklaration, Installation, Tests und Doxygen gemeinsam migrieren.
Keine Kompatibilitätsalias-Schicht für falsche interne Begriffe aufbauen.

## Referenzmaßstab

Benchmark: Filament Engine/Scene/RenderableManager und veröffentlichte Cesium-
Asset-/Streaming-Verträge als Beispiele klarer Zuständigkeiten. 2188 hält Quellen.
Keine Behauptungen über proprietäre RAGE-Klassen oder eine universelle Verb-Liste.

## Abnahme

- [ ] Öffentliche Typen sind über fachlich benannte öffentliche Header auffindbar.
- [ ] Externer Minimalclient benötigt keine src/-Header oder Checkout-Includepfade.
- [ ] Module haben gerichtete Abhängigkeiten und eindeutige Ressourcenbesitzer.
- [ ] Geänderte Namen bezeichnen nach Aufruferprüfung genau die ausgeführte Aufgabe.
- [ ] Alte Namen/Includes vollständig entfernt; Build, Doxygen und Tests migriert.
- [ ] Rein strukturelle Änderungen erhalten Verhalten und Referenzbilder;
      fachliche Korrekturen erhalten unabhängige Orakel statt falscher Altbilder.
- [ ] make lint samt clang-tidy und passende Make-Tests ohne neue Befunde.
