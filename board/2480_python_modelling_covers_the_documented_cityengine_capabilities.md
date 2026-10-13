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
Scope/Extrusion, Komponenten, Unterteilung, Assets, UVs, Material-Overrides und Projektion teilweise umgesetzt/geprüft.
Kein vollständiger Modellierkern; null vollständige Architektur-Rezeptfamilien. Aktueller Bild-/Prüfstand in 2460 und Showcase-Manifest.
SDK-Inventar/Ghidra-Ziele/fünf RPKs: [2400](2400_esri_capabilities_and_interior_primitives_have_executable_evidence.md); SDK 3.4 versus PyPRT 1.12/PRT 3.3 unterscheiden.

## Offset: metrische Flächen und Shape-Anbindung
`Shape.offset`: signierte planare 3D-Flächen, Löcher, Zerfall/Kollaps; affine UV0–9, Tags/Material/Gruppen und signed Scope erhalten.
Positive Randflächen überlappen die Offsetfläche mit umgekehrter Normale; orientierte Flächensumme bleibt Quelle.
`esri_offset_oracle`: gepinnter SDK-Helfer im begrenzten Kindprozess; fünf analytische/konkave Fälle direkt verglichen.
Ghidra 0055b090/00671880: XZ-Ring, positive Einrückung, CGAL-Skelett und nur größtes Fragment; kein vollständiges CGA-Orakel.
GEOS erhält alle Teile. Zwölf Frame-/Distanzfälle plus Hof/Körper/Extrusion; Normale, nichtaffine UVs und Okklusionslaufzeit offen.

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
  statische Ghidra-Pfade, PRT-Ausgabe und eigene Umsetzung getrennt belegen; fehlende Semantik offenhalten.

## Scope und Extrusion
`modelling_scope/modelling_shape`: geteilte unveränderliche Geometrie erst zur Ausgabe transformieren.
`t` folgt Scope-Achsen; `r` addiert Pivot-Eulerwinkel, Zentrumrotation hält den Mittelpunkt;
`s` setzt absolute/relative Größen einschließlich Spiegelung; `translate` unterstützt dokumentierte Koordinatensysteme.
Neun Kontrollgruppen inklusive nichtkommutierender Rotationen, schiefer Pivots, UV/Besitz/Normalen;
zwei PRT-Translationen. Isolierte PRT-Rotation/Skalierung, singuläre Normalen und Floating-Selector bleiben offen.
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

## Unterteilung: 2D-Verfahrensstufe
`modelling_split_sizes/split_plan/split_area`: absolute/relative/floating Größen, flache Repeats und globale Indices.
GEOS-Schnittpolygone mit Löchern/Multipart; adjust/noAdjust-Bounds, verschachtelte Funktionsaufrufe ohne Flächenverlust.
Flächenteilung nutzt monotone Summen der CDT-Dreiecksflächen; zwischen Eckpunktpositionen ist die Summe quadratisch.
Direkte Inversion gegen 64-Schritt-Bisektion und wiederholte GEOS-Cuts vergleichen, Quellfläche/Schnittgrenzen erhalten.
Neun SDK-Größenbeispiele, analytisches Dreieck, 198 GEOS-Flächenintegrale sowie fünf Grundrisse × zwei Achsen geprüft.

## Unterteilung: geschlossene Körper und offene Flächen
`Shape.split`: native Dreiecke mit baryzentrischen UV-/Normalenwerten schneiden, logische Quellflächen rekonstruieren.
GEOS/CDT schließt einseitige Querschnitte; korrekte koplanare Quellflächen behalten, Gegenflächen mit Grund auslassen.
Höfe/Material/Gruppen/Tags, signed Scope, Objektframes und SplitInfo erhalten; neue Caps haben explizite Material-/Gruppenwahl.
SDK-UV-/Materialgleichheit bleibt offen. Manifold-Schnitt verworfen: Hofgrenzfall hat korrektes Volumen, aber Zusatzfläche/ungültige Kanten.
Eigener Weg: 15 Grundriss-/Achsenfälle, vier Frames/zwölf Achsenfälle und 1.008 nichtaffine UV-/Normalenecken geprüft.
Profil: skalare NumPy-Kreuzprodukte/Projektionen vermeiden, Normen pro Quelle teilen und eindeutige Flächen ohne zweite Zuordnung übernehmen.
Offene Kantenkomponenten erzeugen keine Caps; neun ungültige/unsupported Eingaben einschließlich Misch-/Nullvolumen prüfen.

