# Nördliche gemeinsame Geh-/Radbrücken (#517)

Die [feste Eingabe](fixtures/stair-north-bridges-vienna.json) erweitert den
[vorigen 28-Quellen-Verbund](stair_west_bridges_comparison.md) um vier ganze
ursprüngliche Teile desselben PBF: 138663000:10 (131,883 m), 1574593390:2
(46,0 m), 273682380:0 (433,2 m) und 852893530:0 (215,4 m), zusammen rund 826 m.
Zwei Quellen sind echte Brücken; Quelle/Tags/IDs und bisherige Achsen/Priors
bleiben erhalten. Neue Achsen sind vollständige Originale. Keine gemessenen
Deckhöhen oder vollständige OSM-/native Rekonstruktion behauptet.

## Funktionales Profil und wirklicher Körper

Die beiden als `path` gelieferten Teile haben ausdrücklich `bicycle=designated`
und `foot=designated`. Nur eine zusätzlich gewählte gemeinsame Geh-/Radrolle
verwendet deshalb das bestehende Radprofil: 2 m Breite/6 % maximale Neigung.
Der generische Pfad mit 1,5 m/20 % ist kein gleichwertiger funktionaler Vergleich.
Unmarkierte Pfade, fehlende/falsche Bezeichnungen, Indoor/Tunnel oder unpassende
Ebenen werden damit nicht zugelassen. Originale werden nicht umetikettiert.
Gewählte echte gemeinsame Enden erhalten ganze Profilhüllen; alte Kontakte und
getrennte Brückenebenen bleiben bestehen. Bloße Nähe erzeugt keine Verbindung.

Die ursprüngliche 10-nm-Erstpunkt-Kanonisierung erzeugt am neuen Brückenkopf
von 273682380:0 eine vierfach belegte vertikale Körperkante. Zwei Randpunkte liegen
13,3 nm auseinander; eine zentrierte Variante mit weiterhin höchstens 10 nm
Verschiebung behebt diesen ersten Fehler, aber nicht den ganzen Fall. Ein zweiter
Punktspalt von 24,7 nm lässt sich innerhalb dieser Verschiebung nicht vollständig
kanonisieren. Beide Varianten bleiben ganze Gegenbeispiele. Kein defekter Körper
wird durch einen positiven Solverstatus zugelassen.

Der neue Fall wählt ausdrücklich eine interne Konstruktionsschwelle von 100 nm.
Der erste Eingangspunkt bleibt kanonisch; tatsächliche Verschiebung höchstens
24,745 nm im Körperversuch. Die bisherigen tatsächlichen Quellflächen-, Profil-,
Kontakt-, Klassen-/Neigungs- und Float32-Gates bleiben unverändert. Die interne
Schwelle wird nicht als unverändert ausgegeben. Alte 10-nm-Defaults bleiben.
Der installierbare [numerische Kern](../../../lib/welding.md) zählt Radiuspaare
vor deren Arrayallokation, begrenzt Vertices/Paare und weist zu lange transitive
Ketten ab. Er allein garantiert weder Topologie noch gültige 3D-Exporte.

## Begrenzte Pfeilerwahl

142 nominale 12-m-Stationen überschreiten den bisherigen 128-Stationen-Pool.
Der alte Default bleibt ablehnend. Die neue ausdrücklich gewählte verteilte
Auswahl behält erste/letzte Station jedes Brückenteils und verteilt die übrigen
Plätze über die verbleibenden Kandidaten. Höchstens 128 physische Stationen,
640 Versuche und acht Sekunden je Variante. Jede nicht gewählte Station behält
Quell-ID, Nominale und Grund `support_station_budget`. Keine pauschale globale
Optimalität oder strukturelle Tragfähigkeitsberechnung behauptet.

## Quellen und Reproduktion

