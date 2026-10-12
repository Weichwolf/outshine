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
Nutzerauftrag: zuerst die prozeduralen CityEngine-Modellierfähigkeiten vollständig in Python nachbauen,
danach moderne/zeitgenössische Architekturrezepte bis zur vollständigen OSM-Klassifikations-/Kombinationsabdeckung ausbauen.
Einzelne konstruktive Python-Verfahren bestehen; ein allgemeiner vollständiger Modellierkern fehlt.
Fünf gleiche Scope-Operationen auf 6.208–545.112 Vertices: abschließende Ausgabe 3,1–4,9× schneller als fünf Vertexdurchläufe;
größter Prüfmesh p50 9,93 statt 48,61 ms, 21 warme Python-Läufe mit zwei OpenBLAS-Threads; keine Frame-/GPU-Zeit.
Stand 9b730451f: vier Flächen-/Kanten-Extrusionsarten und `comp(f/e/fe/v/fv)` mit Auswahl, Verbindung und Scopes.
31 aktuelle Bilder: acht Innenraum-, 16 Einzelgebäude-/Funktions-, vier Gruppen- und drei Stadtansichten; weiterhin null vollständige Rezeptfamilien.
Showcase: fehlende Messwerte aus Diagrammen auslassen, fehlende Zahlen kennzeichnen; geänderte HTML/CSS/JS-Dateien automatisch neu laden.
23 Gebäude-/Innenraum-/Gruppen-/Stadtbilder sind pixelgleich; sieben neue Sweep-Kontrollen plus metrischer Grundriss optisch geprüft.
Scope/Flächen-/Kantenextrusion/Komponenten/Ghidra/Kamera/drei Benchmarks/Katalog/Render/Docs: 13 Schritte samt Launcher terminal 0 auf 9b730451f.
SDK-Katalog: 72 Operations-, 70 Funktions-, 10 Attribut- und 49 Bibliotheksfunktionsseiten;
Seitenzahlen zählen weder geprüfte Python-Funktionen noch Rezepte. Überladungen/Semantik einzeln inventarisieren.
49 Ghidra-Ziele und fünf verfügbare RPKs sind Referenzen aus [2400](2400_esri_capabilities_and_interior_primitives_have_executable_evidence.md).
Eigene Implementierung, keine Übernahme dekompilierten Fremdcodes; SDK 3.4 / PRT 3.3 ausdrücklich unterscheiden.
Besitzer: Gebäudeagent auf `buildings`, `.venv`, `test/experiments/buildings/`; native Integration bleibt offen.

## Verträge und Umsetzung
- Gemeinsame Python-Geometriefunktionen und direkte ausführbare Python-Rezepte; gemeinsame Form-/Material-/Scope-Typen.
  Keine zusätzliche JSON-Regelschicht erforderlich. Reports/Cache-Speicherung bleiben getrennte Ausgabeverträge.
- Lokale/Weltachsen, metrische Scope-Größen, Polygonringe/Höfe, Bounds, Material/UV/Normalen und Herkunft explizit halten.
  Transformationen, Komponentenzerlegung, Split/Repeat, Instanzen und Insert teilen diesen Vertrag.
- Numerische/String-/Array-/Zufalls-/Geometriefunktionen und Regelattribute anhand tatsächlicher SDK-Semantik abgleichen.
  Zufall deterministisch pro Quelle/Regel; fehlende/ungeeignete Operationen mit konkretem Grund ausweisen.
- Jede dokumentierte Fähigkeit erhält einen tatsächlichen Status und passende Referenz/Gegenfälle;
  API-Name, gelesene Dokumentation, Ghidra-Pfad, PRT-Ergebnis und eigene geprüfte Funktion sind getrennte Belege.
  Weitere nachgewiesene Modellierfähigkeiten erweitern den Katalog; unbekannter Umfang bleibt offen.
- Vorhandene GEOS-/Manifold-/Dach-/Extrusions-/Mesh-Verfahren wiederverwenden; kostenbestimmende Alternativen
  auf denselben echten Eingaben und Qualitätsanforderungen vergleichen. SIGGRAPH/Primärquellen vor neuen Verfahren.
- Durchgängige Python-API: Quelle/Parameter → Modellieroperationen → vollständig ergänzter Rohling → Details/Bild.
  Eingabe-/Cachetrefferkosten trennen, Detailbedarf vor Expansion bestimmen; Logik/Kollision unabhängig vom Render-LOD.
- Für jede tatsächlich gebaute Fähigkeit sichtbare kleine 2D-/2.5D-/3D-Belege und geeignete echte Grundrisse liefern;
  interne Arbeit braucht den fehlenden Feature-Vertrag oder einen gemessenen Engpass.

