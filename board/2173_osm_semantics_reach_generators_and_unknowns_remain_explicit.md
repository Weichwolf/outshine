Type: feature
State: active
Architecture: ready
Parent: 2169
Area: world, generators
Tags: webcam, measured
Depends:

# OSM semantics reach generators and unknowns remain explicit

## Belegter IST-Zustand

`src/engine/streaming/StructureBuildQueue.cpp::RawOf` leert `raw.Ways`, füllt im gelesenen Pfad nur
Außenringe, `height` und einen aus `roof:shape` abgeleiteten Pitched-Wert. Innenringe
werden übersprungen. Der aktuelle Renderlog meldet für Feldkirch 31 673 Gebäude,
1 147 mit OSM-Höhe und 30 526 mit Default: 30 526 / 31 673 = 96,4 % Default.
Das ist eine Laufstatistik dieses Providerpfads, keine Aussage über die gesamte OSM-Datenbank.
Die wiederholten hohen Prismen in Olympiaturm und den Städten sind sichtbar; welchen
Anteil Quellenverlust und Defaultgenerator haben, muss die Provenienz zeigen.
`StructureBuildQueue::PitchedOf` reduziert sogar ein vorhandenes `roof:shape`
auf flach/geneigt; `BuildingShape::RoofOf` erfindet daraus Giebel, Walm oder
Mansarde. Der gepinnte Hockenheim-Roh-OSM-Ausschnitt enthält keine Gebäude;
er belegt keine Dach-Tag-Abdeckung. Eine weltweite Quote wird erst aus einem
versionierten, regional geschichteten OSM-Sample mit Nenner Gebäude/Parts,
Provider-Tagverlust und `roof:shape`/Höhen-/Material-Abdeckung berichtet.
`BuildingShape::MassOf` wendet `RowCut` auch auf Hall/Block an; `PlotParts`
verwirft deren Nutzung und klassifiziert kleine Teile neu als Reihenhäuser.
Hockenheim-Markierungen 3/4 zeigen serielle Giebel an einer langen anonymen
Fassade: ein generischer Massingfehler, kein Place-Fall.

## Implementierung

1. Provider-Schema gegen originale OSM-Semantik prüfen: Tiles können Tags schon vor
   outshine verlieren. Verfügbare/fehlende Tags pro Zoom zählen; erforderliche Semantik
   im Providervertrag sichern. Quellwechsel/Ergänzung weiterhin ausschließlich OSM.
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
   Der visuelle Solarpunk-2050-Default darf unbelegte Dach-/Fassadendetails,
   Begrünung und Energieanlagen nur als markierte plausible Konstruktionen
   ergänzen. Dachfläche, Last, Sonne, Wasser und Wartungszugang begrenzen sie;
   `osm-only` und explizite OSM-Material-/Formangaben haben Vorrang.
4. OSM-Brücken/Tunnel/Layers und Stützmauern als Konstruktionen erhalten. Einheitliches
   Höhen-/Kontaktmodell mit 2121; keine Brücke als auf DEM gepresstes Straßenband.
5. Ausführbarer Massing-Schritt: `generators/building/BuildingShape.cpp` wendet
   `RowCut` nur auf Terrace an. Hall/Block bleiben ein Baukörper. Analytische
   Hall-/Terrace-Grundrisse prüfen Teile, Nutzung, Höhe und Dach; Hockenheim
   Markierungen 3/4 vor/nach öffnen. `make format`, Building-Suite und
   `LINT_JOBS=2 make lint` müssen bestehen.

## Abnahme

- [ ] Fixture mit Innenhof, building:part, Höhen-Einheit, fehlender Höhe, Dachausrichtung
      und Brücke überlebt Provider → RawTile → Generator; Tagverlust ist lokalisierbar.
- [ ] Entfernen expliziter Höhe wechselt sichtbar/protokolliert zur abgeleiteten Verteilung;
      bekannte Höhen werden nie durch die Verteilung überschrieben.
- [ ] Olympiaturm/Graz/Feldkirch: plausible Nutzungs- und Höhenverteilung ohne Place-ID-Regeln;
      keine Pflicht zur Kopie ungetaggter Landmarken. Tile-/LOD-Wechsel ändert keine Höhe.
