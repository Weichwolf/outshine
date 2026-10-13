Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators
Tags: buildings, python, esri, cga, modelling

# Python modelling covers the documented CityEngine capabilities

## Ergebnis und Ist
Nutzerauftrag: zuerst alle dokumentierten CityEngine-Modellierfähigkeiten als eigene Python-Funktionen,
danach moderne/zeitgenössische Architekturrezepte für alle lieferbaren OSM-Klassen und gültigen Kombinationen.
Besitzer: Gebäudeagent auf `buildings`, `.venv`, `test/experiments/buildings/`; native Integration bleibt offen.
Stand f097edd46: Scope/Extrusion, `comp`, Breiten-/Flächenteilung, Asset-Einfügung/Dateiimport, sechs UV-Operationen und Material-Overrides;
Semantik ist teilweise geprüft, kein vollständiger Modellierkern und null vollständige Architektur-Rezeptfamilien.
SDK-Inventar, 82 Ghidra-Ziele und fünf RPKs: [2400](2400_esri_capabilities_and_interior_primitives_have_executable_evidence.md); keine Rezeptzahlen.
SDK 3.4 / PyPRT 1.12 mit PRT 3.3 unterscheiden; eigene Umsetzung, kein dekompilierter Fremdcode.
Bildstand f097edd46: 54 Bilder (13 innen / 34 Gebäude/Funktionen / 4 Gruppen / 3 Städte), mindestens 1920×1080; 13.955 OSM-Kontextkörper.
Neue Fassade optisch geprüft; 41 Bilder pixelgleich, zehn nur Fußtext. Zwei Nachtbilder mit bestehender Streuung (2460).
Sechs Python-Prüfungen und 54 Renderings terminal 0 auf f097edd46; native LOD-/Frame-/GPU-/Qualitätsabnahme offen.

## Gemeinsame Verträge
- Direkte Python-Rezepte/Geometriefunktionen, keine JSON-Regelschicht. Affine UVs dürfen vor der Arraybearbeitung komponiert werden.
  Reports und spätere Cache-Speicherung sind getrennte Ausgaben. Vollständige kameraunabhängige Rohlinge,
  bedarfsgesteuerte Details sowie unabhängige Logik/Kollision folgen dem Gesamtvertrag in 2173/2280/2336.
- Metrische Achsen, Polygonringe/Höfe, Bounds, Material/UV/Normalen und Herkunft erhalten; `Face.uv_sets` speichert Zusatzkanäle an logischen Ecken.
  Belegte, fehlende und ergänzte Angaben unterscheiden; Zufall deterministisch pro Quelle/Regel.
- Immutable logische Flächen/Kanten/Punkte neben nativen Triangle-Assets; Konstruktoren prüfen IDs,
  Material-/Gruppenzugehörigkeit und Normalen. Quellindices behalten, keine implizite Vertexverschweißung.
  GEOS/CDT und vorhandene Manifold-/Dachverfahren nutzen; Standardgeometrie nicht neu erfinden.
- `CompInfo` enthält sel/index/total sowie Quellindices/kind und bleibt bei Scope/Extrusion erhalten.
  Auswahl/Ausschluss und `:`/`=`/`=:` sind gemeinsam; Flächen verbinden über Kanten, Draht/Punkte über IDs.
- Jede Fähigkeit braucht konkreten Status, Referenz/Gegenfälle und Bild-/Kostenbeleg. Dokumentation,
  statische Ghidra-Pfade, PRT-Ausgabe und eigene geprüfte Funktion sind unterschiedliche Belege.
  Fehlende Überladungen/Semantik bleiben offen; weitere Fähigkeiten erweitern den Katalog.

