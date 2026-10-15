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

Eingaben und Messergebnisse liegen unter `build/experiments/transport/`;
ausgewählte Bilder unter `build/shots/experiments/transport/`, Logs im System-Temp.
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

## Lokaler Showcase

Wie bei Buildings (8766) und Vegetation (8765) veröffentlicht der Transport-Showcase
einen festen Bildstand. Port 8767 zeigt genau vier ganze Quellkacheln: Wien mit
unabhängigem/gemeinsamem Boden vorher/nachher sowie die ebenen Netze von Tokyo
und Central Park. Kurze Bildunterschriften, Bildlinks und manuelles Aktualisieren bilden die
gesamte Oberfläche. Fehlende Nachweise bleiben sichtbar unvollständig.

```sh
.venv/bin/python -m pip install numpy scipy 'shapely>=2.1' matplotlib pillow
PYTHONPATH=lib/src .venv/bin/python test/experiments/transport/showcase.py --render-tiles --serve
```

`whole_tile_showcase.py` verwendet hashgeprüfte Generatorprodukte aus den unten
beschriebenen Boden- und Komponentenexperimenten. Das erste Bild zeigt ausdrücklich die
bekannte frühere Überdeckung durch unabhängiges Gelände als Vorher-Diagnose;
die anderen drei Bilder verwenden gültige Teilpläne. Die
gespeicherten Eingaben/Produkte müssen zuvor vorhanden sein. Originale
Erzeugungscommits bleiben im Bildreport erhalten; der Rendercommit steht in
`origin.json`. Es wird keine erneute Generatorausführung vorgetäuscht.
GEOS ist als Systembibliothek erforderlich (`libgeos-dev` unter Debian).
Der Solver importiert die installierte Referenzbibliothek: bei vorhandenem `lib/`
einmal `.venv/bin/python -m pip install -e ./lib` ausführen.
Erzeugte Bilder, Produkte und Messreports liegen unter
`build/experiments/transport/whole-tiles/`, die veröffentlichte Website unter
`build/experiments/transport/showcase/site/`. Der Server erzeugt keine Geometrie bei Anfragen.
Blender Workbench rendert frisch mit Fehler-Exitcode und prüft alle vier vollständigen
1920×1080-PNGs. Kachelgrenzen kommen aus den gepinnten Quelldaten; Zuschnitte
erhalten Flächendeckung und Vertexhöhen. Dünne Kachelrahmen sind Anzeigehilfen,
kein Gelände. Es gibt keine Höhenübertreibung. Tokyo und Central Park verwenden
das deklarierte Z=0-Rezept ohne DEM-Höhenfit. Nicht verwendete Quellen behalten
ihre Gründe im ursprünglichen Plan.

```sh
.venv/bin/python test/experiments/transport/showcase.py --publish
.venv/bin/python test/experiments/transport/showcase.py --serve --host 0.0.0.0 --port 8767
.venv/bin/python -W error test/experiments/transport/showcase_checks.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/whole_tile_checks.py
```

