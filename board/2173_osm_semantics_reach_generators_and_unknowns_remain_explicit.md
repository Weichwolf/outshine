Type: feature
State: open
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
bestehen. Klassen-/Dachpläne sind lückenhaft; facadePattern zeichnet bisher flache Fenster.

## Besitzer und nächste Lieferung
OSM-Adapter normalisiert lieferbare Semantik; generators/building besitzt einen gemeinsamen
Gebäudeplan für Kontakte, Form und alle LODs. Renderer erhält Materialparameter, keine Tags.
Zuerst Vollständigkeit/Höhen/Höfe/Sonderklassen in Wien/Rosenheim prüfen und verbessern,
danach Dächer und nahe Öffnungen. Bestehende Inputs nutzen, fehlende Tags nicht erfinden.
2336 besitzt Auswahl/Aggregation; weder die gesamte API noch Orbit sind Voraussetzung.

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
