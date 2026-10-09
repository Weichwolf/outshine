Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: diagnostics, generators, world, render, client
Tags: performance, budgets

# Compact cost profiles expose the work behind loading and frames

## Ergebnis und Ist
Ein Place erklärt seine Lade- und Framekosten mit wenigen Zeilen, ohne laufende Textausgabe.
Vorhanden: acht Profilzeilen je Place, Cache-IO/Bytes/Treffer, Provider-Aufrufe je Datenart,
alle Terrain-Arbeitsportionen, OSM-Aufbereitung und Generatorphasen. Erfolgreiche Frames
tragen Hostphasen und CPU-Encoding je Stufe als Summe/Maximum; feste Namen und indizierte
Metriken vermeiden wiederholte Namensbildung und lineare Suche. Detailausgabe bleibt explizit.
Hardware am 2026-10-08 verifiziert: Apple A18 Pro (Mac17,5), 8 GB, Metal.
Echte GPU-Passzeiten bleiben offen; keine Gleichsetzung mit Fence-Warten.
Rosenheim, gleiches 500-Hz-Sampling: fertige Mip-Karten senken beobachtete Mainthread-CPU
von 4,63 auf 2,96 s und Mip-/Upload-Leaves von 831 auf 4 ms; Laden 3,72 → 2,10 s, gleiche PNGs.
Ein Vorher/Nachher-Paar, kein GPU-Zeit- oder RAM-Gewinnbeweis; 2340 besitzt wechselnde Startspitzen.
Rosenheim-Sampling enthält zusätzlich 77 ms in `MvtLayer::HeapBytes`: unveränderte Stringkapazitäten
werden für Berichte wiederholt durchlaufen. [Modell](../test/experiments/osm_heap_accounting.py):
16 gecachte MVTs/14.593 Strings, 200 Abfragen: 2.918.600 → 14.593 Stringbesuche.
ParsedTile besitzt die einmal erfasste Layer-Kapazität; Ersetzen/Verdrängen bewegt die Ladung
mit dem unveränderten Layer. Native Tests erhalten dieselbe Kapazitätsrechnung; OS-RSS bleibt separat.
Frisches Vorher/Nachher-Sampling: MVT-/OSM-Speicherbericht-Leaves 124,54 → 24,50 ms;
keine MVT-Heap-Samples danach. Mainthread 3,079 → 3,045 s, Laden 2,190 → 2,159 s,
ein Paar ohne belastbaren Gesamtgewinn; Bild/Provider-/Produktarbeit unverändert.

## Kostenreview und konkrete Messlücken
[2280](2280_ready_assets_load_from_a_spatial_cache.md) hält die aktuelle Place-Kostentabelle;
[2336](2336_one_world_refines_seamlessly_from_orbit_to_ground_detail.md) den Ersatz der unnötigen Arbeit.
Kein neuer Profiling-Großumbau: folgende Zähler mit diesen P0-Integrationen ergänzen.
- Hierarchie: angefordert/geladen/verworfen pro Stufe; API-/SSD-/entpackte Bytes, unbekannte
  versus verdeckte Knoten. Gesparte Anfragen zählen vor IO, nicht erst GPU-verdeckte Dreiecke.
- Geometrie: geplant/erzeugt/resident/eingereicht/tatsächlich ausgewählt getrennt; CPU-Terrain-
  Kontakt-/Audiomesh und GPU-Rendergitter explizit benennen. ROW.Triangles zählt heute nur
  erzeugte Gebäudedreiecke; keine GPU-Summe/Verdeckungsquote daraus ableiten. Sichtbare Pixel zählen.
- Peaks: Quelldaten, native Pakete, Decoder-Scratch, Produktarrays, Klassen, Prototypen,
  GPU-Uploads und alte/neue Kandidaten getrennt; Besitz/gleichzeitige Lebensdauer erklären.
  Native Hit bf171f806: 4,08 GiB Footprint, 1,37 GiB RSS; keine Addition. Tokyo liest
  312,71 MiB Gebäude-LOD und 248,35 MiB Prototypen, ohne Quellenbytes. `--measures`
  mit getrennten `vmmap -summary`-Proben abgleichen. SceneResources kopiert PieceMesh-Arrays;
  Bestätigtes Preload-Fence löst jetzt Upload-Scratch; native Readbacks erhalten residente Daten
  und spätere Uploads. Alle zehn Place-Treffer melden danach null Transferkapazität.
  Fence 25,43 ms in Rosenheims Frame 4 wartet bei zwei Slots auf Frame 2; keine GPU-Passzeit.
  Tokyo-CLI-Probe: 560,31 MiB Geräte-Geometriepuffer, 185,55 MiB CPU-Piece-Kopien,
  71,34 MiB Transferkapazität; 1.300,66 MiB lebender C++-Heap enthält Produktarrays.
  Drei vmmap-Proben bestanden, letzte während Aufbau: 2,3 GiB Footprint. Überlappende
  Zähler nicht addieren; Profilinglauf kein Frame-Gate. PNG bleibt 278a2b52.
