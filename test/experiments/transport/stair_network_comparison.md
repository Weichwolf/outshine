# Drei wirkliche Wiener Treppen mit gemeinsamem Fuß-/Radverbund (#434)

Der Versuch verbindet jetzt15 tatsächliche Quellteile mit drei Treppen,
zwei langen parallelen Brücken,13 ganzen gemeinsamen Kontaktbereichen und vier
Unterführungen. Eine prozedural verlängerte Radauffahrt erhält alle tatsächlichen
Anschlussstellen;24 echte Stufen ersetzen die drei glatten Treppenhüllen.
Das bleibt ein lokaler Forschungskandidat:66 weitere Kontextachsen und zwei
äußere Profile sind nicht freigegeben. #434 bleibt aktiv, native Abnahme bei master.

## Quelle, tatsächliche Anschlüsse und notwendige Formänderung

[Original-PBF, native Priors und tatsächliches DEM-Fenster](stair_context_comparison.md)
bleiben unverändert maßgeblich. Neue kleine [Planungseingabe](fixtures/stair-network-vienna.json),
SHA256 `e7070b9a01d05664f3708c4ae1fb425949a6d9d34e6808dda9233c6e57a13305`,
bindet den bisherigen Kontext per SHA. Kein gelöstes Höhenfeld im Fixture.
Die vier ursprünglichen nativen Priors bleiben erhalten; weitere Priors stammen
aus dem gleichen versionierten98×123-DEM-Fenster. Keine neue Netzwerkaufnahme.

Zusätzlich zu den sieben Quellen aus PR #459 kommen diese acht tatsächlichen Teile:

| Index | Feature:Teil | Funktion |
|---|---|---|
| 7 | 377113090:0 | zweite Treppe |
| 8 | 229812042:0 | ihre Fußweg-Rückbrücke |
| 9 | 229812060:2 | parallele lange Fußbrücke |
| 10 | 137774950:6 | ferner Fußbrückenanschluss |
| 11 | 80922970:339 | tatsächlicher unterer Fußweg |
| 12 | 137774950:5 | kurzer Anschluss mitten auf der Radbrücke |
| 13 | 258321780:54 | dritte, kurze Treppe |
| 14 | 80922970:337 | kurzer unterer Fußweg an der Radauffahrt |

Alle IDs tragen `openfreemap:14/8937/5678:transportation:`. Vollständige ursprüngliche
Features, Properties/MVT-Punkte und Quellkoordinaten bleiben im gebundenen Kontext.
Die beiden80922970-Wege sind damit lokal konstruiert, nicht nativ/global freigegeben.
Klassenbreiten/-grenzen kommen unverändert aus dem vorhandenen Projektprofil:
Rad2m/6%, Fuß1.8m/15%, Treppe1.5m/50%-Hülle. Keine Grenze gelockert.

Eine tatsächliche Quellachse endet neben einer anderen; ihre ganzen Bänder
überlappen trotzdem. Der Plan wählt deshalb ausdrücklich den vollständigen
6/14-Breitenkontakt sowie14 am vorhandenen2/6-Endpodest. Das ist kein erfundener
gemeinsamer MVT-Vertex. Getrennte Überführungen desselben Quellpaars bleiben getrennt.
Ein ausgelassener Breitenkontakt wird vom Modell als ungeplant abgelehnt.

Das vollständige15-Quellenmodell mit allen ursprünglichen Achsen ist bei2.82m
Oberflächenabstand unvereinbar. Eine zusätzliche Maximierung des gemeinsamen
Abstands ergibt numerisch1.675463630210m; geprüfte duale obere Schranke
1.675463630211m, Stationaritätsrest9.11e-15, primärer Rest2.92e-11,
Dualitätslücke4.45e-16. Dieser Nachweis gilt nur für diese endliche Anordnung,
gewählten ganzen Podeste, Profile, Float32-Bedingungen und Höhenbudget; keine
exakte Intervallrechnung oder globale/native Unmöglichkeitsbehauptung.

