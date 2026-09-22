Type: defect
State: active
Architecture: ready
Parent: 2218
Depends: 2247
Priority: P1
Area: client, engine, test
Tags: determinism, streaming, capture

# Place captures need a reproducible streaming snapshot

## Beleg

Zwei aufeinanderfolgende Läufe desselben Codes nach 947d891d9:
`make shots PLACE='--no-vegetation --preload-seconds 120 Graz'`.
Ausgaben `Graz-e1e6a2b6.png` und `Graz-3869196b.png`: 68/921600 Pixel verschieden,
49 um mehr als 1/255; maximale Kanalabweichung 32/255. Nur linker Hang,
x=14..378, y=351..434. Zweiter Lauf ist pixelgleich zum älteren Ausgangsbild.
Beide PNGs geöffnet. Reproduzierbarkeit fehlt; die Ursache ist noch nicht isoliert.
Referenzen: `build/shots/reference/empty-tile-publication/Graz-before.png` und
`Graz-after-first.png`. Keine Toleranzerhöhung und kein Neupinnen zum Kaschieren.

Stand 2026-09-18: Drei Cache-warme Läufe mit `--no-vegetation --preload-seconds 120 Graz`
erzeugten nach dem Capture-Clientpfad `Graz-cf91dc2d.png`; der direkte Vergleich zweier
getrennter Dateien meldet 0/921600 abweichende Pixel. Der Screenshot und Readback liegen
jetzt im RAII-Bereich, öffentliche Weltmutationen sind gesperrt. Das beweist nur den
seriellen Clientlauf. `GroundPublication` schließt während Capture zusätzlich den
Ground-Candidate-Commit vor jeder Live-/CPU-Änderung; dessen Negativtest ist grün.
Pro-Tile-Quell- und Produktrevisionen sowie die vertauschte Workerfertigstellung fehlen weiter.

Malcesine, gleicher Build 2026-09-22: zwei aufeinanderfolgende cache-warme Captures
`--no-vegetation --measures` unterschieden sich in 1/921600 Pixeln, maximal
4/255 bei (1119,251). Terrain-Sheet-Digest und OSM-Tile-Reihenfolge waren gleich;
`TilePieces::Digest` unterschied sich in beiden Hälften. Dieser Digest faltet
sortierte Tile-IDs und Bake-Digests; unklar bleibt, ob Tile-Bestand oder Bake-Inhalt
abweicht. Die Terrain-Press-Abnahme verfolgt diesen Strukturprodukt-Befund nicht.

Sechs weitere normale Malcesine-Captures mit Tile-Provenienz: fünfmal Bild
`762c673c`, einmal `9367bebc`. Im Ausreißer änderte nur Struktur-Tile 24 seinen
Bake-Digest und die Höhenquelle von Fallback (1) auf fein (0). Das erzeugte
554 abweichende Pixel am rechten Ufer (x=1125..1279, y=461..495), maximal
170/255. Beide PNGs geöffnet. `StructureBuildQueue::Posts` wählt feine Blöcke
nach momentaner Verfügbarkeit und fällt sonst auf Block/Sampling zurück;
`BakeRevision` enthält keine Höhenquellenrevision. Das erklärt diese Variante.
Der frühere Ein-Pixel-Unterschied in Tile 8 ist dadurch noch nicht erklärt.

## Verbindliche Architekturentscheidung

Capture bindet einen vollständig publizierten Weltstand, nicht eine Wartezeit.
`PlaceCamera.cpp::Draw` ruft nach preload mehrfach advance auf; dadurch ist der
Preload-Stand allein kein Beleg für den schließlich aufgenommenen Stand. Das ist
belegter Kontrollfluss, noch keine bewiesene Ursache der 68 abweichenden Pixel.

Die Engine besitzt eine zeitlich begrenzte Capture-Sitzung auf ihrem Engine-Thread.
Sie hält Referenzen auf publizierte Produkte, keine zweite Weltkopie. Ihr Vertrag:
- Eintritt erst bei vollständiger Abdeckung des expliziten Kamera-/LOD-Arbeitssets;
  Quelle, Inputrevision und akzeptierte Produktrevision je Tile gehören zum Stand.
