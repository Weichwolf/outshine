# Transport-Experimente

Python-Verfahren für zusammenhängende OSM-Straßen, Wege und Schienen. Alle Aufrufe
laufen vom Repository-Root. Ergebnis, Abhängigkeiten und offene Lücken stehen in
[#18](https://github.com/Weichwolf/outshine/issues/18),
[#19](https://github.com/Weichwolf/outshine/issues/19),
[#20](https://github.com/Weichwolf/outshine/issues/20) und
[#21](https://github.com/Weichwolf/outshine/issues/21).

- `transport_*.py`: Vektorplan, Bauteile, Fahrkurven, lokale Prüfungen und Renderings.
- `road_*.py`, `earthwork_*.py`, `clearance_aprons.py`: frühere Profil-/Kontaktvergleiche.
- `probes/<Thema>/`: fallbezogene Verfahrensvergleiche und Fehlerdiagnosen.
- `transport_network_recipes.json`: gemeinsame Rezept- und Fahrzeugparameter.
- Weitere Dateien erhalten frühere Asset-/LOD-/Speichervergleiche.
- [Gemeinsamer Rezeptpilot](recipe_transport.md): tatsächlicher Quellverbund,
  Profilplan und physisch geprüfter Körper auf dem kleinen Kern aus #483/#494.
- [Ganze äußere Radkorridore](stair_corridor_comparison.md): 23 Wiener Quellen,
  ganze Anschlüsse, zehn Unterführungen und DEM-kontrolliertes Terrain im Budget.
- [Konstruktives Eckpodest](stair_corner_comparison.md): 24 ganze Quellen,
  zwölf ganze Endprofile und unveränderte Unterführungs-/Terrain-/Pfeilergrenzen.
- [Ganze gewählte Podeste](stair_landing_comparison.md): alle 41 inneren
  Endprofile, neun kleine Ergänzungen und derselbe installierbare Profilkern.
- [Nullflächen nach Konformierung](stair_conformance_comparison.md): fester
  Wiener Gegenfall des begrenzten Bodenmoduls und unveränderte Flächenprüfung.
- [Begrenztes Bodenmodul](stair_module_comparison.md): vier neue ganze Quellen,
  tatsächliche alte/neue Wandkontakte und lokales Terrain am gespeicherten 32er Körper.
- [Weitere Modulstufe](stair_module_chain_comparison.md): geprüften Bestand weiterverwenden,
  einzelne feste Seitenkontakthöhen und einen ganzen ursprünglichen Radweg ergänzen.
- [Südwestlicher Straßenverbund](stair_southwest_comparison.md): sechs weitere Quellen,
  ganze geneigte Bestandsprofile, ausdrücklicher Innenkontakt und 795 m Straßen/Wege.
- [Lokaler Generatoradapter](stair_local_comparison.md): residente Terrainpatches,
  getrennte Straßenkörper und begrenzte vollständige Nachbarschaften.
- [Automatisches residentes Wachstum](stair_growth_comparison.md): neue Assets
  als tatsächliche Anschlusswurzeln, begrenzte Quellenwahl und gemeinsame alte Profile.
- [Unabhängige Grundmodule](stair_seed_comparison.md): acht ganze Straßenquellen
  außerhalb des alten Kerns, mit begrenztem Rastermodell und ganzen Originalprofilen.
- [Vier weitere ganze Fußwege](stair_network_extension_comparison.md): 69 Quellen,
  unabhängige Gesamtprüfung mit lokalen Ownern und kleine sichtbare Netzerweiterung (#587).
- [Gemeinsamer Bestandsknoten](stair_shared_root_comparison.md): 71 Quellen,
  vollständige alte Profile und ein gemeinsames Podest statt ausgelassener Kontakte (#595/#597).
- [Schmaler Weg am breiteren Bestand](stair_narrow_profile_comparison.md): 72 Quellen,
  vollständige alte Referenzprofile und ein weiterer ganzer Fußweg (#606).
- [Ganze metrische Profillinien](stair_metric_line_comparison.md): echter alter
  Endprofil-Gegenfall, vollständige Linien statt drei Samples; Bestand unverändert (#614).
- [Explizite Fußgängerkontakte](stair_surface_contact_comparison.md): 76 ganze Quellen,
  gemeinsame Knotenboxen, gebundene alte Owner und vollständige D/Q-Querungsportale (#602).
- [Bestandsraster und gemeinsame Pflasterfläche](stair_cached_grid_comparison.md):
  77 ganze Quellen auf unveränderten76 Körpern, volle Fahrprofile und sechs gleiche
  Quellenkohorten; Arbeits-/Allokationskosten bleiben erheblich (#627/#633).
- [Fahrfreiheit über altem Fußpodest](stair_shared_headroom_comparison.md): logische
  Höhenbänder mit beiden Herkünften, tatsächliche77er Fachverbraucher und identische
  Volumen mit92,62% weniger Anpassungen; mehr temporärer JSON-Peak (#642).
- [Ganze Pfeilerumfahrung](stair_pier_ground_comparison.md): ursprünglicher952-m-Radweg,
  vollständige alte Pfeilerkontakte und feinere lokale Terrainbindung auf der53er
  Ausgangswelt; damalige54er Kohorte,79er Weiterführung im folgenden Bericht (#660/#668).
- [Strenge Abdeckungsprüfung](stair_coverage_boundary_comparison.md): hinreichender
  Kanten-/Randnachweis in der normalen Referenzbibliothek, GEOS-Rückfall für andere
  Topologien; gleicher ganzer Zustand und79er Produkte bei62,69%weniger kumulativ
  angeforderten Bytes, ohne belegten RAM-/Gerätegewinn (#678).
- [Zusammensetzung gültiger Regionen](stair_composition_comparison.md): drei
  Alternativen an tatsächlichen Ersetzungen; gleiche79er Produkte und Fachleser,
  weitere44,46%weniger kumulativ angeforderte Bytes beim vollständigen Laden (#678).
- [Vollständiger79er Bestand](stair_resident_snapshot_comparison.md): geprüfte
  Root-Profile mit allen78alten Quellen, unveränderten Asset-IDs und höheren
  Freiräumen zusammenführen; ganze Radquelle samt Raum-/Fach-/Bild-/Kostenprüfung.
- [Präzision neuer Kreuzungen](stair_junction_precision_comparison.md): echter
  Float32-Gegenfall, neun Kandidaten und getrennte Überlappungskontakte (#600/#602); noch keine Netzzulassung.
- [Höhenabhängige Übergabe](stair_occupancy_comparison.md): tatsächliche 65er Körper
  und Stützen, quelltreue Footprint-Korrekturen und freie Räume unter Brücken (#568).
- [Präzision der Konfliktabfrage](stair_occupancy_precision.md): falsch leere Masken,
  Flächenunterlauf, unabhängiges Orakel und konstante Bibliotheksabfrage (#568).
- [Tatsächlicher Architekturverbrauch](stair_consumer_comparison.md): bestehender
  Buildings-Erzeuger, aktuelle Terrainanker und Prüfung seiner fertigen Körper (#568).
- [Tatsächlicher Pflanzenverbrauch](stair_plant_comparison.md): vorhandene Pinus-/
  Habitatprogramme, gemessene Hüllen und vollständige reale Zurückstellungen (#568).
- [Konsistente Terrainkontakte](surface_contact_comparison.md): tatsächliche dünne
  Dreiecke, endliche gemeinsame Kanten und ausgabegleicher Normalpfad (#589).
- [Sekundäre Original-Endkontakte](stair_endpoint_comparison.md): alte Wegenden
  an neuen Durchgangswegen, mit ganzen Profilen und unverändertem Bestand.
- [Exakte lokale Terrainflächen](stair_patch_comparison.md): fünf geprüfte Änderungen
  mit stabilen Teilflächen-IDs und begrenzten Raumabfragen erhalten (#542).
- [Verbundene Quellenpräfixe](stair_prefix_comparison.md): gültige ganze Quellen
  nach gemeinsamer Geometrieablehnung begrenzt erhalten (#539).
- [Automatische begrenzte Quellenmodule](stair_frontier_comparison.md): ursprüngliche
  Knoten, tatsächlich gespeicherte Innenprofile und ganze physische Abnahme (#537).
- [Horizontale Straßenkurven](stair_road_curve_comparison.md): installierter Klothoidenkern,
  erhaltene Originalknoten, ganze tatsächliche Fahrspurhüllen und Kreisbogenvergleich.

Gemeinsame feste Eingaben liegen unter `fixtures/`; private Arbeitsdaten, Berichte
und Hilfsumgebungen liegen ausschließlich unter `/tmp` außerhalb des Repositorys.
Forschungs-/Diagnosebilder bleiben im System-Tempverzeichnis; die integrierte
öffentliche Places-Galerie besitzt der Hauptagent.
Für den aktuellen Linux-Stand gelten die folgenden expliziten Pfade:

```sh
TRANSPORT_PY=/tmp/outshine-transport-python-355154544/bin/python
TRANSPORT_WORK=/tmp/outshine-transport-workspace-0241b3f81
```

Neue eigene Umgebungen ebenfalls extern mit `python3 -m venv <Temp-Pfad>` erzeugen.
Die Probes verwenden ihre jeweiligen Eingabestände; gemessene Hashes bleiben unverändert.
Für Probe-Aufrufe zeigt `PYTHONPATH=test/experiments/transport` auf die Solvermodule.

```sh
python3 test/experiments/transport/transport_roadbed_window_experiment.py --help
python3 test/experiments/transport/transport_coupled_roadbed_experiment.py --help
python3 test/experiments/transport/transport_roadbed_window_render.py --help
```

Transport besitzt Verkehrsnetze; Buildings und Vegetation besitzen ihre eigenen Rezepte.
Gemeinsame räumliche Verträge stehen in [#18](https://github.com/Weichwolf/outshine/issues/18). Gebäudekonflikte hier prüfen nur diese Schnittstelle.
Historische Berichte behalten ursprüngliche Pfade, Hashes und Messcommits nach der Verschiebung.
Vor erneutem Anwenden hashgeprüfter Produkte die Nachweise unter den neuen Modulnamen regenerieren.

## Vier repräsentative Transportaufnahmen

Lokale Python-Transportbilder im System-Tempverzeichnis zeigen Zürich Gleisvorfeld, Basel Badischer Bahnhof,
Wien Brücken-/Tunnelverbund und New York Midtown. Der Nutzer hat die frühere Auswahl
und ihre niedrigen Kameras durch diese transportreichen Ansichten ersetzt (#283).
Die getrennte Deklaration [transport-gallery.json](reference/transport-gallery.json)
fixiert echte Quellorte, vollständige Kacheln und erhöhte Schrägansichten;
die gemeinsamen nativen Places-Szenarien werden dadurch nicht geändert.
Gleiche Kameras bei späteren Vergleichen; nach geöffnetem Vergleich bei deutlich
sichtbarer Verbesserung aktualisieren. Keine Bilder committen; nur master veröffentlicht
integrierte Places. Kein HTTP-Server.

`capture_transport_gallery.py` speichert vollständige Originalquellen, IDs und
Attribute aus dem versionierten OpenFreeMap-Snapshot sowie DEM-Receipts.
`transport_gallery.py` ruft dieselben vorhandenen Komponenten-, Schienen- und
gewichteten Höhenkerne auf. Wien verwendet die bereits geprüften echten
Brücken-/C1-Zufahrts-/Portalrezepte aus #21 mit ihren drei vollständigen
Quellkacheln; aktuelle Neuberechnung, alte Kontakte und ganze Freiräume werden
nochmals geprüft. Keine Nahansichten, Einblendungen oder zusätzlichen Bilddateien in der Galerie.
`ground_layout` ergänzt bei den anderen Orten den konformen Boden statt
sichtbarer Himmelslöcher unter dem Netz. Ganze Gradientenkegel und vollständige
Gleiskontakte werden aus den gespeicherten Meshes unabhängig erneut geprüft.
Jede Quelle behält ihren Nutzungsstatus und einen lokalen Auslassungsgrund.
Die vier Bilder zeigen diese Python-Produkte in 1280×720; native Bilder werden
nicht kopiert. [Abnahme und Grenzen](transport_gallery_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/capture_transport_gallery.py --cache "$TRANSPORT_PLACE_SOURCES" --dem-sources "$TRANSPORT_DEM_SOURCES" --output "$TRANSPORT_GALLERY_INPUTS"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/transport_gallery.py --data-root "$TRANSPORT_GALLERY_INPUTS" --dem-sources "$TRANSPORT_DEM_SOURCES" --output "$TRANSPORT_GALLERY_OUTPUT" --logs "$TRANSPORT_GALLERY_LOGS" --research-root "$TRANSPORT_RESEARCH_REFERENCE"
```

Alle Variablen benennen ausdrücklich gewählte externe Pfade; der Cache ist das
Client-Quellverzeichnis mit `openfreemap/` und `mapterhorn/` darunter. Die
Erzeugung benötigt einen sauberen eingefrorenen Commit und frische Ausgaben.
`TRANSPORT_RESEARCH_REFERENCE` enthält den ursprünglichen hashgeprüften
`bridge-continuations/`-Plan aus PR133/144; keine native Geometrie wird importiert.
Nur abschließend tatsächlich geöffnete PNGs werden in die Galerie übernommen.
Alte geprüfte Netz-/Höhenprodukte bleiben als Regressionen erhalten. Die neue
Ortsauswahl belegt weder vollständige Ebenen-/Fahrnetzplanung noch LOD/native
Kosten oder Gerätebudgets.

## Wiener gemeinsamer Boden unter Brücken und Portalen (#299)

`wien_ground_experiment.py` vergleicht die unveränderlichen alten unteren
Straßenhöhen mit einer gemeinsamen DEM-/Straßen-/Bodenplanung. Die neue Planung
darf untere Straßen senken; ihre XY-Flächen und die oberen C1-/Tunnelprodukte
bleiben erhalten. Ganze affine Überlappungen begrenzen Boden unter Brücken und
über Tunneldächern; offene Portale erhalten echte Aussparungen. Alte freigegebene
Kontaktprodukte und öffentliche Bibliotheksverträge werden dadurch nicht ersetzt.
Gleiche Eingaben, isolierte Worker, feste Grenzen und tatsächlich geöffnete
720p-Vorher/Nachheransichten; eine vollständig ausgefüllte Erdmasse ist damit
nicht nachgewiesen. Forschungsbilder bleiben lokal; nur master veröffentlicht
integrierte Places. [Vergleich und Grenzen](wien_ground_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/wien_ground_experiment.py --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --dem-sources "$TRANSPORT_DEM_SOURCES" --output "$TRANSPORT_WIEN_GROUND_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" -m unittest wien_ground_checks ground_surface_checks surface_height_checks
```

Das Ausgabeverzeichnis muss frisch sein; Quellen werden ausschließlich aus den
ausdrücklich gewählten hashgeprüften alten Bridge-Produkten und dem DEM-Cache
gelesen. `--no-images` führt denselben Geometrie-/Kostenvergleich ohne Blender aus.

## Wiener Bodenverkehr um den Brücken-/Portalverbund (#303)

`wien_network_experiment.py` verwendet dieselben vollständigen drei Wiener
Quellkacheln wie #283/#299. Die vorhandenen Komponentenrezepte ergänzen neue
Bodenquellen; alte obere C1-/Portalflächen bleiben erhalten. Geschosswege ohne
gewählte Fläche werden ausdrücklich zurückgestellt. Neue untere Höhenkontakte
sind ein eigener Plan, keine stillschweigende Ersetzung alter Verbraucherprodukte.

Die dichten Randvertices bestimmen nicht mehr die Anzahl der Höhenvariablen.
`transport_field_plan.py` plant ein gemeinsames affines Feld auf dem bisherigen
groben Geländeplan. `affine_field_surface.py` erzeugt daraus tatsächliche
Verkehrs-/Bodenfacetten über eine gemeinsame genodete Kantenanordnung; alle
endgültigen Flächen und Freiräume werden unabhängig geprüft.
[Vergleich, Grenzen und Belege](wien_network_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/wien_network_experiment.py --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --capture "$TRANSPORT_GALLERY_INPUTS/inputs/ViennaResearch-input.json" --basis-reference "$TRANSPORT_WIEN_GROUND_OUTPUT" --dem-sources "$TRANSPORT_DEM_SOURCES" --output "$TRANSPORT_NETWORK_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_NETWORK_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_NETWORK_OUTPUT/after-scene.json"
```

Alle Verzeichnisse sind ausdrücklich gewählte externe System-Temp-/Quellpfade.
`TRANSPORT_WIEN_GROUND_OUTPUT` ist der hashgeprüfte vollständige #299-Vergleich;
`TRANSPORT_BLENDER_PY` besitzt `bpy==5.2.2`. Frische Ausgaben und ein sauberer
eingefrorener Quellstand sind Pflicht. Die beiden regionalen Bilder sind
1280×720; `after-1080-scene.json` prüft dieselbe Kamera zusätzlich lokal in
1920×1080. Kamera-, Mesh-/Normalenimport und Tageslicht verwenden den gemeinsamen
#291-Pfad unter `outshine_ref.rendering`. Keine Bilder nach `doc/` veröffentlichen.

`transport_field_cost.py prepare` erzeugt optional einen dichten Nachfit-Input
auf exakt derselben geprüften Geometrie. `measure --allocations` vergleicht
gespeicherte Inputs in getrennten Prozessen mit dem nur dafür benötigten
`memray==1.20.0`. Erfasste Allokationen und instrumentierte Zeiten bleiben von
normalen Solverzeiten getrennt; der gültige kleine Höhenplan bleibt Rückfallprodukt.

## Ganze Brückenpfeilerkontakte auf dem Wiener Netz (#326)

`wien_support_experiment.py` ergänzt prozedurale Pfeiler zwischen tatsächlicher
Deckunterseite und dem unveränderten #303-Boden. Unterer Verkehr und Portale
bleiben frei; kleine Stationsverschiebungen vermeiden zusätzliche Konflikte.
Beide Kontaktflächen werden gemeinsam zugeschnitten und vollständig geprüft.
Nicht gebaute Stationen behalten Quelle und Grund. Das ist kein Statiknachweis.
[Vergleich, Quellen und Grenzen](wien_support_comparison.md).

Der Konstruktionskern ist mit #332 installierbar unter
[`outshine_ref.transport.supports`](../../../lib/supports.md). Experiment und
frisches Wheel verwenden denselben Kern; unabhängige Referenz-/Volumenprüfung
bleibt separat. [Migration, Identität und Kosten](support_library_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/wien_support_experiment.py --network "$TRANSPORT_NETWORK_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_SUPPORT_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/wien_support_verify.py --products "$TRANSPORT_SUPPORT_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_SUPPORT_OUTPUT/verification.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SUPPORT_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SUPPORT_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SUPPORT_OUTPUT/after-1080-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" -W error -m unittest bridge_support_checks
```

Ausgaben sind frische explizite System-Temp-Pfade, Erzeugerstände sauber und
eingefroren. Die regionale Kamera ist gegenüber #303 mäßig von 1400 m auf 1100 m
gezoomt; das Vorher/Nachherpaar selbst benutzt dieselbe Kamera und Präsentation.
1280×720 für den Eindruck, 1920×1080 nur zur lokalen Detailprüfung. Keine Bilder
nach `doc/` veröffentlichen. `support_station_cost.py` misst optional mit
`memray==1.20.0` je einen isolierten Plan (`--method fixed` bzw. `shifted`) vor
Export und unabhängigem Replay; CLI verlangt dieselben drei Eingabepfade und
ein frisches `--output`. Instrumentierte Zeiten sind separat ausgewiesen.

## Geneigte Schienen auf tatsächlichen Wiener Bodenbetten (#340)

`tilted_rail_experiment.py` verwendet den bestehenden horizontalen
`outshine_ref.transport.rail_hardware`-Kern in einer orthonormalen Bett-Ebene.
Starre ENU-Transformation erhält Spurweite, Profil und Schwellenmaße. Ganze
Kontaktflächen sind gegen den tatsächlichen #303-Verkehrsboden geprüft; die
unveränderten #332-Pfeiler-/Brücken-/Portalprodukte bleiben die Basis.
[Verfahren, Eingaben, Bild und Kosten](tilted_rail_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/tilted_rail_experiment.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_SUPPORT_OUTPUT" --output "$TRANSPORT_TILTED_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/tilted_rail_replay.py --products "$TRANSPORT_TILTED_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_SUPPORT_OUTPUT" --output "$TRANSPORT_TILTED_OUTPUT/verification.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_TILTED_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_TILTED_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_TILTED_OUTPUT/after-1080-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/tilted_rail_cost.py --products "$TRANSPORT_TILTED_OUTPUT" --method batch --output "$TRANSPORT_TILTED_COST_BATCH"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/tilted_rail_cost.py --products "$TRANSPORT_TILTED_OUTPUT" --method scalar --output "$TRANSPORT_TILTED_COST_SCALAR"
```

Ausgabe-/Kostenpfade sind ausdrücklich gewählte frische System-Temp-Pfade;
Erzeugerstand sauber und eingefroren. Optionale `--profile`-Datei benötigt nur
für die Allokationsprobe `memray==1.20.0`. 1280×720 für den regionalen Vergleich,
1920×1080 lokal zur Detailprüfung. Alle Bilder bleiben lokal. Neue obere
Schienenprofile ergänzt #352; Weichen und Übergänge über C0-Knicke bleiben getrennte Arbeit.

## Schienen auf gewählten Brückendecks (#352)

`upper_rail_experiment.py` ergänzt den #340-Stand auf den ausdrücklich gewählten
oberen U-Bahn-Achsen des bestehenden Wiener Brückenplans. `rail_bed_patches.py`
verbindet ausschließlich tatsächlich koplanare benachbarte Deckfacetten vor dem
Randabstand. Gegenprobe `triangle` behält die inneren Dreieckskanten. Beide
verwenden denselben metrischen Profilkern und dieselben ganzen Kontaktbedingungen.
Originale Brücken, Pfeiler, Portale und untere Gleise bleiben erhalten.
[Verfahren, Vergleich, Pins und Grenzen](upper_rail_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/upper_rail_experiment.py --reference "$TRANSPORT_RESEARCH_REFERENCE" --baseline "$TRANSPORT_TILTED_OUTPUT" --method patch --output "$TRANSPORT_UPPER_RAIL_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/upper_rail_replay.py --products "$TRANSPORT_UPPER_RAIL_OUTPUT" --reference "$TRANSPORT_RESEARCH_REFERENCE" --baseline "$TRANSPORT_TILTED_OUTPUT" --output "$TRANSPORT_UPPER_RAIL_OUTPUT/verification.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_UPPER_RAIL_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_UPPER_RAIL_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_UPPER_RAIL_OUTPUT/after-1080-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/upper_rail_cost.py --reference "$TRANSPORT_RESEARCH_REFERENCE" --products "$TRANSPORT_UPPER_RAIL_OUTPUT" --method patch --output "$TRANSPORT_UPPER_RAIL_COST"
```

Für den konservativen Vergleich `--method triangle` und getrennte frische Temp-Pfade
verwenden, anschließend denselben Replay ausführen. Die Kostenprobe akzeptiert
optional `--profile` mit `memray==1.20.0`; Instrumentierung ist kein normales Timing.
Nur explizite frische System-Temp-Ausgaben; eingefrorener sauberer Erzeugerstand.
Regionale Bilder 1280×720, 1920×1080 nur lokale Prüfung, keine Galerie/Publikation.
Keine Weichen, stetigen Knickübergänge, durchgängigen Zugrouten oder native Abnahme.

## Installierbare geneigte Gleiskörper (#376)

Die Erzeuger aus #340/#352 importieren jetzt denselben öffentlichen Kern
`transport.rail_hardware.build_planar_rail_hardware`. Das flache API bleibt erhalten.
133 bereits zugelassene echte Boden-/Deckstücke stehen mit Original-IDs und
ausdrücklich gewählten Dreiecken unter `lib/tests/fixtures/planar-rail-vienna.json`.
`capture_planar_rail_cases.py` ist ein expliziter einmaliger Referenzeingang;
normale Tests ändern diese Daten nicht. [API](../../../lib/rail_hardware.md#explicit-affine-bed-planes-376),
[Source-/Wheel-/Geometrie-/Bild-/Kostenbelege](planar_rail_library_comparison.md).

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest test_planar_rail_hardware test_rail_hardware tilted_rail_checks upper_rail_checks tilted_rail_input_checks rail_hardware_checks rail_graph_checks rail_admission_checks
PYTHONPATH="$TRANSPORT_PACKAGE_ROOT:lib/tests:test/experiments/transport" OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/planar_library_build.py --inputs lib/tests/fixtures/planar-rail-vienna.json --lower "$TRANSPORT_TILTED_OUTPUT" --upper "$TRANSPORT_UPPER_RAIL_OUTPUT" --latest "$TRANSPORT_SIDEWALK_OUTPUT" --package-root "$TRANSPORT_PACKAGE_ROOT" --output "$TRANSPORT_PLANAR_OUTPUT"
PYTHONPATH="$TRANSPORT_PACKAGE_ROOT:lib/tests:test/experiments/transport" OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/planar_library_replay.py --inputs lib/tests/fixtures/planar-rail-vienna.json --lower "$TRANSPORT_TILTED_OUTPUT" --upper "$TRANSPORT_UPPER_RAIL_OUTPUT" --latest "$TRANSPORT_SIDEWALK_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --reference "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_PLANAR_OUTPUT"
PYTHONPATH="$TRANSPORT_PACKAGE_ROOT:lib/tests:test/experiments/transport" "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_PLANAR_OUTPUT/after-scene.json"
```

`TRANSPORT_PACKAGE_ROOT` explizit `lib/src` oder ein frisches externes Wheel-Ziel;
für Wheel-Prozesse kein `lib/src` in PYTHONPATH. Ausgabe frisch im System-Temp,
sauber eingefrorener Commit. Gesamtbild übernimmt dieselbe regionale Kamera aus
#372.720p für den Eindruck,1080p nur lokale Prüfung; Bilder bleiben lokal.
Die Migration erhält Geometrie/Bild und beansprucht keine neue sichtbare Qualität.
Bettauswahl, Weichen, Knickübergänge, neue Quellfreigaben und native Abnahme bleiben offen.

## Quellgebundene Belagsemantik (#357)

`transport_surface_experiment.py` ergänzt ganze Familienanteile auf den unveränderten
unteren Wiener Verkehrsfaces aus #303 im vollständigen #352-Körper. Verwendete
noded edges/Breiten/Quell-IDs und ausdrücklich alte untere Quellen steuern die
Zuordnung; obere/deep Quellen malen keinen Belag auf den Boden darunter.
Flächengewichtete lineare Albedo aus dem vorhandenen Materialbaukasten ist eine
Facettenapproximation; sie ersetzt keine exakten Materialgrenzen oder Pixelfilter.
Positionen, Topologie, alte Materialslots und Normalen bleiben exakt erhalten.
[Verfahren, Pins, Bilanz, Bild und Kosten](transport_surface_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/transport_surface_experiment.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_UPPER_RAIL_OUTPUT" --output "$TRANSPORT_SURFACE_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/transport_surface_verify.py --products "$TRANSPORT_SURFACE_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_UPPER_RAIL_OUTPUT" --output "$TRANSPORT_SURFACE_OUTPUT/verification.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SURFACE_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SURFACE_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SURFACE_OUTPUT/after-1080-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/transport_surface_cost.py --products "$TRANSPORT_SURFACE_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_UPPER_RAIL_OUTPUT" --method cells --output "$TRANSPORT_SURFACE_COST_CELLS"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/transport_surface_cost.py --products "$TRANSPORT_SURFACE_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_UPPER_RAIL_OUTPUT" --method whole --output "$TRANSPORT_SURFACE_COST_WHOLE"
```

Alle Ausgaben sind explizite frische System-Temp-Pfade, Erzeuger sauber eingefroren.
Optionale `--profile`-Datei benötigt nur für die Allokationsprobe `memray==1.20.0`.
Renderer liest optional `face_base_colour_linear_rgba` pro Face als unverändertes
lineares `FLOAT_COLOR/CORNER`-Attribut bis zum tatsächlichen Base-Color-Eingang;
ohne dieses Feld bleibt der bisherige Palettenpfad erhalten. 1280×720 für den
Eindruck, 1920×1080 nur lokale Prüfung; keine Galerie/Publikation. Materialrezepte,
Beleuchtung und native APIs behalten ihre Besitzer; keine neuen Landcoverdaten.

## Räumliche Straßenrandprofile (#364)

`curb_experiment.py` baut optionale prozedurale Bordkörper außerhalb der gesamten
bestehenden Verkehrsfläche. Nominell höchstens18cm XY-Breite und12cm vertikale
Höhe; vorhandene Fahr-/Geh-/Gleisbreiten bleiben erhalten. Gewählte tatsächlich
verwendete befestigte urbane Quellen binden die Auswahl. Querungen, Endports und
obere Bauwerke/Pfeiler/Portale werden ausgespart. Der installierbare Ganzflächenkern
konstruiert auf dem ursprünglichen Höhenfeld; tatsächlicher feiner Boden bleibt
unabhängige Kontaktreferenz. [Vergleich, Pins, Bild und Kosten](curb_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/curb_experiment.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_SURFACE_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_CURB_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/curb_verify.py --products "$TRANSPORT_CURB_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_SURFACE_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_CURB_OUTPUT/verification.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CURB_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CURB_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CURB_OUTPUT/after-1080-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/curb_cost.py --products "$TRANSPORT_CURB_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --method fine --output "$TRANSPORT_CURB_COST_FINE"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/curb_cost.py --products "$TRANSPORT_CURB_OUTPUT" --network "$TRANSPORT_NETWORK_OUTPUT" --method field --output "$TRANSPORT_CURB_COST_FIELD"
```

Frische explizite System-Temp-Ausgaben und sauber eingefrorene Quellen. Optionales
`--profile` benötigt `memray==1.20.0` ausschließlich für Allokationsmessung. Beide
Kostenmethoden nutzen dieselben tatsächlich von beiden Methoden zugelassenen
Footprints; einseitige/budgetbedingt fehlende Fälle bleiben im ganzen Vergleich.
Regionale1280×720-Ansicht mit1100m horizontaler Spannweite,1080 nur lokal; keine
Galerie oder Bilderpublikation. Vollständige Gehweghöhen/Absenkungen, zusätzliche
Verbraucherreservierungen, native oder Geräteabnahme werden nicht daraus abgeleitet.

## Breite Gehwegkörper hinter den Borden (#372)

`sidewalk_experiment.py` ergänzt begrenzte tatsächliche Gehwegkörper hinter den
geprüften Bord-Footprints aus #364. Das gesamte Straßenrandprofil bleibt innerhalb
von2m XY außerhalb der alten Verkehrsfläche; Höhe vertikal12cm über dem gewählten
Boden. Bestehende Verkehrs-, Querungs-, Endport- und Bauwerksaussparungen gelten
weiter. Ganze Kontaktkappen und sämtliche affinen Teilintervalle der tatsächlichen
Bordanschlusslinien werden unabhängig geprüft. Ursprüngliche Körper und Quellnutzung
bleiben erhalten. [Ergebnisse, Alternativen und Grenzen](sidewalk_comparison.md).

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/sidewalk_experiment.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_SURFACE_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --curbs "$TRANSPORT_CURB_OUTPUT" --output "$TRANSPORT_SIDEWALK_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/sidewalk_verify.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_SURFACE_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --curbs "$TRANSPORT_CURB_OUTPUT" --output "$TRANSPORT_SIDEWALK_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SIDEWALK_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SIDEWALK_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/sidewalk_cost.py --curbs "$TRANSPORT_CURB_OUTPUT" --method combined --output "$TRANSPORT_SIDEWALK_COST_COMBINED"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/sidewalk_cost.py --curbs "$TRANSPORT_CURB_OUTPUT" --method individual --output "$TRANSPORT_SIDEWALK_COST_INDIVIDUAL"
```

Ausgaben explizit im System-Temp und frisch; Erzeuger benötigt sauberen eingefrorenen
Commit. Kostenworker optional `--profile <frische Memray-Datei>` mit `memray==1.20.0`;
normale und instrumentierte Zeiten bleiben getrennt. `after-1080-scene.json` nur
lokale Prüfung,720p für den Eindruck. Keine durchgängige Fußroute, abgesenkten
Querungen, Gebäudezugänge/-kollisionsfreiheit, neue Verbraucherreservation oder
native/LOD-/Geräteabnahme daraus ableiten. Keine Bilderpublikation durch Fachagenten.

## Lokale bündige Fußrampen (#385)

`foot_ramp_experiment.py` ergänzt auf den tatsächlichen #364/#372-Produkten
begrenzte Rampen an verwendeten Fußquellen. Ganze geneigte Kappen, Nullhöhenenden,
seitliche Anschlüsse und atomare Randumbauten werden geprüft. Unlösbare Teile
erhalten den alten Stand samt Grund. Ein lokales geometrisches Produkt belegt
keine ganze Querung oder Fußroute. [Verfahren, Kosten, Bilder und Grenzen](foot_ramp_comparison.md).

```sh
PYTHONPATH="$TRANSPORT_SOLIDS_PACKAGE:lib/src:lib/tests:test/experiments/transport" OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest foot_ramp_checks
PYTHONPATH="$TRANSPORT_SOLIDS_PACKAGE:lib/src:test/experiments/transport" OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/foot_ramp_experiment.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_SURFACE_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --curbs "$TRANSPORT_CURB_OUTPUT" --sidewalks "$TRANSPORT_SIDEWALK_OUTPUT" --method contact --output "$TRANSPORT_RAMP_OUTPUT"
PYTHONPATH="$TRANSPORT_SOLIDS_PACKAGE:lib/src:test/experiments/transport" OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/foot_ramp_verify.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_SURFACE_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --curbs "$TRANSPORT_CURB_OUTPUT" --sidewalks "$TRANSPORT_SIDEWALK_OUTPUT" --output "$TRANSPORT_RAMP_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_RAMP_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_RAMP_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/foot_ramp_cost.py --products "$TRANSPORT_RAMP_OUTPUT" --method vector --output "$TRANSPORT_RAMP_COST_VECTOR"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/foot_ramp_cost.py --products "$TRANSPORT_RAMP_OUTPUT" --method scalar --output "$TRANSPORT_RAMP_COST_SCALAR"
```

Explizite frische System-Temp-Ausgaben, sauber eingefrorene Quellen. Der bereits
vorhandene optionale `solids`-Baustein benötigt `manifold3d==3.5.4` für den
Körperschnitt-Gegenvergleich und dessen Kontrollen. Hier extern nach
`/tmp/outshine-transport-ramp-385-solids` installiert; keine gemeinsame Umgebung
verändert. Der gewählte Kontaktweg selbst braucht diesen Backendaufruf nicht.
Optionales `--profile` im Kostenworker braucht `memray==1.20.0` nur für Messung.
Regionale1280×720-Bilder bleiben lokal; `after-1080-scene.json` nur lokale Prüfung.
Keine neue gemeinsame/native/Reservations-API oder Material-/Lichtparameter.

## Weitere Radbrücke mit wirklichen Bodenanschlüssen (#404)

Eine207-m-Radbrücke und ihre vorhandenen Bodenwege erhalten gemeinsame Profile,
ganze Unterführungsflächen und20Pfeiler. Das feste lokale Quellen-/Bodenfixture
reicht für die portablen Geometriekontrollen. Der vollständige Szenenvergleich
liest die vorher erzeugten Netz-/Rampen-/Pfeilerprodukte explizit ein.
[Verfahren, Double-/Float32-Abnahme, Kosten und Bildgrenzen](cycle_bridge_comparison.md).

```sh
PYTHONPATH="$TRANSPORT_SOLIDS_PACKAGE:lib/src:test/experiments/transport" "$TRANSPORT_PY" -W error -m unittest cycle_bridge_checks foot_ramp_checks
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/cycle_bridge_experiment.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_RAMP_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_CYCLE_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/cycle_bridge_verify.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_RAMP_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_CYCLE_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CYCLE_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CYCLE_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/cycle_bridge_cost.py --products "$TRANSPORT_CYCLE_OUTPUT" --method chain --output "$TRANSPORT_CYCLE_COST_CHAIN"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/cycle_bridge_cost.py --products "$TRANSPORT_CYCLE_OUTPUT" --method lp --output "$TRANSPORT_CYCLE_COST_LP"
```

Nur frische Ausgaben im System-Temp; optionales `--profile` braucht das vorhandene
`memray==1.20.0`. Keine neuen Runtime-/SDK-Abhängigkeiten. Die Szenen bleiben lokal
und werden in1280×720 geprüft; keine Galerie oder neue Verbraucherfreigabe.
Die verbindlichen12%/2.50m werden auch am tatsächlichen Float32-Produkt geprüft;
konservative Baureserven ersetzen keine gelockerten Bedingungen.

## Verbundene Autobahn- und Wegbrücken (#419)

Zwei gegenläufige Fahrbahnen, neun Bodenanschlüsse und eine untere Wegbrücke über
sechs echte affine Bodenzellen. Ganze Neigungs-/Durchfahrts-/Stützenkontakte,
begrenzter örtlicher Rückfall und tatsächliche Float32-Produkte werden geprüft.
[Verfahren, Quellen, Kosten, Bild und Grenzen](layered_bridge_comparison.md).
`TRANSPORT_CYCLE_OUTPUT` ist die vollständige Szene aus #404,
`TRANSPORT_RAMP_OUTPUT` liefert die erhaltenen Rand-/Rampenkörper aus #385.
Ausgaben und Kostenpfade müssen frisch im System-Tempverzeichnis liegen.

```sh
PYTHONPATH="$TRANSPORT_SOLIDS_PACKAGE:lib/src:test/experiments/transport" "$TRANSPORT_PY" -W error -m unittest layered_bridge_checks cycle_bridge_checks foot_ramp_checks
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/layered_bridge_experiment.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_CYCLE_OUTPUT" --borders "$TRANSPORT_RAMP_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_LAYERED_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/layered_bridge_verify.py --network "$TRANSPORT_NETWORK_OUTPUT" --baseline "$TRANSPORT_CYCLE_OUTPUT" --borders "$TRANSPORT_RAMP_OUTPUT" --supports "$TRANSPORT_SUPPORT_OUTPUT" --reference-root "$TRANSPORT_RESEARCH_REFERENCE" --output "$TRANSPORT_LAYERED_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_LAYERED_OUTPUT/before-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_LAYERED_OUTPUT/after-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/layered_bridge_cost.py --products "$TRANSPORT_LAYERED_OUTPUT" --method chain --output "$TRANSPORT_LAYERED_COST_CHAIN"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/layered_bridge_cost.py --products "$TRANSPORT_LAYERED_OUTPUT" --method lp --output "$TRANSPORT_LAYERED_COST_LP"
```

`--support-method fixed` mit anderem frischem Erzeugerausgabepfad prüft den echten
Teilnetzfall; denselben Reader darauf ausführen. Kosten optional mit `--profile`
und vorhandenem `memray==1.20.0`; keine neue Runtime-/SDK-Abhängigkeit.

## Konstruktive Treppe im tatsächlichen Vierwegefall (#434)

Der [Vergleichsbericht](stair_junction_comparison.md) liefert den örtlichen
Vierwegekörper mit echten Stufen und ganzen Float32-Bedingungen. Äußere Profile
sind vorgeschlagen; die tatsächliche Wiederanbindung bleibt in #434 offen.
Der Eingang aus #400 liegt vollständig versioniert unter `test/outshine/data/transport/`.
`TRANSPORT_LAYERED_OUTPUT` liefert die bisherige Forschungspalette aus #419.
Alle Ausgabe-/Kostenpfade liegen frisch im System-Tempverzeichnis.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_junction_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_experiment.py --palette-scene "$TRANSPORT_LAYERED_OUTPUT/after-scene.json" --output "$TRANSPORT_STAIR_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/stair_junction_verify.py --palette-scene "$TRANSPORT_LAYERED_OUTPUT/after-scene.json" --output "$TRANSPORT_STAIR_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_OUTPUT/overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_OUTPUT/diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_cost.py --products "$TRANSPORT_STAIR_OUTPUT" --solver highs-ds --output "$TRANSPORT_STAIR_COST_DS"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_cost.py --products "$TRANSPORT_STAIR_OUTPUT" --solver highs-ipm --output "$TRANSPORT_STAIR_COST_IPM"
```

Optionale separate `--profile`-Aufnahmen benötigen das vorhandene `memray==1.20.0`.
Keine neue Runtime-/SDK-Abhängigkeit, keine gemeinsame/native/Reservations-API.

## Tatsächlicher Nachbarkontext mit beiden Überführungen (#434)

Der [Kontextbericht](stair_context_comparison.md) erweitert den isolierten Versuch
auf sieben tatsächliche Achsen und vier ganze Anschlüsse. Original-PBF und
98×123-DEM-Fenster sind versioniert.74 weitere Kontextachsen und drei neue
Außenkontakte bleiben ungeprüft; kein vollständiger Wiedergewinnungsnachweis.
Alle Ausgabepfade müssen frisch im System-Tempverzeichnis liegen.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_junction_checks stair_context_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_context_experiment.py --palette-scene "$TRANSPORT_LAYERED_OUTPUT/after-scene.json" --output "$TRANSPORT_STAIR_CONTEXT_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/stair_context_verify.py --palette-scene "$TRANSPORT_LAYERED_OUTPUT/after-scene.json" --output "$TRANSPORT_STAIR_CONTEXT_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_CONTEXT_OUTPUT/before/overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_CONTEXT_OUTPUT/after/overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_CONTEXT_OUTPUT/before/diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_CONTEXT_OUTPUT/after/diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_cost.py --context test/experiments/transport/fixtures/stair-context-vienna.json --products "$TRANSPORT_STAIR_CONTEXT_OUTPUT" --solver highs-ds --repetitions 7 --output "$TRANSPORT_STAIR_CONTEXT_COST_DS"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_cost.py --context test/experiments/transport/fixtures/stair-context-vienna.json --products "$TRANSPORT_STAIR_CONTEXT_OUTPUT" --solver highs-ipm --repetitions 7 --output "$TRANSPORT_STAIR_CONTEXT_COST_IPM"
```

Die feste Aufnahme kann mit `capture_stair_context.py --vector-receipt ...
--elevation-receipt ... --output ... --dem-output ...` aus den erhaltenen
unveränderten Quellreceipts offline wiederholt werden. Ein neuer Download unter
derselben Versions-URL ist ausdrücklich kein Identitätsbeweis.
`--profile` für separate Kostenaufnahmen benötigt das vorhandene `memray==1.20.0`.
Kein neuer nativer/öffentlicher Vertrag; kein SDK zur Laufzeit.

## Drei Treppen, parallele Fuß-/Radbrücken und längere Radauffahrt (#434)

Der [Netzvergleich](stair_network_comparison.md) erweitert den wirklichen Kontext
auf15Quellen, drei echte Treppen/24Stufen,13ganze Kontakte und vier Unterführungen.
Eine prozedurale Radauffahrt erhält alle tatsächlichen Anschlussorte. Originale
und Priors bleiben erhalten;66weitere Kontextachsen und zwei tatsächliche
Außen-/Geländeanbindungen sind noch nicht freigegeben. Ausgabe-/Kostenpfade frisch
im System-Tempverzeichnis; `TRANSPORT_LAYERED_OUTPUT` liefert nur die bekannte Palette.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_junction_checks stair_context_checks stair_network_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_network_experiment.py --palette-scene "$TRANSPORT_LAYERED_OUTPUT/after-scene.json" --output "$TRANSPORT_STAIR_NETWORK_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/stair_network_verify.py --palette-scene "$TRANSPORT_LAYERED_OUTPUT/after-scene.json" --output "$TRANSPORT_STAIR_NETWORK_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_NETWORK_OUTPUT/before/overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_NETWORK_OUTPUT/after/overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_NETWORK_OUTPUT/before/diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_NETWORK_OUTPUT/after/diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_NETWORK_OUTPUT/before/west-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_STAIR_NETWORK_OUTPUT/after/west-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_cost.py --network test/experiments/transport/fixtures/stair-network-vienna.json --products "$TRANSPORT_STAIR_NETWORK_OUTPUT" --solver highs-ds --repetitions 7 --output "$TRANSPORT_STAIR_NETWORK_COST_DS"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_cost.py --network test/experiments/transport/fixtures/stair-network-vienna.json --products "$TRANSPORT_STAIR_NETWORK_OUTPUT" --solver highs-ipm --repetitions 7 --output "$TRANSPORT_STAIR_NETWORK_COST_IPM"
```

Für alle59betroffenen Kontrollen zusätzlich die bestehenden Module
`foot_ramp_checks cycle_bridge_checks layered_bridge_checks` ausführen; ältere
Rampenkontrollen benötigen das dort dokumentierte optionale `manifold3d==3.5.4`.
Separate Kostenaufnahmen mit `--profile` benötigen das vorhandene `memray==1.20.0`.
Keine neue Runtime-/SDK-Abhängigkeit oder öffentliche/native/Reservations-API.

## Ganze tatsächliche Außen-Nachbarwege (#434)

Der [Außenvergleich](stair_outer_comparison.md) verbindet die bisherigen beiden
Außenprofile vollständig mit drei ganzen Nachbarwegen.18Quellen,24Stufen und sechs
Unterführungen bleiben lokal geprüft. Die vollständige Originalkachel wird bilanziert;
Terrain-/Cut-/Fill-Produkte, übrige Quellen und fünf neue Außenprofile bleiben offen.
Eingänge und die kleine technische Palette sind versioniert; dieser Versuch benötigt
keinen früheren privaten Showcase-Ausgabebaum. Alle Ausgabe-/Kostenpfade frisch im
System-Tempverzeichnis wählen.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_outer_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_outer_experiment.py --output "$TRANSPORT_OUTER_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/stair_outer_verify.py --output "$TRANSPORT_OUTER_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_OUTER_OUTPUT/before/overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_OUTER_OUTPUT/after/overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_OUTER_OUTPUT/before/diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_OUTER_OUTPUT/after/diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_OUTER_OUTPUT/before/west-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_OUTER_OUTPUT/after/west-scene.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_cost.py --outer test/experiments/transport/fixtures/stair-outer-vienna.json --products "$TRANSPORT_OUTER_OUTPUT" --solver highs-ds --repetitions 7 --output "$TRANSPORT_OUTER_COST_DS"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_junction_cost.py --outer test/experiments/transport/fixtures/stair-outer-vienna.json --products "$TRANSPORT_OUTER_OUTPUT" --solver highs-ipm --repetitions 7 --output "$TRANSPORT_OUTER_COST_IPM"
```

Beim Außenversuch wird ein begrenzt abgebrochener Kostenversuch ausdrücklich mit
`accepted_model=false`, ohne gültigen Median oder Ausgabe gespeichert. Prozess0
allein bedeutet keine gültige Geometrie. Ältere Kostenmodi behalten ihre strikte
Ergebnisprüfung. `--profile` separat mit installiertem `memray==1.20.0` verwenden.
Der vollständige70er-Lauf ergänzt `foot_ramp_checks cycle_bridge_checks
layered_bridge_checks stair_junction_checks stair_context_checks stair_network_checks`;
die älteren Rampenkontrollen benötigen das dort dokumentierte optionale Manifold.
Eine explizite DEM-Neuaufnahme verlangt `stair_outer_inputs.py --dem-receipt ...
--output ...` mit dem unveränderten ursprünglichen Rohreceipt. Normale Tests ändern
keine Eingänge. Keine neue Runtime-/SDK- oder öffentliche/native API-Abhängigkeit.

## Gegenprobe: echte Terrainkontakte am Treppenverbund (#434)

Der [Terrain-Gegenvergleich](stair_terrain_comparison.md) mit `stair_terrain_experiment.py`
vergleicht eine begrenzte sanfte Bodenfläche, ganze
Stützwände und einen geschützten Brücken-Aushub an denselben unveränderten18Quellen
und demselben echten DEM-Fenster. Der Versuch liefert ausdrücklich noch kein
freigegebenes Terrainprodukt: geschlossene Körper und erfolgreicher Kernelabschluss
beweisen weder Float32-Kontakte noch eine überzeugende Geländeform.
Zusätzlich zu den obigen Paketen braucht dieser getrennte Diagnoseversuch
`clarabel` und `manifold3d==3.5.4`; keine SDK-/Runtime-Abhängigkeit.
`TRANSPORT_OUTER_OUTPUT` bezeichnet einen frisch erzeugten/geprüften Außenversuch
aus dem vorigen Abschnitt, `TRANSPORT_TERRAIN_OUTPUT` einen neuen expliziten Temp-Pfad.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_terrain_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_terrain_experiment.py --network-products "$TRANSPORT_OUTER_OUTPUT" --output "$TRANSPORT_TERRAIN_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_terrain_verify.py --network-products "$TRANSPORT_OUTER_OUTPUT" --output "$TRANSPORT_TERRAIN_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_TERRAIN_OUTPUT/before-overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_TERRAIN_OUTPUT/after-overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_TERRAIN_OUTPUT/before-diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_TERRAIN_OUTPUT/after-diagnostic-scene.json"
```

Die Kontrollen enthalten echte18-Quellen-Gegenfälle und synthetische wechselnde
Cut-/Fill-Vorzeichen; keine Quelle wird freigegeben. Privater Installationspräfix
bei Bedarf in `PYTHONPATH` ergänzen. Der gespeicherte Reader löst/trianguliert/gräbt
nicht neu. Native Gates und vollständige Terrain-/Verkehrsabnahme bleiben getrennt.

## Kontinuierliche Böschungen mit ganzen Exportkontakten (#434)

Der [Böschungsvergleich](stair_blend_comparison.md) plant kontinuierliche Bodenhöhen
mit ganzen Brückenbedingungen, festen ursprünglichen Bodenprofilen und örtlichen
nicht begehbaren Stützflächen. Für tatsächliche Float32-Kontakte wird ein räumlich
hergeleitetes Höhenband geprüft; ein erfolgreicher Solverstatus ist keine Freigabe.
Zusätzlich zu NumPy/SciPy/Shapely ist `clarabel==0.11.1` erforderlich. Das neue
Verfahren benötigt weder Manifold noch das CityEngine-SDK. Ausgabe frisch im
System-Tempverzeichnis; der explizite Außenversuch stammt aus den versionierten
Eingängen des vorigen Abschnitts.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_blend_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_blend_experiment.py --network-products "$TRANSPORT_OUTER_OUTPUT" --output "$TRANSPORT_BLEND_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_blend_verify.py --network-products "$TRANSPORT_OUTER_OUTPUT" --output "$TRANSPORT_BLEND_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_BLEND_OUTPUT/before-overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_BLEND_OUTPUT/after-overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_BLEND_OUTPUT/before-diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_BLEND_OUTPUT/after-diagnostic-scene.json"
```

Gleicher fester Plan und gleiche Qualitätsbedingungen, Vorbereitung/Prüfung außerhalb
der Fit-Zeit; beide Prozesse nacheinander bei normaler Priorität starten:

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_blend_cost.py --products "$TRANSPORT_BLEND_OUTPUT" --smoothing 16 --output "$TRANSPORT_BLEND_OUTPUT/membrane-cost.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_blend_cost.py --products "$TRANSPORT_BLEND_OUTPUT" --smoothing 0 --output "$TRANSPORT_BLEND_OUTPUT/data-cost.json"
```

Optionale Allokationsmessung benötigt `memray==1.20.0`: jeden Kostenaufruf separat
mit frischem `--output` und zusätzlichem frischem `--profile /tmp/…/trace.bin`
ausführen. Profilzeiten ersetzen keine normalen Zeiten. Abgelehnte Kandidaten
haben ausdrücklich keine gültige Ausgabemediane, auch bei Prozessstatus 0.

Andere Quellen und äußere Anschlüsse bleiben ausdrücklich ungeprüft. Keine native
oder vollständige Wege-/Geräte-/Statikabnahme aus dem örtlichen Höhenmodell ableiten.

## Östliche Radwegschleife mit ganzen Anschlusskappen (#434)

Der [Schleifenvergleich](stair_cycle_comparison.md) ergänzt den ganzen tatsächlichen
östlichen Radweg und plant das verbundene Gelände neu. Dieselben Python-/Blender-
Pakete wie oben. Feste PBF-/DEM-Fenster sind versioniert, Erwerb ist für die normale
Reproduktion nicht nötig. Originale 18-Quellen- und Böschungsprodukte explizit aus
den vorigen Abschnitten übergeben; alle Ausgaben frisch im System-Tempverzeichnis.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_cycle_checks stair_blend_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_experiment.py --base-network "$TRANSPORT_OUTER_OUTPUT" --base-terrain "$TRANSPORT_BLEND_OUTPUT" --output "$TRANSPORT_CYCLE_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_verify.py --base-network "$TRANSPORT_OUTER_OUTPUT" --base-terrain "$TRANSPORT_BLEND_OUTPUT" --output "$TRANSPORT_CYCLE_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CYCLE_OUTPUT/before-overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CYCLE_OUTPUT/after-overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CYCLE_OUTPUT/before-diagnostic-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_CYCLE_OUTPUT/after-diagnostic-scene.json"
```

Kosten bei normaler Priorität, nacheinander; Matrixaufbau/Solve auf demselben festen
Plan, tatsächliche Ausgabeprüfung danach außerhalb der Zeit:

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_cost.py --products "$TRANSPORT_CYCLE_OUTPUT" --solver highs-ds --output "$TRANSPORT_CYCLE_OUTPUT/ds-cost.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_cost.py --products "$TRANSPORT_CYCLE_OUTPUT" --solver highs-ipm --output "$TRANSPORT_CYCLE_OUTPUT/ipm-cost.json"
```

Optional wie oben `memray==1.20.0`, separater frischer `--output` und
`--profile /tmp/…/trace.bin`. Die zwei DEM-Fenster können ausdrücklich neu aufgenommen
werden: `stair_cycle_inputs.py --receipts ORIGINAL-RECEIPT EAST-RECEIPT --output /tmp/…/stair-cycle-vienna.json`.
Die Receipts müssen die im festen Rezept belegten ursprünglichen Bytes tragen;
neue Quellenstände ersetzen keine Referenz automatisch.

## Nördlicher Radweg: erhaltene Kontaktfacetten und Rundungsreserven (#434)

Der [Nordvergleich](stair_cycle_north_comparison.md) nutzt dieselben Werkzeuge auf
einer zweiten festen Instanz. Bisherige Ostprodukte explizit übergeben; neue
Eingabe enthält keine gelösten Höhen und erzeugt keine Katalogeinträge je Straße.
Dieselben Pakete; kein zusätzlicher Erwerb oder neue Runtime-Abhängigkeit.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_cycle_north_checks stair_cycle_checks stair_blend_checks stair_blend_precision_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_experiment.py --fixture test/experiments/transport/fixtures/stair-cycle-north-vienna.json --base-network "$TRANSPORT_CYCLE_OUTPUT" --base-terrain "$TRANSPORT_CYCLE_OUTPUT" --output "$TRANSPORT_NORTH_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_verify.py --fixture test/experiments/transport/fixtures/stair-cycle-north-vienna.json --base-network "$TRANSPORT_CYCLE_OUTPUT" --base-terrain "$TRANSPORT_CYCLE_OUTPUT" --output "$TRANSPORT_NORTH_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_terrain_cost.py --products "$TRANSPORT_NORTH_OUTPUT" --mode selective --output "$TRANSPORT_NORTH_OUTPUT/selective-cost.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_terrain_cost.py --products "$TRANSPORT_NORTH_OUTPUT" --mode all --output "$TRANSPORT_NORTH_OUTPUT/all-cost.json"
```

Kostenprozesse nacheinander bei normaler Priorität. Optionale Memray-Messung wie
oben mit separatem frischem `--output` und `--profile`. Dieselben vier Renderaufrufe
wie im Ostvergleich auf `TRANSPORT_NORTH_OUTPUT` anwenden. Die Kamera wird aus den
tatsächlichen ausgewählten Quellflächen abgeleitet; alle Bilder bleiben lokal 1280×720.
Gewöhnliche Außenwege dürfen keine indoor-/Ebenenquellen durch ein generisches
Mesh-Rezept ersetzen. Globale/native Abnahme bleibt offen.

## Südlicher Radweg und vollständiger DEM-Eckbereich (#434)

Der [Südvergleich](stair_cycle_south_comparison.md) ergänzt denselben Operator um
den ganzen südlichen Quellteil. Die Eingabe enthält vier ausdrückliche DEM-Crops;
auch die südöstliche Ecke der rechteckigen Terrainfläche braucht echte Höhen.
Fehlende Fenster werden abgelehnt, nicht durch benachbarte Höhen ersetzt.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_cycle_south_checks stair_cycle_north_checks stair_cycle_checks stair_blend_checks stair_blend_precision_checks
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_experiment.py --fixture test/experiments/transport/fixtures/stair-cycle-south-vienna.json --base-network "$TRANSPORT_NORTH_OUTPUT" --base-terrain "$TRANSPORT_NORTH_OUTPUT" --output "$TRANSPORT_SOUTH_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_cycle_verify.py --fixture test/experiments/transport/fixtures/stair-cycle-south-vienna.json --base-network "$TRANSPORT_NORTH_OUTPUT" --base-terrain "$TRANSPORT_NORTH_OUTPUT" --output "$TRANSPORT_SOUTH_OUTPUT"
```

Die vier Renderaufrufe und beide Terrain-Kostenaufrufe oben mit
`TRANSPORT_SOUTH_OUTPUT` verwenden. Die ganze ausgewählte Quelle bestimmt die
nähere Kamera. Forschung bleibt lokal in 1280×720; keine Place-Galerie.
Explizite Neuaufnahme: `stair_cycle_inputs.py --receipts ORIGINAL-RECEIPT EAST-RECEIPT SOUTH-RECEIPT SOUTHEAST-RECEIPT --sources openfreemap:14/8937/5678:transportation:32651280:3 openfreemap:14/8937/5678:transportation:39881120:23 openfreemap:14/8937/5678:transportation:39881120:51 --output /tmp/…/stair-cycle-south-vienna.json`.
Geschützte Profile, Fokus und Solveroptionen bleiben erklärte Instanzparameter;
Aufnahme erzeugt keine gelösten Höhen oder diese zusätzlichen Entscheidungen.

## Tatsächliche Pfeiler unter dem Treppen-/Radbrückenverbund (#434)

Der [Pfeilervergleich](stair_support_comparison.md) benutzt die unveränderten
geprüften Südprodukte, den vorhandenen installierbaren SupportVolumeBuilder und
dieselben Python-/Blender-Pakete. Keine erneute Höhenpassung oder SDK-Abhängigkeit.

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_support_checks test_supports
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_support_experiment.py --products "$TRANSPORT_SOUTH_OUTPUT" --output "$TRANSPORT_SUPPORT_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_support_verify.py --products "$TRANSPORT_SOUTH_OUTPUT" --output "$TRANSPORT_SUPPORT_OUTPUT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SUPPORT_OUTPUT/before-overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SUPPORT_OUTPUT/after-overview-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SUPPORT_OUTPUT/before-junction-scene.json"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_BLENDER_PY" test/experiments/transport/render_transport_research.py "$TRANSPORT_SUPPORT_OUTPUT/after-junction-scene.json"
```

Alle Ausgaben frisch im System-Tempverzeichnis; lokale Forschung in 1280×720.
Optionale Kostenmessung, nacheinander bei normaler Priorität:

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_support_cost.py --products "$TRANSPORT_SUPPORT_OUTPUT" --mode fixed --output "$TRANSPORT_SUPPORT_OUTPUT/fixed-cost.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_support_cost.py --products "$TRANSPORT_SUPPORT_OUTPUT" --mode shifted --output "$TRANSPORT_SUPPORT_OUTPUT/shifted-cost.json"
```

Optional `memray==1.20.0`, separater frischer `--output` und `--profile /tmp/…/trace.bin`;
keine Python-Allocatorereignisse/native Stack-Traces, gleiche Geometrie-/Zeitgrenzen.
Die normale vollständige Auswertung braucht keine weiteren Pakete.
Der Reader baut weder Pfeiler noch Kontakte neu. Feste/verschobene Stationen
tragen Auslassungsgründe; die gültige Variante mit mehr Pfeilern wird dargestellt.
Abstände sind Gestaltungswerte, keine Statik oder vollständige Quellenfreigabe.

## Gemeinsame Pfeilervorbereitung (#434)

[Vorbereitungsvergleich](stair_support_preparation.md): Standard ist jetzt die
gemeinsame Vorbereitung. Ganze Ausgabe und physische Abnahme bleiben gleich;
`per_source` erhält die frühere Variante ausschließlich als Kostenreferenz.

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest stair_support_checks stair_support_traffic_checks test_supports
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_support_cost.py --products "$TRANSPORT_SUPPORT_OUTPUT" --mode shifted --preparation per_source --output "$TRANSPORT_SUPPORT_OUTPUT/per-source-cost.json"
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" test/experiments/transport/stair_support_cost.py --products "$TRANSPORT_SUPPORT_OUTPUT" --mode shifted --preparation shared --output "$TRANSPORT_SUPPORT_OUTPUT/shared-cost.json"
```

Kostenprozesse nacheinander bei normaler Priorität; dieselben optionalen Memray-
Flags und frische Ausgaben wie oben. Die vorhandenen Erzeugungs-/Readeraufrufe
verwenden ohne neue Quell-/Instanzparameter den gemeinsamen Pfad.

## Historische Place-Kameras (#209)

Die folgenden Aufrufe reproduzieren die bisherigen Wien-/Tokyo-/Central-Park-/
Feldkirch-Vergleiche mit den ursprünglichen nativen Kamerareferenzen. Sie werden
für bestehende Geometrie- und Kostenbelege erhalten, nicht als aktuelle Galerie.

Private Renderergebnisse und Kamerabelege liegen außerhalb des Repositorys.
Der Offline-Aufruf `transport_place_shots.py` erhält explizit den aktuellen
Arbeitsstand, den geprüften Wiener Höhenplan, deklarierte DEM-Receipts und sein
Ausgabeverzeichnis im System-Tempverzeichnis. Alle Forschungsbilder bleiben lokal;
Herkunft, Rezeptgrenzen und Kosten stehen im
verlinkten Fach-Issue/PR. Besitzer: #209 unter #19.


`capture_place_windows.py` wählt ganze MVT-Kacheln aus Kamera, horizontalem
Blickfeld und 1600 m lokalem Bedarf. Die bestehende unveränderliche
OpenFreeMap-Snapshot-URL bleibt erhalten. Alte Quellen/Attribute/IDs bleiben
vorhanden. Wiens verifizierter Höhenplan wird unverändert reserviert; neue
Quellen in seiner Nähe erhalten einen expliziten Reservierungsgrund. Die
zusätzlichen lokalen Netze werden nicht als mit diesem Plan gekoppelt ausgegeben.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" test/experiments/transport/capture_place_elevation.py --cache "$TRANSPORT_PLACE_SOURCES" --output "$TRANSPORT_CAMERA_ELEVATION"
PYTHONPATH=lib/src "$TRANSPORT_PY" test/experiments/transport/capture_place_windows.py --cache "$TRANSPORT_PLACE_SOURCES" --port-plan "$TRANSPORT_PORT_PLAN" --retain-flat-release "$TRANSPORT_FLAT_RELEASE" --output "$TRANSPORT_CAMERA_INPUTS"
PYTHONPATH=lib/src "$TRANSPORT_PY" test/experiments/transport/component_surface_experiment.py --data-root "$TRANSPORT_CAMERA_INPUTS" --worker Vienna components
PYTHONPATH=lib/src "$TRANSPORT_PY" test/experiments/transport/transport_place_shots.py --flat-release "$TRANSPORT_FLAT_RELEASE" --port-plan "$TRANSPORT_PORT_PLAN" --camera-products "$TRANSPORT_CAMERA_INPUTS" --elevation-inventory "$TRANSPORT_CAMERA_ELEVATION" --output "$TRANSPORT_NEW_SHOTS"
```

Die Variablen benennen ausdrücklich gewählte externe Pfade. Vor dem Rendern
werden alle vier Komponenten-Worker isoliert ausgeführt und ihre Eingaben,
kompletten Quellbilanzen und Produkte als `generation.json` hashgebunden.
Wien behält sein angepasstes C0-Boden-/C1-Straßenprodukt; ergänzte Netze verwenden
den expliziten ebenen Höhenbezug am lokalen DEM-Ursprung. Profil-/Ebenen- und
Weltvollständigkeit bleiben unabhängig von diesen Kamerabildern zu prüfen.

Central Park behält optional seine geprüfte ganze alte Kachel als separates
Fenster. Der gemeinsame Drei-Kachel-Aufbau überschritt 60 CPU-Sekunden; die
serielle Alternative lieferte nach ihrem 15-Sekunden-Vorbereitungsbudget keine
Fläche. Die bestehende Kachel wird deshalb unverändert reserviert; zusätzliche
Quellen außerhalb ihres 64-m-Schutzbereichs werden unabhängig geplant. Alle
Quellen und Gründe bleiben erhalten, ohne behauptete Kopplung der Fenster.

Der gemeinsame Eingang `outshine_ref.places` liest Kameras, prüft Quellbytes
und dekodiert MVT/Terrarium. `capture_place_elevation.py` deklariert für relative
Kameras z16 mit z15/z14-Eltern; `--fetch` erlaubt ausdrücklich fehlende neue
Quellen. Ohne diesen Schalter bleibt der Aufruf offline. Gemeinsame Quellen
liegen unter `<Cache>/mapterhorn/` beziehungsweise `<Cache>/openfreemap/`.
Der Renderer hat keinen Netzwerkpfad und dokumentiert die tatsächlich benutzte
DEM-Stufe. Absolute Kameraangaben benötigen keine künstliche Bodenhöhe.

Die historischen Transportpläne bleiben im expliziten skalierten Mercator-Rahmen
und behalten ihre verifizierten Höhen, Indizes und Quell-IDs. Nur die Darstellung
übersetzt ihre Vertices über geographische Koordinaten nach WGS84-ENU des
Szenarios. Kein Geoid-Ausgleich oder neuer Gradienten-/Kontaktbeleg daraus.
`reference/place-sources.json` bindet die bisherigen starren Flachrezept-Datumswerte
an ihre ursprünglichen groben DEM-Receipts; sie werden nicht durch feinere neue
Kamerahöhen umgedeutet. Die historische Pixelzentrum-Interpolation in
`capture_height_targets.py` bleibt ein ausdrücklich getrennter numerischer
Vergleichsvertrag; Provider und Terrariumdecoder verwendet auch sie gemeinsam.
Migration und Frame-/Bildbelege besitzt #225 unter #216.

Ältere Verfahrensansichten bleiben als private Geometriekontrollen nutzbar:

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" test/experiments/transport/whole_tile_showcase.py --data-root "$TRANSPORT_WORK"
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/whole_tile_checks.py
```

Die Pfadbeispiele älterer Versuche unten dokumentieren die damalige Erzeugung.
Ihre privaten Linux-Daten liegen außerhalb des Repositorys: Arbeitsstand
`/var/tmp/outshine-transport-workspace-0241b3f81/`, referenzierte Belege
`/var/tmp/outshine-transport-evidence-0241b3f81/`. Die frühere relative Wurzel
`build/experiments/transport/` entfällt dort. Beim weiteren Ausbau erhalten diese
Versuchsaufrufer explizite Ein-/Ausgabepfade.

## Geschlossene 3D-Körper und gespeicherte Wiener Quellkachel

### Schienenprofile und Schwellen auf gewählten Betten (#250)

`rail_place_experiment.py` liest die gepinnten genodeten Achsen und unveränderten
Nutzungsbilanzen der vier Kamerafenster. Der wiederverwendbare Profil-/Instanzkern
liegt in `outshine_ref.transport.rail_hardware`; Vertrag und Rezeptannahmen stehen
in [lib/rail_hardware.md](../../../lib/rail_hardware.md). Keine Place-Sondergeometrie
oder neue Quellen. Fehlende Spurweiten verwenden ausdrücklich das prozedurale
1,435-m-Rezept. Bestehende alte C1-/Flachfenster bleiben unverändert.

Weichen, Querungen und nahe Fremdflächen erhalten konservative lokale Lücken.
Ungeeignete Stücke behalten ihren Quellenbezug und konkreten Grund; ihre gültigen
Betten verschwinden nicht. Ein echtes nach Zuschnitt überlappendes Tokyo-Paar liegt
unter `fixtures/rail-overlap-tokyo.json`. Die optionale Hardwarezulassung prüft
Schiene/Schiene und Schwelle/Schwelle; vertikale Auflage bleibt ein Kontakt.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/rail_place_experiment.py --camera-products "$TRANSPORT_CAMERA_INPUTS" --output "$TRANSPORT_RAIL_PRODUCTS"
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/rail_place_verify.py --products "$TRANSPORT_RAIL_PRODUCTS" --camera-products "$TRANSPORT_CAMERA_INPUTS" --output "$TRANSPORT_RAIL_PRODUCTS/verification.json"
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/rail_instance_cost.py --products "$TRANSPORT_RAIL_PRODUCTS" --mode instances --output "$TRANSPORT_RAIL_INSTANCE_COST"
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/rail_instance_cost.py --products "$TRANSPORT_RAIL_PRODUCTS" --mode baked --output "$TRANSPORT_RAIL_BAKED_COST"
```

Alle Variablen sind frische explizite externe Temp-Pfade. Der Vier-Orte-Worker
behält 2 GiB AS/60 CPU-Sekunden; optionaler Aufwand je Fenster kooperativ 15 s,
Äste/Stücke/Paararbeit zusätzlich begrenzt. `rail_place_verify.py` prüft Exporte
gegen tatsächliche gepinnte Bett-Dreiecke, sämtliche Spur-/Volumen-/Instanzwerte,
ganze Footprints, Fremdflächen und Quellenachsen. Freigaben brauchen saubere
eingefrorene Erzeugerstände und vollständige aktuelle Kernelhashes.

Der Instanz-/Baked-Vergleich misst dieselben tatsächlichen Schwellen nach dem
Quell-/Kontaktaufbau. Sieben Batches je frischem Worker, Daten-/Arbeitspayload und
explizite Kopien getrennt von Gesamt-RSS. Optional `--profile <frischer Pfad>`
mit extern installiertem `memray==1.20.0`: ein Aufruf mit Python-/Systemallokationen
und nativen Stacks. Profilzeiten/RSS enthalten Profilerkosten; angeforderte
Allokationsbytes sind keine einzigartigen/live Bytes, interne Kopiertotals bleiben
unbekannt. Normale Messungen und GPU-/Native-Abnahme bleiben getrennt.

`transport_place_shots.py --rail-products "$TRANSPORT_RAIL_PRODUCTS"` fügt nur
gebackene Inspektionsgeometrie hinzu und erhält die bisherigen Körperbuffer als
exakte Präfixe. Beide werden gemeinsam nach Szenario-ENU übersetzt. Metrische
Spur-/Kontaktbelege bleiben dem historischen Planrahmen zugeordnet. Nach frischem
Vierer-Rendering die Bilder tatsächlich vergleichen und bei sichtbarem Gewinn
die vier Präsentations-PNGs aktualisieren.

`transport.close_surface` aus `lib/` schließt bereits konforme Fahrflächen mit
expliziten Vertexhöhen. Obere Anschlussvertices bleiben gemeinsam, Innenlöcher
offen; Boden und Randwände bilden einen gerichteten Körper. Das ist ein
Geometrieverfahren, kein Höhen-/Verkehrsnetzsolver. Der Vergleich prüft dieselben
Körperbuffer, Kanten und Volumina für NumPy-Sortierung und eine Dictionary-Referenz.
Frische Worker messen sieben Aufbauzeiten und ihren vollständigen Peak-RSS.

```sh
"$TRANSPORT_PY" -m pip install -r test/experiments/transport/requirements.txt
"$TRANSPORT_PY" -m pip install -e ./lib
PYTHONPATH=lib/src "$TRANSPORT_PY" test/experiments/transport/capture_transport_tile.py --output "$TRANSPORT_WORK/inputs"
"$TRANSPORT_PY" test/experiments/transport/transport_flat_network.py --input build/experiments/transport/inputs --output build/experiments/transport/flat-vienna Vienna
"$TRANSPORT_PY" test/experiments/transport/transport_flat_surface_network.py --input build/experiments/transport/flat-vienna --output build/experiments/transport/surface-vienna-cuts --junction-mode cuts Vienna
"$TRANSPORT_PY" test/experiments/transport/surface_solid_benchmark.py
"$TRANSPORT_PY" test/experiments/transport/surface_solid_showcase.py
```

Die Aufnahme liest ausschließlich die bereits gepinnte OpenFreeMap-Kachel aus dem
Client-Quellcache und prüft ihren Hash. Originalfeatures, Attribute, IDs und
zurückgestellte Flächen bleiben im gespeicherten Input erhalten. Die ebene
Nutzungsbilanz nennt jeden Achsteil samt konkretem Grund bei Nichtverwendung.
Eine Quellkachel ersetzt nicht das frühere vollständige Wiener Testfenster.

Blender ist als Systempaket erforderlich (`blender` unter Debian). Vier
Workbench-Inspektionen in 1920×1080 liegen unter `build/experiments/transport/solid/`:
geneigter T-Anschluss, Ring, getrennte Querung und Wiener Quellkachel. Die Kamera
prüft sämtliche Vertices auf vollständige Bildabdeckung; Skriptfehler ergeben
einen fehlgeschlagenen Renderprozess. Der Showcase veröffentlicht nur den letzten
vollständig gerenderten, zu Bild und Report passenden Stand.
Flatheight und Brückenniveaus sind explizite Rezeptannahmen. DEM, Rampen, Stützen,
fahrbare Kurven und native Abnahme bleiben offen. Bibliotheksübernahme: #50.

## Grobes DEM und ganze Fahrflächen (#20)

`capture_height_targets.py` liest behaltene Mapterhorn-Terrarium-Kacheln (512×512)
und interpoliert an Pixelzentren mit Nachbarkacheln. Neue Quellen werden nur mit
`--fetch` erworben; vorhandene Cachedateien werden nicht automatisch erneuert.
URL, Bytes und SHA-256 bleiben im Report. Das DEM ist ein weicher Höhenprior,
keine beobachtete Fahrbahnhöhe. `surface_height_frames.py` ordnet den verwendeten
Originalachsen prozedurale Längs-/Quergefälle zu; Originalattribute bleiben erhalten.
Anschlüsse sowie mehrdeutige/degenerierte Richtungen bekommen konservative isotrope
Grenzen; jede unsichere Richtung wird mit Face und Quell-ID festgehalten.

```sh
"$TRANSPORT_PY" -m pip install -e './lib[transport]'
PYTHONPATH=lib/src "$TRANSPORT_PY" test/experiments/transport/capture_height_targets.py --mesh "$TRANSPORT_WORK/surface-vienna-cuts/Vienna-flat-surfaces.npz" --input "$TRANSPORT_WORK/inputs/Vienna-input.json" --output "$TRANSPORT_WORK/heights"
"$TRANSPORT_PY" test/experiments/transport/surface_height_frames.py
"$TRANSPORT_PY" -W error test/experiments/transport/height_source_checks.py
"$TRANSPORT_PY" -W error test/experiments/transport/surface_height_checks.py
"$TRANSPORT_PY" test/experiments/transport/surface_height_benchmark.py
"$TRANSPORT_PY" test/experiments/transport/surface_height_showcase.py
```

Bei fehlendem DEM-Cache zuerst denselben Capture-Aufruf einmal mit `--fetch`:
`curl` ist neben den Python-Paketen erforderlich. Der Vergleich nutzt identische
Vertexpriors, Gradientenrahmen und acht Sekunden Solverzeit. Clarabel löst die
elliptischen Ganzflächenbedingungen; SciPy/HiGHS verwendet ein konservativ
einbeschriebenes 16-Eck. Beide erfüllen denselben geforderten Neigungsstandard,
die lineare Approximation hat aber eine kleinere zulässige Menge. Präparation,
Solverzeit und Peak-RSS frischer Worker werden getrennt ausgewiesen; akzeptierte
Flächen erhalten zusätzlich eine unabhängige Normalenprüfung.

Vier 1920×1080-Ansichten zeigen die gesamte gespeicherte Wiener Kachel sowie das
vollständige Bauteil mit dem schlechtesten DEM-Gradienten, jeweils vorher/nachher
mit derselben Kamera und ohne vertikale Überhöhung. Orange zeigt unzulässige
Ganzflächengradienten im ungeprüften Prior, Grün Anschlüsse und Grau Korridore.
Die Detailauswahl schneidet keine Dreiecke und löst die Höhen nicht erneut.
Dateien liegen unter `build/experiments/transport/heights/`. Ganze Flächen,
gemeinsame Höhen und geschlossene Körper sind geprüft; C1/Fahrkurven, finaler
Terrainanschluss, funktionale Zugänge und erhöhte Verkehrsrezepte bleiben offen.

## Gemeinsame Bodenfläche und vorbereitete Kontakte (#20)

`ground_surface.py` ergänzt außerhalb der tatsächlichen ebenerdigen Fahrflächen
eine begrenzte konforme CDT-Partition. Die Verkehrsfläche erhält keine neuen XY-
Koordinaten und bleibt über Vertex-/Face-Zuordnungen nachvollziehbar. Ein gemeinsamer
Höhenplan begrenzt Straßen nach ihren Rezepten und Bodenfacetten nach dem ausdrücklich
prozeduralen Böschungsrezept 2:3. Unabhängige flache Straßenpläne können einen steilen
Zwischenraum erzeugen; die gemeinsame Lösung plant deren Höhen ausdrücklich neu.
Input-/Output-Coverage, vollständige Domain, Ränder und ganze Gradienten werden geprüft.

```sh
"$TRANSPORT_PY" -W error test/experiments/transport/ground_surface_checks.py
"$TRANSPORT_PY" -W error test/experiments/transport/ground_surface_experiment.py --compare-geometry
"$TRANSPORT_PY" -W error test/experiments/transport/ground_surface_experiment.py
"$TRANSPORT_PY" -W error test/experiments/transport/surface_contact_benchmark.py
"$TRANSPORT_PY" -W error test/experiments/transport/ground_surface_showcase.py
```

Der Geometrievergleich nutzt optimierte Coverage-Union und allgemeine Union samt
denselben Vor-/Nachprüfungen; identische vorbereitete Meshs werden verlangt. Der
Kontaktvergleich nutzt die installierte `geometry.SurfaceSampler`-Bibliothek:
STRtree-Prädikat gegen Bounding-Boxes plus exakten Distanzfilter, identische Treffer,
Face-IDs/Höhen/Normalen/Fehlgründe und unabhängige Ebenenprüfung. Je Methode frischer
Worker und fünf Wiederholungen; Indexvorbereitung, Query inkl. Ergebniskopie und
Prozess-Peak-RSS getrennt. Keine Solver-/Providerarbeit in der fertigen Kontaktabfrage.

Vier 1920×1080-3D-Inspektionen unter `build/experiments/transport/ground/` zeigen die
gesamte Kachel und einen 120×120-m-Kontaktbereich vor/nachher mit gleicher Kamera.
Vorher ist absichtlich ungültig: unverändertes grobes DEM durchdringt gefittete Straßen.
Nachher ist die gemeinsame obere Fläche genau einmal belegt. Ein 5-m-Schaublock
visualisiert ihren Ausschnitt; er ist kein Aushubvolumen-/Material-/Kollisionsnachweis.
Brücken/Dächer gehören nicht in diese ebenerdige Bodenpartition. Gespeicherte Daten
und Z-Bezug bleiben erhalten; keine vollständige Place-, C1- oder Eingangsabnahme.
Das C0-Mesh approximiert den DEM-Prior; der gemessene Fehler an Face-Zentren wird
ausgewiesen und beweist keine globale DEM-Fehlerschranke. Adaptive Verfeinerung und
glatte Profile bleiben weitere Arbeit; funktionale Zugangskontakte bleiben separat.

## Gegenbeispiel: feste Terrain-Fächer verfeinern (#20)

```sh
"$TRANSPORT_PY" -W error test/experiments/transport/ground_refinement_checks.py
"$TRANSPORT_PY" -W error test/experiments/transport/ground_refinement_experiment.py
"$TRANSPORT_PY" -W error test/experiments/transport/ground_refinement_showcase.py
```

Kantenmittelpunkte und Flächenzentren erhalten bei gleichem Budget alle bisherigen
Vertices, Straßen und äußeren Ränder. Analytische Ganzflächenintervalle begrenzen
die neue Einzelhöhe; eine lokale DEM-Stichprobe verhindert den zuerst gemessenen
Extremfehleranstieg. Beide Methoden reduzieren danach den gemessenen Mittel-/RMS-
Fehler. Der Zentrenpfad kostet weniger und verbessert p95 stärker; der Kantenpfad
hat den kleineren gemessenen Maximalfehler. Die Prüfmenge bleibt unabhängig von
den eingefügten Kandidaten. Dies ist keine globale DEM-Fehlerschranke.

Die tatsächlich geöffneten 1920×1080-Ansichten (grob, 50%-Morph, fein) zeigen jedoch
dreieckige Buckel und scharfe Falten. Dieser Pfad ist als **visuelle Lösung verworfen**;
er wird weder in `lib/` übernommen noch ersetzt er die freigegebene Showcase.
Reproduktion, Meshs und Messungen liegen unter `build/experiments/transport/refinement/`.
Der vertikale grob/fein-Unterschied ist für affine Eltern-/Kindflächen an den
feinen Vertices exakt begrenzt; das beweist weder C1 noch einen Bildgewinn.
Als Nächstes die freie Terrain-Konnektivität zwischen tatsächlichen Straßenrändern
prüfen, anstatt die unpassenden alten Innenkanten weiter zu fixieren.

## Bodenverbindungen austauschen (#20)

```sh
"$TRANSPORT_PY" -W error test/experiments/transport/ground_reconnect_checks.py
"$TRANSPORT_PY" -W error test/experiments/transport/ground_reconnect_experiment.py
"$TRANSPORT_PY" -W error test/experiments/transport/ground_reconnect_showcase.py
```

Der gespeicherte Zentrenversuch liefert dieselben 2500 zusätzlichen Punkte für zwei
Verbindungsvarianten: GEOS-Delaunay mit Winkelgewinn gegen rein probengesteuerten
Kantentausch. Beide halten sämtliche XYZ, Straßenfaces und Außenränder fest.
Streng konvexe Bodenpaare dürfen die Diagonale tauschen, wenn die neuen Ganzflächen
die Neigungsgrenze und denselben lokalen DEM-Qualitätscheck erfüllen. Bis acht
Durchgänge, eine Million Proben, 65536 Faces/Vertices und zwölf Sekunden kooperative
Zeitgrenze; eine begrenzte Phase kann überlaufen. Kein Frame-Verfahren.

Drei frische Worker je Variante prüfen dieselben 40734 unabhängigen Abfragen, volle
Coverage/Rand und Neigung, unveränderte Quellen und Verkehrsgeometrie. Zeit umfasst
den Kantentausch einschließlich Vorbereitung/DEM-Proben, nicht spätere unabhängige
Abnahme oder PNG-Export; Prozess-Peak-RSS wird separat gemessen. Ergebnisse und sechs
1920×1080-Ansichten liegen unter `build/experiments/transport/reconnect/`.
Gleiche Kamera vor/nachher für Kachel und 120×120-m-Bereich, keine Höhenüberhöhung.

Geöffnete Detailbilder zeigen weniger Sternfalten; gemeinsame Kontakte bleiben
erhalten. C0-Facetten bleiben sichtbar. Globale DEM-/C1-/native Abnahme und stabile
LOD-Übergänge fehlen: der alte affine Eltern-/Kindbeweis gilt nach dem Kantentausch
ausdrücklich nicht mehr. Dieser Pfad bleibt Experiment, keine `lib/`-Promotion und
kein Ersatz des freigegebenen #69-Bodenplans.

## Externe CityEngine-Operationsreferenz (#19)

```sh
"$TRANSPORT_PY" -W error test/experiments/transport/esri_transport_catalog.py
"$TRANSPORT_PY" -W error test/experiments/transport/esri_transport_ghidra.py
"$TRANSPORT_PY" -W error test/experiments/transport/verify_esri_transport_ghidra.py
"$TRANSPORT_PY" -W error test/experiments/transport/verify_esri_transport_oracle.py
"$TRANSPORT_PY" -W error test/experiments/transport/esri_transport_comparison.py
"$TRANSPORT_PY" -W error test/experiments/transport/esri_transport_showcase.py
```

Die externe SDK-Core-Binary liegt unter `~/Downloads/esri_ce_sdk-3.4.12206/bin/`.
SDK-Version über die öffentliche `prt::getVersion`-ABI geprüft; SHA256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
`nm -S --defined-only` und `c++filt` katalogisieren 16675 Textsymbole, davon 507
Geometriekandidaten einschließlich Aliassen/Cold-Clones. Elf Funktionen decken
Offset, Schnitt, Projektion, Triangulation, Extrusion und Version ab.

Ghidra verwendet den vorhandenen Buildings-Skriptansatz, Snap-Projekte ausschließlich
unter `~/snap/ghidra/common/`. Disassemblierung ist auf jeden ELF-Symbolbereich begrenzt.
Ausgehende Sprünge werden als dokumentierte CALL_RETURN-Grenzhypothese modelliert;
ihr Zielcode gehört nicht zum untersuchten Körper. Ursprüngliche Adressen/Mnemonics,
direkte Aufrufe/PLT-Auflösung, indirekte Unsicherheit, Bytezählung und Exporthashes
stehen im Report. Kein verifizierter Decompiler-Typvertrag oder Vollständigkeitsbeweis
aller ausführbaren Pfade. Neue Nonce, exakte Bereiche und sämtliche frischen Exporte
sind Pflicht; Exit 0 mit fehlendem/altem Teilstand wird abgewiesen. Prozess360 s,
Decompiler25 s pro Funktion; globale Autoanalyse aus. Keine externen Codeexporte im Git.

Linux-Orakel laufen in eigenen Prozessen: CPU8 s, Adressraum2 GiB, Parent-Wartegrenze10 s.
Blockierter C-Aufruf und verweigerte3-GiB-Abbildung prüfen diese Grenzen; Prozessstart
ist kein universeller harter Whole-call-Nachweis. `Vector3`-/Skalar-Ausgaben haben
Canaries, private Offset-Vektoren geprüfte lesbare Speicherbereiche. Nur gepinnte
skalare Schnitt-/Ray- und Offset-ABIs werden aufgerufen. Ray-Modus2 liefert den
Multiplikator der eingegebenen Richtung, nicht grundsätzlich Meter.

Der SDK-Triangulationsbackend `libglutess.so` besitzt SHA256
`2b10c97d5a3c92a2885786d4e0d497cb753b9786ef0b450db1ee61381c1fda78`.
Seine dokumentierte GLU-C-API verwendet opaque Handles; sie beweist nicht den privaten
`util::Triangulator`-/Processor-Vertrag. Raw-Rasterdreiecke bleiben gespeichert;
exakte Nullfaces werden vor CAD-Ausgabe entfernt, Randpunkte mit dem gemeinsamen
`conform_triangles` eingepasst. SDK-Extrusion und Mesh-/Hauptachsenobjekte bleiben
Decompiler-Hypothesen. Die 0,35-m-Bildkörper erzeugt unsere eigene Schließbibliothek.

29 Offset-, 13 Triangulationsfälle und 256 echte Wiener Quellsegmentpaare vergleichen
dieselben Eingaben, Geometrie/Topologie/Grenzen und Kosten. Float32-XY/Abstand beim
Offset sind für beide Varianten identisch; Transport nutzt außerdem Rundprofile,
dieser Vergleich untersucht ausdrücklich den Mitre-Inset. Der kleine SDK-Helfer
liefert beim Dumbbell nur ein Fragment, GEOS beide; Löcher haben keinen expliziten
ABI-Parameter und werden als fehlendes Orakel ausgewiesen. `edgesIntersect` hat
beobachtete halb offene Enden und akzeptiert auch schiefe Annäherungen; kein Ersatz
für echte räumliche Kontakte. Triangulationsdomänen/Ränder/Höfe/Quellpunkte stimmen
nach offengelegtem CAD-Adapter im geprüften Umfang überein.

Vier gleiche Offset-Qualitätsfälle haben je Methode drei frische Worker/fünf Samples.
Zeit für vorbereitete Operation und XY-Decode, Startup getrennt; SDK-Speicherprüfungen
zählen. Peak-RSS aus Linux VmHWM gehört zum aktuellen Exec-Adressraum einschließlich
Imports/Quelle/Receipt; `ru_maxrss` wird separat gehalten, weil Fork-Vorgeschichte
kleine Workerpeaks verdecken kann. Keine Geräte-/Runtime- oder Netzgenerator-Abnahme.
Ergebnisse, Assembly, externe Hypothesen und sechs1920×1080-Bilder liegen unter
`build/experiments/transport/esri-reference/`. Keine SDK-/Fremdcode-Abhängigkeit in lib/runtime.

```sh
"$TRANSPORT_PY" -W error -m unittest discover -s lib/tests
"$TRANSPORT_PY" -W error test/experiments/transport/surface_solid_checks.py
```

### Stetige Höhenübergänge (#20)

Die Bibliothek liefert flache Endkontakte, ganzintervallige Neigungsgrenzen und
begrenzte vertikale Chordabweichung. Parabolisch/linear/parabolisch und kubischer
Smoothstep verwenden dieselben Endhöhen, Stationslängen und Budgets. Aus
`abs(z') <= min(a*s, g, a*(L-s))` folgt durch Integration die mögliche Höhendifferenz.
Die kleinste Länge ist `2*sqrt(abs(dz)/a)` bis `abs(dz)<=g²/a`, danach
`abs(dz)/g+g/a`. Bei längerer vorgegebener Strecke minimiert die parabolische Variante
die notwendige Spitzen-Zweitableitung: `4*abs(dz)/L²` ohne Neigungssättigung,
sonst `g²/(g*L-abs(dz))`. Eigene Herleitung, nur eindimensionale flache Endkontakte.
Kubisch benötigt `max(1.5*abs(dz)/g, sqrt(6*abs(dz)/a))`.

SIGGRAPH 2008 [Interactive Procedural Street Modeling](https://www.sci.utah.edu/~chengu/street_sig08/street_sig08.pdf)
(Figur 2 optisch geprüft) und Galin et al., Eurographics 2010
[Procedural Generation of Roads](https://doi.org/10.1111/j.1467-8659.2009.01612.x)
(Figuren 17/18 optisch geprüft) ordnen Plan, Querschnitt und Terrain ein.
Die feste [ASAM OpenDRIVE 1.8.1 Elevation-Spezifikation](https://publications.pages.asam.net/standards/ASAM_OpenDRIVE/ASAM_OpenDRIVE_Specification/v1.8.1/specification/10_roads/10_05_elevation.html)
belegt lokale kubische Stationspolynome als Darstellung; kein Format-/SDK-Port.

`fixtures/vertical-transition-cases.json` hält 256 längste gerade, ebenerdige Achsen
der gespeicherten Wiener OpenFreeMap-Kachel, Original-Tags/Quell-IDs/XY,
Mapterhorn-Endhöhen und beide Quellenreceipts. Kein neuer Quellabruf.
Flache Endneigungen sind eine deklarierte Rezeptannahme, kein Nachweis vorhandener
Kreuzungsneigungen. Tatsächliches Quergefälle0.015 verbraucht 0.6 des Querneigungs-
budgets0.025; für Längsneigung bleiben 0.8 des Klassenlimits in derselben Ellipse.
Die ganze rotierte Fahrfläche, Quellenendkontakte und unabhängige 257 Höhenproben
je Profil werden geprüft. Zweitableitung0.002/m, Chordfehler0.01m, höchstens4096
Profilvertices. Fehlende Breite wird ausdrücklich mit4m ergänzt.
Parabolisch246/256 gültig, kubisch229/256;17 zusätzliche Lösungen, zehn in beiden
Verfahren zu kurz. Jede unlösbare Quelle behält ID/Tags und benötigte Länge.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error -m unittest discover -s lib/tests -p 'test_transport*.py'
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/vertical_transition_checks.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/vertical_transition_experiment.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/vertical_transition_benchmark.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/vertical_transition_showcase.py
```

Nur `vertical_transition_experiment.py --capture-inputs` ersetzt die Eingabefixture
aus Originalquellen/DEM-Cache; normale Läufe und Tests verändern keine Referenz.
Kosten: drei frische Worker je Methode, sieben Batches derselben229 gemeinsamen
gültigen Quellen, First-Import/Profilvorbereitung getrennt. Linux VmHWM umfasst
Imports/Eingaben/Profile, `ru_maxrss` separat; keine isolierte Algorithmusspeicher-
oder Geräte-/Framebehauptung. Ergebnisse unter `build/experiments/transport/vertical-profiles/`.
Sechs geöffnete1920×1080-Anschlussdetails zeigen denselben ersten Abschnitt der
vollständigen Chordmeshes, ohne Höhenüberhöhung. Ganze Fahrmeshes bleiben separat.
C1 betrifft den analytischen Plan; das endliche Mesh bleibt C0 und seine Endneigung
weicht höchstens `sqrt(2*a*height_error)` vom flachen Kontakt ab. Kein C2-, DEM-Innenfit-,
Fahrdynamik-, vollständiger Netz-/Brücken-/Tunnel- oder LOD-Nachweis.

### Vorgegebene Endneigungen (#163)

`lib/` ergänzt `EndpointTransition`/`plan_endpoint_transition` für feste Längen
und explizite Anfangs-/Endgrade. Hermite-Passung und integrierte erreichbare
Grad-Hüllen verwenden identische Grenzen; die Hüllen lösen zusätzliche skalare
Fälle. Nicht monotone Längenfeasibilität ist ein eigener Gegenfall. Die bisherigen
flachen Profile und alle 256 Quell-/DEM-Pins bleiben unverändert.

`endpoint_grade_contacts.py` prüft die komplette Kontaktlinie auf der ausdrücklich
gewählten portablen Wiener #69-Höhenfläche. Außen-Seite, affine Höhen und beide
Gradienten müssen über die gesamte verwendete Breite passen; Lücken, überlappende
Höhenflächen und wechselnde Gradienten werden lokal mit Original-ID ausgewiesen.
Quellbreiten bleiben erhalten; fehlende Breite erhält ausdrücklich das gleiche
4-m-Rezept wie PR96, separat von der ursprünglichen fehlenden Angabe dokumentiert.
Auf den 256 Originalachsen: 207 nicht planare Kontaktstreifen, 40 verschiedene
Querneigungen, neun kompatible Kontaktpaare. Diese neun liegen auf **Exterior-
Bodenfaces**, jeweils einer gemeinsamen affinen Ebene. Sie belegen Bodenanschlüsse,
keine bestehende Straßenkreuzung oder vollständige Netzprofilierung. C1-Portebenen
müssen vor dem gemeinsamen Höhenfit konstruktiv geplant werden; kein nachträgliches
Nullsetzen oder Verengen der Kontakte. Bankwechsel bleiben offen.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error -m unittest discover -s lib/tests -p test_endpoint_grades.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/endpoint_grade_checks.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/endpoint_grade_benchmark.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/endpoint_grade_render.py
```

Ergebnisse nach `build/experiments/transport/endpoint-grades/`: drei frische Worker
je Methode, je sieben identische Neun-Quellen-Batches. CPU15s, Wall20s, AS2GiB,
BLAS1; Imports, Flächenvorbereitung, ganze Kontaktwahl und Mesh-/Topologieprüfung
getrennt. Die neun affinen Kontakte verwenden den direkten geraden Pfad beider
Methoden; vier weitere gemeinsame nicht affine/beschleunigte Kontrollprofile
besitzen getrennte gleiche Kostenbatches. Zwei weitere Gegenfälle prüfen
Methodenfehlschlag bzw. Unlösbarkeit. Ganze rotierte Bänder erfüllen dieselbe Gradellipse, 0.002/m und maximal
0.002m Chordfehler; unabhängige4097 Proben und geschlossene Körper prüfen das Resultat.
Validierungsbilder zeigen vollständige Profile mit den gewählten schiefen Endebenen
ohne Höhenüberhöhung sowie die unveränderte ganze Elternkachel als Kontext. Sie
werden nicht als zusammengesetztes neues Netz ausgegeben. Die öffentliche Showcase
bleibt bei ihren vier ganzen Kacheln; API-/Kostenbelege und Grenzen stehen im PR.

### Gemeinsam geneigte ganze Straßenports (#169)

Das geometrisch geprüfte Experiment [#169](https://github.com/Weichwolf/outshine/issues/169),
PR #175, mit Bibliotheksübernahme [#177](https://github.com/Weichwolf/outshine/issues/177)
plant gemeinsame geneigte Straßenports vor dem Höhenfit. Die vollständigen
unveränderten Wiener XY-/Port-/DEM-Eingaben samt Quellen und Herkunft liegen
portabel unter [reference/port-planes/](reference/port-planes/README.md).
`port_plane_checks.py` prüft ganze schiefe Kontakte, rationale Bernstein-Schranken,
globale Konus-/LP-Fits und unabhängige Ableitungen. `port_plane_world_checks.py`
prüft gemeinsamen Boden, konforme Red/Green-Verfeinerung und die adaptive Pipeline.
`port_plane_experiment.py` ist der Straßenvergleich, `port_plane_world.py` der
ungefeinerte gekoppelte Bodenversuch, `port_plane_adaptive.py` begrenzt Verfeinerung
und erneuten Fit. Alle liefern explizite Geometrie-/Kontakt-/Budgetzustände.
Eine gültige grobe Fläche ist noch kein 0.01m-Höhenchordnachweis. Ausgabe erst nach
vollständiger Prüfung, geöffneter 3D-Inspektion und dokumentiertem Kostenvergleich
freigeben; vorhandene Showcase-/Höhenprodukte bleiben bis dahin erhalten.

Der Red/Green-Versuch scheitert auf dünnen echten Quelldreiecken. Die Alternative
`port_plane_slice_experiment.py --fit --rounds 6` unterteilt nach rationalen
Parameterlinien und passt den Boden gemeinsam an. `port_plane_slice_checks.py`
prüft Rundungsgrenzen, schmale Zellen, ursprüngliche Beobachtungen und Budgets;
`port_plane_verify.py` kontrolliert gespeicherte ganze Pläne unabhängig, danach
öffnet `port_plane_render.py` ganze und nahe Vorher/Nachheransichten. Herleitung,
Grenzen und Reproduktion stehen beim [portablen Eingang](reference/port-planes/README.md).
Analytisch C1, endliche Faces und Boden C0; kein vollständiger Dynamik-/LOD-Beleg.
Der ausgewählte `--fit`-Pfad importiert die öffentliche installierbare
`transport.plan_port_heights`-API; Originaleingaben/Quellen und Budgetdiagnosen
bleiben im Experiment. Mathematische Kerne besitzen ausschließlich `lib/`.

Der Solververgleich [#217](https://github.com/Weichwolf/outshine/issues/217) ersetzt
versuchsweise ausschließlich den Fit-Aufruf: `port_highs_experiment.py` bindet
HiGHS 1.15.1 mit nachträglichen Konus-Stützebenen in denselben öffentlichen
Ganzplaner ein. `port_highs_checks.py` prüft unabhängige Kreis-/Ellipsenlösungen,
gemeinsame Kontakte und Abbrüche. Eine LP-Statusmeldung ersetzt keine vollständige
Konus-, Geometrie- oder Kontaktprüfung. `support` wählt je Sparse-Spaltenmuster
den stärksten verletzten Konus; geprüft werden weiterhin alle Originalkonusse.
Der [Ganzplanvergleich](port_highs_comparison.md) verwirft die getesteten Varianten
unter unveränderten Grenzen; Clarabel bleibt die Referenz. Herleitung und
Primärquellen stehen in #217. Kein experimenteller Pfad wurde nach `lib/` übernommen.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error -m unittest port_highs_checks
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error test/experiments/transport/port_highs_experiment.py --backend highs-ipm-support --output "${TMPDIR:-/tmp}/outshine-transport-delayed-cuts/whole"
```

Erforderlich sind `highspy==1.15.1` und die bisherigen Python-Abhängigkeiten.
Jeder Lauf braucht ein frisches explizites Temp-Ausgabeverzeichnis. Der Worker
behält 3 GiB Adressraum, 90 CPU-Sekunden, 8 Sekunden je Fit und 100 Iterationen;
extern nach 110 Sekunden abbrechen. `--snapshot` exportiert optional den ersten
originalen Sparse-Fit in einen weiteren frischen Temp-Pfad; seine Kosten werden
getrennt genannt. Arbeitszähler zählen ausgewiesene Frontend-Materialisierungen,
explizite Kopien und API-Nutzlast, keine unbekannten internen Solverallokationen.

### Lokale Zweiebenenrezepte (#21)

`overpass_recipe.py` verbindet #96-Rampen mit einem starren Überlappungskern.
Quell-XY und DEM-Endhöhen bleiben fest; originale Stationsknicke werden erhalten.
Miter-Streifen unterstützen höchstens256 Quellpunkte und30° Richtungswechsel;
größere/gefaltete oder übersteile Fälle werden lokal begründet abgelehnt.
Klassenbreiten, fehlende4m-Breite und4,5m Durchfahrt sind explizite Rezeptannahmen.
Layer liefert ausschließlich eine Ordnung, keine Meterhöhe oder automatische Verbindung.
Treppenflächen und unklare/same-level Ordnung haben hier kein Rezept.

`overpass_clearance.py` überlagert vollständige projizierte Dreiecke. Die Differenz
ihrer affinen Z-Felder erreicht ihr Minimum an einem Vertex des Schnittpolygons;
so zählen Querneigung, schräge Ecken und die tatsächliche Deckunterseite.
Positive Kleinstüberlappungen bleiben erhalten, bloße Randkontakte gelten nicht als
Durchfahrt. Max8192 Faces/Fläche,65536 Kandidaten und1s kooperative Prüfphase;
Budgetende liefert keine vollständige Freigabe. Beweiszeugen/Checks bleiben im Report.
Eigene Hypothese aus elementarer affiner Geometrie, unabhängig vom SDK-Schnitthilfer.
Galin2010 Figuren17/18 optisch erneut geprüft: Profil/Querschnitt und Segmentierung
sind getrennte Aufgaben, nicht bereits ein vollständiger Verkehrs-/Bauwerksnachweis.

Portable `fixtures/overpass-vienna-cases.json`:53 unabhängige hypothetische Quellpaare
aus bisheriger Wiener Kachel und zusätzlicher14/8939/5680 aus demselben OpenFreeMap-
Snapshot20260927_080001_pt. Letztere:215 Features/793 Achsteile/44 Flächenquellen.
Original-IDs/Tags/XY, DEM-Endziele, Quellenhashes/Receipts und jede nicht ausgewählte
Quell-ID mit gemeinsamem Scopegrund bleiben erhalten. Keine Originalquelle ersetzt.
Normale Tests/Vergleiche lesen nur Pins; `capture_overpass_cases.py` ersetzt sie
ausdrücklich aus vorbereiteten Eingaben, `--fetch` erwirbt nur fehlende DEM-Kacheln.
Parabolisch11/53, kubisch4/53 gültig;37 Paarversuche behalten mindestens die separat
geprüfte untere Fläche. Das ist keine gemeinsam komponierte Netzabdeckung; eine
mehrfach vorkommende obere Quelle hat alternative lokale Pläne.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/overpass_checks.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/overpass_experiment.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/overpass_showcase.py
```

Ergebnisse/ganze Meshes/Produktbindungen unter `build/experiments/transport/overpasses/`.
Drei frische Worker je Methode, sieben Batches derselben vier gemeinsamen gültigen
Paare; komplette Vorbereitung/Geometrie/Neigungs-/Abstandsprüfung enthalten.
Source-JSON/Warmup/Produktserialisierung/Renderer getrennt; CPU10s/AS2GiB/Parent30s,
ein BLAS-Thread. Whole-exec LinuxVmHWM, kein Algorithmus-only-/Runtime-/Gerätenachweis.
Sechs geöffnete1920×1080-Vorher/Nachherbilder mit gemeinsamer Kamera aus beiden
Geometrien; Ganzmodell und Nahdetail, ohne Höhenüberhöhung. Vorher ist absichtlich
unzulässige Interpolation der DEM-Enden, kein schlechter gemachter Bestandsplan.

`overpass_products.py` bindet vorhandene `Plan`/`LocalFrame`/`Reservation`-Verträge:
Deck/Bodenverkehr/Quelle ausdrücklich in Experimentmetadaten, getrennte Sampler;
per-Face Höhenprismen statt einer bis zum Boden reichenden Bridge-Reservation.
Diese Prismen sind konservativ und können zusätzliche Kontakte blockieren; keine
exakten Swept-Volumes oder neue öffentliche Kontakt-API. Nur Experiment, kein neues
Bibliotheks-/natives Ebenenrezept. C1 und1cm Chordbudget gelten für Stationsprofile,
nicht für horizontale Knicke oder eine globale Fehlergrenze des Miter-Surfaces.
Freie Ports, abgestimmte Nachbarn, DEM-Innenfit/Terrain, Pfeiler/Statik, Gleishardware,
Fahrdynamik, drei Ebenen und Tunnelportale bleiben offen.

### Gemeinsamer Brückenquellplan (#21)

`shared_bridge_*` ersetzt alternative Paarprofile durch je eine gemeinsame Fläche
für Deck und bekannte Außenwege. Die neue portable Fixture erfasst die am häufigsten
gekreuzte Brückenquelle aus dem Paarpin, erweitert deren zusammenhängende Brücken-
Korridore und nimmt **alle positiven Flächenüberschneidungen** im vollständigen
Wien-Donau-Input hinzu. Quellachsen bleiben original; Flat-Cap-/Miter-Buffer mit
Mitergrenze2 und klassengeleiteter/Fallback4m-Breite sind markierte Rezepte.
Geometrieunion/CDT mit gemeinsamen Vertexidentitäten erhält die ganze Vereinigung;
Original-Quellen/Tags und zusätzliche Contributors je Face bleiben separat erhalten.

Sechs gleich geordnete Brückenteile und17 Außenwege werden gemeinsam geplant;
zehn Innenwege bleiben mit `indoor_surface_requires_buildings_contact` ungelöst.
Deren ursprüngliche Achsen/Tags/Korridore sind übergeben; `level` wird nicht in
Meter übersetzt. Die Durchfahrtsprüfung gilt ausschließlich gegen die expliziten
Außenflächen, nicht gegen unbekannte Innenetagen oder die gesamte Nachbarwelt.
Eine vollständige Bilanz enthält alle793 Achsquellen und44 Quellen ohne Achsenrezept.

Ground benutzt den gemeinsamen #65-Höhenfit mit isotropem Gradientenlimit0,025,
höchstens2m Abweichung vom gemittelten linearen Quell-Endprior und2s Solverbudget.
Querneigung ist hier frei innerhalb dieser Schranke, keine feste0,015-Vorgabe.
Konus und konservatives16-Eck-LP werden mit denselben Eingaben/Qualitätsbedingungen
verglichen; die LP besitzt eine kleinere zulässige Menge. Das gemeinsame flache
Deck wahrt0,35m Dicke und4,5m Durchfahrt plus0,02m Numerikreserve über der vollständigen
Flächenüberlagerung. Jeder Deckport darf gegenüber dem DEM-Prior höchstens8m steigen.
Mit festem DEM-Port bleibt das Deck lokal aus; Ground bleibt gültig. DEM ist Gelände,
kein gemessener Brückenanschluss. Die angehobenen Außenanschlüsse benötigen künftig
passende Nachbarrezepte; die flache Deckfläche allein beweist keine Gleisfahrkurven.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/shared_bridge_checks.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/shared_bridge_experiment.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/shared_bridge_showcase.py
```

Normale Prüfungen lesen nur Pins. `capture_shared_bridge.py` ersetzt die Fixture
**ausdrücklich** aus vorbereitetem Input und vorhandenen DEM-Bytes, ohne Netzabruf.
Ausgaben unter `build/experiments/transport/shared-bridge/`; drei frische Worker je
Methode mit fünf vollständigen Builds nach einem Warmup, CPU10s/AS2GiB/Parent30s,
ein BLAS-Thread. Geometrie/Fit/Ganzflächenabstand/Ports mitmessen; Imports/JSON/Warmup/
Produkte/Render außerhalb der Zeit, im Linux-VmHWM enthalten. GEOS-/Solver-Schritte
können kooperative Limits überlaufen; die Prozessgrenzen begrenzen den ganzen Worker.
Max64 Quellen,256 Achspunkte/Quelle,8192 Vertices/Faces je Fläche; keine Frame-Abnahme.

Sechs1920×1080-Vergleiche zeigen die **tatsächlich ausgelassene** feste-Port-Brücke
gegen das gemeinsame angehobene Deck: Kontrolle, Ganzmodell, Durchfahrtdetail.
Gleiche Kamera aus beiden Zuständen, keine Höhenüberhöhung. Bestehende Plan-/Reservation-
Verträge mit Source/Kind/Frame im Begleitprodukt; Prismen bleiben konservativ.
Keine neuen lib-/nativen APIs, SDK-Abhängigkeit, Terrain/Pfeiler/Statik/Schienenhardware,
außen verbundenen Verkehrswege, vollständigen Innenkontakte oder Dreiebenen-/Tunnelrezepte.
SIGGRAPH2008 graph→geometry und Galin2010 Figuren17/18 (erneut optisch geprüft)
begründen getrennte Netz-/Profil-/Querschnittsarbeit; keine vollständige Nachbildung.

### Echte westliche Schienenanschlüsse (#115)

`bridge_approach_*` ergänzt das #112-Deck um zwei echte Brücken-/Ground-Schienenpaare
in der westlichen Nachbarkachel. Die südliche Kachel dient der Quell-/Randprüfung;
ihre fortlaufenden Brückenteile sind in diesem Versuch noch nicht angebunden.
Gleicher OpenFreeMap-Snapshot und gleicher48,21/16,37-Ursprung. Positive vollständige
Decküberlappung plus eindeutiger exakter Subway-Endkontakt wählen die westlichen
Paare; tilelokale Feature-Nummern allein begründen keine Quellidentität/Verbindung.
Originale Pufferachsen/Tags/IDs bleiben erhalten; vereinigte Flächen erscheinen einmal.

Ein gemeinsames skalares C1-Höhenfeld `z=h(dot(XY-origin,unit_axis))` verbindet beide
Spuren. Die Rampen beginnen früher im Brückenstück; die235/238m Ground-Stücke allein
reichen nicht. Bestehendes Deck und vollständige bekannte Unterfahrflächen bleiben
im konstanten Feldteil geschützt. Die beiden fernen DEM-Prioren werden ausdrücklich
um je0,08984375m auf ihren gemeinsamen Mittelwert angepasst, Grenze0,1m.
Dieser Kontakt ist ein Rezeptanker, keine Freigabe des anschließenden Tunnelportals.
Analytisches Längsfeld:0,024 Neigung und0,002/m zweite Ableitung; keine geglättete
horizontale Schienenachse. Ganze Mesh-Gradienten müssen unter0,025 bleiben.

`projected_profile_surface.py` schneidet die gesamte vereinigte Domäne an Höhen-
Chordstationen und trianguliert die Streifen konform. Jede Face liegt in einem
linearen Chord; `max|h''|*step²/8` begrenzt jetzt die **ganze Fläche**, nicht nur
Achsenproben.1cm Fehlerbudget plus1e-7m numerischer Guard, unveränderte Rootkontakte.
`adaptive_profile_surface.py` ist die faire Alternative: gemeinsame flache Kontakt-
regionen zuerst schneiden, dann projizierte Maximalspannen per geteiltem Edge-Mittel
verfeinern. Der baryzentrische Fehler ist höchstens `max|h''|*Var(t)/2`, daher
`max|h''|*(max(t)-min(t))²/8`. Kleine Höhenfehler sichern noch keine Facettenneigung;
deren zusätzliche Ganzflächenprüfung und die exakten Kontaktregionen sind verbindlich.
Beide Profile/parabolisch-kubisch und beide Geometrieverfahren erfüllen denselben Vertrag.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/bridge_approach_checks.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/bridge_approach_experiment.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/bridge_approach_showcase.py
```

Ausgaben: `build/experiments/transport/bridge-approaches/`. Normal nur Pins lesen;
`capture_bridge_approaches.py` ist ausdrücklich neu erzeugende Offline-Erfassung aus
vorbereiteten Nachbarinputs und vorhandenen DEM-Bytes.3142 Achsquellen/117 Flächen-
quellen erhalten eine vollständige Bilanz;27 bekannte Quellen verwendet,17 überlappende
Innen-/Tunnel-/Etagenquellen bleiben begründet ungelöst. Buildings hat die zehn alten
Innenkontakte ausdrücklich als `buildings_floor_surface_not_planned` abgelehnt (#111).
Kein unbekannter Innenweg wird auf DEM/Nullhöhe oder auf das höchste Deck gesetzt.

Drei frische Worker je Profil/Geometrie, fünf vollständige Anschlussbuilds nach
Warmup; vorbereitete Root-/Unterflächen und Imports/JSON außerhalb der Zeit, im
Linux-VmHWM enthalten. CPU10s/AS2GiB/Parent30s, ein BLAS-Thread;8192 Faces/Vertices,
4096 Höhenstationen,16 adaptive Iterationen und2s kooperative Geometriephase.
GEOS-Schritte können Phasenlimits überlaufen; Prozessgrenzen begrenzen den ganzen Worker.
Sechs1920×1080-Bilder vergleichen den tatsächlichen bisherigen Rootstand mit beiden
Höhenprofilen im Ganzmodell und am realen Anschluss, ohne Höhenüberhöhung. Weitere
Kachelränder/Portale/Innenflächen, Terrain, Pfeiler/Statik/Gleishardware, Fahrzeughüllen,
volle Verkehrswege und native Integration bleiben offen. Keine lib- oder SDK-Runtime-Änderung.
Galin2010 Profil-/Querschnittstrennung und SIGGRAPH2008 graph→geometry sind die Basis;
das projizierte Feld und seine Flächenfehlergrenze sind eigene überprüfte Konstruktionen.

### Bibliotheksübernahme der Höhenflächen (#122)

Der Stationsschnittkern aus #119 liegt jetzt unter `outshine_ref.geometry` als
`ProjectedSurfaceBudget`/`projected_height_surface`; der vollständige C1-Profiladapter
heißt `transport.mesh_projected_profile`. `bridge_approach_recipe.py` importiert
diesen Kern. Die frühere direkte Tabellenfunktion ist nur ein kompatibler Import-
wrapper; die Mittelpunktalternative nutzt ebenfalls die begrenzten Kontaktregionen.
Gemeinsame Plan-/Source-/Kind-Verträge bleiben beim Aufrufer unverändert.

Default:2s kooperativ,16384 Randkoordinaten,4096 Stationen,8192 Vertices/Faces,
65536 Vereinigungspaare und1048576 Interfacekandidaten. Kandidaten werden vor
Allokation gezählt bzw. mit beschränktem maximalem Queryoutput gebatcht.
Budgetende/numerisch ungültige Fläche liefert kein unvollständiges Zertifikat.
Falsche Typen/Werte sind API-Fehler. Die ganze Höhe ist zum analytischen Feld
zertifiziert; die erzeugten Facetten bleiben C0. Gleiche echte Eingaben und
Globalflächen-/Budgetgegenfälle, Wheelimport und unveränderte Bilder/Kosten
werden geprüft. Bibliotheksübernahme allein ist keine native Weltabnahme.


## Schienenfeld über den Kachelrand (#128)

`capture_bridge_continuations.py` liest dieselben drei vollständigen, gehashten
20260927-OpenFreeMap-Eingaben wie der Anschlussversuch. Explizite Aufnahme mit
`--write-fixture`; normale Prüfungen/Experimente verändern keine Pins.
Fixture `bridge-continuations-vienna.json`, SHA256
`fcc91443d76d69223c8173061cf1f2f69d78446a9cc26fd58d9482c3e4f68db4`:
19 kompatible positiv überlappende Layer-1-Subway-Brücken, zwei bestehende Zufahrten,
48 bekannte untere Außenflächen und 37 lokale ungelöste Quellen.
Alle 3259 Quell-IDs/Achsenflächen der drei Eingaben werden bilanziert; 69 verwendet.
Die komplette Original-MVT-Aufbereitung bleibt in den bisherigen Eingaben erhalten.

Das ursprüngliche westliche C1-Höhenfeld bleibt erhalten. Ein disjunkter östlicher
flacher C1-Übergang auf derselben Projektionsachse sichert die zusätzlichen bekannten
Durchfahrten: 4,52 m mit 0,35 m Deckdicke und 2 cm Reserve. Die benötigte Anhebung
wird über ganze affine Flächenschnitte bestimmt; mindestens erforderliche Profillänge
unter 2,4 % Grad und 0,002 1/m zweiter Ableitung. Anhebung über 8 m oder fehlender
Raum liefert den vorher unabhängig geprüften Plan statt ungültiger neuer Geometrie.
Diese Länge ist nur für dieses Höhen-/Grenzrezept minimal, keine Netzoptimalität.
Kachel-Clips haben keinen gewählten Bodenkontakt; deren DEM-Werte sind weiche
Geländeprioren. Die frühere 8-m-Priorgrenze des kleinen Deckrezepts wird neuen
internen Brückenclips nicht als Bodenanker aufgezwungen. Alte Kontakte bleiben erhalten.

`bridge_continuation_field.py` komponiert die beiden installierbaren Profilrezepte;
`geometry.projected_height_surface` erzeugt das komplette Stationsmesh.
`retained_surface.py` erweitert den alten Boden über dessen Exterior, übernimmt
ursprüngliche XY-Vertices vorrangig und fixiert alle alten affinen Höhenkontakte.
Nur nach numerischem Schweißen/Splitten exakt flächenlose Faces werden entfernt;
anschließend volle konforme Coverage, Flächenfehler und Hausdorff ≤1e-8 m geprüft.
Die Bibliotheks-Triangulation bleibt unverändert und lehnt numerisch kollabierte
Eingaben weiterhin ab. Neuer Boden bleibt unter 2,5 % und höchstens 2 m Prioränderung.
Vollständige obere Neutriangulation und erhaltende Erweiterung werden bei denselben
Flächen, Profilen, Fehler-/Neigungs-/Kontakt-/Freiraumanforderungen verglichen.
Maximal 128 gewählte Quellen, 64 Brücken, 64 untere Quellen, 8192 Vertices/Faces
je Produkt; weitere Geometrie-/Stations-/Paargrenzen über `ProjectedSurfaceBudget`.
GEOS-/Solverzeit kooperativ, harte Kinder CPU15s/AS2GiB und Parent40s.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/bridge_continuation_checks.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/bridge_continuation_experiment.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/bridge_continuation_showcase.py
```

Ausgaben: `build/experiments/transport/bridge-continuations/`. Drei frische Worker
je Methode, fünf Samples nach Warmup; alter Root-/Anschlussaufbau außerhalb der
Erweiterungszeit, im Peak enthalten. Neun 1920×1080-Bilder, jeweils gleiche Kameras.
Analytisches XY-Höhenfeld C1, endliche Faces C0; Originalachsen bleiben ungeglättet.
Keine Fahrkurven-/Gleishardware-/Terrain-/Wasser-/Pfeiler-/Innenraum-/Tunnel-/Weltabnahme.
Vorhandene Plan/Reservation-Verträge unverändert; Source/Kind/Frame explizit im Produkt.
Grundlage: bestehende SIGGRAPH-Graph/Geometrie-Trennung und die geprüften Profil-/CAD-
Bausteine aus #96/#119/#125; keine SDK-Abhängigkeit und keine neue Fremdcodekopie.


## Gewählte U-Bahn-Portale am vorhandenen Gleisplan (#139)

`capture_tunnel_portals.py --write-fixture` liest zwei echte Tunnelachsen aus der
bereits gepinnten Westkachel. Ihre Endpunkte schließen exakt an die bisherigen
Erdgleise an. Gewählte Kontaktflächen liefert der bestehende Höhenplan; entfernte
Quellclips sind keine Geländekontakte. Originale, Quell-IDs, Attribute und die
ungeplanten Innenetagen bleiben erhalten. Die explizite Breite von 4 m ist eine
markierte Rezeptannahme, die Innenhöhe 4,5 m ebenfalls.

SIGGRAPH 2008, Seite 2/Figure 2, trennt Graph und Geometrie. Galin et al. 2010,
Seite 8/Figures 17–18, ergänzt parametrische Querschnitte und segmentiert die
Trajektorie gegenüber dem Gelände. Beide PDF-Seiten wurden optisch geöffnet.
Wir nutzen die vorhandene Geometrie und eigene Bibliotheksoperatoren; eine
gewählte grobe C0-DEM-Fläche bildet die Deckungsreferenz. Der C1-Höhenübergang
beginnt flach auf dem tatsächlich gewählten alten Boden und endet auf einem
plausibel gewählten Untergrundniveau. Nach einer offenen Absenkung beginnt die
Auskleidung; Roof-/Innenraumabstände werden über vollständige Schnittpolygone
geprüft. Geschlossene Komponenten und offene Luftquerschnitte an den entfernten
Clips werden getrennt geprüft. Endliche Faces bleiben C0.

`tunnel_portal_recipe.py` erhält alle alten affinen Flächen und liefert bei einem
Budget-/Semantikfehler den gültigen bisherigen Plan. Konstante Dach-/Wandflächen
verwenden das geprüfte CDT-Verfahren mit Flächen-/Randnachweis; unnötige
Höhenchords entfallen. Sehr kleine GEOS-Restdreiecke werden beim numerischen
Zusammenführen wie beim früheren erhaltenden Adapter ausdrücklich entfernt,
ohne die strenge Projektionsbibliothek zu verändern. Die Referenzdecke,
Auskleidung und das Gelände sind keine vermessene Rekonstruktion oder statische
Freigabe. Raumkurven/Fahrbarkeit, Schienenhardware, globale Kontakte und native
Echtzeitqualität bleiben offen.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/tunnel_portal_checks.py
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error test/experiments/transport/tunnel_portal_experiment.py
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error test/experiments/transport/tunnel_portal_showcase.py
```

`tunnel-portals/` unter `build/experiments/transport/` enthält Produkte,
Nutzungsbilanz, frische Worker-Messungen und Körperansichten. Der kubische
Übergang ist ein einzelnes Polynom mit geringerem Spitzengefälle; beide Verfahren
werden mit gleichen Kontakten, Achsen, Portalposition, Innenraum-/Deckungs- und
Ressourcenanforderungen verglichen. Kein allgemeiner Laufzeitvorsprung wird aus
kleinen Unterschieden behauptet. Die Showcase behält genau vier ganze
Kachelbilder; die separate Portalinspektion zeigt einen ausdrücklich markierten
Dach-/Geländeschnitt und eine 5-cm-Anzeigeplatte, kein Erdvolumen.

Räumliche Produkte verwenden bestehende Plan-/Reservation-Typen und metrische
Höhenintervalle. Quellenbindungen, Struktur-/Verkehrszellen, gewählte Terrainfläche
und Vorherplan werden gesondert dokumentiert. Keinen Tunnelgrundriss pauschal
gegen Gebäude/Freiräume abziehen; unbekannte Etagenkontakte aus #111 bleiben lokal
mit `buildings_floor_surface_not_planned` abgelehnt. Implementation/Prüfbelege
stehen im zugehörigen PR; #21 bleibt aktiv.


## Installierbare Ganzflächen-Höhenabstände (#146)

Brückenfreiheit, Portal-Innenraum/Deckung und Erhalt alter Flächen benutzen
`geometry.vertical_clearance` aus `lib/`. Die frühere parallele Experimentkopie
ist entfernt; elementare Abstandsfälle sind in `lib/tests/test_clearance.py`
gebündelt, physische Rezept-/Realquellenprüfungen bleiben getrennt. Unvollständige
Budgetläufe veröffentlichen kein vermeintliches Minimum. Zwei Flächen ohne
positive XY-Überlappung sind kein belegter Durchfahrtkontakt.

`height_gap_experiment.py` erzeugt einen portablen Korpus tatsächlicher
Brücken-/Portal-Flächenpaare und vergleicht identische Buffer mit dem eigenen
bisherigen Git-Stand e43a3fd73. Dessen Source-SHA256 ist explizit gepinnt; eine
Kopie unter `build/experiments/transport/height-gap-library/` dient nur dem
Vergleich und gehört nicht zur Runtime. Keine Änderungen an normalen Pins.
Frische Worker messen vollständige Abfragen und Peak-RSS getrennt von
Eingabevorbereitung/Importen; Paket-/Wheelprüfung und die geöffneten
Kachel-/Portalprodukte folgen der Bibliotheksübernahme.

```sh
PYTHONPATH=lib/src "$TRANSPORT_PY" -W error -m unittest discover -s lib/tests -p test_clearance.py
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" -W error test/experiments/transport/height_gap_experiment.py
```

## Whole-tile component recovery (#153)

`transport_component_surfaces.assemble` first uses the existing common surface
builder. A valid result is retained unchanged; it is not unnecessarily rebuilt
per component. After a kernel/geometry or output-limit failure, the physical band
union after curb refinement identifies independent parts. Bounded local attempts
keep valid complete parts and give each omitted source a specific reason.
Axis connectivity alone is insufficient: wide parallel bands can physically meet.
Recovery cuts/meshes consume the already planned physical domain; rebuilding
curbs per component was rejected after a Hong Kong domain mismatch.
Whole-tile inspection declares its 262144-face preparation budget, matching the
assembler output cap; SurfaceSampler and the older ground inspection retain
their 65536-face default. Existing numeric tolerances and hard process limits remain.
Clipped inspection vertices use batches of at most 4096 queries, with one unchanged
1000000-candidate budget across all batches; no sampler limit is raised for denser
tiles. Any unresolved contact still rejects the inspection mesh.
`original_edges` and `source_indices` bind unchanged direction/access masks to the
original prepared graph. This does not certify drivable turns or physical profiles.

Input edges/coordinates, local calls/edges, total output faces and cooperative time
are bounded. A geometry call can overrun cooperative time; hard CPU/address-space
and wall limits belong to the isolated experiment. No unknown minimum/partial
component is published as valid. Shared Plan/Reservation/HeightInterval contracts
remain unchanged. This is an experiment; further library migration remains #50.

`component_surface_experiment.py` compares the monolithic, serial-component and
fast-with-recovery methods on the same complete captured inputs and recipe masks.
Eight whole source tiles cover Vienna, Flensburg, Central Park, Tokyo, Hong Kong,
Feldkirch, Cape Town and Sydney. New acquired bytes are separately recorded in
`build/experiments/transport/components/inventory.json`; historical pins and cache
files are retained. The offline inputs contain every original transportation
feature, part ID, attribute and deferred polygon reason. Source transport bodies
use a declared flat Z=0/depth0.25 m recipe, not DEM or a complete city world.

When captured widths are absent, `source_roads` applies the existing recipe for
the normalized class (`minor` becomes `residential`; `path`/`transit` use their
subclass). Explicit source widths and axis recipes retain precedence. These
widths are procedural assumptions, counted in `normalization.inferred_widths`;
unknown classes, route-only features, other levels and indoor sources keep their
own reasons. Original coordinates, tags and source IDs are retained. Historical
input/product pins are not rewritten by this correction (#183).

```sh
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" -W error \
  test/experiments/transport/width_recipe_checks.py
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/transport \
  "$TRANSPORT_PY" -W error test/experiments/transport/component_surface_checks.py
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/transport \
  "$TRANSPORT_PY" test/experiments/transport/component_surface_experiment.py --data-root "$TRANSPORT_WORK/components"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" \
  test/experiments/transport/component_surface_showcase.py
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" \
  test/experiments/transport/whole_tile_showcase.py
```

SIGGRAPH2008 Fig.2/page2 was inspected visually: retained graph and 3D geometry
are separate stages, not a reproduced full paper algorithm. Source acquisition
uses the [documented offline request policy](https://openfreemap.org/quick_start/)
and a unique User-Agent. Provider credits remain visible in the four-image page.

Der eingefrorene Vier-Orte-/Instanzvergleich zu #250 steht in
[rail_hardware_comparison.md](rail_hardware_comparison.md).

## Gemeinsame Höhen der ergänzten Kamera-Netze (#265)

`camera_height_experiment.py` vergleicht den vorhandenen Minimax-Konusfit mit
flächengewichteter quadratischer Passung unter denselben ganzen anisotropen
Face-Neigungsbedingungen. Eingaben sind die gepinnten Kamera-Meshes und dieselben
retained z11-DEM-Bytes, keine neue Akquisition. Alle Bettfacetten unter vollständigen
geprüften Schienen-/Schwellenfootprints bleiben auf ihrem bisherigen Datum fest.
Die quadratische Passung importiert den installierbaren Kern
`outshine_ref.transport.weighted_heights.fit_weighted_heights`; Gewichtung,
Besitz, horizontale Referenz und Budget stehen in
[lib/weighted_heights.md](../../../lib/weighted_heights.md).
Der bisherige Minimax-Einstieg bleibt als Vergleich erhalten.

Bei der quadratischen Variante werden feste Kontakte vor einer konservativen
Rücknahme exakt projiziert. Für das flache zulässige Referenzbett d gilt
`gradient(d + alpha*(z-d)) = alpha*gradient(z)`. Mit gemessenem maximalem
normalisiertem Face-Gradient M wird `alpha=min(1,(1-1e-5)/M)` gewählt.
Eine unabhängige ganze Face-/Kontaktprüfung mit unveränderter Toleranz folgt;
dies ist ein zulässiger Kandidat und kein Optimalitätsnachweis. Unzulässige
Minimax-Ergebnisse bleiben ohne Mesh und erhalten das bisherige gültige Bett.

```sh
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" -W error -m unittest camera_height_checks
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" -W error test/experiments/transport/camera_height_experiment.py --prepare --camera-products "$TRANSPORT_CAMERA_INPUTS" --rail-products "$TRANSPORT_RAIL_PRODUCTS" --dem-sources "$TRANSPORT_DEM_SOURCES" --output "$TRANSPORT_HEIGHT_INPUTS"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" -W error test/experiments/transport/camera_height_experiment.py --inputs "$TRANSPORT_HEIGHT_INPUTS" --place Feldkirch --mode least_squares --output "$TRANSPORT_HEIGHT_RESULT"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" -W error test/experiments/transport/camera_height_verify.py --inputs "$TRANSPORT_HEIGHT_INPUTS" --result "$TRANSPORT_HEIGHT_RESULT" --rails "$TRANSPORT_RAIL_PRODUCTS" --place Feldkirch --mode least_squares --output "$TRANSPORT_HEIGHT_RESULT/verification.json"
```

Alle Ausgabevariablen benennen frische externe Temp-Pfade. Je Variante/Ort ein
frischer Worker, Solverlimit 8 s plus gemessene Vorbereitung/Schrittüberlauf,
Adressraum 2 GiB und 60 CPU-s. Die unabhängige Prüfung rekonstruiert alle tatsächlich
unverändert hohen Bettfacetten und verlangt vollständige Footprint-Abdeckung;
Vertexkontakte allein reichen nicht. Änderungen an fester Kamera/Galerie bleiben
an den vollständigen sauberen Replay und geöffneten Vorher/Nachher-Vergleich
gebunden. Quelle/XY/Indices, alte C1-/Flat-Fenster und Hardware bleiben erhalten;
C1-Netzprofile, Ebenen, geneigte Gleise und native Integration bleiben offen.

`transport_place_shots.py --height-products "$TRANSPORT_HEIGHT_RUN"` konsumiert
die sauberen unabhängigen Ergebnisse unter
`<Place>-least_squares/<Place>-least_squares.{json,npz}` mit
`verification.json` und den vorbereiteten Eingaben unter `inputs/`.
Dazu gehören dieselben `--camera-products` und `--rail-products` wie im Fit.
Nur neue Kamera-Fenster erhalten ihre geprüften Höhen; unveränderte alte
Wiener/Central-Park-Produkte bleiben erhalten. Ein explizit ungültiger Fit
behält das vorherige Bett mit Grund; schmutzige oder fremd gebundene Ergebnisse
werden nicht dargestellt. Der Standard ohne `--height-products` bleibt eben.

Aktueller eingefrorener Vergleich, Grenzen, Kosten und Bildbelege:
[camera_height_comparison.md](camera_height_comparison.md).

Bibliotheksübernahme und aktuelle Source-/Wheel-/Kosten-/Bildprüfung:
[weighted_height_library_comparison.md](weighted_height_library_comparison.md).