Zuerst lokale SIGGRAPH-Quellen: [Chen et al. 2008, S. 2/Fig. 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
Graph und Geometrie getrennt; [Hoppe et al. 1993, S. 3–4/Abschnitt 4](../../../doc/references/transport/siggraph/1993-mesh-optimization.pdf),
Konnektivität gegenüber geometrischer Einbettung und Prüfung legaler Änderungen.
Hoppe S. 3 und 4 einschließlich Pseudocode/Gleichungen optisch geprüft. Unsere
numerische Kanonisierung ist keine Implementierung ihres Optimierungsverfahrens.
Die tatsächliche Körper-/Kontaktprüfung bleibt eine unabhängige Pflicht.
[SciPy count_neighbors](https://docs.scipy.org/doc/scipy/reference/generated/scipy.spatial.cKDTree.count_neighbors.html)
zählt Radiuspaare; [query_pairs](https://docs.scipy.org/doc/scipy/reference/generated/scipy.spatial.cKDTree.query_pairs.html)
liefert ungeordnete Indexpaare ohne Diagonale. Beim Selbstvergleich wird die
Diagonale vom Zähler abgezogen und durch zwei geteilt. Primärdokumentation
2026-10-16 geprüft; lokal verwendete Version steht in den Messbelegen.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest test_welding stair_north_bridge_checks -q
python test/experiments/transport/stair_cycle_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-north-bridges-vienna.json \
  --base-network /tmp/outshine-transport-west-bridges-full-14b0a8f75 \
  --base-terrain /tmp/outshine-transport-west-bridges-full-14b0a8f75 \
  --output /tmp/outshine-transport-north-bridges-<commit>
```

Danach derselbe Aufruf mit `stair_cycle_verify.py`; Pfeiler mit
`stair_support_experiment.py`/`stair_support_verify.py`, derselben Fixture und
den neuen Produkten. Der folgende geprüfte Stand ergänzt die früheren
Körperhypothesen; diese allein sind kein vollständiger Geländebeleg.

## Aktueller vollständig geprüfter Fall

Quellencommit `f5e58b20a`, Integration `bda5d5052`: je 99 ausgeführte Quell- und
Wheelkontrollen mit teilweise überlappenden Fachfällen bestanden terminal mit
Exit 0. Betroffen: numerischer Kern, Profilhüllen, projizierte Flächen, neue/vorige
Brücken, Landungen, Pfeiler, Eck-/Korridor-/Radwegfälle und Terrainzellwahl.
Das frische Wheel hat SHA256
`9aaf486c0f5dc793fe0c8ce28c316156b5ea6997cab5db3d737a3445e49e0179`;
alle 198 installierten Pythonmodule stimmen bytegleich mit der Quelle überein.
Die wirkliche volle Erzeugung konsumiert den installierten Kern, mit expliziter
Pfadprüfung. Keine Editable-/Checkout-Importannahme als Installationsbeleg.
Die Integration übernimmt Buildings #518 ohne Änderung unserer geprüften Quellen,
Bibliothek, Eingaben oder Renderhelfer. Keine native Implementierung geändert.

32 Quellen, alle 56 inneren Endprofile, alle 16 bisher geschützten Paarprofile,
14 ganze Durchgangsflächen und drei Treppen/24 Stufen bestehen in Double und
tatsächlichem Float32. Kleinster Float32-Durchgang 2,519989 m. Geschlossener
Quellkörper: 4536 Vertices/9108 Facetten, davon 3339 obere/216 Riserfacetten;
Volumen 4336,110793 m³. Körperkanten/zyklische zusammenhängende Vertexlinks und
ganze Quellflächen bestanden. Keine globale Selbstschnitt-/Fahr-/Gehhüllenabnahme.
266 Querschnitte/3844 genodelte Segmente, 4224 LP-Variablen/88535 Zeilen/262193
Nichtnullwerte bleiben innerhalb der bisherigen Arrangement-/Face-/Solvergrenzen.
Der neue Kern bearbeitet 1896 rohe XY-Vertices/15 Radiuspaare; 1881 kanonische
Vertices, 24,745 nm tatsächlicher Maximalversatz. Eigene Ausgabe 37680 B;
explizite Indexkopie 30336 B, Differenz 30336 B, Abstände 15168 B.
Das ist kein vollständiger interner Allokations-/Peakbericht.

Frische volle Erzeugung und unabhängige gespeicherte Netz-/Terrain-/Pfeilerreader
bestanden mit Exit 0. Zusätzlich bestehen diese Reader mit gesperrtem CDT,
LP-/Kegelsolver, Körper-/Pfeilerbuilder und Arrayexporter. Der vorige 28-Quellen-
Reader wird separat ohne Änderung seines ursprünglichen Belegs geprüft.
Voriger Körper und Terrainplan bleiben arrayweise bytegleich. Auch der neue
100-nm-Kern erzeugt auf diesem vorigen 28-Quellen-Eingang exakt denselben Körper;
dies ist keine allein aus unveränderten Dateinamen abgeleitete Behauptung.

Das ganze Terrainrechteck bleibt 2044993,089198 m² groß. 12373 Punkte/24388
Höhenfeldfacetten, geschlossener Körper 24746 Vertices/49488 Facetten.
Zwei ausdrücklich begrenzte freie Verfeinerungen: 69 Zellen/108 Kantenmittelpunkte,
danach 203/365. Unverändert höchstens 16384 Punkte/32768 Feldfacetten, acht
Solversekunden/100 Iterationen je Solve, Erdneigung 2/3, konstruktiv 10 und
wirklicher 5-m-Bereich. `AlmostSolved` allein genügt nicht: tatsächliche Maxima
Double/Float32 0,664668769/0,665873762 für Erde, 9,998000298/9,998496124 für
konstruktiv. 94 konstruktive Facetten/14,861158 m² im wirklichen 5-m-Bereich.
Keine Klassen-/Kontakt-/Geometriegrenze wurde für den neuen Konstruktor geändert.

Alle vier vollständigen Original-Terrarium-Decodes stimmen mit den ganzen festen
Crops überein; alle 12373 endgültigen Zielhöhen stimmen exakt mit unabhängiger
vollständiger Rohabtastung überein. Alle 2405 ursprünglichen ganzen Bodenfacetten
bleiben als XYZ-Dreiecke bytegleich in Double/Float32 erhalten. Ganze tatsächliche
Bodenkontakte 0/0 m; keine innere Höhensprungstelle. Kein globales Null-
Überlappungsvolumen oder gemessenes geodätisches Datum behauptet.

Die verteilte Wahl bearbeitet 128 von 142 nominalen Stationen; 14 bleiben mit ID,
Nominale und Budgetgrund erhalten. Feste Variante: 119 ganze Pfeiler/128 Versuche/
15150 Kontaktpaarprüfungen/4424 Körperfacetten. Verschobene gewählte Variante:
124/153/15860/4628. Vier weitere Stationen bleiben wegen anderer gewählter
Verkehrsquellen ausgenommen. Mindeste tatsächliche Float32-Höhe 0,108795 m;
größter tatsächlicher Float32-Kontaktfehler 15,171 µm innerhalb der individuell
abgeleiteten Grenze. Die nominale 12-m-Teilung bleibt; ausgeführte Abstände können
wegen der ausdrücklich dokumentierten Auswahl größer sein. Kein unbemerkter
Vollständigkeits-/Statik-/Fundament-/Bodenkennwertnachweis.

1657 originale Linienteile bleiben erhalten: 32 Kandidaten, 1625 übrige Linien
und 71 Flächen mit Originalen/IDs und Gründen. Acht Außenprofile bleiben offen:
Anfang 4420992360:6, Ende 32651280:3, Anfang 39881120:51, beide Enden
39881120:50, Anfang 39881120:25, Ende 273682380:0 und Anfang 852893530:0.
Der alte Anfang 39881120:39 ist jetzt ganz angeschlossen.
`admitted_source_ids=[]`; native und vollständige Verkehrsfreigabe bleiben offen.

## Kontrollierte Kosten und tatsächliches Bild

Je ein normaler frischer Prozess und je ein separater Memray-1.20.0-Prozess mit
Pythonallokatoren/native traces, Commit `bda5d5052`, SciPy 1.18.1, Python 3.13.5,
OpenBLAS/OMP je ein Thread. i5-1135G7, zwei physische/vier logische Kerne, rund
16 GiB. Eigene breite Prüfungen/Renderer liefen außerhalb dieser Kostenworker;
keine exklusive Hostreservierung oder native Geräte-/Frameabnahme.

| Ganzer Körper | Voriger 28er Default | Gleiche 28, begrenzter Kern | Neuer 32er Fall |
|---|---:|---:|---:|
| Normale Aufbauzeit, s | 3,1363 | 2,8943 | 3,2622 |
| Prozesspeak, B | 252137472 | 254545920 | 268009472 |
| LP Variablen / Zeilen | 3868 / 81515 | 3868 / 81515 | 4224 / 88535 |
| LP Nichtnullwerte | 241425 | 241425 | 262193 |
| Behaltene Sparsearrays, B | 2912576 | 2912576 | 3163216 |
| Explizite XY-Quantisierungskopie, B | 45336 | 45336 | 49464 |
| Eigene Körperarrays, B | 556548 | 556548 | 602228 |
| Profilierte Zeit, s | 14,4365 | 15,8987 | 18,3812 |
| Allokationsereignisse | 11014605 | 11014846 | 12031860 |
| Kumulativ angefordert, B | 1422860965 | 1422965970 | 1552276537 |
| Getrackter Heappeak, B | 194311333 | 194312012 | 211841735 |

Die beiden 28er Eingänge unterscheiden sich ausschließlich durch die interne
100-nm-Konstruktorwahl; beide tatsächlichen Körper sind bytegleich mit der
vorigen geprüften Referenz. Alle ganzen Qualitätsbedingungen bestehen.
Der neue Kern garantiert eine Vorzählung/Kapazität, keine Beschleunigung:
einzelne normale Zeitunterschiede sind kein belastbarer Tempogewinn. Beobachtet
sind beim gleichen Körper 241 zusätzliche Allokationsereignisse/105005 zusätzlich
angeforderte Bytes; diese Beobachtung ersetzt keine vollständige interne Kostenformel.

Zeit/Tracker umfassen Podestauswahl, Arrangement, LP und geschlossenen Körper nach
Imports/Quellendecode und bisheriger einzelner Bodenpodestergänzung. Ganze physische
Prüfungen und Datei-IO liegen außerhalb des Trackers; Prozesspeak enthält diese
Prüfungen. Terrain/Pfeiler/Renderer liegen außerhalb dieser Körpermessung.
Interne Gesamtkopien unbekannt; explizite Kopien und behaltene Arrays getrennt.
Normale und profilierte Zeiten bleiben getrennt. Der neue volle Körper ist
arrayweise bytegleich zwischen normalem/profiliertem Worker, ursprünglicher
100-nm-Hypothese und wirklicher voller installierter Erzeugung.

Ganze Netz-/Terrainerzeugung vor Display separat 29,8497 s/269217792 B Prozesspeak;
dabei lief die installierte Testsuite gleichzeitig. Letzter Feldsolve 4,2492 s,
5,8402 s mit Vorbereitung; beide freien Probesolves 1,1122/1,2790 s. Ganze
Pfeilergeneration inklusive beider Varianten/Display separat 6,6942 s/135274496 B.
Peaks/Phasen nicht addieren; kein Ladezeit-/Runtime-/A18-/RTX-Nachweis.

Vier frische tatsächliche 1280×720-Bilder auf `bda5d5052`, alle geöffnet und ganz
decodiert: vorher/nachher jeweils ganze 2248,388-m-Übersicht und moderater
1450-m-Netzausschnitt. Alle drei langen Brücken bleiben im gemeinsamen Bild;
gleiche Kamera, Licht und Palette je Paar. Tatsächliche Mesh-/Normal-/Farb-/Szenen-
und installierte Helperbindungen geprüft. Blender Eevee/llvmpipe/LLVM 19.1.7;
16,18–17,18 s pro Offlinebild, kein gemessener GPU-/nativer Frame.
Die zusätzlichen beiden nördlichen Brückenspannen sind auch in der Übersicht
sichtbar. Dünne helle Wege bleiben kontrastarm, die Umgebung klinisch. Keine
Nahansichten oder öffentliche Ersatzgalerie. Pixels: 0,4832 % der Übersichtspixel,
0,8549 % im Netzausschnitt geändert; nicht jede Pixeländerung ist ein Bildgewinn.

Originalbelege, Wheel, normale/profilierte Kosten, Roh-/Default-/Reader-/Bildbelege
und nötige numerische Gegenbeispiele lokal unter
`/tmp/outshine-transport-frontiers-8ec28760c/`. Ganze aktuelle Produkte:
`/tmp/outshine-transport-north-bridges-full-bda5d5052/`; Pfeiler/vier geöffnete Bilder:
`/tmp/outshine-transport-north-bridges-support-bda5d5052/`.
Kein Fremdcode, keine SDK-/native Runtime-Abhängigkeit hinzugefügt.
