Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: world, generators, engine
Tags: buildings, osm, roofs, facades

# Buildings preserve original form and gain spatial detail

## Ergebnis und vorhandene Fähigkeit
Städte zeigen korrekte Grundrisse, Höfe, Parts, Dächer und Sonderbauwerke sowie räumliche
Eingänge, Laibungen, Rahmen und Sockel. Original-Footprints, Höhenintervalle, native
Bakes und Terrain-Stempel existieren. Klasse/Dach-Tags erreichen den Meshplan noch
nicht vollständig; Fassaden wirken repetitiv. Rosenheims Schornsteine sind keine Wohnhäuser.

## Nächste Lieferung und Besitzer
`generators/osm/buildings` normalisiert Original-Grundrisse und Tags/Einheiten;
BuildingHeightInterval ist das native Höhenprodukt.
StructureBake/BuildingMesh konsumieren gepinnte Original-IDs und Tags. Native
Footprints/Höhen und Nahdetails müssen denselben öffentlichen Produktvertrag aus 2188
nutzen; Abstand/Fehler vor Terrain- und Detailarbeit auswerten.
Zuerst Sonderbauwerksklasse und Dachform im nativen Rosenheim-/Flensburg-Bild liefern;
danach zusammenhängende Straßenfront, Eingang und räumliche Fenster an Nahgebäuden.
Bestehende Queue/Publikation verwenden; kein MVT-Zwischenformat oder Ortsmodell.

## Parametrische Gebäudestruktur
- Originalringe/Parts und Höhen definieren zuerst Hülle, offene Höfe und Dachplan.
  Fronten aus Straßenkontakt und Nutzung bestimmen Eingangsseite; Geschosshöhe,
  Fensterachsen und Achsränder als ein stabiler Plan je Gebäude, nicht Shaderzufall.
- BuildingMesh erzeugt nahe Laibungen, Rahmen, Traufen, Dachkanten und belegte Balkone/
  Gauben aus diesem Plan. Wiederholbare Formen teilen Geometrie; Position/Variation sind
  kompakte Instanzen. Mittlere Hüllen und Fernverbände konsumieren denselben Plan (2336).
- FacadeUv kodiert heute Stil/Achsen/Geschosse; facadePattern zeichnet flache Fenster.
  Diesen Pfad zu explizitem metrischem Fassaden-/Materialparametervertrag ausbauen.
  Gebäudeform und Öffnungsposition sind Generatorarbeit, Baustoff/Glasantwort ist 2171.
- Detail/Abstandsfehler vor Mesh/Instanzen prüfen; konservative Bounds und Produktversion
  umfassen tatsächliche Dach-/Fassadengeometrie. Unbekannte Öffnungen/Dekoration bleiben
  deterministische Ergänzung, nicht nachträglich als Originaltags ausgegeben.

## Umsetzung und Invarianten
- building/part, height/min_height, levels/min_level, roof shape/height/levels/direction,
  Nutzung, Material und Farbe erhalten. Metrische Höhe hat Vorrang; fehlende Werte
  folgen erklärter Policy, widersprüchliche Werte bleiben Fehler. Eltern/Parts nicht doppeln.
- Höfe offen halten und erhöhte Parts nicht auf den Boden stempeln. Explizite Sonderklasse
  schlägt generische Gebäudeannahme; Schornsteine, Kirchen und Türme unterscheiden sich.
- Belegte Dachform/Material erhalten. Unbekannte Form bleibt unbekannt; plausible Ergänzung
  nutzt Grundriss, Nutzung und regionale OSM-Evidenz mit stabilem Objekt-/Welt-Seed.
  Keine unbegründete Serienausstattung oder kamerabhängige Umgestaltung.
- Straßenkontakt bestimmt Front, Eingang und Sockel. Nahdetails erhalten echte Tiefe;
  Fassaden-UVs und metrischer Maßstab stimmen mit Materialauswertung aus 2171 überein.
- Raum-/Höhenbezug, Terrainkontakt und Quellbesitz bleiben vom Import bis zur Publikation
  konsistent. 2336 begrenzt Details vor Erzeugung und fasst entfernte Gebäude zusammen.

- Stadtidentität braucht belegte Silhouetten: Kirchturm, Hallen, Balkone, Gauben,
  Traufe und Dachaufbauten nach Klasse/Form erzeugen. Fehlende Landmarkenbeschreibung
  erlaubt keine Foto-Sondergeometrie; allgemeine Grammatiken erhalten die Unsicherheit.

## Abnahme
Rosenheim/Flensburg zeigen erkannte Sonderbauten und Dächer; Wien/Feldkirch erhalten
Parts und gültige Höhenintervalle. Höfe, Eingänge und Fenster besitzen plausible Tiefe;
keine fehlenden Gebäude oder Textur als Ersatz für ganze Nahfassaden. Unbelegte Ergänzung
bleibt von Originalangaben unterscheidbar. Die öffentliche Generator-API ist kein Sonderpfad.