## Scope und Extrusion
`modelling_scope/modelling_shape`: geteilte unveränderliche Geometrie erst zur Ausgabe transformieren.
`t` folgt Scope-Achsen; `r` addiert Pivot-Eulerwinkel, Zentrumrotation hält den Mittelpunkt;
`s` setzt absolute/relative Größen einschließlich Spiegelung; `translate` unterstützt dokumentierte Koordinatensysteme.
Neun Kontrollgruppen inklusive nichtkommutierender Rotationen, schiefer Pivots, UV/Besitz/Normalen;
zwei PRT-Translationen. Isolierte PRT-Rotation/Skalierung, singuläre Normalen und Floating-Selector bleiben offen.
Fünf gleiche Scope-Operationen auf 6.208–545.112 Vertices (c3dc4f1c7): abschließende Ausgabe 3,1–4,9× schneller;
größter Mesh p50 9,93 statt 48,61 ms, 21 warme Läufe, zwei OpenBLAS-Threads; keine Frames/GPU.
`modelling_faces/modelling_extrude`: Flächenarten face.normal/world.up/world.up.flatTop/vertex.normal.
Scope-X orthogonal zur neuen Y-Achse projizieren, eng an Geometrie anpassen; negative Distanz behält negatives Scope-Y.
Flat-Top nimmt mindestens die globale Quellspanne; Firstkontakt kann getrennt geschlossene Keile ergeben.
Vertexnormalen aus aktuellen verbundenen logischen Flächen-IDs, authored Normalen verwerfen.
Erneute Extrusion ersetzt eigene frühere Auto-Tags, Nutzer-Tags erhalten; Seiten-UVs folgen A/B/B/A.
24 Polygon-/11 Dachfälle, drei PRT-Prismen und analytische Distanzen; GEOS-Batching brachte keinen belastbaren Gewinn.
`modelling_edge_extrude`: eine Seitenfläche je Kante, keine künstlichen Flächenkappen;
per-Edge-Normalen trennen Ecken, Welt-/Vertex-Sweeps teilen verschobene Endpoint-IDs.
Flat-Top-Endpunktkontakt ergibt Dreiecke; flächenlose Sweeps lokal mit Quellindex/Grund auslassen.
Acht analytische Streifen, getrennte/verbundene/räumliche Ecken und sechs echte PRT-Wandflächenvergleiche.

## Komponenten und Metadaten
`modelling_components/modelling_wire_components`: Richtung/Neigung/Tags/Ränder, zUp/noAlign und nullgroße Achsen.
Face-Scopes übernehmen vorhandene Dreiecke, nichtaffine Corner-UVs/Normalen und Dicke verwundener Kappen.
Kanten behalten erste Richtung; e mittelt adjazente Normalen, fe zeigt außerhalb des Flächenrandes.
Vertices mitteln Flächennormalen; fv mittelt normierte Nachbarkanten. Normen folgen invers-transponierten Transformationen.
SDK/Ghidra belegen 0,8 mm Mindestlänge eindeutiger Kanten; kein isoliertes Selektions-/Pivotorakel vorhanden.
`modelling_group_components/modelling_metadata`: Quellgruppen, Materialwerte und gefüllte Lochkomponenten.
Gruppen dürfen mehrere Materialien enthalten. Richtungs-/Tagkeywords gelten bei mindestens einem passenden Mitglied;
boolesche Verknüpfungen arbeiten auf der Gruppe, Python-Prädikate erhalten Gruppenfläche/Mitglieder.
Native Dreiecke/Corner-UVs kopieren; nur gefüllte Lochflächen benötigen neue GEOS-Triangulierung.
Lochkomponenten behalten Eltern-Normalenorientierung, Corner-UVs, Material/Gruppe; das Quellenloch bleibt erhalten.
Immutable Namen/Materialwerte teilen; Face-/Edge-/Point-Gruppen und native Palette-IDs samt Tabellen vererben.
Gleiche deklarierte Werte/Name bündeln unterschiedliche IDs; unaufgelöste IDs getrennt halten, keine Defaults erfinden.
`group_names/material_names` folgen Komponentenindices ohne Vertexausgabe. Vier g/m-Frames, zwei Lochflächen/UVs,
Draht-/Punktgruppen, sechs ungültige Eingaben sowie drei echte PRT-Prismen nach Bounds/Volumen geprüft.
PyEncoder liefert keine Gruppen-/Materialmetadaten; PRT-Prismen plus gefüllte Höfe sind kein isoliertes g/m/h-Orakel.
Kosten 0587b922a: fünf Grundrisse, 30 Komponenten-/20 Namensmessungen, je 21 warme Läufe, zwei OpenBLAS-Threads.
Native Übernahme p50 1,66–4,92 statt 2,18–15,49 ms beim GEOS-Neuaufbau, gleiche Geometrie/UVs/Metadaten/Scopes.
Namen p50 5,8–51 µs aus Metadaten statt 0,57–1,41 ms mit Geometriekontext. Finale Ausgabe/Frames/GPU ausgeschlossen.

