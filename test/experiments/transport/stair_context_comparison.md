# Tatsächlicher Nachbarkontext der Wiener Treppe (#434)

Der örtliche Versuch plant jetzt sieben wirkliche Quellachsen, vier gemeinsame
Anschlüsse und beide Überführungen derselben langen Radbrücke. Das ferne
Brückenende steigt gegenüber dem isolierten Vierwegeversuch um1.474157m.
Die zwölf echten Treppenstufen bleiben erhalten. Die74 weiteren bekannten
Kontextachsen und drei neuen Außenkontakte sind noch ungeprüft; keine Quelle
wird damit als vollständig wiedergewonnen freigegeben. #434 bleibt aktiv.

## Unveränderte Originale und getrennte Höhenpriors

Maßgeblich ist die von master gelieferte vollständige
[Original-PBF](../../outshine/data/transport/wien-junction-14-8937-5678.pbf),
OpenFreeMap/OpenMapTiles20260927_080001_pt, Kachel14/8937/5678,
218795B, SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
Das aktuelle [native Fixture](../../outshine/data/transport/wien-junction-sources.json)
hat SHA256 `c852f608cff325830659eed74688796527da4de33cb5deb14aabe2d992fee656`.
Gegenüber dem ursprünglichen EDF7-Snapshot ergänzt es nur `origin.payload_file`;
alle elf isolierten Körperarrays sind byteidentisch geblieben. Der alte Snapshot
bleibt für die bisherigen Belege erhalten; der [erste Bericht](stair_junction_comparison.md)
besitzt Herleitung, Quellen und Double-/Float32-Gegenbeispiele.

Der neue [feste Kontext](fixtures/stair-context-vienna.json), SHA256
`a04eb7a347f54c94ae9104e3da9b84e09ee83e10b5f75f48485d04a3121e5985`,
erhält27 vollständige ursprüngliche Feature-Dictionaries, sieben Kandidatenteile
und74 ausdrücklich nicht zugelassene Kontextteile. MVT-Punkte, Properties, IDs,
geographische und lokale Koordinaten bleiben erhalten. Der32-m-Erwerbsabstand
ist ein konservatives Suchfenster, keine Freiraum- oder Reservationsgarantie.
Der lokale Frame benutzt weiterhin die nativen111132m/Lat-Grad und
111320*cos(Referenzbreite)m/Lon-Grad, Ursprung48.242427962074906/16.386505365371704.

Ein neuer Download derselben URL lieferte218657B, SHA256
`002c1f8dc546f45aa3188969d41dbbacc7e1dafd958b1bd6ee7285b63684d132`:
571 statt572 Transportfeatures,16 geänderte/fehlende vollständige Features.
Alle sieben ausgewählten Kandidatenteile stimmen überein, aber sechs der74
Kontextteile aus39881120/80922970/258321780 unterscheiden sich. Derselbe
Versionsname beweist daher keine identische Quelle. Der neue Download ersetzt
die ursprüngliche PBF ausdrücklich nicht.

Mapterhorn14/8937/5678,56194B, SHA256
`67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177`,
liefert ein tatsächliches98×123-Float32-Höhenfenster aus dem512×512-Terrariumraster.
[NPZ](fixtures/stair-context-vienna-dem.npz), SHA256
`d9923989312f6dc58315dea22c17ef8f3d094ee52053e3629e0fed7672071db4`.
Die offline Gegenprüfung interpoliert unabhängig mit denselben511 Randposten.
Das Fenster deckt die Kandidaten und ihren Halo ab, nicht sämtliche unbeschnittenen
Kontextachsen. Die ursprünglichen nativen Drapierhöhen bleiben getrennte Priors;
Abweichungen bis etwa1.09m beweisen weder dieselbe ursprüngliche DEM-Aufbereitung
noch einen bestimmten Provider-/Abtastfehler. Keine gelöste Höhe steht im Fixture.

