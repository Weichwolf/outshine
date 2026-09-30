Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2092
Depends:
Area: engine, host, world, streaming
Tags: compute, io, budgets, ownership

# World processing uses one compute worker independently of parallel IO

## Ergebnis und belegte Lücke
DSM, Original-OSM und Wetter laden parallel; schwere Weltverarbeitung verwendet
genau einen Compute-Worker. Render- und Audioarbeit behalten eigene Zuständigkeiten.
Aktuell wählt Tasks::ComputeThreads bis zu acht Threads; TerrainLoader leitet bis
zu sechs zusätzliche TilePool-Arbeiter ab. Ein einzelner Loader mit einem Worker
belegt deshalb keinen gemeinsamen Engine-Vertrag. Umsetzung im DSM-/OSM-Ausbau.

## Architektur und Umsetzung
- Engine besitzt einen gemeinsamen Compute-Executor. Terrain-Decode/Resampling,
  OSM-Parsing, Netz-/Geometriegenerierung und Wetterinterpretation liefern endliche
  Jobs dorthin. TilePool und Generatoren starten keine parallelen privaten Pools.
  Bestehende Besitzer, Abbruch, atomare Publikation und Straßenalgorithmen erhalten.
- libcurl-Multi besitzt die Netzwerktransfers und nutzt begrenzte Parallelität;
  unabhängige Quelldaten werden gebündelt angefordert, nicht seriell abgewartet.
  Lokale IO hat begrenzte eigene Arbeiter. Anzahl und In-flight-Bytes folgen
  deklarierten IO-/Speicherbudgets, nicht der Anzahl logischer CPU-Kerne.
- IO wartet unabhängig vom Compute-Executor. Ein Compute-Job mit fehlenden Bytes
  publiziert Bedarf und gibt den Worker frei; keine blockierende Downloadschleife
  auf dem einzigen Compute-Worker. HTTP-Buchhaltung ist keine Geometriearbeit.
- Endliche Jobs und begrenzte Queues erlauben Priorisierung, Rückstau und Abbruch.
  Kamerabedarf, Lebensdauer und Quellrevision qualifizieren jedes Ergebnis.
  Keine prozessweite CPU-Affinität; ein Worker begrenzt aktive Weltverarbeitung,
  der Scheduler darf ihn zwischen Hardwarekernen verschieben.
- Rendern folgt der SDL-/Host-Threadzuständigkeit, Audio seinem Echtzeitpfad.
  Beide konsumieren vollständige Publikationen und warten nicht auf IO/Compute.
  Koordination im Frame bleibt begrenzt; sie führt keine großen Weltjobs aus.

## Abnahme
Gemessene aktive Transformationsjobs bleiben unter parallelen DSM-/OSM-/Wetter-
Anfragen bei eins; Netzwerktransfers laufen gleichzeitig. Gestallte IO sperrt weder
Compute noch Rendern. Keine zusätzlichen TilePool-/Generator-Compute-Threads.
Warmstart und Drehung erhalten Weltinhalte, Straßenqualität und 720p60-Place-Gate;
Zeit-/Bytegrenzen bleiben getrennt belegt. Ein bloßes Herabsetzen eines Pools reicht nicht.
