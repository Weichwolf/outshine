# Ganze Pfeilerkontakte unter dem unveränderten Wiener Verbund (#434)

Die langen Rad-/Fußbrückendecks des [Südstands](stair_cycle_south_comparison.md)
haben bisher keine sichtbaren Pfeiler. Derselbe installierbare
[SupportVolumeBuilder](../../../lib/supports.md) ergänzt geometrisch ganze
Stützen zwischen wirklichem Terrain und tatsächlich ausgewählten Brückenunterseiten.
Der vorherige Verkehrs-/Terrainkörper, seine Quellen, Profile und Materialien
bleiben unverändert. Kein SDK, Fremdcode, neue gemeinsame/lib/native/Reservations-API.

## Gestaltung, Gegenfälle und ganze Kontaktprüfung

Alle als Brücke deklarierten Quellen erhalten ab 6 m Innenstationen im 12-m-Rhythmus;
kurze Quellen ohne Innenstation behalten ihren ausdrücklichen Grund. Eine Stütze
misst 0,8 m längs und 0,45 m quer zur tatsächlichen lokalen Achse. Die feste Variante
verwendet nur die Sollstation, die zweite versucht 0/−2/+2/−4/+4 m. Alle tatsächlichen
anderen ausgewählten Verkehrsflächen einschließlich Treppen und Brücken sind
konservativ mit direktem `dwithin`-Abstand von 0,2 m ausgeschlossen, unabhängig von ihrer Höhe. Das wird
für ursprüngliche und tatsächliche Float32-Geometrie separat geprüft. Weitere
Pfeiler dürfen sich ebenfalls in keiner Präzision berühren. Kein Hindernisnachweis
für die weiterhin nicht zugelassenen übrigen Quellen.

Der Bibliothekskern zerlegt den ganzen Fußabdruck gegen beide ursprünglichen
stückweise affinen Kontaktfelder. Ein Höhenwert am Mittelpunkt oder an den vier
Ecken reicht nicht: ein Innenpeak bleibt echter Gegenfall. Ganze obere/untere
Kappen und senkrechte Randwände müssen exakt zum gespeicherten Körper gehören;
positive Höhe, geschlossene orientierte Kanten, zyklische Vertexlinks, positive
Volumina und unveränderte Flächenorientierung bleiben in Double/Float32 Pflicht.
Ein Double-gültiger schmaler Körper kann nach Float32-Rundung kollabieren und
wird dann abgelehnt. Die Wahl verlangt 0,1001 m Double-Höhe als Reserve; die
Abnahme verlangt mindestens 0,1 m in beiden tatsächlichen Präzisionen.

Der Reader bestimmt den maximalen ganzen Double-Kontaktfehler e mit beiden
vorzeichenbehafteten Flächenabständen. Lokale Originalkontaktflächen müssen den
Fußabdruck samt tatsächlichem XY-Rundungsradius abdecken, damit ihr größter
affiner Gradient L dort eine Lipschitz-Grenze ist. Aus den tatsächlich gerundeten
Vertexkoordinaten folgt derselbe ausreichende Umschlag wie beim Terrain:
`e + dz_cap + dz_reference + L * (dxy_cap + dxy_reference)`.
Die tatsächlichen beiden Float32-Flächenabstände müssen darunter bleiben
(numerischer Vergleichszuschlag 1e−10 m); die ganze tatsächliche Kappe muss
zugleich innerhalb der gerundeten Referenz liegen. Kein exakter Nullkontakt oder
Null-Überlappungsvolumen aus einer Vertex-/Solver-/Prozessmeldung abgeleitet.