## Unterteilung: 2D-Verfahrensstufe
`modelling_split_sizes/split_plan/split_area`: absolute/relative/floating Größen, flache Repeats und globale Indices.
GEOS-Schnittpolygone mit Löchern/Multipart; adjust/noAdjust-Bounds, verschachtelte Funktionsaufrufe ohne Flächenverlust.
Flächenteilung nutzt monotone Summen der CDT-Dreiecksflächen; zwischen Eckpunktpositionen ist die Summe quadratisch.
Direkte Inversion gegen 64-Schritt-Bisektion und wiederholte GEOS-Cuts vergleichen, Quellfläche/Schnittgrenzen erhalten.
Neun SDK-Größenbeispiele, analytisches Dreieck, 198 GEOS-Flächenintegrale sowie fünf Grundrisse × zwei Achsen geprüft.
Messung 725034641: 30 Fälle/21 warme Läufe, gesamte 2D-Teilung inklusive Aufbau/Cuts 0,61–1,21 ms p50;
GEOS-Bisektion 4,93–7,52 ms. Maximaler Flächenfehler 3,6e−12 m²; kein nativer Mesh-/UV-/Framebeleg.

## Unterteilung: geschlossene Körper und offene Flächen
`Shape.split`: native Dreiecke mit baryzentrischen UV-/Normalenwerten schneiden, logische Quellflächen rekonstruieren.
GEOS/CDT schließt einseitige Querschnitte; korrekte koplanare Quellflächen behalten, Gegenflächen mit Grund auslassen.
Höfe/Material/Gruppen/Tags, signed Scope, Objektframes und SplitInfo erhalten; neue Caps haben explizite Material-/Gruppenwahl.
SDK-UV-/Materialgleichheit bleibt offen. Manifold-Schnitt verworfen: Hofgrenzfall hat korrektes Volumen, aber Zusatzfläche/ungültige Kanten.
Eigener Weg: 15 Grundriss-/Achsenfälle, vier Frames/zwölf Achsenfälle und 1.008 nichtaffine UV-/Normalenecken geprüft.
Profil: skalare NumPy-Kreuzprodukte/Projektionen vermeiden, Normen pro Quelle teilen und eindeutige Flächen ohne zweite Zuordnung übernehmen.
Offene Kantenkomponenten erzeugen keine Caps; neun ungültige/unsupported Eingaben einschließlich Misch-/Nullvolumen prüfen.
Messung a256e35da: vier Körper p50 26,9–76,8 statt 46,4–147,0 ms; 60 Vorgängerteile in allen Daten exakt gleich.
Offene Teilungen p50 5,1–8,8 ms; sechs Plan-/Achsenfälle und Fassadenachsen geprüft. Finale Ausgabe/Frames/GPU ausgeschlossen.

