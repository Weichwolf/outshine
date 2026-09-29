Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Area: generators, world
Tags: buildings, visual
Depends: 2173

# Häuser besitzen Straßenfront, Eingänge und räumliche Fassaden

## Ergebnis

In Straßenhöhe sind Häuser gebaute Körper: Sockel, Erdgeschoss, Eingang, zurückliegende
Verglasung, Rahmen, Laibungen, Dachkante und Entwässerung erzeugen räumliche Tiefe.
Nutzung und Konstruktion bestimmen den Rhythmus; keine identische Fenstertextur auf allen Flächen.
Höfe und Durchfahrten bleiben frei. Hallen, Wohnhäuser und geschlossene Blockränder unterscheiden sich.

## Vorhanden und Umsetzung

StructureBuildQueue::RawOf übergibt bereits Straßenlinien mit Halbbreite. BuildingShape und
BuildingMesh erzeugen native Körper, Dächer und einzelne Details. Den bestehenden Generator
fortführen; keine zweite Hausbibliothek und keine Place-Sondermodelle.

1. 2173 erhält belegte Grundrisse, Innenringe, Parts, Höhe/Geschosse und Dachform. Diese Daten
   plus Gelände und zugängliche Straßenfront bestimmen einen stabilen Konstruktionsplan.
2. StructureBake/Frontage wählen einen erreichbaren Zugang. Räumlich nahe Autobahn, Brücke
   oder Tunnel ist keine passende Hausfront. Nachbar-Tiles dürfen den Zugang nicht ändern.
3. BuildingShape zerlegt Grundriss und Nutzung in Baukörper, Geschosse, Hof und Dach.
   BuildingMesh erzeugt zunächst einen vollständig detaillierten Straßenabschnitt:
   Eingang mit Schwelle, Fensterlaibungen/Rahmen, Sockel und sauberer Dachabschluss.
4. Materialzuweisungen folgen diesen Bauteilen (2171). Glas liegt hinter Rahmen/Laibung;
   Türen landen auf begehbarem Niveau, Brandwände bleiben geschlossen. Kein Detail durchdringt
   Nachbarbau, Durchfahrt oder Dach. Unbelegte Details variieren deterministisch nach Gebäude-ID.
5. In Entfernung verlieren dieselben Gebäude kleine Details kontrolliert. Footprint, Nutzung,
   Kontakt und Identität bleiben stabil; kein zweiter quellenfremder LOD-Baukörper.

## Fehler und Abhängigkeiten

Belegte Maße/Parts und freie Durchfahrten sind harte Grenzen. Widersprüche melden;
keine angeblich gültige Konstruktion durch stilles Weglassen. Fehlende Attribute bekommen
explizite plausible Defaults. Kein Warten auf einen fertigen weltweiten Router oder Vegetation.
2173 blockiert die vollständige semantische Konstruktion; 2171 kann gleichzeitig entstehen.

## Fertig, wenn

Darmstadt, Husum, Rosenheim und Wien zeigen bei 5/30/200 m glaubwürdige unterschiedliche
Häuser; dieselbe Straßenfahrt verbindet Details mit der Gesamtstadt ohne Sprünge.
Ein zugemauerter Hof, schwebender Eingang oder blockierte Durchfahrt widerlegt das Ergebnis.
Besitzerdateien: StructureBuildQueue, StructureBake, BuildingShape, BuildingMesh, FacadeUv.
make format; betroffene Generator-Suites; make lint; alle Places rendern und PNGs öffnen.
