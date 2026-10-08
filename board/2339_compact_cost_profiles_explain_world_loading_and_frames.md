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
Echte GPU-Passzeiten bleiben offen; keine Gleichsetzung mit Fence-Warten.
Rosenheim 13a88b4dd: 500-Hz-Sampling ordnet 0,83 von 4,63 s Mainthread-CPU (18 %)
exklusiv Textur-Upload/Mip-Erzeugung zu; gemessenes Laden 3,72 s. Native Treffer berechnen
Mips erneut. 2280 prüft fertige Mip-Payloads, 2340 wechselnde Startspitzen; kein GPU-Zeitbeleg.

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
- CP/Tokyo Geometriephase 2,17/2,87 s, längster Schritt 1,21/1,79 s: Allocation/Decode/
  Validierung/Upload/Fence einzeln messen. Gesamtphase beweist keine einzelne Ursache.
- Producer-/Codecversionen und echte Cachehits getrennt ausweisen; fachfremde Änderungen
  invalidieren keine Prototypen (2280).
- Frame: stabiles Fenster und Ressourcenbereitschaft/erste Einreichungen getrennt ausweisen;
  Wien hat 27,87 ms Fence-Warten im schlechtesten Frame, aber keine direkte GPU-Passzeit.
- Fehlende Metrik als unbekannt kennzeichnen; heutiger CostReport-Helper liefert sonst Null.
Diagnostik kostet gemessen etwa 12–13 ms je gesamtem Lauf, einzelne Veröffentlichung bis 0,30 ms;
kein aktueller Hauptengpass. Eltern-/Kindzeiten und asynchrone Arbeit nicht addieren.
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
