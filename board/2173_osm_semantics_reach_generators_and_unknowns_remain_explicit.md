Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, world, engine
Tags: buildings, roofs, facades, semantics

# Complete buildings preserve source form and gain spatial detail

## Ergebnis und Ist
Vollständige Grundrisse/Höfe/Parts, plausible Dächer/Sonderbauten und räumliche Nahfassaden.
BuildingHeightInterval, native Footprints, StructureBake/BuildingMesh und Terrain-Stempel
bestehen. Wohnhausöffnungen nutzen Geschosse/Achsen mit gefilterten Fernhüllen und
bedarfsabhängigen Nahlaibungen. Klassen-/Dachpläne und Sonderbauten bleiben lückenhaft.
BuildingForm beschreibt eine prozedurale Formfamilie aus Grundriss/Höhenproportionen,
keine belegte Nutzung; unbekannte schlanke Formen erhalten keine erfundenen Spitzdächer.
Rohattribute bleiben unverändert verfügbar; kanonische Abfragen erhalten Originalwerte
und bevorzugen ausdrücklich gelieferte Schlüssel. Gelieferte Wandfarben erreichen den nativen
Plan und Renderer. Dichte GPU-Farbadressierung vermeidet Leerräume; Sonderklassen und
Materialkalibrierung bleiben lückenhaft, die Laufzeitabnahme ist offen.

## Besitzer und nächste Lieferung
OSM-Adapter normalisiert lieferbare Semantik; generators/building besitzt einen gemeinsamen
Gebäudeplan für Kontakte, Form und alle LODs. Renderer erhält Materialparameter, keine Tags.
StructurePlan/StructureMesher gehören zu generators/building; world besitzt keine
Generator-Eingabeverträge. Erscheinung wird dort vorbereitet und als native Material-/
Vertexparameter publiziert. Quellklassen bleiben aus world und Renderer heraus.
Zuerst Vollständigkeit/Höhen/Höfe/Sonderklassen in Wien/Rosenheim prüfen und verbessern,
danach Dächer und nahe Öffnungen. Bestehende Inputs nutzen, fehlende Tags nicht erfinden.
2336 besitzt Auswahl/Aggregation; weder die gesamte API noch Orbit sind Voraussetzung.

