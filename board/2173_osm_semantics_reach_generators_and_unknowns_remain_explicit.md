Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends: 2188, 2280
Area: generators, world, engine
Tags: buildings, osm, roofs, facades

# Buildings preserve source form and gain spatial detail

## Ergebnis und Ist
Vollständige Städte mit korrekten Grundrissen/Höfen/Parts, plausiblen Dächern und echten
Nahöffnungen/Rahmen/Sockeln. Native Footprints/BuildingHeightInterval, StructureBake,
BuildingMesh und Terrain-Stempel bestehen; Klassen/Dachplan sind lückenhaft, facadePattern
zeichnet flache Fenster. Sonderklasse schlägt Wohnhausannahme, wenn in der Quelle belegt.

## Besitzer und fehlende Verträge
2280 liefert normalisierte Gebäuderinge/Höhen/lieferbare Attribute; 2188 den öffentlichen
Detail-/Produktvertrag. `generators/osm/buildings` besitzt Schema-/Semantikadapter und Plan,
BuildingMesh/StructureBake native Form. Renderer erhält metrische Materialparameter,
keine Quell-Tags. Erst vollständige Stadt/Höfe/Höhen und erkannte Sonderbauten/Dächer,
danach räumliche Eingänge/Fenster; vorhandene Queue und Publikation nutzen.

## Verfahren und Invarianten
- MultiPolygon-Ringe/Höfe/Parts und Höhenintervall erhalten. MVT-Featurezahl ist keine
  Gebäudezahl; Eltern/Parts nicht doppeln, erhöhte Parts nicht auf Boden stempeln.
- Gelieferte height/min_height/levels, Dachform/-höhe, Nutzung/Material/Farbe normalisieren.
  Metrische Höhe hat Vorrang; fehlende Werte folgen erklärter stabiler Klassenpolicy.
  Nicht gelieferte Dach-/Sondertags bleiben unbekannt; keinen Originalbefund erfinden.
- Ein stabiler Gebäudeplan legt Geschosse/Achsen/Öffnungen und Straßenfront/Eingang fest.
  Klassen-/Dachgrammatiken erzeugen plausible regional passende Formen; keine Ortsmodelle.
- Nahe Laibungen/Rahmen/Traufen/Balkone/Gauben besitzen Tiefe; wiederholte Teile teilen
  Geometrie/Instanzen. Mittlere Hüllen/Fernverbände verwenden denselben Plan (2336).
- FacadeUv zu metrischen Maßen/Seeds ausbauen; Fensterposition ist Generatorarbeit,
  Baustoff/Glas/Alterung Shaderarbeit aus 2171. Keine flache Textur als Nahgeometrieersatz.
- Abstand/Fehler vor Terrain/Mesh/Instanzen prüfen; Bounds umfassen tatsächliche Dach-/
  Fassadenform. Raum-/Höhenbezug und Herkunft bis Kontakt/Publikation konsistent halten.

## Abnahme
Wien/Feldkirch zeigen vollständige Grundrisse/Höfe und gültige Höhen; Rosenheim/Flensburg
belegte Sonderbauten und plausible Dächer. Nahöffnungen haben Tiefe, Fernstadt bleibt
stabil und gebündelt. Fehlende Quellsemantik und prozedurale Ergänzung unterscheidbar.
