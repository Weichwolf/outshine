# Ganze gewählte Podeste und wiederverwendbare Profilkonstruktion (#511)

Der bisherige [24-Quellen-Eckstand](stair_corner_comparison.md) besteht zwölf
ausdrücklich geschützte überlappende Paarprofile. Die neue Prüfung erfasst alle
48 Quellendprofile: 41 an gewählten inneren Knoten, sieben weiterhin außen.
Beim alten Körper sind nur 24 der 41 inneren Profile ganz getragen; 17 bleiben
unvollständig. Der erste neue ganze Körper mit neun zusätzlichen Podesten
besteht sämtliche 41 Profile in Double und tatsächlichem Float32. Originale,
Achsen/Priors/Attribute/IDs und alle bisherigen Quellen bleiben erhalten.

Die optionale [feste Eingabe](fixtures/stair-landings-vienna.json) wählt keine
neuen Verbindungen oder Endpunkte. Sie erzeugt an bereits gewählten echten
Knoten nur tatsächlich fehlende ganze Profilflächen. Alle Nachbarn werden zuerst
zusammen betrachtet. Die neun Profilhüllen ergänzen zusammen 0,591984 m² außerhalb
des vorhandenen Verbunds; ihre ganze gemeinsam besessene Fläche ist 7,539117 m².
Die bestehenden expliziten Eckprofile und ihr ursprünglicher Acht-Podest-Vertrag
bleiben unverändert. Die neue Auswahl hat eine eigene Obergrenze von 16 kleinen
Podesten je bestehender begrenzter Komponente. Unveränderte Mesh-/LP-/Terrain-,
Zeit-/Speichergrenzen und lokale Auslassungsgründe bleiben erforderlich.

Der kleine [installierbare Kern](../../../lib/junction_pads.md) besitzt nur die
metrische Konstruktion aus zwei bis vier zentrierten Profilen. Quellen, Ebenen,
Treppen-/Brückenauswahl, Höhen, Reservationen und tatsächliche Kontaktprüfung
bleiben beim Aufrufer. Der bisherige Ground-only-Aufrufer importiert denselben
Kern mit seinen alten Semantik-/Budgetregeln. Keine SDK-/Fremdcode-Übernahme.

Die unabhängige Prüfung am gespeicherten Mesh kontrolliert Besitzerfläche und
die Vereinigung ihrer gewählten Nachbarflächen getrennt über den ganzen
Endquerschnitt: tatsächliche flache gleiche Höhe und ganze XY-Deckung in beiden
Präzisionen. Ein echtes vollständig gemeinsames Randprofil darf Kante an Kante
anschließen; positive gemeinsam besessene Fläche ist dafür nicht erforderlich.
Die bisherigen zwölf ausdrücklich überlappenden Paarprofile bleiben separat
verbindlich. Geschlossener gemeinsamer Körper, ganze Quellflächen, Klassen,
Unterführungen und zyklische Vertexlinks bleiben zusätzliche Prüfungen.

Zuerst SIGGRAPH Chen et al. 2008, S. 2/Fig. 2 auch optisch geprüft:
[Graph und 3D-Geometrie](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf)
sind getrennte Stufen. Das Paper schließt konstruktive Kreuzungsdetails aus;
unsere begrenzte Profilhülle ist kein dort belegter Kreuzungsalgorithmus.
Bestehende GEOS-/CDT-Bibliotheksoperationen bleiben die Grundlage.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest test_junction_pads stair_landing_checks \
  stair_corner_checks stair_corridor_checks stair_cycle_checks \
  stair_cycle_north_checks stair_cycle_south_checks stair_terrain_cells_checks -q
python test/experiments/transport/stair_landing_cost.py \
  --method profile_hull --output /tmp/outshine-transport-landing-hull.json
python test/experiments/transport/stair_landing_cost.py \
  --method profile_box --output /tmp/outshine-transport-landing-box.json
python test/experiments/transport/stair_cycle_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-landings-vienna.json \
  --base-network /tmp/outshine-transport-stair-corner-full-edcd738ce \
  --base-terrain /tmp/outshine-transport-stair-corner-full-edcd738ce \
  --output /tmp/outshine-transport-landings-<commit>
