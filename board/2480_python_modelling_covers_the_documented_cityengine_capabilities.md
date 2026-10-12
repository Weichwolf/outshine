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
Stand c3dc4f1c7: vier Scope-Operationen in dokumentierten Formen, neun Kontrollgruppen und zwei PRT-Translationen;
fünf räumliche Kontrollen plus metrischer Grundriss im aktuellen Showcase. Keine neue Architektur-Rezeptfamilie.
Fünf gleiche Operationen auf 6.208–545.112 Vertices: abschließende Ausgabe 3,1–4,9× schneller als fünf Vertexdurchläufe;
größter Prüfmesh p50 9,93 statt 48,61 ms, 21 warme Python-Läufe mit zwei OpenBLAS-Threads; keine Frame-/GPU-Zeit.
Die 23 bestehenden Ansichten sind pixelgleich erneut erzeugt. Scope/Ghidra/Kamera/Render/Docs jeweils terminal 0;
der gemeinsame Launcher endete nach den abgeschlossenen Kindschritten mit 143, kein Gesamtstatus-0-Anspruch.
SDK-Katalog: 72 Operations-, 70 Funktions-, 10 Attribut- und 49 Bibliotheksfunktionsseiten;
Seitenzahlen zählen weder geprüfte Python-Funktionen noch Rezepte. Überladungen/Semantik einzeln inventarisieren.
37 Ghidra-Ziele und fünf verfügbare RPKs sind Referenzen aus [2400](2400_esri_capabilities_and_interior_primitives_have_executable_evidence.md).
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
Aktiver Ausbau: Polygonflächen mit Löchern/Material/Tags, Extrusion entlang Flächen-/Vertexnormalen sowie Welt-Aufwärtsachse/Flat-Top;
Flächenorientierung, negative/Null-Distanzen, Hoföffnungen, geteilte Kanten, UVs und neue Scope-Ausrichtung prüfen.
Logische Polygonflächen bleiben neben der nativen Triangulierung erhalten; GEOS/CDT nutzen, keine Polygontriangulierung neu erfinden.
`modelling_faces`/`modelling_extrude`: eigene immutable Polygonringe/Corner-UVs neben Triangle-Assets; vier Flächen-Extrusionsarten.
Scope-X orthogonal zur neuen Y-Achse projizieren, eng an Geometrie anpassen, negative Distanz behält negatives Scope-Y.
Flat-Top nimmt mindestens die globale vertikale Quellspanne; Kontakt am First kann zwei getrennt geschlossene Keile ergeben.
Vertexnormalen folgen verbundenen logischen Vertex-IDs und der normierten Summe benachbarter Flächennormalen; authored Normalen verwerfen.
Getrennte deckungsgleiche Vertices werden nicht implizit verschweißt; Extrusionsausgabe erhält tatsächliche Flächenadjazenz.
Lokale Entwicklung: 24 Polygon-/11 Dachfälle, drei echte PRT-Prismen, drei analytische PRT-Distanzen und fünf ungültige Eingaben.
Kostenvergleich: batched/individual GEOS bei identischem vollständigem Vertrag, 21 warme Läufe; bisher kein belastbarer Zeitgewinn.
Nächste Abnahme: eingefrorene lokale Prüfungen, gemeinsame Dachkamera, metrische Schnitte und echter OSM-Hofkörper im Showcase.
Offen: Kantenextrusion/`comp(e/fe)`, tangentialer/kollabierter Sweep, weitere UV-Kanäle, isolierte PRT-Flat-Top-/Vertexnormal-Regeln und `mergeVertices`.
Aktiver Vertrag: `t` folgt aktuellen Scope-Achsen; `r` addiert Pivot-Eulerwinkel, Zentrumrotation erhält den Mittelpunkt;
`s` setzt absolute/relative Größen einschließlich Spiegelung. Geteilte Geometrie erst zur Ausgabe transformieren.
`modelling_scope`/`modelling_shape`: eigene Scope-/Frame-/Triangle-Asset-Typen und direkte Python-Operationen;
`t`, `r`, `s`, `translate` mit dokumentierten Formen, geteilte unveränderliche Geometrie, Ausgabe erst nach den Operationen.
Neun Kontrollgruppen inkl. nichtkommutierender Rotationen, schiefer Pivots, Mittelpunkt/Spiegelung, UV/Besitz/Normalen;
zwei ausführbare PRT-Translationen erhalten Lochfläche. Kein isoliertes PRT-Rotations-/Skalierungsorakel vorhanden.
Kanten-/Punktkonstruktoren, singuläre authored-Normalen und undokumentierter Floating-Selector bleiben offen.
`modelling_scope_probe`: gleiche fünf Transformationen, sofortige Vertexarbeit gegen abschließende Ausgabe;
fünf räumliche Kontrollbilder und metrischer Grundriss am gespeicherten Hauskontrollkörper, keine neuen OSM-Rezepte.
Ghidra 43d5485f6: `r` addiert drei gespeicherte Zahlen; Zentrumdispatch ruft vor/nach Änderung Unit-Cube→Pivot-Matrizen auf.
43/54/161 Instruktionen, keine indirekten Stellen; ein außerhalb liegender Zentrumblock. Translation delegiert nach Skalenauswertung an `Shape::translate`.
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf), Abschnitt 3: Shape/Scope, Split/Repeat, Komponenten und Bauteilinstanzen.