Der nächste Versuch ändert daher die Lage statt die Neigungs-/Freiraumgrenzen:
Nach dem tatsächlichen Fußweganschluss erhält Radquelle6 zwei neue Zwischenpunkte
bei1/3 und2/3 der verbleibenden Sehne, jeweils um denselben seitlichen Betrag
versetzt. Beide Enden und der innere Fußweganschluss bleiben exakt am Quellort.
Die erste Strecke bleibt unverändert. Die vorherige ganze Achse bleibt zusätzlich
als `original_xy_m` erhalten. Keine einzelne neue Weltposition ist handmodelliert.
Begrenzte Varianten0/6/12/18m:0/6/12 unvereinbar,18 gültig. Das ist die erste
gültige Variante dieser kleinen Menge, kein optimales Lageverfahren.

Radquelle6 wächst von36.415113 auf58.217201m; größter Abstand eines neuen
Zwischenpunkts zur ursprünglichen Achse13.003312m. Die18m sind der gewählte
Versatz zur Sehne, kein behaupteter Hausdorff-Abstand. Neue Punkte erhalten
tatsächliche DEM-Priors über die inverse lokale/geographische Projektion und
unabhängige Fensterinterpolation. Gegenprüfung gegen den vollständigen ursprünglichen
Terrarium-Decode über `Elevation`: maximale Höhenabweichung0. Alle anderen14
Quellachsen und sämtliche ursprünglichen Feature-Dictionaries bleiben unverändert.
Kein gemeinsamer/lib/native/Reservationsvertrag geändert, kein SDK zur Laufzeit.

## Körper, unabhängige Gegenprüfung und Bild

Freeze `600d02f42e198928bba497749e2f60165c1d2eb0`.
Python3.13.5, NumPy2.2.4, SciPy1.18.1/HiGHS1.12.0,
Shapely2.2.0/GEOS3.14.1. Die finite Flächen-/Float32-Herleitung und
SIGGRAPH-/Eurographics-Primärquellen stehen im [ersten Bericht](stair_junction_comparison.md).
Graph, Lage, Profil, Stufen und Terrainbedarf bleiben getrennte eigene Konstruktionen.
Die ganzen breitenbasierten Podeste einschließlich0.75m Zusatz sind erhalten.

3171Variablen/69234Ungleichungen/204792Sparse-Nonzeros; jede Variante LP-Zeitlimit5s,
Prozesse3GiB/90CPU-s. Frischer vollständiger Erzeuger einschließlich vier Varianten,
zusätzlicher Kapazitätsgegenprobe, Vorher-/Nachherkörpern und Audits:
16.311647s/225960KiB Peak-RSS. Das ist kein Echtzeit- oder Weltladebudgetnachweis.

Nachher3598Vertices/7216Flächen,2550obere und252Setzstufenfacetten,
Volumen585.993520m³. Treppen0/7/13 haben12/7/5Stufen mit17.181757/16.703479/
16.769744cm Höhe. Alle Trittbänder zusammenhängend und mit0.28-m-Scheibenplatz;
kleinste zulässige Scheibenmittelpunktfläche0.122649m². Tatsächliche Float32-
Setzstufen höchstens17.182923cm; ganze Podestabweichung0, Monotonieverstoß0.
Größte tatsächliche Float32-Radnorm0.05991177, Fußwegnorm0.14808631.

Alle positiven nicht gemeinsamen Bandüberlappungen sind erfasst, auch als
Flächenprüfung unabhängig von bloßen Achsschnitten. Vier getrennte Unterführungen
1/2,6/2,5/9 und11/9: Double jeweils mindestens2.52m, tatsächlicher Float32-Export
mindestens2.519989014m zwischen unterer Verkehrsfläche und oberer Körperunterseite.
Jede ganze geplante Quellbandfläche genau einmal; geschlossene gerichtete Ränder,
3598 zusammenhängende zyklische Vertexlinks, keine kollabierte/invertierte
Float32-Fläche. Das ersetzt keine allgemeine Selbstschnitt-, Statik-, Handlauf-,
Geh-/Fahrhüllen- oder gesetzliche Treppenprüfung.

Nachher-Körper-SHA256 `a196e3e8140042e935e08e72b6b05ee0715eb55f997b79aec31936bb06df6997`;
Feld-SHA256 `9720cc92791f4b1943bc5f5776b379c64d056f59fca307ba9be6a83df056717e`.
Alle elf bisherigen Vier- und Siebenachsen-Körperarrays bleiben byteidentisch.
59 betroffene Kontrollen bestanden; neun neue behandeln tatsächliche drei Treppen,
ursprüngliche Unvereinbarkeit/duale Schranke, feste Anschlüsse/Quellidentität,
fehlenden Breitenkontakt, ganze Überlappungen, vertauschte Treppenbesitzer,
zweiten LP-Algorithmus und den echten Gegenfall zweier nur an einem Vertex
berührender kanten-geschlossener Tetraeder. Letzterer scheitert am Vertexlink.

