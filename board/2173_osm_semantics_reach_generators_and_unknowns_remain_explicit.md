Type: feature
State: open
Area: world, generators
Tags: webcam, measured
Depends: nothing

# OSM semantics reach generators and unknowns remain explicit

## Belegter IST-Zustand

`src/engine/StructureBakes.cpp::RawOf` leert `raw.Ways`, füllt im gelesenen Pfad nur
Außenringe, `height` und einen aus `roof:shape` abgeleiteten Pitched-Wert. Innenringe
werden übersprungen. Der aktuelle Renderlog meldet für Feldkirch 31 673 Gebäude,
1 147 mit OSM-Höhe und 30 526 mit Default: 30 526 / 31 673 = 96,4 % Default.
Das ist eine Laufstatistik dieses Providerpfads, keine Aussage über die gesamte OSM-Datenbank.
Die wiederholten hohen Prismen in Olympiaturm und den Städten sind sichtbar; welchen
Anteil Quellenverlust und Defaultgenerator haben, muss die Provenienz zeigen.

## Implementierung

1. Provider-Schema gegen originale OSM-Semantik prüfen: Tiles können Tags schon vor
   outshine verlieren. Verfügbare/fehlende Tags pro Zoom zählen; erforderliche Semantik
   im Providervertrag sichern. Quellwechsel/Ergänzung weiterhin ausschließlich OSM.
2. `RawTile`/`StructureBakes`/`src/generators/building/StructureBake.*`: stabile Feature-ID,
   Multipolygon mit Löchern, building/part, height/min_height, levels/min_level,
   roof shape/height/levels/direction/orientation, Material/Farbe und Nutzung erhalten.
   Zahlen mit Einheiten normalisieren; explizit, abgeleitet, unbekannt unterscheiden.
3. Explizite Höhe hat Vorrang; Levels mit plausibler Geschosshöhe ableiten; ungeklärte
   Gebäude regional/nutzungsabhängig aus einer deklarierten Verteilung generieren.
   Seed aus Feature-ID und World-Seed, niemals Tile-Ankunft oder Kameraentfernung.
   Teile und Elternumriss nicht doppelt extrudieren; Innenhöfe bleiben frei.
4. OSM-Brücken/Tunnel/Layers und Stützmauern als Konstruktionen erhalten. Einheitliches
   Höhen-/Kontaktmodell mit 2121; keine Brücke als auf DEM gepresstes Straßenband.

## Abnahme

- [ ] Fixture mit Innenhof, building:part, Höhen-Einheit, fehlender Höhe, Dachausrichtung
      und Brücke überlebt Provider → RawTile → Generator; Tagverlust ist lokalisierbar.
- [ ] Entfernen expliziter Höhe wechselt sichtbar/protokolliert zur abgeleiteten Verteilung;
      bekannte Höhen werden nie durch die Verteilung überschrieben.
- [ ] Olympiaturm/Graz/Feldkirch: plausible Nutzungs- und Höhenverteilung ohne Place-ID-Regeln;
      keine Pflicht zur Kopie ungetaggter Landmarken. Tile-/LOD-Wechsel ändert keine Höhe.

Wahl: OSM-Semantik plus generische prozedurale Bauformen; Unreal-PCG ist das strukturelle
Vorbild, RAGE die visuelle Referenz, kein belegter Quellcodevertrag.
[OSM Simple 3D Buildings](https://wiki.openstreetmap.org/wiki/Simple_3D_Buildings).

## Verkehr und Vegetation am selben Eingangsvertrag

Auch OSM node/way/relation IDs, highway/railway, bridge/tunnel/layer, access/oneway,
lanes/turn restrictions, gauge/electrified sowie tree/species/genus/leaf_type durchreichen.
Ein auf Bildkacheln generalisierter Linienzug ohne IDs ist keine vollständige logische
Karte. 2133 besitzt Konnektivität, 2175 Bauwerke, 2176 Artenauswahl. Fehlende Tags werden
gezählt und plausibel ergänzt; keine vermeintliche Messgenauigkeit aus Defaults.