## Architekturentscheidung: begrenzte Gebäuderegeln
[Instant Architecture (SIGGRAPH 2003)](https://peterwonka.net/Publications/pdfs/2003.SG.Wonka.InstantArchitecture.high.pdf)
und [CGA Shape (SIGGRAPH 2006)](https://peterwonka.net/Publications/pdfs/2006.SG.Mueller.ProceduralModelingOfBuildings.final.pdf):
Lokale PDFs: [2003](../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
[2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf).
Grundkörper → Seiten/Geschosse/Achsen → Öffnungen/Module. Ein stabiler Plan führt alle LODs;
Quellangaben überschreiben plausible Ergänzungen. Kein allgemeiner Grammatikinterpreter nötig.
[Grammar-based Encoding (EGSR 2010)](https://peterwonka.net/Publications/pdfs/2010.EGSR.Haegler.GrammarBasedEncoding.pdf)
([lokales PDF](../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf))
begründet kompakte Fassadenparameter mit Shaderauswertung. Sichtbare Tiefe/Silhouette bleibt
Geometrie; subpixelige Muster werden gefiltert. Gemeinsame Nahmodule instanzieren, erst nach
budgetierter Auswahl expandieren. Farbvariation allein schließt dieses Feature nicht.
Die vorhandenen Öffnungen nutzen Achsen/Geschosse für Wohnhaus-, Reihenhaus-
und Blocköffnungen. Kontaktwände/Giebel und Hallen/Türme/Sonderbauten bleiben geschlossen.
Fine ersetzt Wandrechtecke durch geschlossene Laibungen mit zurückgesetzten Glasscheiben;
Shell wertet denselben Öffnungsplan gefiltert aus. Keine zusätzlichen Fernmesh-Dreiecke.
Die konservative Detailauswahl bleibt gültig; keine kleinere Fehlerschranke behaupten.
Nächste Lieferung: schmale Rahmen und Mittelpfosten aus demselben Öffnungsplan.
Nahglas erhält Achsen-/Geschosskoordinaten im freien Fassadenstil 7; keine neuen Attribute
oder Dreiecke. Hülle und Nahglas verwenden dieselben analytisch gefilterten Rahmen-/
Glasflächen, getrennt von Wandfarbe. Gesamtdeckung und Material bleiben über LOD kohärent.
Fine ist kein Auftrag zur ungeprüften Expansion aller Fassadendetails. Sichtabhängige
Zellprodukte wählen Laibungen separat aus projizierter Tiefe und konservativer Entfernung;
die Hülle behält denselben gefilterten Öffnungsplan. Feste explizite Tile-LODs bleiben fest.
Fine-Zellprodukte binden Eye/Projection auch bei explizitem LOD an ihre Wiederverwendung;
die CPU-Vergleichsreferenz fordert weiterhin volle geometrische Genauigkeit.

## Gemeinsame Polygontriangulierung
Base besitzt PolygonTriangulation, Gebäude und Wasser konsumieren denselben nativen Vertrag.
Innenhöfe/Inseln benötigen randtreue Triangulierung mit Löchern. GEOS ≥3.10 liefert diese
über die C-API aus dem Systempaket (`brew install geos`, apt `libgeos-dev`). Keine GEOS-Typen
im öffentlichen Vertrag, kein gebündelter Fremdcode. Bestehende einfache Dachtriangulierung
bleibt bestehen; ungültige Polygone werden nicht durch Flächenverlust kaschiert.

## Quellsemantik und Sonderbauten
Alle gelieferten Attribute unverändert speichern; Abfrage bietet normalisierte Aliase,
ohne Tag-Duplikate. Ein ausdrücklich gelieferter Schlüssel schlägt seinen abgeleiteten Alias.
Höhe/Unterkante, Klasse/Nutzung, Dach, Material/Farbe und Parts konsumieren, soweit geliefert.
Bekannte Schornsteine, Türme und Wassertürme behalten Baukörper/Höhe, erhalten aber keine
Wohnhausfassade. Native Klassifikation bis Gebäudeplan und Materialwahl führen.
[OpenMapTiles-Building-Schema](https://github.com/openmaptiles/openmaptiles/blob/master/layers/building/building.yaml)
exportiert nicht sämtliche OSM-Tags; tatsächlich gelieferte Eigenschaften prüfen.
Fehlende Klasse bleibt unbekannt; Schlankheit allein beweist keinen Schornstein/Kirchturm.
Die prozedurale Formwahl berücksichtigt Höhe relativ zur größten Grundrissausdehnung:
19 Meter allein machen keinen Turm; breite Blöcke/Hallen behalten ihren Formplan.
Hohe schlanke Formen erhalten ohne gelieferte Dachneigung ein neutrales Flachdach.
Ein Spitzdach ist keine aus Höhe abgeleitete Quellsemantik. Gelieferte Dachangaben gehen vor;
Höhe, Grundriss und alle LOD-Bounds bleiben erhalten.
[Recherche](../doc/references/README.md): kompakte Regeln statt einer zweiten Gebäudepipeline.

## Gebäudefarbe bis zum Bild
Gemeinsamer CssColour-Parser in base; UI und OSM nutzen alle standardisierten CSS-Farbnamen
und dieselbe Hex-Farbschreibweise. Gültige Quellnamen nicht auf eine Teilpalette reduzieren.
`building:colour` schlägt OpenMapTiles' Alias `colour`; ungültige/teiltransparente
Wandfarben melden, nicht als belegte Farbe übernehmen. Linearer Wandfarbparameter im
Gebäudeplan; vorhandener nativer Vertex-Faktorpfad, optional und ohne OSM-Typen im Renderer.
Gelieferte Farbe ersetzt die Wandpalette und färbt keine Fenster/Rahmen/Türen oder Dächer.
Fernverbände mischen linear mit Grundrissfläche als Gewicht; unbekannte Anteile verwenden
die deklarierte Grundfarbe. Das Ergebnis ist eine abgeleitete Mischung, keine Quellangabe.
Quelle/Plan, gemischte Mesh-Streams, Fernverband und tatsächliches Place-Bild gemeinsam prüfen.
Gebäudefarbe ist pro Baukörper konstant: nicht dauerhaft als vier Floats je Ecke vervielfachen.
Die native Material-/Batchdarstellung kompakt halten; optionale GPU-Attribute dürfen keine
Lücken bis zum höchsten gemeinsamen Positionsindex reservieren. Layout-Ownership: 2188.

## Verfahren
- MultiPolygone/Höfe/Parts erhalten, Eltern/Parts nicht doppeln. Sonderklasse schlägt
  Wohnhausannahme, wenn geliefert. Featurezahl ist keine Gebäudezahl.
- Gelieferte Höhe/Unterkante/Geschosse, Dachform/-höhe, Nutzung/Material/Farbe normalisieren.
  Endliche geordnete Höhenintervalle sind vorzeichenbehaftet relativ zum Geländebezug.
  Nichtnull-Unterkanten behalten ihr Intervall ohne künstlichen Sockel/Parzellenteilung;
  erhöhte/unterirdische Parts stempeln keinen falschen Boden. Unbekannte Werte folgen
  stabiler Klassenpolicy; widersprüchliche Quellen bleiben von Ergänzungen unterscheidbar.
- Ein kompakter Plan bestimmt Geschosse/Achsen/Öffnungen, Straßenfront/Eingang, Dach und
  Kontaktflächen. Kontakte ohne Fassadenmesh erzeugen. Regionale Grammatik ist plausible
  Ergänzung, keine behauptete Rekonstruktion oder Place-Sondergeometrie.
- Nahe Laibungen/Rahmen/Traufen/Balkone/Gauben besitzen Tiefe; wiederholte Teile instanzieren.
  Hüllen/Fernverbände verwenden denselben Plan. Bounds umfassen alle Formelemente.
- FacadeUv liefert metrische Koordinaten; Öffnungsposition/Form ist Generatorarbeit,
  Baustoff/Glas/Alterung Shaderarbeit aus 2171. Terrain- und Renderbezug gemeinsam halten.

## Abnahme
Wien/Feldkirch zeigen vollständige Formen/Höfe/Parts; Rosenheim/Flensburg korrekte gelieferte
Sonderklassen und plausible Dächer. Nahöffnungen räumlich, Fernstadt stabil und gebündelt.
Keine zusätzliche Feinmesh-Arbeit für unsichtbare Details; AGENTS-Bild-/Ladebudget halten.
