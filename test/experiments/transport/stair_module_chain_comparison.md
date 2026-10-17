# Weitere Bodenmodulstufe mit tatsächlichem Seitenkontakt (#528)

Die neue ausdrücklich gewählte Familie aus
[#530](stair_southwest_comparison.md) ergänzt Straßen und ganze geneigte
Außenprofile. Die hier geprüfte örtlich flache Familie samt Gegenfällen bleibt erhalten.

Das [feste Rezept](fixtures/stair-west-module-chain-vienna.json) ergänzt den
ursprünglichen Radweg `1044461680:3` um 107,403 m. Der geprüfte
[36-Quellen-Bestand aus #522](stair_module_comparison.md) wird als Cache-Elternteil
gelesen. Alte Körper, Brücken, Treppen und 124 Pfeiler bleiben erhalten.
37 lokale Quellkandidaten und 64 ganze innere Endprofile sind damit geprüft;
zehn Außenprofile bleiben offen. 1620 weitere Original-Linienteile und alle
71 Flächen behalten ihre Originale, Attribute, IDs und Auslassungsgründe.
`admitted_source_ids=[]`: keine native Quellenfreigabe oder Weltabnahme.

## Tatsächlicher Grenzfall und Verfahren

Der alte Radweg `39881120:24` variiert insgesamt um mehr als einen Meter.
Sein ganzes äußeres Profil ist auf 100 nm eben. Die zusätzlich berührte
seitliche Fläche weicht aber um **112,095 µm** von dieser Ebene ab. Nur drei
Profilpunkte oder ein skalares Höhendatum würden den ganzen Kontakt verfehlen.
Der identische [Uniform-Gegenfall](fixtures/stair-module-chain-flat-countercase.json)
wird weiterhin abgewiesen, ohne Solver- oder Geometriegrenzen zu erhöhen.

Der neue Modus `piecewise` übernimmt die tatsächlichen einzelnen Höhen des
alten Bodenmeshs als feste Kontakte. Er konsumiert den schon installierbaren
`fit_surface_heights`-Kern mit den gleichen ganzen Neigungsnormen und
Zeit-/Iterationsgrenzen. Diese Variante minimiert maximale Höhenabweichung;
der bisherige gewichtete quadratische Kern mit gemeinsamem Datum bleibt im
Modus `flat` unverändert. Verschiedene Zielfunktionen sind kein fairer
Geschwindigkeitsvergleich; hier entscheidet die benötigte feste Kontaktmenge.

Ganze Dreiecks-/Linienschnitte prüfen die Höhen der eigentlichen Profile.
Die Extrema angrenzender Facetten dürfen eine geneigte Seitenberührung
enthalten, obwohl das geschnittene Profil eben ist. Aus tatsächlichen alten
Kappen entstehen stückweise lineare vertikale Kontaktstreifen. Beide Körper
müssen jeden Streifen vollständig und entgegengesetzt orientiert tragen.
Diese Kontakte werden weder durch bloße Nähe noch durch zwei Endpunkthöhen
ersetzt. Nicht auflösbare Zwischenknicke werden abgelehnt, nicht geglättet.

Die neue Float32-Seitenkontaktprüfung hat ausdrücklich einen anderen
Darstellungsvertrag als die unveränderte flache Rechteckfamilie:

- Die kanonische Double-XY-Ebene bleibt fest. XY-Wache aus tatsächlichen lokalen
  Vertexrundungen beider Körper plus 100 nm Konstruktion.
- Höhenwache: 100 nm plus lokale Referenzneigung mal XY-Wache; bei Float32
  zusätzlich die gemessenen lokalen Z-Rundungen beider Körper. Hier höchstens
  **0,100249 µm Double / 13,239 µm Float32**. Der erste flache Modus behält
  seine eigene bisherige 0,1-µm-Höhenwache.
- Ein auf einer langen alten Facette interpolierter Float32-Punkt und ein neu
  einzeln gerundeter Vertex sind nicht zwingend höhengleich. Das ist eine
  begründete Rundungshülle, kein exakter Nullspaltnachweis. Ein zusätzlicher
  0,1-mm-Höhenversatz fällt weiterhin durch die ganze Wandprüfung.

Der [echte geneigte Außenprofil-Gegenfall](fixtures/stair-module-chain-nonflat-profile-vienna.json)
am Fußweg `80922970:318` bleibt ablehnend: rund 24,1 mm Querhöhendifferenz
passen nicht zur derzeit noch örtlich ebenen Profilfamilie. Beide vollständigen
Ablehnungsprodukte enthalten nur Bericht, Quellbilanz und Readerbeleg; der alte
36er Verbund bleibt verwendbar. Kein verworfenes neues Mesh bleibt liegen.

## Cache- und Terrainumfang

`--cached-module OUTPUT FIXTURE` ist ein expliziter, wiederholbarer
Experimentparameter: höchstens vier geordnete unterschiedliche Eltern und
64 Quellen im abgeleiteten Besitzbitfeld. Das alte 32-Quellen-Planungsbudget
und das neue Acht-Quellen-Modulbudget bleiben unverändert. Keine alten Planer
laufen. Der Readerbeleg muss zum Elternbericht passen; Rezept, Ursprung,
Quellreihenfolge, vollständige Exportmenge, Elternkette, Körper und Darstellung
müssen durch ihre Hashes gebunden sein. Vier Gegenfälle prüfen veränderte
Körperbytes, fehlende Exporteinträge, falsches Rezept und fehlende Eltern.
Auch der neue 37er Stand wurde erfolgreich als zweite Cache-Elternstufe gelesen.

Alte Körper werden als unveränderte Komponenten zusammengetragen. Die neue
Besitzmetadatenansicht nutzt `uint64`; ursprüngliche Quelldateien und ihre
13 alten Körperarrays werden nicht verändert. Ein schon verfeinertes Terrain
darf die gleiche Bodenfläche anders triangulieren. Die tatsächlichen geprüften
Bodenkörper und Quellzuordnungen bleiben maßgeblich; bloß gleiche Facettenzahlen
sind kein Kopplungsbeweis. Kein gemeinsamer Cache-/Streaming-/Reservationsvertrag
und keine öffentliche Bibliotheks- oder native API geändert.

Nur 135 alte Terrainfacetten werden ersetzt. Ganze Randhöhen bleiben bytegenau,
alte Straßenbetten bleiben feste Referenzen. Keine der alten Pfeilerregionen ist
von dieser zweiten Ergänzung betroffen. Lokales und ganzes gespeichertes Feld
müssen exakt zusammenpassen; außerhalb bleiben 25035 XYZ-Dreiecke bytegleich.
Globale Diagnoseassembly und ganze Gegenprüfung bleiben zusätzlicher Aufwand,
kein Nachweis eines billigen Runtime-Cachetreffers.

## Prüfstand und Kosten

Geometrie-/Profilerstand `c184a5d66`; aktuelle Szenen und zwei vollständige
Gegenrezepte `dd3f4f9bb`. Danach nur Referenzdokumentation. Die Kamerakorrektur
verschiebt und erweitert die mittlere Sicht von 1450 auf 1650 m; sie schneidet
den westlichen Zusatz nicht mehr am linken Rand ab. Alle acht tatsächlichen
NPZ-Arraymengen des profilierten und aktuellen normalen Hauptlaufs sind
bytegleich. Das alte Profiling enthält die vorherige Kameraserialisierung.

- Neuer geschlossener Körper: 14 obere Vertices/16 Facetten, insgesamt
  28 Vertices/52 Facetten. Alle Double-/Float32-Faces bleiben positiv und
  geschlossen mit zusammenhängenden Vertexlinks. Volumen 64,441885/64,442704 m³;
  maximale normierte Quellneigung 0,310073/0,295074 bei unverändert 6 % Grenze.
- Drei ganze Wandkontakte/2,053835 m, zwei zusätzliche ganze innere Endprofile.
  Eigene neue Fläche 214,806283 m², ausdrücklich mitbenutzt 0,107514 m² der
  alten Quellkappe. Kein neuer projizierter Brückenkontakt; mindestens
  391,5168 m Abstand zu ganzen alten Pfeilerprojektionen.
- Lokales Terrain: 152 Punkte/247 Facetten, 125 feste Höhen, 55 exakte alte
  Randhöhen. Lokale Erdneigung Double/Float32 höchstens 0,12220; keine neue
  konstruktive Facette. Ganzes Feld 12820 Punkte/25282 Facetten, Diagnosekörper
  25640 Vertices/51276 Facetten. Bestehende globale Klassen-/Neigungs-/5-m-
  Regeln unverändert; alle 124 unteren Pfeilerkontakte bestehen, maximal
  weiterhin 15,170 µm Float32-Fehler. Kein Statik-/Fundamentnachweis.
- 70 betroffene Quellkontrollen und dieselben 70 gegen das unveränderte
  installierte Wheel bestanden; nach Kamerakorrektur 21 betroffene Kontrollen
  erneut. Ganze frische Erzeuger und gespeicherte Reader mit gesperrten alten
  Planungseinstiegen; Reader außerdem ohne CDT, LP/Kegelsolver, Meshbau oder
  Arrayexport. Vier vollständige Roh-DEM-Crops und **12988** ursprüngliche
  Ziel-/Achsenabfragen gegen vollständige originale WebP-Decodes bitgleich.
- Aktueller normaler Gesamtaufbau **15,405 s / 176865280 B Peak-RSS**;
  alternative ursprüngliche Höhenvariablen **15,469 s / 176312320 B**.
  Die Prozesse liefen parallel; kein belastbarer Laufzeitvorsprung behauptet.
  Gleiche lokale Eingabearrays außer Variablenmodus, gleiche Qualitätsgrenzen,
  beide vollständigen Felder unabhängig zugelassen. Die feste Substitution
  reduziert 170 auf 45 Solvervariablen, 611 auf 486 Zeilen, 911 auf 372
  A-Nichtnullen und 660 auf 87 P-Nichtnullen. Zusätzlich 5876 B gehaltene
  reduzierte Sparse-Matrizen und 4080 B explizite lineare Verschiebungsdaten.
- Ganzer Memray-1.20-Lauf nach Imports einschließlich Einlesen, Erzeugung,
  aller Gegenprüfungen, Katalog, Displayarrays, Export und Bericht:
  **82355130 Allokationsereignisse / 9120802382 B kumulativ angefordert /
  53988323 B verfolgter Heap-Peak / 283570176 B Prozess-Peak-RSS**.
  Instrumentierter Aufbau 99,834 s; PNG und unabhängiger Reader ausgeschlossen.
  Offizielle native Statistik in getrenntem begrenztem Prozess, beide terminal
  erfolgreich. Dessen eigener RSS-Peak ist nicht gemessen. Symbolkorrekturhinweise
  für malloc/free bleiben im Log; keine vollständige Stackklassifikation behauptet.
- Sieben Produkt-Arraymengen 3087726 B, Display zusätzlich 9720384 B;
  expliziter globaler XY-/Indexscratch 308448 B und Außenvergleich 3605040 B.
  Wand-Endprüfung 58416 Facettenprojektionen, gleicher zusätzlicher Umfang
  für Rundungsauswahl. Interne Gesamtkopien bleiben unbeobachtet. Angeforderte
  Bytes sind weder Live-Speicher noch eindeutige Daten oder kopierte Bytes.

Linux/WSL2, i5-1135G7, vier sichtbare logische CPUs, rund 15,44 GiB RAM;
Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0. CPU-Limit 120 s und
Adressraumlimit 3 GiB je Erzeuger/Reader; Solver 8 s/100 Iterationen unverändert.
Der Profiling-Lauf verbringt 86,045 s in der ganzen Weltgegenprüfung. Diese
Kosten dominieren weiterhin; keine Frame-, A18-, RTX- oder Ladeabnahme.

Vier neue, vollständig dekodierte und geöffnete Blender-Eevee-Bilder zeigen
ganzen Umfang und moderate Sicht, jeweils vor/nachher, **1280×720**.
llvmpipe/LLVM 19.1.7, 11,209–13,205 s je Bild, parallel zu Readern;
keine Geräte-FPS. Der zusätzliche Ast ist sichtbar, aber schmal und blass.
Das unvollständige Netz und die klinische Umgebung bleiben wesentliche
Bildlücken. Forschungsbilder bleiben privat; keine Ersatzgalerie oder Server.

## Reproduktion und Belege

Die lokalen SIGGRAPH-/Primärquellen und ihre optisch geprüften Abbildungen
stehen im [Vorgängervergleich](stair_module_comparison.md#quellen-und-reproduktion).
Die konkrete Eltern-/Kontaktbehandlung ist eigene konservative Forschung;
keine SDK-/Fremdcode- oder Runtime-Abhängigkeit. Die SDK-Operationsreferenz
aus #91 beweist keinen vollständigen OSM-Generator.

```sh
export PYTHONPATH=lib/src:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest stair_module_checks stair_module_chain_checks -q
python test/experiments/transport/stair_module_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-west-module-chain-vienna.json \
  --base "$TRANSPORT_BASE" --supports "$TRANSPORT_SUPPORTS" \
  --cached-module "$TRANSPORT_FIRST_MODULE" \
    test/experiments/transport/fixtures/stair-west-ground-module-vienna.json \
  --output "$TRANSPORT_NEXT_MODULE"
python test/experiments/transport/stair_module_verify.py \
  --fixture test/experiments/transport/fixtures/stair-west-module-chain-vienna.json \
  --base "$TRANSPORT_BASE" --supports "$TRANSPORT_SUPPORTS" \
  --cached-module "$TRANSPORT_FIRST_MODULE" \
    test/experiments/transport/fixtures/stair-west-ground-module-vienna.json \
  --output "$TRANSPORT_NEXT_MODULE"
```

`TRANSPORT_FIRST_MODULE` ist der ganze geprüfte #522-Export samt Readerbeleg.
Für beide Gegenrezepte nur Fixture und frischen Temp-Ausgabepfad wechseln;
für den Kostenvergleich `--variable-mode heights` wählen. Der vorhandene
`stair_module_cost.py` bietet das optionale native/Python-Memray-Profiling.
Die festen Kontaktinputs sind echte abgeschnittene Elternflächen samt
Herkunftshashes, keine vollständigen Weltcaches oder neue Ausgabeorakel.

Aktueller Hauptlauf/Bilder: `/tmp/outshine-transport-module-chain-full-dd3f4f9bb`;
alternative Höhen: entsprechender `chain-heights-dd3f4f9bb`-Pfad;
Profiling: `/tmp/outshine-transport-module-chain-profile-c184a5d66`;
Belege/aktueller nativer Trace: `/tmp/outshine-transport-module-chain-evidence-774fea5d4`.
Diese privaten Ausgaben sind keine Testabhängigkeit. Benötigte originale
PBF-/DEM-Eingaben, Elterncaches und Gegenfälle bleiben erhalten.
Allgemeine geneigte Profile, weitere Klassen/Ebenen, automatische Modulgraphen,
LOD-/Runtime-Integration und weltweite Abdeckung bleiben bei #434 offen.