## Tatsächliche Anschlüsse und Überführungen

| Index | Originalfeature:Teil | Rolle | Breite / Gradientengrenze |
|---|---|---|---|
| 0 | 377113090:1 | ursprüngliche Treppe | 1.5m / 0.50 Hülle |
| 1 | 97041640:5 | ursprünglicher unterer Radweg | 2m / 0.06 |
| 2 | 229812320:0 | ursprüngliche lange Brücke | 2m / 0.06 |
| 3 | 1013938082:0 | ursprüngliche Rückbrücke | 2m / 0.06 |
| 4 | 39881120:26 | tatsächlicher Radnachbar, foot=yes | 2m / 0.06 |
| 5 | 4420992360:7 | tatsächlicher Fußwegnachbar | 1.8m / 0.15 |
| 6 | 97041640:6 | ferner unterer Radweg | 2m / 0.06 |

Alle IDs tragen den Präfix `openfreemap:14/8937/5678:transportation:`.
Die ersten vier Profile bleiben unverändert aus dem nativen Eingang; die drei
neuen stammen aus dem vorhandenen Projekt-Profilrezept, dessen SHA im Ergebnis
gebunden ist. Keine Neigungsgrenze wurde für eine Lösung gelockert.

Gemeinsame ganze flache Kontaktbereiche:0/1/3,0/2/3,1/4/5 und2/6.
Die letzte Quellpaarung besitzt außerdem eine echte Überführung an einem anderen
Ort. Allein nach Quellpaar zu klassifizieren wäre falsch: Der gemeinsame
Endknoten ist ein Anschluss, der getrennte Überlappungsbereich eine Unterführung.
Die Prüfung wählt die tatsächliche Überlappungskomponente am Quellkreuzungspunkt
und verlangt dort vollständige exklusive obere/untere Flächen. Eine falsche
gemeinsame Kontaktmarkierung darf die Unterführung nicht verschwinden lassen.

Die bisherigen beiden Außenprofile sind nun innere Anschlüsse. Drei neue äußere
Profile bleiben vorgeschlagen: Radweg4 endet bei165.064279m, Fußweg5 beginnt
bei165.381775m, Radweg6 beginnt mit166.611149–166.614783m statt dem
166.631714-m-Prior. Deren tatsächliche Nachbarn39881120:28/4420992360:6,
377113090:0/229812042:0 und39881120:34 sind erhalten, aber noch nicht konstruktiv
angebunden. Insbesondere weitere Treppen, U-Bahn-Brücken und quarantänisierte
80922970-Fußwege bleiben echte Kontextaufgaben, keine stillschweigenden Freigaben.

## Ganze Körper, gespeicherter Reader und Kosten

Freeze Erzeuger/Kosten/Bild:`32f2e389b2a00f62cb81fba54a965a370a6bd5d6`;
Reader-JSON-Roundtripkorrektur:`aacd22cd496d5205613c6a93cf6592fd9aab4e32`.
Python3.13.5, NumPy2.2.4, SciPy1.18.1/HiGHS1.12.0,
Shapely2.2.0/GEOS3.14.1. Keine neue Runtime-, SDK- oder öffentliche API-Abhängigkeit.
Gemeinsame Flächenanordnung711XY-Knoten/1040Facetten/1823Segmente,
1496LP-Variablen/33393Ungleichungen/98639Sparse-Nonzeros. Dieselben ganzen
Gradienten-/Monotonie-/Kontaktbedingungen und Float32-Reserven wie im ersten
Bericht;3GiB/90CPU-s Prozessgrenzen, LP-Zeitlimit5s bleiben wirksam.