Der gespeicherte Reader löst/trianguliert nicht neu. Er bindet Eingaben/Profilrezept,
geplante Achsen und Priors, ganze Körper/Felder/Quellen, alle13Podeste, drei
Stufenfolgen, vier komplette Unterführungen, tatsächliche Darstellungsarrays,
zwei vorgeschlagene Außenprofile und66 explizit noch nicht freigegebene Quellen.
`admitted_source_ids=[]` bleibt im Ergebnis und Reader erhalten.

Sechs tatsächliche1280×720-Bilder geöffnet: Vorher/Nachher in500-m-Übersicht,
110-m-Ost- und110-m-Westansicht. EEVEE5.2.2/llvmpipeLLVM19.1.7,16Samples,
zwei Threads/nice10,4.538–5.988s und1724764–1729668KiB Peak-RSS je Render.
`pixels.py`:13522/65334/24253 veränderte Pixel in Übersicht/Ost/West.
Zweite parallele Brücke, zusätzliche echte Treppen und größere Radschleife sind
sichtbar. Blau sind hier die konstruktiven Bandkörper, braun die Stufen;
keine erfundene umliegende Stadt/DEM und keine veröffentlichte Ersatzgalerie.

## Kosten und verbleibende Außenanbindung

Identische endliche Bedingungen und ganze akzeptierte Körperqualität, sieben normale
Wiederholungen nach warmen Importen und beiden Solvern; BLAS/OMP je1, Prozesse
nacheinander nach Abschluss eigener Renderer. Geteilte Maschine, kein isolierter
Gerätenachweis. Ganzkörperaudits außerhalb der gemessenen Aufbereitung/LP-Phase.

| Messung | Dual-Simplex | Interior-Point |
|---|---:|---:|
| Aufbereitung + LP, Median | 2034.501ms | 4344.882ms |
| normaler Worker Peak-RSS | 230187008B | 228831232B |
| getrennt instrumentierte Phase | 7680.883ms | 9908.589ms |
| Allokationsereignisse | 5597548 | 5583670 |
| angeforderte Bytes | 819013504B | 1188740575B |
| getrackter Heap-Peak | 164369983B | 164145276B |

Memray1.20.0 separat, nicht mit normalen Zeiten vermischt. Beide behalten2470192B
Sparse-Arrays,37584B explizite Float32→Double-XY-Kopien und12528B Ergebnisarray.
Angeforderte Bytes sind keine eindeutigen/live Bytes oder vollständigen internen
Kopien. Dual-Simplex bleibt lokal gewählt; die Python-Formulierung ist teuer und
noch kein allgemeiner Bibliotheks-/Native-/Framepfad.

Das bisherige Außenprofil am zweiten Treppenfuß ist jetzt tatsächlich innerlich
angebunden. Zwei neue Profile bleiben vorgeschlagen: Radquelle4 endet unverändert
bei165.064279m. Radquelle6 beginnt jetzt bei164.411168–164.452583m statt dem
166.631714-m-Prior, also2.179130–2.220546m tiefer. Das verlangt echte gewählte
Nachbar-/Geländekontakte und zusammenpassende Cut-/Fill-Produkte; ein Prior allein
beweist sie nicht. Zusätzlich bleiben66 Kontextachsen einschließlich U-Bahn-Trassen,
weiterer Bodenwege und Plattformen ungeprüft. Keine native/global vollständige
Rückgewinnung der ursprünglich ausgelassenen Quellen behauptet.

Kanonisch `/tmp/outshine-transport-stair-network-600d02f42/`, Kosten
`/tmp/outshine-transport-stair-network-cost-600d02f42/`;
[Reproduktion](README.md). Originaleingänge und frühere echte Gegenfälle bleiben
erhalten. Nächste fachliche Abnahme: die beiden Außenprofile und tatsächlichen
restlichen Nachbar-/Gelände-/Ebenenkontakte gemeinsam planen und ganz prüfen.