- Kamera, Simulationszeit, Wetter, Seed, Qualität und temporale Samplefolge fixieren.
  Bereitschaft darf nicht allein aus leeren IO-Queues abgeleitet werden.
- Während Settling und Readback keine Weltpublikation und kein Simulationstick.
  Renderer darf seine temporale Historie mit deklarierter Samplefolge aufbauen.
  Worker dürfen private Produkte vorbereiten; Rückstau bleibt begrenzt.
- Ende/Abbruch löst Pins per RAII; reguläres Streaming setzt danach fort.
  Timeout oder fehlende Quelle liefert Fehler, kein erfolgreiches Teilbild.
- Screenshot und Messlauf sind getrennte Phasen: bewegtes Streaming erst nach Ende
  der Capture-Sitzung messen. Keine eingefrorene Welt als Streamingbenchmark ausgeben.

Keine allgemeine Pause-API für alle Subsysteme einführen. Den schmalen Engine-Vertrag
am Clientbedarf ableiten; outshine-client bleibt einziger dateibasierter Renderpfad.
Snapshot-Bindung ist kein Beweis für deterministische Generatorprodukte: bei gleichen
Eingängen abweichende CPU-Produkte müssen an der Merge-/Generatorursache behoben werden.

## Ausführbare Schritte und Abnahme

1. Zuerst ohne Architekturumbau zwei Cache-offline-Captures vergleichen: Arbeitsset,
   akzeptierte Revisionen, Kamera/Zeit/Samples und native Produktdaten. Nur eine kompakte
   Differenzdiagnose ins System-Temp; keine vollständigen Meshlogs. Reihenfolgeunterschiede
   über stabile Quellidentitäten vergleichen, nicht über zufällige Runtime-Handles.
   `Delivery::Answer` und `TilePool::Landing` tragen die deklarierte Quellenrevision
   bereits. Erst die residenten Terrain-/Vektorpfade verlieren sie; WI 2248
   führt eine wertbesitzende Identität bis zu den konsumierten Tiles. Der
   Candidate erfasst genau sein genutztes Arbeitsset, nicht den gesamten Cache.
2. Engine-eigene Capture-Sitzung und Client-Anbindung implementieren. Bestehende
   preload-/Readiness-Bedingungen wiederverwenden; Readback wartet auf seinen Submit.
   Referenz: lokales ../SDL, Stand fa2c02b, include/SDL3/SDL_gpu.h, Fence-Vertrag.
3. Öffentlicher Test unter test/outshine/include/Outshine/: kleine OSM-/DEM-Fixture,
   vertauschte Workerfertigstellung, gleiche Inhalte; verspätetes Produkt während
   Capture bleibt privat, nach Freigabe wird Streaming fortgesetzt. Abbruch/Timeout
   gibt Pins frei und erhält die nutzbare Welt. Mutation während Capture als Negativkontrolle.
4. Für Malcesine zuerst sortierte `TilePieces`-Tile-IDs und jeweilige Bake-Digests
   zweier Captures vergleichen. Beim ersten Unterschied Inputrevision,
   Worker-Abschluss und gebackene Geometrie dieses Tiles verfolgen. Erst bei
   gleichen CPU-Produkten GPU-Eingaben/Readback untersuchen; keine Toleranzerhöhung.
   WI 2247 ersetzt akzeptierte Fallback-Bakes pro Tile. Das FineOnly-Gate allein
   lässt Malcesine-Tile 24 stehen; globales Rebake erreichte in Graz vor Capture
   keine Refined-Qualität und wurde verworfen. Die neue Abnahme verlangt beide
   Places, veränderte Quellrevisionen und vertauschte Workerabschlüsse.
   Gleicher Backendstand und Snapshot liefern gleiche vereinbarte Bildmetrik.
5. make format; make suite SUITE=outshine/include/Outshine; make lint;
   make shots PLACE='--no-vegetation --preload-seconds 120 Graz' zweimal.
   PNGs öffnen und mit test/scripts/pixels.py vergleichen. Keine Neupins zur Kaschierung.