Nachher1720Vertices/3444Flächen,1189obere/124Setzstufenfacetten,
Volumen322.825844m³, zwölf Stufen à17.181757cm. Ganze Podestabweichung0;
größte tatsächliche Float32-Radnorm0.05985377, Fußwegnorm0.14741753.
Beide Unterführungen haben Double2.52m und Float32 mindestens2.519989014m
zwischen unterer Verkehrsfläche und oberer Körperunterseite. Kein offener oder
falsch orientierter Rand, kein kollabiertes Exportdreieck; ergänzend besitzen
alle1060/1720 verwendeten Vorher-/Nachhervertices einen zusammenhängenden
zyklischen Link. Das ist kein allgemeiner Statik-/Selbstschnitt-/Gehhüllennachweis.
Nachher-Körper-SHA256
`74bad725e77af4e139c30fb8d162a2aaa31b2d6d042e840dc9a69b662d973f90`.

50 betroffene Kontrollen bestanden, darunter neun neue reale Kontextkontrollen.
Der gespeicherte Reader löst und trianguliert nicht neu; er bindet Originale,
Profile, alle Quellenflächen, Körper-/Feld-/Darstellungsarrays, vier Podeste,
tatsächliche Float32-Grenzen, beide Unterführungen, drei vorgeschlagene Außenprofile
und alle74 ausdrücklich noch nicht zugelassenen Quellen. Frischer vollständiger
Erzeuger2.078049s,155644KiB Peak-RSS; kein Weltaufbau-/Frame-/Gerätebudgetnachweis.

Gleiche endliche Bedingungen und ganze akzeptierte Körperqualität, sieben normale
Wiederholungen nach warmen Importen und beiden Solvern, BLAS/OMP je1,
Prozesse nacheinander ohne eigene Renderer:

| Messung | Dual-Simplex | Interior-Point |
|---|---:|---:|
| Aufbereitung + LP, Median | 1078.157ms | 1906.201ms |
| normaler Worker Peak-RSS | 159883264B | 159891456B |
| separate instrumentierte Phase | 3672.970ms | 4538.904ms |
| Allokationsereignisse | 2638564 | 2631430 |
| angeforderte Bytes | 388436743B | 564391088B |
| getrackter Heap-Peak | 79477605B | 79354858B |

Memray1.20.0 separat, nicht mit normalen Zeiten vermischt. Beide behalten1189656B
Sparse-Arrays, explizite Float32→Double-XY-Kopien17808B und5936B Ergebnisarray.
Angeforderte Bytes sind weder eindeutige/live Bytes noch gesamte interne Kopien.
Dual-Simplex bleibt für diesen Versuch gewählt; die dichte Python-Formulierung
bleibt teuer und ist kein allgemeiner Laufzeitpfad. Vor einer Bibliotheksübernahme
sind kleinere Variablen-/Objektmengen und ein vollständiger Kontextnachweis nötig.

Vier tatsächliche Vorher-/Nachherbilder1280×720 geöffnet:500-m-Übersicht und
110-m-Diagnose am fernen Anschluss; EEVEE5.2.2/llvmpipeLLVM19.1.7,16Samples,
zwei Threads/nice10. Renderzeiten3.790/3.822/4.059/5.008s,
Peak-RSS1724648–1726628KiB. `pixels.py`:9372 veränderte Übersichtspixel,
39631 Diagnosepixel. Die neuen Verbindungen und der angehobene Fernanschluss
sind sichtbar; die lange Gesamtachse bleibt dünn. Keine umliegende Stadt/DEM
hinzuerfunden und keine Präsentationsgalerie veröffentlicht.

Kanonische lokale Belege: `/tmp/outshine-transport-stair-context-32f2e389b/`,
Kosten unter `/tmp/outshine-transport-stair-context-cost-32f2e389b/`.
[Reproduktion](README.md). Original-PBF und DEM-Fenster bleiben versioniert;
Rohdownload-Caches und frühere echte Gegenbeispiele bleiben erhalten.
Offen: vollständige Kontextfreiheit, gewählte Außen-/Geländekontakte,
Handläufe/Gehhüllen, Stützen/Statik und native Wiedergewinnung bei master.