## Flächenteilung, Assets und UVs
`Shape.split_area`: logische Normalen mitteln, orientierte 30°-Achsenwache; wirkliche Dreiecksflächen ohne Abflachen oder Caps teilen.
30 Quell-/Achsen-/Framefälle, 102 nichtaffine UV-/Normalenecken, verwundene/ungleich große Flächen und acht ungültige Eingaben geprüft.
SDK-Projektion/UV-Gleichheit und Normalengewichte offen; gemischte Primitive/rohe Parts, Repeats und UV-/unitSpace bleiben Lücken.
`Asset.from_shape`/`Shape.insert`: drei Modi, yUp/zUp und Nullachsen; Scope-Zero 0,1 mm, ausgegebene Mindestabmessung 0,01 mm.
`Asset.from_file`: Trimesh 5.1.1/Pygltflib 1.16.5 für OBJ/glTF/GLB; statische Knoten/Spiegelung, UV0/UV1, Normalen und genaue Materialwerte.
Hashes/Bilder erhalten; f/g/m übernehmen Zusatzkanäle, übrige Bearbeitung meldet unsupported. Vier Dateien, fünf ungültige Fälle, Integer-UV/Farben und schiefe Normalen geprüft.
UV: Skalierung/Rotation/Translation/Kopie/Löschen; vier Normalmodi kollektiv/pro Fläche. Corner/Höfe/UV-Abwesenheit erhalten; Wire-/Punkt-Edits unsupported.
Material/Texturbytes und Projektionsframes erhalten. Sechs Scope-/Weltachsen, relative/signierte Floating-Maße und Versätze vor Clamp geprüft.
Achsenbilder in 1920×1080 optisch geprüft; große Koordinaten, PRT-/LOD-/Shadergleichheit offen.
## Aktiver Dachblock: Pult und First
`Shape.roof_shed`: planare Flächen, gerundete/zyklische Kantenwahl, signierter Winkel oder positive Höhe, Höfe und ausgerichteter Scope.
Nichtaffine UV0–9, Material/Gruppen/Tags erhalten; Dachmasken auch nach `split`. Neue geometrische Normalen; Null-Traufseiten mit Quellindex auslassen.
Ghidra 00526480 delegiert an 005225f0: 2.976 Instruktionen, zwei indirekte Aufrufe offen; Kantenindex zyklisch, Winkelzeichen dreht Dachrichtung.
30 analytische Volumen-/Topologie-/UV-/Framefälle plus getrennte Flächen/Streams; Höhenplan und zwei 1080p-Ansichten optisch geprüft.
`Shape.roof_ridge`: zwei Ebenen, First parallel zu projiziertem Scope-X; native Dreiecksschnitte erhalten nichtaffine UV0–9.
36 Volumen-/Frame-/Hof-/UV-Flächenfälle, gemeinsame logische Firstkanten; vier 2D/3D-Ansichten optisch geprüft.
Trim-Planes, isolierte PRT-Regeln, SDK-UV-/Normalensemantik und gekoppelte Mehrflächen-Topologie fehlen.
[2440](2440_building_recipe_families_have_distinct_architecture_and_examples.md) besitzt anschließend moderne/zeitgenössische Rezepte:
alle lieferbaren Nutzungs-/Form-/Geschoss-/Dach-/Material-/Part-/Kontextkombinationen passend treffen;
Teil-/Geschossnutzung lokal komponieren, Widersprüche erhalten. Unknown-Ersatz zählt nicht als spezifischer Treffer.
Familien/Varianten/Instanzen getrennt zählen; funktionale Räume/Wege/Zugänge/Nachtlicht und Bildbelege sind Pflicht.

## Primärquellen
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf), Abschnitt 3: Scope/Split/Repeat/Komponenten.
[CGA roofShed](https://doc.arcgis.com/en/cityengine/latest/cga/cga-roof-shed.htm)/[roofRidge](https://doc.arcgis.com/en/cityengine/latest/cga/cga-roof-ridge.htm): Dachorientierung, Winkel/Höhe, Tags und Scope.
[CGA Comp](https://doc.arcgis.com/en/cityengine/latest/cga/cga-comp.htm): Auswahl/Pivots; [UV-Normalisierung](https://doc.arcgis.com/en/cityengine/latest/cga/cga-normalize-uv.htm): Modi, Flächentrennung.
[CGA Extrude](https://doc.arcgis.com/en/cityengine/latest/cga/cga-extrude.htm): Flächen/Kanten, UVs/Tags, signierte Scopes und Mindestspanne.
[CGA Split](https://doc.arcgis.com/en/cityengine/latest/cga/cga-split.htm), [SplitArea](https://doc.arcgis.com/en/cityengine/latest/cga/cga-split-area.htm): Umfang/Breiten versus Geometriefläche, Wiederholung und Scope-Anpassung.
[glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html): Normalisierung/Hierarchie; [Trimesh](https://trimesh.org/trimesh.exchange.load.html)/[Pygltflib](https://pypi.org/project/pygltflib/): Standarddecoder, eigene Kanal-/Metadatenadapter.
