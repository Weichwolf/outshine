Type: feature
State: active
Parent: 2169
Area: generators, world, physics
Tags: webcam, measured
Depends: 2133, 2121

# Transport alignments and structures preserve bridges, tunnels and stacked routes

## Befund und Auftrag

Der Nutzer benennt Straßen-/Schienen-/Wegegeneratoren, Brücken, Tunnel und komplexe
3D-Situationen ausdrücklich als fehlerhaft. Webcam-Renders Wien/Husum/Feldkirch zeigen
auffällige Deck-/Ufer-/Straßenkontakte; Tunnelinnenräume und gestapelte Knoten wurden durch
diese neun Außenkameras nicht geprüft. `StreetField.cpp` filtert Tunnel vor dem Generator.
`src/generators/road/Corridors.*`, `ProfiledRoadMesher.*`, `Infrastructure.*` enthalten Brücken-/Rampen-
Logik. Eine vollständig sichtbare Schienen-/Tunnel-Pipeline ist damit nicht nachgewiesen.

Kontextprüfung: ProfiledRoadMesher::TrySweep baut ReferenceLine/Rise/Bank und ruft Ribbon::Sweep;
die Kurvenklassen sind damit Produktionscode. Carriageway::Surface berücksichtigt
Offsetkrümmung und Bankrate; Differentialtests prüfen die Decknormale. Ribbon prüft
Randnormalen/Null-Schulter und verweigert horizontale Offset-Faltung. Globale
Selbstüberschneidung und Kontakt-/Renderübereinstimmung bleiben offen.
ProfiledRoadMesher skaliert Stützstationen und Höhenraten jetzt konsistent auf die gefittete
Linienlänge. Gemeinsame Anschlussgradienten mehrerer Abschnitte bleiben zu prüfen.
Angebotene Kurven-/Fahrnetzklassen benötigen eigene Korrektheits-, Budget- und
Grenzfallnachweise; deren Existenz oder Lint-Erfolg ist keine Driving-Abnahme.

## Architektur und Implementierung

Logische Karte (2133) → räumliches Alignment → getrennte Render- und Kollisionsprodukte.
Alignment ist quell-/regelbasierte Referenzgeometrie, kein Kamera-LOD-Mesh. Dieselben
Edge-/Structure-IDs tragen Lage, Höhenprofil, Querneigung, Breite und Anschlussbedingungen.
NPCs auf dem Netz fragen (s,t) ab; Physik hat unabhängig verfeinerte Kontaktgeometrie.

1. Höhen je Struktursegment lösen: DEM-Stützwerte auf bodengebundenen Abschnitten,
   Abutments/Portale als Bedingungen, Deck zwischen Auflagern, Tunnel unter Deckung.
   Straßenkreuzungen teilen eine Höhe nur bei logischer Verbindung. Layer ist keine
   pauschale +5-m-Regel. Fehlende Höhen/Spannweiten als plausible Konstruktion ausweisen.
2. Beschränkter lokaler Solve für Anschlusslage/-tangente, Gradiente/Krümmung, Clearance,
   Deckdicke, Querneigung und Erdbewegung. Infrastrukturklassen verschieden behandeln;
   Gleis braucht kontinuierliche Krümmung/Überhöhung, Treppe einzelne begehbare Stufen.
   Unlösbare Bedingungen mit betroffenen IDs/Residuen melden, nicht global glattbügeln.
3. Brücke: Fahr-/Gleisträger mit Unterseite, realer Dicke, Geländer, Widerlagern und plausiblen
   Pfeilern. Unterquerte Verkehrswege/Wasser frei halten. Nur Auflager/Rampen stempeln.
   Kein bis zum Boden gezogener Skirt verschließt die Unterführung.
4. Tunnel: separate befahrbare Röhre, Portale und Geländeöffnung, Innenwand/Decke,
   anschließende Rampen und Licht-/Sichtbarkeitswechsel. Ein Höhenfeld kann keinen Hohlraum
   darstellen: lokale Terrain-Lochmaske plus geschlossen angebundene Portal-/Tunnelgeometrie.
   Zwei übereinanderliegende Wege dürfen nicht dieselbe Heightfield-Zelle beanspruchen.
5. Schiene: OSM rail/tram/light_rail, Spurweite, Weichen/Übergänge, Schotterbett oder
   straßenbündige Einbettung; Schiene/Schwelle/Leitung nach projizierter Größe generieren.
   Tram kann Straße teilen, bleibt eigenes Netz. Wege: befestigt/unbefestigt/Treppe/Steg
   mit gültiger Breite und Kontakt statt universellem Straßenband.
6. Render-LOD aus Alignment mit Fehlergrenze ableiten; Kollisionsfehler separat begrenzen.
   Analytischer Lane-Follower ersetzt keine Rigid-body-Kollision mit Brüstung/Decke/Unterseite.
   Tilegrenzen teilen Profilrandbedingungen; Bake asynchron, Mesh-Swap atomar (2124).

## Aufsteigende Referenzsuite

