# Ganze reale Nachbarwege am Wiener Treppenverbund (#434)

Die bisherigen beiden Außenprofile sind jetzt über ihre ganze Breite an tatsächlich
gewählte Nachbarflächen gebunden.18 vollständige Quellteile, drei Treppen/24Stufen,
15 gemeinsame Podeste und sechs Unterführungen sind lokal geprüft. Fünf weiter
außen liegende Profile sowie übrige Quellen und Terrain-/Cut-/Fill-Produkte bleiben
offen. Das ist weiterhin kein vollständig freigegebener Wiener oder weltweiter Transportgenerator.

## Feste Eingänge und gemeinsame Planung

Originale, Frame, native Höhenpriors und Herkunft bleiben wie im
[vorigen Netzvergleich](stair_network_comparison.md). Maßgeblich ist die unveränderte
OpenFreeMap/OpenMapTiles20260927_080001_pt-PBF14/8937/5678,
SHA256 `97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
Die [neue Planungseingabe](fixtures/stair-outer-vienna.json), SHA256
`a2989e49b3df55ee9d28f72e39b096ef9b81877017a6a96c33671b202398ab73`,
bindet den bisherigen15-Quellenplan und ergänzt drei vollständige Originalteile:

| Index | Feature:Teil | Achslänge | Breite / Gradientengrenze |
|---|---|---:|---:|
| 15 | 39881120:28 | 807.304432m | 2m / 0.06 |
| 16 | 4420992360:6 | 4.299572m | 1.8m / 0.15 |
| 17 | 39881120:34 | 176.324851m | 2m / 0.06 |

IDs tragen weiterhin `openfreemap:14/8937/5678:transportation:`. Alle ursprünglichen
Properties/Feature-Dictionaries und17 andere Achsen bleiben erhalten. Einzig Quelle6
erhält eine neue geplante Radauffahrt; ihre ursprüngliche Achse bleibt separat erhalten.

Das echte Mapterhorn-Raster14/8937/5678 bleibt dieselbe Quelle,
SHA256 `67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177`.
Das größere [231×260-Float32-Fenster](fixtures/stair-outer-vienna-dem.npz),21125B,
SHA256 `0f726d835cf5a05e311b745fb6883e86c3a393d9fd724fe484fd7019ef962371`,
beginnt bei Pixel252/211 des512×512-Rasters. Explizite Offline-Neuaufnahme aus dem
erhaltenen Rohreceipt ist bytegleich. Neue Rampenpunkte stimmen gegen den vollständigen
ursprünglichen Terrarium-Decode über `Elevation` exakt überein. Native Drapierpriors
bleiben getrennt; daraus folgt keine Identität mit dessen ursprünglicher DEM-Aufbereitung.

Chen et al., SIGGRAPH2008, S.2/Fig.2 trennt Netz und Geometrie; Galin et al.,
Eurographics2010, §6/Fig.17–18 trennt Profile, Bauwerke und Terrainänderungen.
Beide PDF-Seiten wurden hier wieder optisch geprüft; [Primärquellen](../../../doc/references/transport/README.md).
Die folgende endliche Lage-/Profilkonstruktion ist eigene Arbeit, keine Übernahme
von Paper- oder dekompiliertem Fremdcode. Keine SDK-Abhängigkeit.

## Gemessene Gegenfälle und gefaltete Auffahrt

Der bisherige Zweipunktversatz passt mit den ganzen drei Nachbarn nicht mehr:
gemeinsame Oberflächenkapazität numerisch höchstens2.581456m statt benötigter2.82m.
Eine erste gefaltete Rückführung mit Rückkehrstation0.5 gerät mit ihrem ganzen
Bandrand erneut unter das Deck; ihre geprüfte numerische obere Grenze ist1.624235m.
Primäre/duale Residuen und Unvereinbarkeitsstatus stehen im frischen Variantenbericht.
Dies gilt nur für diese endlichen Anordnungen und Grenzen, nicht für beliebige
Layouts, native Operanden oder die weltweite Lösbarkeit.

Das eigene Faltrezept arbeitet im Sehnenrahmen zwischen tatsächlichem innerem
Fußweganschluss und Brückenende. Außen-/Innenabstand sind sechs/zwei Bandbreiten;
die Rückkehr liegt bei Station0.7, der Vorlauf ist begrenzt. Varianten6/12/18m
werden bis zur ersten ganzen gültigen Geometrie geprüft;6m besteht bereits.
Keine Weltposition ist handmodelliert. Beide Enden und der innere Quellanschluss
bleiben exakt am Ort. Quelle6 wächst von ursprünglich36.415113m beziehungsweise
58.217201m im isolierten15-Quellenversuch auf70.599159m.

Ein tatsächlicher6/14-Achsschnitt liegt0.203m neben dem gewählten Breitenkontaktpunkt.
Die Einordnung prüft deshalb den ganzen deklarierten flachen Kontaktbereich. Er
bleibt eine gemeinsame Fläche. Getrennte Überführungen werden weiterhin anhand
aller nicht gemeinsamen Bandüberlappungen überprüft; keine Freiraumbedingung entfernt.
Neigungen, Breiten, ganze Podeste einschließlich0.75m Zusatz und Float32-Reserven
bleiben unverändert. Knickradien, Fahr-/Gehhüllen und rechtliche Treppenabnahme fehlen.

## Ganze Kontakte und gespeicherte Körper

Geometrie-/Bild-/Kostenfreeze:`87d1deb53844740a79f78b5875a1e0e0140c3bd0`.
Python3.13.5, NumPy2.2.4, SciPy1.18.1/HiGHS1.12.0, Shapely2.2.0/GEOS3.14.1.
4007Variablen/88326Ungleichungen/261368Sparse-Nonzeros. LP-Limit5s,
Prozesse3GiB/90CPU-s. Erzeugerphase in frischem Prozess nach Python-Imports,
einschließlich Gegenfällen, Kapazitätsprüfungen, Vorher/Nachher und vollem Katalog:
24.824318s/272692KiB Peak-RSS. Kein Frame-, Weltlade- oder Gerätenachweis.

Nachher4416Vertices/8848Flächen,3146obere/244Setzstufenfacetten,
Volumen1183.533520m³. Treppen0/7/13 haben12/7/5Stufen mit17.181757/16.703479/
15.665388cm Höhe; tatsächliche Float32-Setzstufen maximal17.182923cm.
Zusammenhängende Tritte mit0.28-m-Scheibenplatz, alle15Podeste flach/gemeinsam,
Monotonieverstoß0. Größte tatsächliche Float32-Radnorm0.05987347,
Fußwegnorm0.14808631. Ganze geplante Bandflächen genau einmal; geschlossene
gerichtete Ränder und4416 zusammenhängende zyklische Vertexlinks. Keine kollabierte
oder invertierte Exportfläche; keine allgemeine Selbstschnitt-/Statikfreigabe.

Alle sechs Unterführungen1/2,6/2,17/2,5/9,11/9 und17/9 haben Double mindestens2.52m,
Float32 mindestens2.519989014m tatsächliche lichte Körperhöhe.
Nachher-Körper-SHA256 `104f44633cfebac880108761cbb571366cf9fe2ea3b60ce07ebe3a566d428ef2`,
Feld-SHA256 `5e4dad92510739691d155a42a7637e556813c6e220e8d69d4b737d36e5e67475`.
Die bisherigen Vier-/Siebenachsenkörper bleiben in allen11Arrays, der15-Quellenkörper
in allen13Arrays bytegleich.

Die beiden früheren Außenprofile liegen vollständig auf gemeinsamen indizierten
Kappen mit ihren tatsächlichen Nachbarn:4/15 bei165.062096689m und6/17 bei
164.160355031m. Ganze Linienabdeckung auch nach tatsächlicher Float32-Rundung,
mit aus XY-ULPs hergeleiteten Guards3.815µm beziehungsweise61.036µm; unbedeckte
Länge0. Die Kappen sind jeweils flach und teilen ihre tatsächlichen Körpervertices.
Ein Verlust der Nachbarzuordnung wird abgelehnt. Fünf neue Außenprofile bleiben
ausdrücklich vorgeschlagen: beide Enden von15/17 und der freie Anfang von16.

70 betroffene Kontrollen bestanden. Der unabhängige gespeicherte Reader löst und
trianguliert nicht neu: Quellen/Profiles/NPZs, alle ganzen Felder/Podeste/Stufen,
Exportgrenzen, beide echten Anschlussprofile und alle übrigen Quellen werden gebunden.
Die neue kleine [Forschungspalette](fixtures/research-palette.json) macht Reproduktion
und Replay mit versionierten Eingängen eigenständig. Tatsächlich verwendete RGBA-
Werte, Indizes und Normalen werden weiterhin exakt geprüft; der vollständige alte
Palette-Datei-SHA bleibt Herkunftsangabe. Unbenutzte private Szenenpfade beeinflussen
diesen Vergleich nicht. Eine Änderung der tatsächlich verwendeten Farbe scheitert.

## Vollständige Quellenbilanz und Bilder

Die Originalkachel enthält572Features:1657Linienteile plus71Verkehrsflächen.
18Linienteile sind Kandidaten;1639Linienteile und alle71Flächen bleiben mit
Original-ID/Properties und konkretem Nichtverwendungsgrund im `source-catalog.json`.
39Linienreferenzen berühren den Kandidatenraum:35mit bekannten Bandbreiten und vier
als unbekannte Profile mit32-m-Achssuchfenster. Das Fenster ist keine Kollisionsgarantie.
Sieben tatsächliche Polygone überlappen; sechs tragen `class=bridge`, eines `pedestrian`.
Mehrfachrepräsentationen/physische Ebenen sind vor einer Freigabe zu klären.
Unter anderem beide81129490-U-Bahn-Teile sowie4935280960:0 bleiben ungeplant.
Ein vollständiger Katalog ersetzt keine Höhen-/Kontakt-/Freiraumprüfung.
`admitted_source_ids=[]` bleibt erhalten.

Sechs1280×720-Bilder tatsächlich geöffnet: gemeinsame1020.544-m-Gesamtansicht mit
allen Eingabevertices im Bild,110-m-Ost/Westdetails; jeweils vorher/nachher.
EEVEE5.2.2/llvmpipeLLVM19.1.7,16Samples, zwei Threads/nice10;
6.878–11.775s und1681604–1729260KiB Peak-RSS je Render.
Alle sechs Blender-Unterläufe meldeten terminal0; die übergeordnete Hülle endete
danach mit143. Separate Prüfung von Frische, PNG-Dekodierung,1280×720, Bild-/Szenen-/
Mesh-SHAs und tatsächlichen Importverträgen besteht terminal0; die Hülle wird nicht
als erfolgreich gewertet. Keine Unterläufe neu gestartet. `render-completeness.json`
bewahrt beides. `pixels.py`:6548/68075/26957 geänderte Pixel in Gesamt/Ost/West.
Ganze Nachbarwege und zusätzliche Durchfahrten sind sichtbar. Nur technische
Bandkörper/Stufen, kein gerendertes Gelände, keine veröffentlichte Ersatzgalerie.

## Kosten und verbleibende Geländeanforderung

Gleiche finite Bedingungen und Ganzkörperqualität; sieben normale DS-Wiederholungen
nach warmen Importen und beiden Solverversuchen. BLAS/OMP je1, Prozesse nacheinander,
eigene Renderer abgeschlossen, geteilter Linux-Host. Während des IPM-Normallaufs
lief auch ein CPU-Pixelvergleich; dessen Abbruch trat bereits im ruhigen DS-Warmup
und separatem Kontrollversuch auf. Kein isolierter Gerätevergleich.
DS-Median2530.021ms, normaler Peak-RSS264388608B; jeder gemessene Körper separat
außerhalb der Phase geprüft. IPM bricht am unveränderten5-s-Solverlimit ab:
5987.169ms inklusive Aufbereitung, `accepted_model=false`, kein gültiger Median/Output.

Separate Memray1.20.0-Aufnahmen: DS9456.884ms,7114123Allokationsereignisse,
1043335023angeforderte Bytes,209901440B getrackter Heap-Peak. IPM-Abbruchphase
12383.021ms,6620471Ereignisse/1118210906angeforderte Bytes/103111656B Heap-Peak;
das ist keine gleichwertige erfolgreiche Ausgabe. Beide Modelle3152448Sparse-Bytes,
explizite Float32→Double-XY-Kopien47544B; gültiges DS-Ergebnis15848B.
Angeforderte Bytes sind weder eindeutige/live Bytes noch gesamte interne Kopien.
DS bleibt lokal gewählt; die dichte Python-Formulierung bleibt teuer.

Die Achsdiagnose gegen das wirkliche DEM zeigt−3.386244 bis+1.582689m Änderung.
Das6/17-Anschlussprofil liegt2.471359m unter seinem DEM-Prior. Diese Werte beweisen
keine ganze Terrainanbindung, Cut-/Fill-Geometrie oder Volumina. Solche Produkte,
restliche räumliche Ebenen/Kontakte, fünf äußere Anschlüsse, Fahrhüllen und native
Wiedergewinnung bleiben offen. #434 bleibt aktiv.

Kanonisch `/tmp/outshine-transport-stair-outer-87d1deb53/`, Kosten
`/tmp/outshine-transport-stair-outer-cost-87d1deb53/`; [Reproduktion](README.md).
Originalquellen und frühere echte Gegenfälle bleiben erhalten. SDK- und native
Runtime-Abhängigkeiten wurden nicht eingeführt.
