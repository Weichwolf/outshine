Type: defect
State: active
Architecture: ready
Parent: 2218
Depends:
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
2. Engine-eigene Capture-Sitzung und Client-Anbindung implementieren. Bestehende
   preload-/Readiness-Bedingungen wiederverwenden; Readback wartet auf seinen Submit.
   Referenz: lokales ../SDL, Stand fa2c02b, include/SDL3/SDL_gpu.h, Fence-Vertrag.
3. Öffentlicher Test unter test/outshine/include/Outshine/: kleine OSM-/DEM-Fixture,
   vertauschte Workerfertigstellung, gleiche Inhalte; verspätetes Produkt während
   Capture bleibt privat, nach Freigabe wird Streaming fortgesetzt. Abbruch/Timeout
   gibt Pins frei und erhält die nutzbare Welt. Mutation während Capture als Negativkontrolle.
4. Bei gleichen CPU-Produkten GPU-Eingaben/Readback untersuchen; keine Toleranzerhöhung.
   Gleicher Backendstand und Snapshot liefern gleiche vereinbarte Bildmetrik.
   Backendübergreifende Bitgleichheit ist kein Vertrag.
5. make format; make suite SUITE=outshine/include/Outshine; make lint;
   make shots PLACE='--no-vegetation --preload-seconds 120 Graz' zweimal.
   PNGs öffnen und mit test/scripts/pixels.py vergleichen. Keine Neupins zur Kaschierung.

Die Ursachenuntersuchung ist sofort ausführbar. Nur deterministische Place-Abnahme
wartet auf diesen Nachweis; native Migration und analytische Tests sind unabhängig.