## Flächenteilung und nächster Block
`Shape.split_area`: logische Normalen mitteln, orientierte 30°-Achsenwache; wirkliche Dreiecksflächen ohne Abflachen oder Caps teilen.
30 Quell-/Achsen-/Framefälle, 102 nichtaffine UV-/Normalenecken, verwundene/ungleich große Flächen und acht ungültige Eingaben geprüft.
Kosten b387226f2: 14 wechselnde Messpaare × 21 Läufe; p50 5,1–9,2 statt 9,5–14,0 ms, alle Fälle schneller; Flächenfehler ≤1,8e−12 m².
SDK-Projektion/UV-Gleichheit und Normalengewichte offen; gemischte Primitive/rohe Parts, Repeats und UV-/unitSpace bleiben Lücken.
`Asset.from_shape`/`Shape.insert`: drei Modi, yUp/zUp und Nullachsen; Scope-Zero 0,1 mm, ausgegebene Mindestabmessung 0,01 mm.
72 Fälle/Normen/Attribute/Eigentum geprüft; 1.000 Platzierungen p50 53 statt 269–340 ms; Arrays 5,2–5,4 kB statt 5,2–5,4 MB; Vorbereitung/GPU/Frames ausgeschlossen.
`Asset.from_file`: Trimesh 5.1.1/Pygltflib 1.16.5 für OBJ/glTF/GLB; statische Knoten/Spiegelung, UV0/UV1, Normalen und genaue Materialwerte.
Hashes/Bilder erhalten; f/g/m übernehmen Zusatzkanäle, übrige Bearbeitung meldet unsupported. Vier Dateien, fünf ungültige Fälle, Integer-UV/Farben und schiefe Normalen geprüft.
UV: Skalierung/Rotation/Translation/Kopie/Löschen; vier Normalmodi kollektiv/pro Fläche. Corner/Höfe/UV-Abwesenheit erhalten; Wire-/Punkt-Edits unsupported.
UV-Komposition p50 0,075–0,210 statt 0,231–0,662 ms; fünf Grundrisse/21 Paare, gleiche Daten, keine Frames. Material-Overrides getrennt; texture hält Bytes durch comp/split/extrude/insert. Aktiv: SDK-Projektion sowie prozedurale Platten mit Farb-/Rauheits-/Normalkarten und Mitsuba-UV-Bindung (2171); 1080p-Kontrollhaus; metrische UV-Kanten an vier Gebäuden geprüft. CGA-/LOD-/Shadergleichheit offen.
[2440](2440_building_recipe_families_have_distinct_architecture_and_examples.md) besitzt anschließend moderne/zeitgenössische Rezepte:
alle lieferbaren Nutzungs-/Form-/Geschoss-/Dach-/Material-/Part-/Kontextkombinationen passend treffen;
Teil-/Geschossnutzung lokal komponieren, Widersprüche erhalten. Unknown-Ersatz zählt nicht als spezifischer Treffer.
Familien/Varianten/Instanzen getrennt zählen; Zuordnung, funktionale Räume/Wege/Zugänge/Nachtlicht und Bildbelege sind Pflicht.
Showcase nur aktuell, mindestens 1920×1080, größere metrische Pläne mit wahrer PNG-Größe; frühere Bilder intern erhalten.

## Primärquellen
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf), Abschnitt 3: Scope/Split/Repeat/Komponenten.
[CGA Comp](https://doc.arcgis.com/en/cityengine/latest/cga/cga-comp.htm): Auswahl/Pivots; [UV-Normalisierung](https://doc.arcgis.com/en/cityengine/latest/cga/cga-normalize-uv.htm): Modi, Flächentrennung.
[CGA Extrude](https://doc.arcgis.com/en/cityengine/latest/cga/cga-extrude.htm): Flächen/Kanten, UVs/Tags, signierte Scopes und Mindestspanne.
[CGA Split](https://doc.arcgis.com/en/cityengine/latest/cga/cga-split.htm), [SplitArea](https://doc.arcgis.com/en/cityengine/latest/cga/cga-split-area.htm): Umfang/Breiten versus Geometriefläche, Wiederholung und Scope-Anpassung.
[glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html): Normalisierung/Hierarchie; [Trimesh](https://trimesh.org/trimesh.exchange.load.html)/[Pygltflib](https://pypi.org/project/pygltflib/): Standarddecoder, eigene Kanal-/Metadatenadapter.
