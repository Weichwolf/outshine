# Südwestlicher Straßenverbund an tatsächlichen Bestandsprofilen (#530)

Der [nachfolgende Kurvenvergleich](stair_road_curve_comparison.md) ergänzt eine
glatte horizontale Fahrgeometrie samt ganzen Fahrzeughüllen und erhält diese
Originalflächen. Die hier dokumentierte erste Straßenfassung bleibt als Vergleich.

Gegenüber der [zweiten Modulstufe](stair_module_chain_comparison.md) kommen
sechs ganze ursprüngliche Quellteile mit zusammen **795,443 m** hinzu.
Das bisher abgelehnte, um **24,104 mm** geneigte Außenprofil des Fußwegs
`80922970:318` wird vollständig angeschlossen. Ein ausdrücklicher Kontakt an
einem ursprünglichen Innenknoten des alten Radwegs schließt eine zweite Verbindung.
Aktuell **43 Quellen, 74 innere Endprofile, ein zusätzliches altes Innenquerprofil
und zwölf Außenprofile**. `admitted_source_ids=[]`: geprüfte Python-Forschung,
keine native Weltabnahme oder vollständige Quellabdeckung. Besitzer:
[#530](https://github.com/Weichwolf/outshine/issues/530),
[#434](https://github.com/Weichwolf/outshine/issues/434).

## Originale und prozedurale Profilwahl

[Rezept](fixtures/stair-southwest-streets-vienna.json), alle IDs mit Präfix
`openfreemap:14/8937/5678:transportation:`:

| Originalteil | Originalklasse | Länge, m | Richtungsangabe |
|---|---|---:|---|
| `249866752:0` | minor | 250,492 | oneway=1 |
| `11842935062:0` | minor | 17,739 | oneway=1 |
| `208163352:0` | minor | 109,220 | oneway=1 |
| `103612620:19` | minor | 392,938 | fehlt |
| `80922970:320` | footway | 19,408 | originale Attribute erhalten |
| `1044461680:4` | cycleway | 5,647 | foot=no erhalten |

Vier `minor`-Quellen ausdrücklich an das vorhandene prozedurale `unclassified`-
Rezept gebunden: **5,5 m Breite, zwei Fahrstreifen à 2,75 m, höchstens 10 %
Längsneigung/3 % Querneigung**, 4,5 m Rezeptfreiraum/13,89 m/s Rezeptgeschwindigkeit.
Bei oneway beide Streifen vorwärts; ohne Richtungsangabe je einer pro Richtung.
Diese Maße und die fehlende Richtung sind gekennzeichnete Annahmen, keine
Messung oder gesetzliche Verkehrsfreigabe. Originalklasse, Achsen, Tags und
IDs bleiben erhalten. Keine automatische Umklassifikation aller minor-Quellen.

Versionierter 218795-B-PBF, OpenFreeMap-Snapshot `20260927_080001_pt`, SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`;
Frame und vier Mapterhorn-Crops wie im Vorgängervergleich. Ganze Crops gegen
vollständige originale WebP-Decodes sowie **14174** Welt-/Lokal-/Modul-/
Originalachsenabfragen bitgenau geprüft. MVT-Knoten beweisen keine genauen
OSM-Knotenkennungen.

## Verfahren und verworfene Alternativen

`stair_module_trace.py` schneidet ganze Querprofile mit tatsächlichen Dreiecken.
Alle stückweise linearen Intervalle und Zwischenknicke werden verglichen;
drei gleich hohe Proben allein reichen nicht. Gegenfall: 1-mm-Zwischenknick
bei drei unveränderten Proben. Maximal 4096 Teilstücke/65536 Teilstücktests je
Profil; konservativ mehrdeutige Überlagerungen können abgelehnt werden.
`stair_module_loops.py` bindet den neuen Radweg am ursprünglichen Innenknoten
`[2446,3763]` von `39881120:24`, mit tatsächlicher gespeicherter Achse/Breite.
Alle fremden Flächenwiederverwendungen brauchen ausdrückliche Quellbindung.
Die vollständigen Gegenrezepte für
[fehlenden Innenkontakt](fixtures/stair-southwest-undeclared-loop.json) und
[unpassende örtlich flache Familie](fixtures/stair-southwest-flat-profile-countercase.json)
werden mit konkretem Grund abgewiesen. Jeweils bleiben die 37 alten Quellen
verwendbar; nur Bericht, Quellbilanz und Readerbeleg werden geschrieben.

Straßen erhalten lokale Originalachs-Tangenten und anisotrope Neigungsrahmen;
Mehrfachbesitz konservative gemeinsame Grenzen. Tatsächliche Double-/Float32-
Facetten zusätzlich gegen unabhängige nominelle Längs-/Querrahmen geprüft:
größter normalisierter Wert **0,921463**, größte Querneigung **2,066753 %**.
Vorhandene Sicherheitsreserven und Grenzen unverändert.

Der erste Gesamtversuch `ae89c9226` wurde trotz gültiger Double-Fläche abgelehnt:
ein dünnes lokales Terrain-Dreieck faltete nach Float32-Rundung um. Der
[feste Gegenfall](fixtures/stair-southwest-float32-fold.json) erhält tatsächliche
Koordinaten und beide Vorzeichen. Alte kreisförmige Maskenschnitte waren für
die neuen ganzen Profile unnötig. Ohne sie bestanden Straßen dennoch zunächst
nicht: eine winzige freie Kreuzungsfacette überschritt den Float32-Rahmen auf
**1,166598**. Auch bloßes Einebnen der Profilhülle reichte nicht. Diese Varianten
sind verworfen, keine schnelleren gültigen Alternativen oder gelockerten Gates.

Gewählt: zwei horizontale neue Kreuzungsbereiche mit 3,5 m Halbausdehnung,
auf tatsächliche Straßenflächen beschränkt. Datum jeweils DEM am gemeinsamen
Originalknoten; beide alten Kontaktknoten ausdrücklich ausgenommen. Ganze
tatsächliche Flächenschnitte prüfen die Höhe: größte Float32-Abweichung
**0,2571 µm**, hergeleitete örtliche Wache dort **1,0465 µm**. Ein zusätzlicher
1-mm-Fehler fällt durch. Begrenztes Konstruktionsrezept, keine Vermessung.
Lokale Terrain-Topologie wird schon vor dem Solve in beiden Darstellungen
geprüft. Exakte feste Höhen werden rekonstruiert, danach tatsächliche
Neigungen und Kontakte geprüft; alter Körper wird nicht abgeflacht.

Profil-/Wandwachen aus tatsächlicher lokaler XY-/Z-Rundung und Facettenneigung.
Ganze Profile: höchstens **6,767 µm** Höhendifferenz, maximale hergeleitete
Wache **16,458 µm**. Acht ganze entgegengesetzt orientierte Wandsegmente über
**7,020871 m**; zusätzlicher 0,1-mm-Spalt wird abgewiesen. Kein exakter
Nullspalt-/Überlappungsvolumenbeweis. Alte Modi `flat`/`piecewise` samt Grenzen
bleiben erhalten; neue Familie verlangt ausdrücklich `profile`/`level_box`.

SIGGRAPH-/Primärquellen und optisch geprüfte Abbildungen stehen im
[Vorgängervergleich](stair_module_comparison.md#quellen-und-reproduktion):
Chen 2008 S. 2/Fig. 2, Hoppe 1993 S. 3–4, Galin 2010 S. 9/Fig. 20.
Kontaktbindung/Kreuzungsrezept sind eigene konservative Forschung. Die frühere
SDK-Operationsreferenz aus #91 wurde hier nicht erneut untersucht; kein
kopierter Fremdcode/SDK in der Runtime oder Beweis eines ganzen OSM-Generators.

## Geometrie, Prüfung und Kosten

Geometriekorrektur `84c46ba35`; normale/alternative Erzeuger, Reader und Bilder
`6d52effd5`; Profiling/zwei vollständige Gegenrezepte `d92d912f5`. Zwischen
letzten beiden Ständen nur Kostenhelfer/Gegenrezepte geändert. Alle neun NPZ-
Arraymengen des normalen/profilierten Laufs in Typ, Form und Bytes gleich.
Bibliothek und installierte **198 Module** unverändert; dieselben bestehenden
Höhen-/Profil-/Körper-/Weld-Kerne, keine öffentliche/native/Reservations-/Cache-API.

- Neuer Körper: 106 Oberflächenpunkte/154 Oberflächenfacetten,
  212 Körpervertices/420 Körperfacetten, 1278,326 m³; geschlossen, positiv
  orientiert, zyklische Vertexlinks, Double/Float32. Straßenkappen benutzen
  vorhandenen dunklen Asphalt, Wege bleiben blass; alte Displayarrays bytegleich.
- Nur 841 alte Terrainfacetten ersetzt. Lokal 859 Punkte/1585 Facetten,
  711 feste Werte/131 wörtlich alte Randhöhen. Keine alten Pfeiler oder
  geschützten Eltern betroffen. Weld 1346→879, 467 Kandidatenpaare, **0 m
  Verschiebung**; 20 unbenutzte Vertices entfernt. Konformierung 33 Kanten/
  310 zusätzliche Facetten. Roh-XY 61488 B, eindeutiges XY 13744 B,
  Zuordnungen 30744 B; ganzer Feld-/Lokalbezug vom Reader bestätigt.
- Welt: 13192 Punkte/26026 Feldfacetten, Diagnosekörper 26384 Vertices/
  52764 Facetten. Außerhalb **24441** alte XYZ-Dreiecke bytegleich. Bisherige
  87 konstruktive Facetten/37,165 m², 5-m-Bereich und Klassen-/Neigungsgrenzen
  erhalten. Alle 124 ganzen unteren Pfeilerkontakte bestehen, maximal 15,170 µm
  Float32-Fehler; neue Quellen mindestens 250,966 m von ihren Projektionen.
  Alte Brücken, drei Treppen/24 Stufen und geschützte Profile erhalten.
- **92** betroffene Kontrollen auf korrigiertem Quellenstand und dieselben
  **92 gegen unverändertes installiertes Wheel** bestanden. Zuvor 153 breitere
  Kontrollen vor Float32-Korrektur, darunter 13 alte Körperarrays exakt gleich;
  kein behaupteter 153er Gesamtlauf auf dem letzten Commit. Ganze frische
  Erzeuger/Reader ohne alte Planer; Reader auch ohne CDT, LP/Kegelsolver,
  Körperbau/Arrayexport. 43er Stand als dritte Cache-Elternstufe mit wörtlich
  gleichen alten XYZ/Körperfacetten lesbar. Vier Eltern/64 Quellbits, acht neue
  Quellen höchstens; altes Kernbudget unverändert.
- Normaler Gesamtaufbau **19,119 s / 179363840 B Peak-RSS**; ursprüngliche
  Höhenvariablen **19,191 s / 179097600 B**. Parallel mit Kontrollen bei Nice 10;
  kein Geschwindigkeitsgewinner. Gleiche lokale Eingaben außer Moduskennung,
  gleiche Grenzen, beide ganze Felder unabhängig zugelassen. Feste Substitution:
  1137→426 Variablen, 3964→3253 Zeilen, 5990→2613 A-Nichtnullen,
  3939→486 P-Nichtnullen; zusätzlich 40604 B reduzierte Sparse-Matrizen und
  27288 B explizite lineare Verschiebungsdaten.
- Memray 1.20, aktueller Prozess nach Imports einschließlich Provider,
  Geometrie, Weltprüfung, Inventar und Export: **92867508 Ereignisse /
  10627424787 B kumulativ angefordert / 55504567 B verfolgter Heap-Peak /
  202473472 B Prozess-Peak-RSS**. Instrumentiert **38,147 s**, davon 30,381 s
  Weltprüfung. Offizielle native Statistik separat terminal erfolgreich.
  Python/native Allokationen erfasst, native Stack-Abwicklung ausgeschaltet.
  Separater nativer NumPy-MALLOC von 1048583 B in beiden Stack-Modi genau
  einmal erfasst; Modus/Endzeit/Python-Allokatoren im aktuellen Trace geprüft.
  Kein Timingvergleich mit früheren nativen Stack-Läufen. Unterprozess-
  Allokationen, PNG und Reader ausgeschlossen; Statistik-Unterprozess-RSS
  und interne Gesamtkopien unbeobachtet.
- Sieben Produkt-Arraymengen **3261244 B**, Display zusätzlich **10002816 B**;
  globaler expliziter XY-/Indexscratch 320504 B, Außenvergleich 3519504 B.
  Abschließende Wandprüfung 162496 Facettenprojektionen; gleicher zusätzlicher
  Umfang zur Rundungsauswahl. Angeforderte Bytes sind keine Live-Datenmenge
  oder kopierten Bytes.

Linux/WSL2, i5-1135G7, vier sichtbare logische CPUs, rund 15,44 GiB RAM;
Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0. Unverändert
120 s CPU/3 GiB Adressraum je Erzeuger/Reader, Solver 8 s/100 Iterationen.
Normale unabhängige Reader 16,761/16,758 s. Globale Diagnose dominiert;
keine billige Runtime-Cacheladung, Frame-/A18-/RTX-/Weltlade-/Statikabnahme.

Vier frische vollständig dekodierte und tatsächlich geöffnete Eevee-Bilder:
ganzer Umfang und moderate Netzsicht, je vor/nachher, **1280×720**.
llvmpipe/LLVM 19.1.7, 11,594–13,122 s/Bild. Neuer dunkler Straßenast deutlich
lesbarer. `test/scripts/pixels.py`: 3882 geänderte Pixel in Übersicht, 5139
in Netzsicht. Netz lückenhaft, Knicke kantig, Umgebung klinisch. Keine
Nahansichtsserie, öffentliche Ersatzgalerie oder Server; Bilder bleiben privat.

## Reproduktion und verbleibender Umfang

```sh
export PYTHONPATH=lib/src:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest stair_southwest_checks stair_module_checks \
  stair_module_chain_checks stair_conformance_checks -q
python test/experiments/transport/stair_module_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-southwest-streets-vienna.json \
  --base "$TRANSPORT_BASE" --supports "$TRANSPORT_SUPPORTS" \
  --cached-module "$TRANSPORT_FIRST_MODULE" \
    test/experiments/transport/fixtures/stair-west-ground-module-vienna.json \
  --cached-module "$TRANSPORT_SECOND_MODULE" \
    test/experiments/transport/fixtures/stair-west-module-chain-vienna.json \
  --output "$TRANSPORT_SOUTHWEST_MODULE"
```

Reader: gleiches Argumentpaket an `stair_module_verify.py`; Alternative mit
`--variable-mode heights` und frischem Temp-Pfad. Allokationen:
`stair_module_cost.py --profile <frischer Tracepfad>`, optional `--no-native-stacks`.
Vorherige ganze verifizierte Elternexporte samt Readerbelegen erforderlich.
Der feste [Kontaktinput](fixtures/stair-southwest-contact-vienna.json) samt NPZ
enthält echte ausgeschnittene Elternflächen/Herkunftshashes, keine neuen
erwarteten Ausgabeflächen oder Weltcaches. Lokale Tests benötigen keine privaten
Elternbäume.

Aktuelle Ausgaben/Bilder `/tmp/outshine-transport-minor-full-6d52effd5`;
alternative Höhen `/tmp/outshine-transport-minor-heights-6d52effd5`;
Profiling `/tmp/outshine-transport-minor-profile-final`; Roh-/Bildbelege,
Trace/Gegenfälle `/tmp/outshine-transport-minor-evidence-e5575e520`.
Private Belege, keine Testabhängigkeit. Originale, benötigte Eltern und
tatsächliche Float32-Gegenfälle erhalten.

Von 1657 ursprünglichen Linien bleiben **1614** sowie alle 71 Flächen mit
Originalen/IDs/Gründen erhalten. Drei neue offene Enden: `249866752:0`,
`103612620:19`, `80922970:320`, jeweils Ende; Anfang `80922970:318` geschlossen.
Automatische Modulgraphen, weitere Klassen/Ebenen, vollständige Fahr-/Gehhüllen,
Knickradien, Terrain-/Cache-/LOD-Verträge, Statik/Boden und native Integration
bleiben offen. #434 und Gesamtziel bleiben aktiv.