Punkt/Transformation → Gerade/Projektion → Bogen/Klothoide → Höhenprofil/Querneigung
→ Querschnitt/Normale/Kontakt → Fahrspur/Kreuzung → gestapelte Brücke/Tunnel →
vollständiges Großbauwerk mit Zufahrten, etwa Golden Gate Bridge als Datenfixture.
Jede Stufe nutzt die angebotenen Klassen und Produktionspfade, keine zweite Engine.
Analytische Sollwerte und Grenzfälle zuerst; danach kombinierte Fahrten, Querschnitte,
PNG-Abnahme und CPU/GPU-/Speicherbudgets. Zufällige/adversariale Varianten ergänzen
feste Beispiele. Benchmarkinhalte bleiben externe Daten, niemals Engine-Sonderfälle.

## Abnahme

| Fall | Logik | Geometrie/Kontakt |
|---|---|---|
| Straße über Straße/Fluss | kein falscher Turn | freie Unterfahrt, Deckdicke, Pfeiler außerhalb Lichtraum |
| dreistöckiger Knoten mit Rampen | nur erklärte Verbindungen | keine gegenseitigen Stamps; Rampen C1 soweit Bauform verlangt |
| Tunnel unter Straße, mit Abzweig | auch fern und unsichtbar routbar | Portal offen, ausreichende Deckung, befahrbare Röhre |
| Bahnbrücke/Weiche/Bahnübergang/Tram | Modus-/Konfliktregeln erhalten | Spurweite, kontinuierlicher Fahrweg, zulässiger Lichtraum |
| Hangweg/Treppe/Fußgängersteg | richtiger Zugang/Modus | tragende Stufen/Flächen, kein Schweben und kein pauschaler Felsvorhang |

- [ ] Pro Fall Querschnitt, Außenbild und Fahrt/Begehung, einschließlich Tunnelkamera.
      Negativkontrollen: Ebenen verbinden, Deck zu dünn, Tunnel schließen, Profilnaht versetzen.
- [ ] Render-/Kontaktabweichung gegen Alignment messen; kein Zentimeteranspruch gegen das
      reale DEM. Physikziel <1 cm gegen das erklärte Alignment separat nachweisen.
- [ ] Versionen stimmen bei Streaming überein; 2092 misst Knotendurchfahrt, Portalwechsel und
      Rückkehr mit vollständiger Landschaft. Außen-Webcam allein schließt dieses WI nicht.

Wahl: analytische Road-/Lane-Referenz wie
[ASAM OpenDRIVE](https://www.asam.net/standards/detail/opendrive/), generische räumliche
Konstruktionen und SDL-kompatible Meshes. Unreal/RAGE als visueller Maßstab; keine
proprietäre Bauwerksrekonstruktion und keine zusätzlichen realen Quelldaten erforderlich.

CARLA als geprüfte Referenz: [OSM→OpenDRIVE](https://carla.readthedocs.io/en/latest/tuto_G_openstreetmap/),
[Digital Twin Tool](https://carla.readthedocs.io/en/0.9.15/adv_digital_twin/) und
[Standalone-Erzeugung](https://carla.readthedocs.io/en/latest/adv_opendrive/).
Editor-Generierung/Speicherung ist vorbereitend; generate_opendrive_world blockiert
laut Dokumentation bis zum Aufbau. Meshabschnitte und explizite Lane-/Junction-Daten
als Referenz prüfen. Für Outshine: bounded Worker-Jobs, Cache, Tile-Randverträge und
atomare Publikation statt blockierendem Weltaufbau. Dokumentierte Junction-Glättung
und Querneigungsgrenzen nicht als Oracle übernehmen. OpenDRIVE bleibt Adapterformat.
Logisches 2D-Netz mit Ebenen-/Verbindungsidentität unabhängig von sichtbarem Mesh;
Alignment ergänzt 3D-Pose, Render-/Kollisionsprodukte dürfen keine Topologie erfinden.

## P0-Nachweis: minimale Decksegmente

ProfiledRoadMesher verarbeitet jetzt Zwei-Punkt-Geraden und verbleibende Zweipunktstücke.
Linienfit, Höhenprofil und Anschlusstore sind getrennt; Profilformeln unverändert.
Zwölf ProfiledRoadMesher-/Kurventests grün. Analytischer Fall: 64 m Länge, 16 m Anstieg,
drei Richtungen, Deckrandlage und appendierte Indizes; Altcode scheitert 15-mal.
Wien ohne Vegetation geöffnet: 3235/921600 Pixel verändert; Gegenprobe mit alter
Mindestlänge reproduziert das Vorbild pixelgenau. Neue Deckflächen, weiterhin
unzureichende Material-/Lichtqualität. p50/p95/p99 5.26/5.68/6.06 ms, 0/120 über
16.67 ms; Warmaufnahme, keine vollständige Streaming-/Bauwerksabnahme.
Endliche Eingaben, Anschlussgradienten, Kontakt und komplexe Bauwerke bleiben offen.

Höhenraten erfüllen nun bei s = u * Lfit/Lquelle die Kettenregel
dh/ds = dh/du * Lquelle/Lfit. Steigender/fallender linearer Quellverlauf prüft
die Eintrittsnormalen analytisch; Altcode scheitert achtmal. Alle 13 ProfiledRoadMesher-/
Kurventests grün. Fehlerzähler sind nichtnullable Referenzen. ProfiledRoadMesher ohne Tidy-
Befunde; insgesamt 61, Writer rot. Wien geöffnet und pixelgleich zur Vorversion;
p50/p95/p99 5.24/5.73/5.99 ms, 0/120 über 16.67 ms. Höhenfix hier nur analytisch nachgewiesen.
