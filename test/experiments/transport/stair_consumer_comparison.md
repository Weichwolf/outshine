# Tatsächlicher Architekturverbrauch der Transportfreiräume (#568)

Stand 2026-10-17: Der vorhandene Buildings-Erzeuger
`tile_facades.FacadeDetails` verarbeitet ursprüngliche Wiener Polygonteile,
deren Grundrisse anhand seiner tatsächlichen Körperhöhen angepasst werden.
Die erzeugten Körper werden anschließend erneut gegen alle Transportvolumen
geprüft. Verfahren ausschließlich im Python-Experiment; keine Fachrezepte,
Bibliotheksverträge oder native Implementierung geändert.
Gemeinsamer Auftrag und weitere Abnahme: [#568](https://github.com/Weichwolf/outshine/issues/568).

## Datenfluss und Ergebnis

Originaler OpenFreeMap-Payload, 41 Building-Features, IDs/Attribute und ungekürzte
Originalgeometrien bleiben erhalten. Zehn ungültige Originalfeatures werden nicht
repariert. Die übrigen Quellen ergeben 100 gültige Polygonteile mit stabilen
Part-IDs und expliziter ursprünglicher Quell-ID. Herkunft von Tile-Clipping und
Koordinatenraster bleibt erhalten; der metrische Rasterschritt wird im benutzten
Projektionsframe aus einem Original-Vektor-Rasterschritt bestimmt.
Die vorhandene Buildings-Context-Anreicherung liest denselben PBF mit ursprünglichen
Landuse-/POI-/Transportbelegen. Sie ergänzt Kontext, keine erfundenen Quelltags.

64 Teile aus 22 ursprünglichen Quellen liefern **65 tatsächliche Gebäudekörper**.
Die 110 Ledger-Einträge bestehen aus 100 Polygonteilen und zehn ungültigen
Originalfeatures, nicht aus 110 vollständigen Gebäuden.
46 Einträge bleiben mit Original und Grund zurückgestellt:

| Grund | Einträge |
|---|---:|
| Anker außerhalb des aktuellen Terrainprodukts | 33 |
| Ungültiges Originalfeature | 10 |
| Ursprüngliches `hide_3d` | 2 |
| Fertiger Körper ragt weiterhin in Transportfreiraum | 1 |

Zwei tatsächliche Teile werden gekürzt: `building:42574490:3:part:1` um
8,556159628230262 m² und `building:13091303570:6:part:1` um
0,16899249882229128 m². Die Quell-IDs beginnen jeweils mit
`openfreemap:14/8937/5678:`. Ein Rest zerfällt in zwei Polygone; Teile werden
ausdrücklich einzeln an denselben vorhandenen Erzeuger übergeben.
Unveränderte Geometrie verwendet den bereits erzeugten Körper erneut.
Diese Mengen sind kein direkter Vergleich mit den früheren Provider-Höhenprismen:
Nun bestimmen tatsächliches Rezept und der aktuelle Anker jedes Polygonteils
das Abfrageband, statt des ursprünglichen DEM-Ankers des gesamten Features.

Zunächst erzeugt der unveränderte Fachcode den ursprünglichen Körper.
Minimales/maximales Z aller seiner Vertices begründet das Höhenband.
Transport schneidet damit den Quellgrundriss; Buildings baut die Restpolygone.
Danach wird die vollständige XY-Projektion jedes fertigen Körpers samt seinem
erneut gemessenen Höhenband geprüft. Ein vorstehender Körper kann daher trotz
freiem Quellgrundriss zurückgestellt werden; dieser Fall tritt tatsächlich auf.
Das kartesische Produkt aus Projektion und Höhenband ist konservativ, keine
exakte 3D-Körperüberschneidung. Linienprojektionen vertikaler Dreiecke ergänzen
keine positive Konfliktfläche; projizierte Flächendreiecke werden vereinigt.

Der Erzeuger arbeitet weiter flach bei Z=0. Der versionierte Adapter
`transport-flat-recipe-to-current-terrain/1` übersetzt seine Vertices danach
starr um das an der ursprünglichen Part-Repräsentativposition gemessene Z.
Dieser Kontakt stammt aus dem vollständigen **aktuellen** Transportterrain,
nicht aus einem unverändert angenommenen DEM. XY bleibt identisch.
Frame, Original-/Restpolygone, Terrainface/-normale und Translation stehen im Ledger.
Ein einzelner Anker belegt weder ein ganzes Fundament noch Hangpassung.
Es werden keine alten Pilotmetadaten als Wiener Höhendatum ausgegeben.

Die 65 exportierten konstanten Gebäude-Reservationen entsprechen den tatsächlichen
Körperprojektionen/-höhen und besitzen Bodyindex, Owner, Originalquelle und Part.
Damit kann Vegetation Transport und erzeugte Gebäude gemeinsam abfragen.
Der wirkliche Habitat-/Pinus-Verbrauch folgt noch; eine bloße Reservierungszahl
oder der bisherige diagnostische Callback gilt nicht als Wuchsprogramm.
Auch die gemeinsame `pipeline.prepare`-Abfolge mit tatsächlichen Fachcallbacks
und die öffentliche affine API sind noch keine Lieferung dieses Versuchs.

## Prüfung, Kosten und Bild

Produzent: `236902141ccced16dc9fda0c726073af41a3187c`.
Quellbibliothek und das frische installierte Wheel aus PR #580 liefern bytegleiche
Vorher-/Nachher-Mesharchive; 205 Bibliotheksmodule unverändert/quellgleich.
Neue Kontrollen: echte Körperhöhe statt Quellhöhe, unveränderte lokale Arrays
bei Translation, reine Höhenberührung, Ablehnung eines generierten Überhangs.
Je **38 Quell-/installierte Kontrollen** einschließlich der bisherigen
Konflikt-/Unterlauf-/Planprüfungen bestehen. Ruff/Format und Funktionen ≤120 Zeilen.
Bibliothek unverändert; die 532 Pakettests aus PR #580 werden dadurch nicht ersetzt.
Native Gates bleiben der Integration vorbehalten.

Frische Leser laden die gespeicherten Körper und Originalquellen erneut, prüfen
vollständige Part-/Bodyzuordnung, tatsächliche Terrainkontakte, Höhenbänder und
alle exportierten Reservierungsprojektionen. Der abschließende Vollscan aller
5986 Transportvolumen ergänzt den indexierten Produzentenpfad.
Die breiten GEOS-XY-/Topologiegrenzen des
[Präzisionsberichts](stair_occupancy_precision.md) bleiben offen.

Quelllauf: Eingabekette/aktuelles Terrain 4,1603 s, Context/Architektur/Abfragen
6,2190 s, Prozess-RSS-Peak 196878336 B. Linux x86_64, i5-1135G7,
NumPy 2.5.4 / Shapely 2.2.0; Entwicklungsprozesse `nice -n 10`, Desktop-/Fremdlast.
Vorher 6433 Vertices/2279 Dreiecke/181740 B; nachher
6529 Vertices/2311 Dreiecke/184428 B eigener Mesharrays.
Keine degenerierten gespeicherten Dreiecke. Keine Frame-/GPU-/Zielgeräteaussage.

Separater Memray-1.20.0-Prozess auf `621171c97`: vollständige Imports, Quellen-/
Terrainladung, Context, Körper/Abfragen und damalige Exporte, Python-Allokatoren
eingeschlossen: **57598998 Allokationen, 6974934167 allokierte Bytes,
136860259 B getrackter Peak**. Das ist hoher Gesamtaufwand, keine Optimierungsabnahme.
Der später ergänzte Reservationen-JSON-Export ist außerhalb dieses historischen
Traces; Körperbytes sind exakt gleich. Interne kopierte GEOS-Bytes und vollständige
Ursachenverteilung bleiben ungemessen. Instrumentierte Zeiten sind keine normale Latenz.

Zwei ganze **1280×720**-EEVEE-Bilder mit identischem, beide Körperstände vollständig
enthaltendem Ausschnitt sind geöffnet. llvmpipe: 10,003/10,482 s.
`test/scripts/pixels.py`: **18/921600** Pixel geändert, davon zwölf um mehr als
1/255. Diese Bilder verwenden die bytegleichen Mesharchive des Vorgängerlaufs
`8c93885af`/Leser `621171c97`; spätere Änderungen betreffen Zähler und Reservationsexport.
Der Gesamtblick bleibt blass und lückenhaft, Fassaden-/Raumdetails sind hier weitgehend
unterhalb des Bedarfs. Kein deutlicher Bildgewinn und keine Veröffentlichung behauptet.
Die vorhandenen Hallen-v2-Rezepte wurden unverändert an drei kontextuell industriellen
Kandidaten exploriert; zwei waren für das Dachrezept ungeeignet, einer überschritt
die Fensterkapazität und lag zudem außerhalb des aktuellen Terrainprodukts.
Die gelieferte Architektur verwendet den allgemeinen vorhandenen Erzeuger;
ein erfolgreiches funktionales Hallenprogramm wird damit nicht behauptet.

## Reproduktion und Herkunft

Private Originale, aktuelle Produkte, Rohtrace und geöffnete Bilder ausschließlich
unter `/tmp/outshine-transport-consumers-296d1b348/`. Ursprüngliche PBF-/DEM-/
Transportproduktkette: [Übergabevergleich](stair_occupancy_comparison.md).
Installiertes Vergleichspaket: frisches #580-Wheel, SHA256
`6d32ec1a9a2db3051b4ecd17d189b7838949a4481c33355264fafd2df9168fa0`.
Zusätzlich zur Transportumgebung werden die vorhandenen Buildings-Abhängigkeiten
benutzt; kein neuer Runtime-Anbieter und kein Fremdcode gebündelt.

| Artefakt | SHA256 |
|---|---|
| `source/comparison.json` | `472bc037bdb24bf1fa6c5a1936f77d4f5596e8b5df6679665a4403a17a1b1b76` |
| `source/buildings-before.npz` | `945a04199f43795410e4ea667101d6467f5c43c0c4a6c62d42868b9a44de09bd` |
| `source/buildings-after.npz` | `2ab326dfa2fa6deb0c018152c40073b3d9689b6e90bcfe3a47e716d64188b390` |
| `source/building-reservations.json` | `1f8d9d5000fbe616437f6fdecd561a5c710755e00ddb9a3ec447f81cace2c863` |
| `display/after.png` | `1f04dfa9f058508a450949be3aed90947e1f8f543fe08238230b717fec64584d` |
| `cost.json` | `bb2b401d6fe77bc60bfdb4d3ff957ed068120ab5b01b2eadc5250423b12b7a72` |

Explizit frische Temp-Ausgaben verwenden; `CONSUMER_PRODUCTS` benennt den bereits
verifizierten Transportstand aus #575, `CONSUMER_OUTPUT` liegt unter `/tmp`:

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport:test/experiments/buildings:test/experiments/assets
"$CONSUMER_PY" -W error -m unittest stair_consumer_checks stair_occupancy_checks stair_occupancy_precision_checks test_reservation_query test_reference
"$CONSUMER_PY" -m stair_consumer_experiment "$CONSUMER_PRODUCTS" --output "$CONSUMER_OUTPUT"
"$CONSUMER_PY" -m stair_consumer_verify "$CONSUMER_OUTPUT" --output "$CONSUMER_RECEIPT"
"$CONSUMER_PY" -m stair_consumer_display "$CONSUMER_OUTPUT" --verification "$CONSUMER_RECEIPT" --scene "$WHOLE_720P_SCENE" --output "$CONSUMER_DISPLAY"
```

CPU 180 s / AS 4 GiB im Produzenten, zusätzlich Wall 240 s im isolierten Aufruf.
Weitere Eingaben/Quellen, echter Vegetationsverbrauch, Fundament-/Hanganschlüsse,
sichtbare Netzvervollständigung und öffentliche affine Bibliotheksübernahme bleiben offen.