## Rezeptausbau danach
[2440](2440_building_recipe_families_have_distinct_architecture_and_examples.md) besitzt moderne/zeitgenössische Architektur und passende Zuordnung.
Alle lieferbaren Klassen/Unterklassen und gültigen Kombinationen aus Nutzung, Form, Höhe/Geschossen, Dach,
Material, Parts und räumlichem Kontext müssen mindestens ein passendes Rezept treffen.
Teil-/Geschossnutzung bleibt lokal; gemischte Angaben komponieren passende Programme, widersprüchliche Angaben erhalten ihre Herkunft.
Allgemeiner Unknown-Ersatz zählt nicht als spezifischer Treffer für bekannte Sonderklassen.
Rezeptfunktionen gemeinsam pflegen, Familien/Varianten/Instanzen getrennt zählen; Zuordnung plus optischer Bildbeleg erforderlich.

## Abnahme und nächster Block
Vollständige Fähigkeitszuordnung mit offenen Lücken; ausgeführte Geometrie-/Scope-/Materialverträge gegen SDK/PRT prüfen.
Reproduzierbare kleine Kontrollkörper, echte OSM-Grundrisse, ungültige/degenerierte Gegenfälle und gemessene Kosten.
Nur aktuelle Bildbelege in den vier Showcase-Ansichtsebenen; keine vorweggenommene native LOD-/Geräteabnahme.
`modelling_wire`/`modelling_wire_components`: `comp(e/fe/v/fv)`, eigene Linien/Punkte statt künstlicher Dreiecke.
Eindeutige Kanten übernehmen erste Quellrichtung/adjazente Normalen; flächenbezogene Kanten zeigen entlang des Randes nach außen.
Vertexnormalen mitteln angrenzende Flächen; Face-Vertex-Tangenten mitteln normierte Nachbarkanten, auch am Hofring.
Auswahl/Operatoren/Attribute bleiben gemeinsam; Kanten-/Punktnormalen folgen invers-transponierten Transformationen.
Kanten haben Scope-Länge entlang x, Punkte drei nullgroße Achsen; Corner-UVs, Material/Tags und Weltlage erhalten.
Eingefrorene Kontrollen f520c0366: acht gedrehte zUp/noAlign-Gruppen, Quellindices/Verbindung/Reextraktion, Besitz und zwölf reale PRT-Bounds.
Ghidra belegt eine 0,8-mm-Mindestlänge für eindeutige Kanten; 49 Ziele, kein isoliertes e/fe/v/fv-Laufzeitorakel.
Fünf Körper, 21 warme Läufe, zwei OpenBLAS-Threads: Komponenten-p50 4,50–40,98 ms; finale Ausgabe/Frames/GPU nicht enthalten.
Dict-Indexierung ist bei vier kleinen Fällen günstiger, NumPy-Sort beim 72-Kanten-Hof; Differenzen unter 0,04 ms.
Dict erhält einfache erste Quellreihenfolge; kein behaupteter Gesamtzeitgewinn. Relevante Kosten entstehen im Komponentenaufbau.
Kanten-/Punktplots bleiben interne Referenzen; aktuelle Lieferung zeigt deren Sweep-Flächen.
Extrusionsvertrag: `comp(e/fe)`, vier Modi, gemeinsame Vertexnormalen des aktuellen Kantennetzes.
Eine Seitenfläche je Kante; keine künstlichen Flächenkappen. Eigene Nachbar-IDs erhalten, Face-Normal-Sweeps dürfen sich trennen.
Welt-/Flat-Top-Sweeps und Corner-UV-/Tag-/Materialvererbung prüfen; signierte Scopes und `CompInfo` bleiben erhalten.
`modelling_edge_extrude`: aktuelle Endpoint-IDs und Edge-Normalen, normierte Nachbarsumme; per-Edge-Sweeps trennen Ecken.
Welt-/Flat-Top-Sweeps teilen Vertices; stehenbleibende Endpunkte ergeben Dreiecke, flächenlose Sweeps lokale Auslassungen mit Index/Grund.
Eingefroren 9b730451f: acht analytische Streifen, getrennte/verbundene/räumliche Ecken und sechs echte PRT-Wandflächenvergleiche bestehen.
Ghidra 5513b3e1f: 49 Ziele, Adjazenz-/Quelltaghelper; optische SDK-Beispiele und eigener 2D-Hofplan geprüft.
Fünf Grundrisse, vier Sweep-Verträge, batched/individual GEOS: 40 Messungen × 21 warme Läufe, komplette Extrusion samt finaler Ausgabe.
Batched-p50 2,64–6,83 ms, individual 2,64–7,47 ms; kein belastbarer Gesamtgewinn. Eingabebau/Komponentenwahl/Frames/GPU ausgeschlossen.
Aktuelle Lieferung: sieben Workbench-Kontrollen plus 2D-Grundriss; native LOD-/Frame-/GPU-Abnahme bleibt offen.
Dachselektoren, Gruppen/Material/Löcher, Normalenkollaps und isoliertes Kanten-PRT-Orakel bleiben offene Verträge.
Originale Flächenringe/Höfe, Corner-UVs, Material/Tags und Weltlage bleiben erhalten; vorhandene Dreiecke übernehmen statt erneut triangulieren.
`comp(f)`: Richtung/Neigung/Tags/Ränder, Ausschluss/Verbindung, zUp/noAlign, nichtaffine UVs und authorierte Normalen.
Verwundene Dachkappen erhalten Geometrie und Scope-Dicke; exakter SDK-Pivot, Straßenbreiten und Trim-Planes bleiben offen.
`CompInfo` bleibt bei Scope/Extrusion; eigene frühere Auto-Tags werden ersetzt, Nutzer-Tags erhalten.
PRT-Prismen belegen Geometrie-Roundtrip, kein isoliertes Selektions-/Pivotorakel.
Native Dreiecke statt GEOS-Neuaufbau (73346b5f6): p50 3,14–7,28 statt 6,40–15,88 ms bei gleichem Vertrag.
21 warme Läufe, zwei OpenBLAS-Threads; Quellanalyse/Auswahl/Scopes/Payloads enthalten, finale Ausgabe/Frames/GPU ausgeschlossen.
Vorige Modellierbilder bleiben interne Referenzen; Showcase zeigt ausschließlich die aktuelle Lieferung.
Logische Polygonflächen bleiben neben der nativen Triangulierung erhalten; GEOS/CDT nutzen, keine Polygontriangulierung neu erfinden.
`modelling_faces`/`modelling_extrude`: eigene immutable Polygonringe/Corner-UVs neben Triangle-Assets; vier Flächen-Extrusionsarten.
Scope-X orthogonal zur neuen Y-Achse projizieren, eng an Geometrie anpassen, negative Distanz behält negatives Scope-Y.
Flat-Top nimmt mindestens die globale vertikale Quellspanne; Kontakt am First kann zwei getrennt geschlossene Keile ergeben.
Vertexnormalen folgen verbundenen logischen Vertex-IDs und der normierten Summe benachbarter Flächennormalen; authored Normalen verwerfen.
Getrennte deckungsgleiche Vertices werden nicht implizit verschweißt; Extrusionsausgabe erhält tatsächliche Flächenadjazenz.
Flächenabnahme 2d52b4ea2: 24 Polygon-/11 Dachfälle, drei echte PRT-Prismen, analytische Distanzen und ungültige Eingaben.
Batched/individual GEOS bei gleichem Vertrag: p50 3,20–7,33 ms, kein belastbarer Gewinn; komplette Extrusion/finale Ausgabe, keine Frames/GPU.
Offen: gemischte Primitivextrusion, tangentialer Flächensweep, weitere UV-Kanäle, isolierte PRT-Flat-Top-/Vertexnormal-Regeln und `mergeVertices`.
Aktiver Vertrag: `t` folgt aktuellen Scope-Achsen; `r` addiert Pivot-Eulerwinkel, Zentrumrotation erhält den Mittelpunkt;
`s` setzt absolute/relative Größen einschließlich Spiegelung. Geteilte Geometrie erst zur Ausgabe transformieren.
`modelling_scope`/`modelling_shape`: eigene Scope-/Frame-/Triangle-Asset-Typen und direkte Python-Operationen;
`t`, `r`, `s`, `translate` mit dokumentierten Formen, geteilte unveränderliche Geometrie, Ausgabe erst nach den Operationen.
Neun Kontrollgruppen inkl. nichtkommutierender Rotationen, schiefer Pivots, Mittelpunkt/Spiegelung, UV/Besitz/Normalen;
zwei ausführbare PRT-Translationen erhalten Lochfläche. Kein isoliertes PRT-Rotations-/Skalierungsorakel vorhanden.
Kanten-/Punktkonstruktoren bestehen; singuläre authored-Normalen und undokumentierter Floating-Selector bleiben offen.
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf), Abschnitt 3: Shape/Scope, Split/Repeat, Komponenten und Bauteilinstanzen.
[CGA Extrude](https://doc.arcgis.com/en/cityengine/latest/cga/cga-extrude.htm): Flächenarten, Mindestspanne, UV-Vererbung, Komponententags und Scope-Ausrichtung.
[CGA Comp](https://doc.arcgis.com/en/cityengine/latest/cga/cga-comp.htm): Auswahl, Ausschluss, Verbindung, Komponenten-Pivots und nullgroße Achsen.
Aktiver Block: `comp(g/m/h)`, Material-/Gruppen-/Lochkomponenten mit geprüfter Quellsemantik.
SDK: Gruppen/Materialien erfüllen Richtungsselektoren, wenn mindestens ein enthaltenes geometrisches Element sie erfüllt.
Ghidra: Gruppenscopes, Materialwert-Vergleich und Lochinversion zuerst prüfen; Gruppen dürfen mehrere Materialien enthalten.
Quellgruppen/Materialdefinitionen neben nativen Indices erhalten, Lochringe/UVs und vorhandene Dreiecke nicht verlieren.