- [ ] `roof:shape=gabled|hipped|flat|mansard` und unbekannter Tag erreichen
      den Generator unterscheidbar; explizite Formen bleiben über Tilefolge,
      LOD und erneuten Import stabil. Ohne Dachtag wird keine Form als gemessen
      ausgegeben. Regionale Stichprobe nennt Dach-Tag-Anteil mit Zähler/Nenner.
- [ ] Gleiche OSM-/DEM-/Seed-Eingabe rendert unter beiden Policies stabil;
      nur unbelegte Dächer wechseln. Hockenheim und eine historisch dichte
      Stadt mit gepinnten OSM-Daten in gleichen Kamera-/Lichtlagen als PNG
      öffnen: weder serielle Einfamilienhausdächer am Ring noch ausschließlich
      Flachdächer in der historischen Stadt. Sichtbare Form, Schatten,
      Stadtsilhouette und Framekosten entscheiden über den `plausible`-Prior.

Wahl: OSM-Semantik plus generische prozedurale Bauformen; Unreal-PCG ist das strukturelle
Vorbild, RAGE die visuelle Referenz, kein belegter Quellcodevertrag.
[OSM Simple 3D Buildings](https://wiki.openstreetmap.org/wiki/Simple_3D_Buildings).

## Verkehr und Vegetation am selben Eingangsvertrag

Auch OSM node/way/relation IDs, highway/railway, bridge/tunnel/layer, access/oneway,
lanes/turn restrictions, gauge/electrified sowie tree/species/genus/leaf_type durchreichen.
Hockenheim-Datengate: VersaTiles `versatiles.osm` v1 z14 im 5x5-Fenster um
14/8581/5603 liefert 348 `kind=track`, 226 `kind=service`, aber kein
`kind=raceway` und kein `highway=raceway`; ein POI `sport=motor` identifiziert
keine befahrbare Runde. Für WI 2260 müssen OSM-Way-/Relation-ID, `highway=raceway`,
Pit-/Service-/Access- und Richtungssemantik am Providervertrag überleben. Ein
gepinnter Roh-OSM-Overlay ist zulässig, wenn die Vektorkacheln die Semantik
nachweislich verlieren; die Zuordnung muss aus Quell-IDs entstehen, nicht aus
Place-Namen oder kamerafesten Heuristiken. Fehlende Identität explizit melden.
Im 25-Tile-Fenster tragen alle 1 162 `streets`-Features MVT-IDs; 98 IDs erscheinen
in mehreren Tiles. Der Decoder erhält nun diese optionalen 64-Bit-Provider-IDs.
Sie sind **keine belegten OSM-Way-IDs**: kein
geprüfter Hockenheim-Raceway-Way passt direkt oder durch einfache Dezimalskalierung.
Roh-OSM zeigt stattdessen 24 `highway=raceway`-Ways und die eindeutige
Grand-Prix-Relation 284588 mit 16 Haupt-Ways und eigener Pitlane-Rolle; das
Quellpin steht in WI 2260. Der allgemeine semantische Provider muss Node-/Way-/
Relation-IDs, Mitgliedsrolle, Tag-Provenienz und gerichtete Nodefolge liefern.
`Data::OsmXmlReader` erhält diese Elemente im gepinnten Ausschnitt;
`World::TransportTopology` baut daraus einen revisionsgebundenen logischen Graphen
und löst die Hockenheim-Relation ohne Place-Zweig auf. Ein versionierter
Streaming-Provider und die Veröffentlichung dieses Graphen in der Welt fehlen noch
(ausführbarer Vertrag 2278).
Ein MVT-Feature-ID-Join ist erst nach einem unabhängigen Nachweis zulässig.
Ein auf Bildkacheln generalisierter Linienzug ohne IDs ist keine vollständige logische
Karte. 2133 besitzt Konnektivität, 2175 Bauwerke, 2176 Artenauswahl. Fehlende Tags werden
gezählt und plausibel ergänzt; keine vermeintliche Messgenauigkeit aus Defaults.