SIGGRAPH zuerst: Chen et al. 2008, [lokales Paper](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
S. 2/Fig. 2: Graph, Profile und Geometrie bleiben getrennte Stufen; optisch bereits
im [Support-Vergleich](support_library_comparison.md) geprüft. Ergänzend Galin et
al. 2010, [Paper](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
S. 8/Fig. 17–18, für diesen Versuch erneut optisch geprüft: Terrainübergänge und
getrennte Straßen-/Brückenrezepte. Hier werden keine Tragwerksformel oder ganze
Paper-Pipeline übernommen. Maße, Stationssuche und Rundungsabschätzung sind
explizite eigene Gestaltungs-/Prüfentscheidungen; keine Statik.

## Geprüfter Stand und tatsächliches Bild

Physische Erzeugung, unabhängiger Leser und vier frische Bilder auf
`7c223fde5791e1023351bab963a33e319d176c3c`. 21 Kontrollen (neun neue und zwölf
bestehende Kernkontrollen), 0,179 s, terminal OK. Der Reader besteht zusätzlich
mit gesperrtem SupportVolumeBuilder, CDT und LP; keine neue Passung/Konstruktion.
Die Innenpeak-/Float32-Kollaps-/Verschiebungs-/Budget-/Konfliktgegenfälle bleiben
abgelehnt. Ein zusätzlicher Eckgegenfall zeigt: ein polygonaler Rundpuffer kann
0,1999 m tatsächlichen Abstand als außerhalb seines 0,2-m-Bandes behandeln.
Direkte Abstände schließen diese Lücke. Alle drei Körper-/Darstellungsdateien
bleiben gegenüber dem vorigen Pufferstand bytegleich; die Kontrolle ist stärker.

Feste Stationen liefern 66 gültige Stützen, Verschiebungen alle 70 Stationen an
fünf Brückenquellen. Der kurze Quellteil 137774950:5 erhält ausdrücklich keine
Innenstation. Vier feste Stationen scheitern an anderem Verkehr, zu geringer
ganzer Höhe oder einer nicht konstruierbaren Kontaktzerlegung; keine solchen
Körper werden exportiert. Größte Achsstationslücke 36/14 m. Die Verschiebung
bleibt ≤4 m; keine Neuplanung oder Veränderung des vorherigen Verkehrs/Terrains.

Die ausgewählte Variante besitzt 1374 V/2468 F, Körper-/Kappenarrays 105128 B,
610 Kontaktfacetten. Alle Körper sind in beiden Präzisionen geschlossen,
orientiert, ohne kollabierte/umgedrehte Flächen und mit zyklischen Vertexlinks.
Kleinste tatsächliche ganze Höhe 0,108243 m; kleinster gemessener Abstand zu
anderen ausgewählten Verkehrsflächen 1,083503 m in den beiden Präzisionen.
Größter ganzer Double-Kontaktfehler 2,8422e−14 m, tatsächlicher Float32-Fehler
15,235 µm bei größtem hergeleitetem Umschlag 24,913 µm. Keine Vermessungsgenauigkeit,
kein exaktes Null-Überlappungsvolumen und kein Statiknachweis daraus abgeleitet.

Vier frische vollständig decodierte und tatsächlich geöffnete 1280×720-PNGs:
gleiche Übersicht und ganzer Brückenverbund mit 420 m Kameraspanne, je vorher/
nachher. Exakte Körper-/Szenen-/Import-/Normal-/Farb-/Helperbindung geprüft.
Die bisherigen Arrays sind vollständiger unveränderter Präfix. Pfeiler und ihre
Schatten machen die vorher schwebenden Decks im näheren Bild sichtbar gestützt;
kein enger Detailausschnitt und keine ausgesparte Geometrie. Pixelvergleich:
4665 geänderte Pixel am Verbund, 845 in der Übersicht; lokalisierte Änderungen,
kein pauschales Bildqualitätsmaß. Blender 5.2.2 LTS EEVEE/llvmpipe, kein nativer
Outshine-/GPU-/A18-/RTX-Nachweis. Bilder bleiben lokal, keine Ersatzgalerie.

## Gleiche Qualität und Kosten

Sieben normale vollständige Pläne je Strategie plus je ein Warmplan, alle
gepackten Körper-/Kappenarrays exakt gegen den unabhängig geprüften Export.
Ein Worker je Strategie, nacheinander bei normaler Priorität, keine eigenen
Builds/Renderer daneben. Host i5-1135G7, zwei physische/vier logische CPUs,
16 GiB RAM; übrige Fachlast nicht isoliert. Beide Strategien verlangen dieselbe
physische Qualität, liefern aber unterschiedlich viele gültige Pfeiler; deshalb
kein Geschwindigkeitsfaktor aus den folgenden Zeiten.

| Gemessene ganze Planung inklusive Vorbereitung und physischer Abnahme | Fest | Verschoben |
|---|---:|---:|
| Gültige Stützen | 66 | 70 |
| Versuche | 70 | 77 |
| Tatsächliche Kontaktpaarprüfungen akzeptierter Stützen | 9832 | 10678 |
| Originalflächenkandidaten akzeptierter Konstruktionen | 334 | 361 |
| Noding-Paarkandidaten akzeptierter Konstruktionen | 2646 | 2884 |
| Ganze Phase, Median ms | 2785,888 | 2900,004 |
| Prozess-Peak B | 152125440 | 152408064 |
| Ausgegebene Körper-/Kappenarraybytes | 98076 | 105128 |

Getrennte Memray-1.20.0-Phasen nach Imports/Quelldecode, mit vollständiger
Vorbereitung, Konstruktion und Double-/Float32-Prüfung: alle Arrays erneut exakt.
10112413/10360734 beobachtete Allokationsereignisse,
1375686712/1406805417 angeforderte B, 46086540/45037964 B getrackter Heap-Peak;
profilierte Phasen 5629,109/5708,753 ms, Prozess-Peaks 163102720/163250176 B.
Python-Allocatorereignisse und native Stack-Traces sind ausgeschaltet; native
Allokationen werden weiterhin erfasst. Keine Gesamtzahl aller Python-Allokationen,
keine unverfälschten Laufzeiten und keine Gleichsetzung kumulativer Bytes mit RAM.
Ein früherer Lauf mit nativen Stack-Traces überschritt das unveränderte äußere
Zeitbudget und lieferte nur ein Teilprodukt: als Gegenbeleg erhalten, nicht als
Kostenbeleg für den vollständigen Stand. Die Grenzen wurden nicht erhöht.

Das vollständige Darstellungsarray wächst 6710032 →7078656 B. Dessen aktuelle
Concatenate-/Cast-Zielpuffer kopieren nach tatsächlichen Datentypen 7078656 B;
dies ist ein expliziter Phasenzähler, nicht der gesamte Kopierverkehr. Erzeugungs-
und Prüfallokationen sind erheblich größer als das Produkt; weitere gemeinsame
Vorbereitung ist ein gemessener Optimierungsbedarf. Interne Gesamtkopien bleiben
unbeobachtet. Aufbau, Offline-Rendering, Lade-/Cachekosten und Framearbeit sind
getrennt; keine Runtime- oder Geräteabnahme.

Produkte, Originalbilder, terminale Logs und Gegenfälle:
`/tmp/outshine-transport-stair-support-7c223fde5/`;
Kosten/Profiler: `/tmp/outshine-transport-stair-support-cost-7c223fde5/`.
Diese lokalen Belege sind keine feste Test-/Runtime-Abhängigkeit.

## Arbeit, Reproduktion und offene Abnahme

[Aufrufe](README.md). Höchstens 128 Stationen/640 Versuche, kooperative 8 s pro
Strategie einschließlich Vorbereitung/Prüfung; Kern und einzelne Abstandsanfragen
haben eigene kooperative 2-s-Limits, so dass ein bereits laufender Schritt die
äußere Zeit überschreiten kann. Größen-/Paargrenzen gelten zusätzlich; ganze
Prozesse 120 CPU-s/3 GiB. Abbruch erhält ein gültiges Teilprodukt mit ausdrücklichen
Auslassungen. Keine unbegrenzte Wartezeit und kein erneuter Netz-/Terrainfit.

Geometrisch belegt ist nur der gewählte endliche Verbund. Fundamente/Lasten,
Bodenkennwerte, statisch sichere Spannweiten, Fahr-/Gehhüllen, Handläufe, übrige
Quellen und native Übernahme bleiben offen. Stationslücken sind entlang der
Originalachse einschließlich ihrer Enden gemessene Gestaltungsmetriken; Enden
sind damit noch keine nachgewiesenen Widerlager. #434 bleibt aktiv.