- CP/Tokyo Geometriephase 2,17/2,87 s, längster Schritt 1,21/1,79 s: Allocation/Decode/
  Validierung/Upload/Fence einzeln messen. Gesamtphase beweist keine einzelne Ursache.
- Producer-/Codecversionen und echte Cachehits getrennt ausweisen; fachfremde Änderungen
  invalidieren keine Prototypen (2280).
- Frame: stabiles Fenster und Ressourcenbereitschaft/erste Einreichungen getrennt ausweisen;
  Wien hat 27,87 ms Fence-Warten im schlechtesten Frame, aber keine direkte GPU-Passzeit.
- Fehlende Metrik als unbekannt kennzeichnen; heutiger CostReport-Helper liefert sonst Null.
Bisher gemessene Ledger-/Ausgabekosten: 12–13 ms/Lauf, Veröffentlichung bis 0,30 ms.
Diese Messung umfasst wiederholte Speicher-Scans nicht vollständig; deren Kosten separat erfassen. Eltern-/Kindzeiten und asynchrone Arbeit nicht addieren.
Offline-Läufe erlauben keine Aussage über API-Limits/Bandbreite. Kalte Netzwerk-Batches und
Cachetreffer mit derselben Hierarchie getrennt messen, bevor ein Anbieter als Engpass gilt.

## Besitzer und Umsetzung
- `world/data/ContentStore`: Hits/Misses, gelesene/geschriebene Bytes und IO-Dauer;
  `SourceSet` besitzt Providerstarts, Retries und Lieferungen je Quelle. 2280s Asset-Index
  misst Abfrage/IO/Entpacken/Upload; Erstaufbau und Treffer getrennt. Treffer dürfen keine
  Quellaufbereitung/Rohling-Neubau verstecken; Runtime-Nahdetails separat messen. SSD/RAM/GPU-Nutzdaten/Peaks getrennt zählen.
- `world/ground/TilePool`: jede ausgeführte Field-/Mesh-Arbeitsportion zählen,
  einschließlich abhängiger Wiederholungen; fertiggestellte Produkte getrennt zählen.
- Generatoren und `engine/FrameMeasurements`: vorhandene Phasen zu Anzahl/Summe/Maximum
  ergänzen; Eltern enthalten ihre Kinder. Überlappende IO-/Workerzeiten nicht addieren.
- `render/SceneRenderer`: Hostphasen und CPU-Encoding je Stufe mit festen Aggregaten;
  nur erfolgreich eingereichte Frames veröffentlichen. GPU-Debuggruppen tragen im Validierungsbuild Stufennamen; der normale Build lässt sie weg.
  SDL_GPU bietet keine Zeitstempel: echte GPU-Passzeiten brauchen einen Backendprofiler;
  fehlende Zeiten ausdrücklich kennzeichnen, Fence-Warten separat ausweisen.
- `engine/DiagnosticLedger`: Namen einmal registrieren, wiederholte Lookups ohne linearen
  Scan; vorhandene Konfliktprüfung und öffentliche Kamera-/Messverträge erhalten.
- `client`: Zusammenfassung nach dem Messfenster, feste Einheiten und eindeutige Fenster.
  Bestehende explizite Detail-/STAT-Ausgaben bleiben verfügbar. Keine GPU-Readbacks zur
  Zeitmessung. Hot Path formatiert keine Profilzeilen und speichert keine unbeschränkten Events.

## Abnahme
Analytische Zählfälle und Fehlerpfade prüfen Teiljobs, Bytes und erfolgreiche Einreichungen.
Place-Bild/Inhalt und exakt deklarierte Framezahl bleiben gleich. Ausgabe trennt CPU-Encoding,
IO-Dauer, Wartezeit und fehlende GPU-Zeit. Messkosten mit festem Profil prüfen und benennen; Stufensummen passen innerhalb des
Host-Encoding-Intervalls. Die Profilzusammenfassung erscheint genau einmal. Unter `shots/places/` ersetzt ein erfolgreich
gespeicherter Shot die älteren Hash-PNGs desselben Place; Fehler erhalten das bisherige Bild.
Keine unbelegte Behauptung, Instrumentierung sei kostenlos. Allgemeine Gates in AGENTS.
