Type: feature
State: active
Priority: P0
Architecture: ready
Parent: 2169
Area: world, generators
Tags: webcam, measured
Depends:

# OSM semantics reach generators and unknowns remain explicit

## Belegter IST-Zustand
Rosenheims Gebäudekacheln verlieren Klasse, Geschosse und Dachtypen. POI-Klassen haben
keinen bewiesenen Identitätsbezug zu Gebäuden; schlanke Form beweist keinen Schornstein.
Original-OSM liefert dagegen Weg [619896097](https://www.openstreetmap.org/way/619896097)
mit `man_made=chimney`, `building=yes`, `height=80`. Seine Lage ergibt vom Place aus
etwa 160,2° Bearing und 478 m Abstand, passend zum linken Referenzbereich. Die Identität
zum bisherigen MVT-Polygon bleibt separat zu belegen; keine Übertragung allein durch Nähe.
Bei mehreren Tags hat die explizite Sonderbauwerksklasse Vorrang vor `building=yes`.

Der Original-API-Ausschnitt enthält unvollständige Fernrelationen. `OsmChunkSetLoader::LoadRegion`
erhält sie; der strikte `Load`-Pfad fordert weiterhin vollständige Referenzen.
Für räumliches Streaming muss WI 2280 referenzielle Vollständigkeit je konsumiertem Produkt
prüfen: offene Referenzen erhalten, relevante Nachbarn anfordern; keine nötigen Bezüge ignorieren. `OsmElements`
prüft dafür die transitive Referenzhülle typisierter Produktwurzeln gemeinsam und zyklusfest.
`world/ground/OsmBuildingFootprints` besitzt daraus Ringe/Koordinaten und pinnt den Quell-Snapshot;
Tags bleiben über typisierte Original-ID erreichbar. Offene/mehrdeutige Ringketten verhindern Publikation.
`OsmXmlReader` erhält Originaldaten mit längengebundenem DOM-Budget und 4-MiB-Limit.

`StructureBuildQueue::RawOf` erhält jetzt Innenringe und numerische Mindesthöhe bis zu
Mesh und Terrain-Stempel. Dachformen bleiben auf flach/geneigt reduziert; Bauwerksklasse
und Original-ID fehlen weiterhin im Bake. Die visuelle Place-Abnahme der Hofgeometrie ist offen.
Originalsemantik über revisionsgebundene Provider erhalten; keine Identitätszuordnung allein durch Nähe.

## Lieferblocker
Feldkirch und Wien brechen nach Weitergabe von `min_height` mit ungültigem Höhenintervall ab.
Feldkirchs Quellkachel enthält `height=4`, `min_height=7`; der räumlich passende Original-Part
987120770 enthält nur `building:levels=1`, `building:min_level=2`, keine metrischen Höhen.
MVT-Identität und Höhenprovenienz sind nicht bewiesen. Kein blindes
Addieren, Ignorieren der Mindesthöhe oder Verwerfen des Bauwerks. Die Quellnormalisierung
muss Rohwerte, Widerspruch und deklarierte Ableitung getrennt erhalten; der native Generator
bekommt erst ein gültiges aufgelöstes Intervall. Wien: Originalwege 241884106/1235545599
haben `min_height=5.5` ohne Gesamthöhe; die Kachel liefert jeweils `height=min_height=5`.
`OsmBuildingHeights::Resolve` liefert Herkunft und gültiges Intervall aus expliziter Policy.
Metrische Widersprüche bleiben Fehler; Geschosskonflikte bleiben markiert. Runtime-Anbindung ist offen.

## Implementierung

1. Rohe OSM-Nodes, Ways und Relations sind die verbindliche semantische Weltquelle.
   `world/data` liest Original-IDs, vollständige Tags, Nodefolgen und Relationsrollen;
   revisionsgebundene räumliche Indizes dürfen diese Informationen nicht reduzieren.
   `OsmXmlReader` und `OsmChunkSetLoader` wiederverwenden; XML/PBF bleiben Adapterformate.
   VersaTiles-Kartenkacheln ersetzen diese Quelle nicht. Den bisherigen Gebäude- und
   Straßenimport durch native OSM-Produkte ersetzen, nicht mit einem optionalen Overlay
   dauerhaft weiterführen. Fehlende Quelldaten explizit melden; keine stillschweigende
   Rückkehr zu semantisch reduzierten Kartenkacheln. WI 2280 besitzt weltweites Streaming.
2. `RawTile`/`StructureBuildQueue`/`src/generators/building/StructureBake.*`: stabile Feature-ID,
   Multipolygon mit Löchern, building/part, height/min_height, levels/min_level,
   roof shape/height/levels/direction/orientation, Material/Farbe und Nutzung erhalten.
   Zahlen mit Einheiten normalisieren; explizit, abgeleitet, unbekannt unterscheiden.
   Bekannte `roof:shape`-Werte als präzise Form durchreichen; unbekannte Werte
   weder als pitched=true noch als Flat umdeuten. Dachdetails wie Schornsteine
   nur aus belegten Tags/Geometrie, sonst keine unbegründete Serienausstattung.
3. Explizite Höhe hat Vorrang; Levels mit plausibler Geschosshöhe ableiten; ungeklärte
   Gebäude regional/nutzungsabhängig aus einer deklarierten Verteilung generieren.
   Seed aus Feature-ID und World-Seed, niemals Tile-Ankunft oder Kameraentfernung.
   Teile und Elternumriss nicht doppelt extrudieren; Innenhöfe bleiben frei.
   Dachform folgt einer im Szenario deklarierten Policy: `osm-only` verwendet
   explizite OSM-Form und belegte `building:part`-Geometrie; fehlende Form
   bleibt unbekannt und erhält nur einen technischen, als unbekannt markierten
   Abschluss. `plausible` ergänzt fehlende Formen aus Nutzung, Grundriss,
   Nachbarbebauung und regionaler OSM-Evidenz mit begrenztem Kandidatenraum.
   Der Seed hängt an Feature-ID und Welt-Seed; Annahme und Konfidenz stehen im
   ConstructionResult. `plausible` ist der visuelle Standard, `osm-only` der
   strikt quellentreue Prüfmodus. Kein Place-Namen-Preset. Explizites OSM hat
   in beiden Modi Vorrang; Policy-Wechsel darf keine belegte Dachform ändern.
   Der Webcam-Meilenstein ergänzt unbelegte Details passend zu Nutzung, Region
   und Aufnahmezeit; keine Solarpunk-Umgestaltung der beobachteten Bebauung.
   Dachfläche, Last, Sonne, Wasser und Wartungszugang begrenzen Konstruktionen;
   explizite OSM-Material-/Formangaben haben Vorrang.
4. OSM-Brücken/Tunnel/Layers und Stützmauern als Konstruktionen erhalten. Einheitliches
   Höhen-/Kontaktmodell mit 2121; keine Brücke als auf DEM gepresstes Straßenband.
5. Vorhandenes Massing erhalten: `BuildingShape::RowCut` ist bereits auf Terrace
   beschränkt. Als nächste vollständige Lieferung Originalklassen und Dachformen
   durch WI 2280 in den Client integrieren: Rosenheims belegter Schornstein ohne
   Wohnfassade, Flensburgs Kirchenklasse ohne höhenbasierte Bürohausannahme.
   Dachlose Quellen bleiben plausible Konstruktionen, keine vermessenen Formen.

## Abnahme

- [ ] Fixture mit Innenhof, building:part, Höhen-Einheit, fehlender Höhe, Dachausrichtung
      und Brücke überlebt Provider → RawTile → Generator; Tagverlust ist lokalisierbar.
- [ ] Entfernen expliziter Höhe wechselt sichtbar/protokolliert zur abgeleiteten Verteilung;
      bekannte Höhen werden nie durch die Verteilung überschrieben.
- [ ] Rosenheim/Flensburg/Feldkirch: plausible Nutzungs- und Höhenverteilung ohne Place-ID-Regeln;
      keine Pflicht zur Kopie ungetaggter Landmarken. Tile-/LOD-Wechsel ändert keine Höhe.
- [ ] `roof:shape=gabled|hipped|flat|mansard` und unbekannter Tag erreichen
      den Generator unterscheidbar; explizite Formen bleiben über Tilefolge,
      LOD und erneuten Import stabil. Ohne Dachtag wird keine Form als gemessen
      ausgegeben. Regionale Stichprobe nennt Dach-Tag-Anteil mit Zähler/Nenner.
- [ ] Gleiche OSM-/DEM-/Seed-Eingabe rendert unter beiden Policies stabil;
      nur unbelegte Dächer wechseln. Rosenheim und Flensburg mit gepinnten
      OSM-Daten in gleichen Kamera-/Lichtlagen als PNG öffnen und gegen die
      Webcam prüfen: keine Wohnfassaden an Sonderbauwerken oder pauschalen
      Flachdächer in der historischen Stadt. Sichtbare Form, Schatten,
      Stadtsilhouette und Framekosten entscheiden über den `plausible`-Prior.

[OSM Simple 3D Buildings](https://wiki.openstreetmap.org/wiki/Simple_3D_Buildings).

## Verkehr und Vegetation am selben Eingangsvertrag

Auch OSM node/way/relation IDs, highway/railway, bridge/tunnel/layer, access/oneway,
lanes/turn restrictions, gauge/electrified sowie tree/species/genus/leaf_type durchreichen.
MVT-Feature-IDs sind keine nachgewiesenen OSM-IDs; kein Join ohne unabhängigen Nachweis.
`Data::OsmXmlReader` erhält Originalelemente und Tags. `World::TransportTopology` und
`OsmTransportLoader` besitzen bereits revisionsgebundene native Verkehrsprodukte und
Quellen; diesen Straßenfortschritt erhalten. Der allgemeine räumliche Eingangsvertrag
muss dieselbe Quelle auch Gebäude- und Vegetationsgeneratoren zugänglich machen.
WI 2280 besitzt weltweites Streaming, 2133 Konnektivität, 2175 Bauwerke, 2176 Artenauswahl.
Fehlende Tags bleiben unbekannt oder markiert plausibel ergänzt; Defaults sind keine Messung.