`--catalog` wählt einen anderen Katalog mit genau vier Bildplätzen. Jeder Eintrag nennt Bild, Messreport und
Erzeugungsnachweis unter `build/`. `origin.json` enthält den Erzeugungscommit,
`working_tree_dirty` und unter `records` die SHA-256-Werte von Bild und Report,
indiziert durch den Report-Dateinamen ohne Erweiterung. Nur übereinstimmende lesbare
PNGs werden veröffentlicht. Synthetische Flächenprüfungen belegen keine glatten
Fahrkurven, vollständige Befahrbarkeit, Höhen oder native Echtzeitqualität.
Der ursprüngliche Serverauftrag steht in [#49](https://github.com/Weichwolf/outshine/issues/49),
die minimale Gesamtansicht in [#135](https://github.com/Weichwolf/outshine/issues/135).

## Geschlossene 3D-Körper und gespeicherte Wiener Quellkachel

`transport.close_surface` aus `lib/` schließt bereits konforme Fahrflächen mit
expliziten Vertexhöhen. Obere Anschlussvertices bleiben gemeinsam, Innenlöcher
offen; Boden und Randwände bilden einen gerichteten Körper. Das ist ein
Geometrieverfahren, kein Höhen-/Verkehrsnetzsolver. Der Vergleich prüft dieselben
Körperbuffer, Kanten und Volumina für NumPy-Sortierung und eine Dictionary-Referenz.
Frische Worker messen sieben Aufbauzeiten und ihren vollständigen Peak-RSS.

```sh
.venv/bin/python -m pip install -r test/experiments/transport/requirements.txt
.venv/bin/python -m pip install -e ./lib
.venv/bin/python test/experiments/transport/capture_transport_tile.py
.venv/bin/python test/experiments/transport/transport_flat_network.py --input build/experiments/transport/inputs --output build/experiments/transport/flat-vienna Vienna
.venv/bin/python test/experiments/transport/transport_flat_surface_network.py --input build/experiments/transport/flat-vienna --output build/experiments/transport/surface-vienna-cuts --junction-mode cuts Vienna
.venv/bin/python test/experiments/transport/surface_solid_benchmark.py
.venv/bin/python test/experiments/transport/surface_solid_showcase.py
.venv/bin/python test/experiments/transport/showcase.py --publish
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
.venv/bin/python -m pip install -e './lib[transport]'
.venv/bin/python test/experiments/transport/capture_height_targets.py
.venv/bin/python test/experiments/transport/surface_height_frames.py
.venv/bin/python -W error test/experiments/transport/height_source_checks.py
.venv/bin/python -W error test/experiments/transport/surface_height_checks.py
.venv/bin/python test/experiments/transport/surface_height_benchmark.py
.venv/bin/python test/experiments/transport/surface_height_showcase.py
.venv/bin/python test/experiments/transport/showcase.py --publish
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
.venv/bin/python -W error test/experiments/transport/ground_surface_checks.py
.venv/bin/python -W error test/experiments/transport/ground_surface_experiment.py --compare-geometry
.venv/bin/python -W error test/experiments/transport/ground_surface_experiment.py
.venv/bin/python -W error test/experiments/transport/surface_contact_benchmark.py
.venv/bin/python -W error test/experiments/transport/ground_surface_showcase.py
.venv/bin/python test/experiments/transport/showcase.py --publish
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
.venv/bin/python -W error test/experiments/transport/ground_refinement_checks.py
.venv/bin/python -W error test/experiments/transport/ground_refinement_experiment.py
.venv/bin/python -W error test/experiments/transport/ground_refinement_showcase.py
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
.venv/bin/python -W error test/experiments/transport/ground_reconnect_checks.py
.venv/bin/python -W error test/experiments/transport/ground_reconnect_experiment.py
.venv/bin/python -W error test/experiments/transport/ground_reconnect_showcase.py
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
.venv/bin/python -W error test/experiments/transport/esri_transport_catalog.py
.venv/bin/python -W error test/experiments/transport/esri_transport_ghidra.py
.venv/bin/python -W error test/experiments/transport/verify_esri_transport_ghidra.py
.venv/bin/python -W error test/experiments/transport/verify_esri_transport_oracle.py
.venv/bin/python -W error test/experiments/transport/esri_transport_comparison.py
.venv/bin/python -W error test/experiments/transport/esri_transport_showcase.py
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
.venv/bin/python -W error -m unittest discover -s lib/tests
.venv/bin/python -W error test/experiments/transport/surface_solid_checks.py
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
PYTHONPATH=lib/src .venv/bin/python -W error -m unittest discover -s lib/tests -p 'test_transport*.py'
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/vertical_transition_checks.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/vertical_transition_experiment.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/vertical_transition_benchmark.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/vertical_transition_showcase.py
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
PYTHONPATH=lib/src .venv/bin/python -W error -m unittest discover -s lib/tests -p test_endpoint_grades.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/endpoint_grade_checks.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/endpoint_grade_benchmark.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/endpoint_grade_render.py
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
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/overpass_checks.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/overpass_experiment.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/overpass_showcase.py
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
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/shared_bridge_checks.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/shared_bridge_experiment.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/shared_bridge_showcase.py
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
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/bridge_approach_checks.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/bridge_approach_experiment.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/bridge_approach_showcase.py
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
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/bridge_continuation_checks.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/bridge_continuation_experiment.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/bridge_continuation_showcase.py
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
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/tunnel_portal_checks.py
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -W error test/experiments/transport/tunnel_portal_experiment.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/tunnel_portal_showcase.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/showcase.py --render-tiles
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
PYTHONPATH=lib/src .venv/bin/python -W error -m unittest discover -s lib/tests -p test_clearance.py
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -W error test/experiments/transport/height_gap_experiment.py
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
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python -W error \
  test/experiments/transport/width_recipe_checks.py
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/transport \
  .venv/bin/python -W error test/experiments/transport/component_surface_checks.py
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/transport \
  .venv/bin/python test/experiments/transport/component_surface_experiment.py
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python \
  test/experiments/transport/component_surface_showcase.py
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python \
  test/experiments/transport/whole_tile_showcase.py
```

SIGGRAPH2008 Fig.2/page2 was inspected visually: retained graph and 3D geometry
are separate stages, not a reproduced full paper algorithm. Source acquisition
uses the [documented offline request policy](https://openfreemap.org/quick_start/)
and a unique User-Agent. Provider credits remain visible in the four-image page.