```

Danach derselbe Aufruf mit `stair_cycle_verify.py`; Pfeiler mit
`stair_support_experiment.py`/`stair_support_verify.py`, derselben Fixture und den
neuen geprüften Produkten. `stair_landing_cost.py --profile <frische.bin>` liefert
getrennte Allokationsphasen. Native/Frame-/Weltlade-/Statikabnahme bleibt offen.

## Vollständige Lieferung und installierter Verbraucher

Verfahrensstand `29f6d5efc`, zusätzliche echte Rechteck-Negativkontrolle
`e897c252d`: 53 Quellkontrollen und dieselben 53 Kontrollen im frischen
installierten Wheel bestanden ohne Skip. Die danach ergänzte Rechteckkontrolle
bestand zusätzlich auf beiden Wegen: insgesamt je 54 abgedeckte Kontrollen.
Die vier öffentlichen Geometriekontrollen wurden nach ihrem Umzug nach
`lib/tests/test_junction_pads.py` auch direkt aus dem aktuellen Quellpaket geprüft.
Ruff check/format und alle betroffenen Funktionen innerhalb 120 Zeilen.

Wheel-SHA256 `ed610bd9fa52346b4a1293d27e5c12c9d4d8532aae690d69c22890773cabadae`;
der tatsächliche Paket-/Primitivpfad liegt im privaten Installationsziel, kein
editierbarer Quellimport. Primitiv-SHA256 Quelle/Wheel identisch:
`ac9f69a0e05ebf479cbfebdff1fa6a053a1bc5fa7da6ca3fb5fe5663cdd376c5`.
Sämtliche tatsächlichen Arraytypen, Formen und Bytes des neuen ganzen Körpers
sowie des bisherigen 24-Quellen-Defaults stimmen im Wheel exakt mit ihren
geprüften gespeicherten Referenzen überein. Die alte Ground-only-Semantik und
deren tatsächliche Negativfälle bleiben erhalten. Keine Experimentimports im Kern.

Frische vollständige Erzeugung und unabhängige gespeicherte Terrain-/Pfeilerreader
bestanden. Aktuelle Reader bestehen auch bei gesperrten CDT-/LP-/Kegelsolvern,
Körper-/Pfeilerbuildern und Arrayexportern. Der alte 24-Quellen-Reader besteht
mit unveränderten Originalreferenzen; dessen ursprünglicher Beleg bleibt erhalten.
Alle vier ganzen Crops und 12521 endgültigen Terrainziele sind exakt gegen die
vier vollständigen ursprünglichen Roh-Decodes geprüft. Alle 3888 tatsächlichen
Bodenfacetten sind XYZ-bytegleich in Double und Float32, ganze gemessene
Bodenkontakte 0/0 m. Keine Behauptung eines globalen Null-Überlappungsvolumens.

Das gleiche ganze Terrainrechteck umfasst rund 2,045 km², endgültig
12521 Punkte/24684 Höhenfeldfacetten. Der geschlossene Wegekörper hat
7040 Vertices/14116 Dreiecke, das Terrain 25042 Vertices/50080 Dreiecke;
das bestehende Facettenbudget betrifft das Höhenfeld. Keine Orientierungskollapse,
offenen Kanten oder unterbrochenen Vertexlinks. Die bisherigen zehn tatsächlichen
Unterführungen und drei Treppen/24 Stufen bestehen. Kleinster tatsächlicher
Float32-Durchgang 2,519989 m; Erdneigung maximal 0,664680456/0,665543537,
konstruktiv 9,997999079/9,997501156 bei Grenze 10. 30 konstruktive Facetten/
7,987259 m² bleiben im wirklichen 5-m-Bereich.

64 feste/70 verschobene Pfeiler bestehen erneut: 80 Versuche, 11606 akzeptierte
Kontaktpaarprüfungen, 2624 Körperfacetten. Ein gemeinsamer Builder mit aktuellen
24684 unteren und 1305 oberen Referenzfacetten, zwei Verkehrsindizes/10182
Dreiecke und keine Verkehrsunion. Explizite Index-XY-Nutzdaten 488736 Byte,
eigene Masken 81456 Byte. Kleinste tatsächliche Float32-Pfeilerhöhe 0,111938477 m;
größter geprüfter Float32-Kontaktfehler 16,203 µm innerhalb der jeweils abgeleiteten
Schranke. Direkte 0,2-m-Fremdverkehrsausschlüsse bleiben Pflicht; keine Statik.

## Gleiche Qualitätsanforderung und Kosten

Das Rechteck erfüllt die reine Profilabdeckung, erzeugt im ganzen Verbund aber
zwei zusätzliche **ungeplante** Bandüberlappungen: `229812320:0` mit
`258321780:54` (0,037759538 m²) und `97041640:6` mit derselben Treppe
(0,055276360 m²). Dazu fehlt das verlangte explizite Trenn-/Ebenenrezept.
Die unveränderte ganze Wechselwirkungsprüfung lehnt es ab; kein Grenzwert wird
erhöht oder ein ungültiger Lauf als gültiger Kostenvergleich gewertet.
`stair_landing_cost.py --method profile_box` endet hier erwartungsgemäß ungleich
null. Der genaue Gegenfall ist abgedeckt; sein Polygon/Quellbeleg bleibt lokal.
Die gültige Profilhülle ist der gewählte Ablauf. Kein allgemeines Optimum behauptet.

Ein normaler frischer Hüllenworker, nach Imports/Eingabedecode und unverändertem
bisherigem Einzelpodest: Auswahl/Podeste, Anordnung, begrenzter LP und geschlossener
Körper 8,684181 s. 6664 LP-Variablen/146208 Zeilen/432970 Nichtnullwerte,
5222300 Byte explizite sparse Arrays, 79032 Byte explizite XY-Quantisierungskopien,
933580 Byte eigene gespeicherte Körperarrays. Prozess-Peak einschließlich der
anschließenden ganzen physischen Prüfungen 381947904 Byte. Die neue allgemeine
Profilprüfung quantisiert den ganzen Körper einmal gemeinsam; expliziter
Float32-/Double-Konversionsscratch dafür 253440 Byte.

Getrennte Memray-1.20.0-Phase mit Python-Allokatoren und nativen Traces:
19043158 Ereignisse, 2526008126 kumulativ angeforderte Byte,
349447985 Byte beobachteter Heap-Peak; instrumentierte Zeit 26,445052 s.
Ganze tatsächliche Prüfungen und Export-IO liegen außerhalb des Trackers;
Terrain/Pfeiler/Blender sind nicht enthalten. Normal-/Instrumentierungszeiten
bleiben getrennt. Kumulative Bytes sind kein Live-Speicher; interne Gesamt-
Kopiermengen bleiben unbekannt. Beide gemessenen Körper sind array-bytegleich
zum ganzen kanonischen Körper, zusätzlich unabhängig auf alle 41 Profile geprüft.

Ganze normale Erzeugung vor abschließendem Displayexport 32,634900 s,
Prozess-Peak bis dahin 382746624 Byte. Der Aufbau bleibt teuer. Geteilter
i5-1135G7 mit 2 physischen/4 logischen Kernen und 16 GiB, Linux/Python 3.13.5;
teils parallele eigene Prüf-/Renderarbeit und fremde Arbeit. Einzelwerte belegen
keinen Tempogewinn, Gerätevergleich, Frame- oder Weltladebudget.

## Tatsächliches Bild und verbleibende Lücken

Vier frische tatsächlich geöffnete 1280×720-Vorher-/Nachherbilder: ganzer
Terrainüberblick und derselbe moderate 750-m-Netzausschnitt mit ganzer Brücke.
Je Paar identische Kamera/Licht/Palette. Tatsächliche vollständige Meshimporte,
Normalen/Farben/Topologie und Helper-/Mesh-/Szenen-/PNG-SHAs sind gebunden.
Die Renderhelfer wurden tatsächlich aus dem geprüften Wheel importiert.
Blender 5.2.2 LTS/Eevee, llvmpipe/LLVM 19.1.7, Offline-CPU; kein Runtime-Bild.

Das Fernbild ändert sich wenig; kleine Podeste sind bei dieser Übersicht kaum
einzeln lesbar. Die Bilder belegen vorhandene ganze Wege/Brücke/Terrain und
keine erkennbare grobe Bildregression, keinen großen gestalterischen Gewinn.
Im Netzausschnitt ändern sich 1317/921600 Pixel, 409 um mehr als 1/255,
größter Unterschied 40/255; keine schwarzen Löcher. Helle dünne Wege und die
weitgehend leere klinische Umgebung bleiben ein Bilddefizit. Keine Nahbilder,
neue Galerie oder geänderte Licht-/Materialverträge.

24 Kandidaten/1657 ursprüngliche Linienteile, 1633 übrige Linien und alle
71 Flächen mit Originalen/IDs/Gründen bleiben erhalten. Alle sieben vorherigen
äußeren Profile bleiben unverändert offen. Ganze innere Endprofile beweisen
keine vollständigen Geh-/Fahrhüllen, Kurven-/Abbiegeradien, Selbstschnittfreiheit,
weiteren Quellen/Indoor-/Tunnelrezepte, Statik oder native Übernahme.
`admitted_source_ids=[]`; #434 und das ganze Transportziel bleiben aktiv.

Aktuelle Körper/Terrainprodukte: `/tmp/outshine-transport-landings-full-fc98f1923/`;
Pfeiler/vier Bilder: `/tmp/outshine-transport-landings-support-29f6d5efc/`;
Quell-/Wheel-/Rohkontakt-/Kosten-/Reader-/Renderbelege:
`/tmp/outshine-transport-junction-discovery-fc98f1923/`. Tatsächliche Commitfelder
sind maßgeblich; Verzeichnisnamen bezeichnen teils den Ausgangsstand. Eigene
Setuptools-Baukopien nach Wheelvergleich entfernt; identische Körperkopien nach
rohem Arrayvergleich auf den aktuellen kanonischen Körper verwiesen. Ursprüngliche
Gegenfälle, Roh-Allokationstrace, feste Inputs und nötiger Vorherstand bleiben erhalten.
