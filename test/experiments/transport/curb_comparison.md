# Straßenrandprofile auf tatsächlichem Wiener Boden (#364)

## Ergebnis und Grenzen

256 geschlossene Bordkörper ergänzen den Python-Forschungsstand aus #357 auf
9.355,649961 m². Alle bisherigen Verkehrsflächen, Körper, Normalen und Beläge
bleiben als exakte Arraypräfixe erhalten. Neue Geometrie: 25.934 Vertices,
50.932 Faces; gesamte Darstellung: 410.862 Vertices, 738.456 Faces.
Dies ist ein begrenztes Experiment, keine öffentliche Plan-/Reservations-API,
vollständige Gehwegplanung, Gebäudezugangsfreigabe oder native Integration.

Ausdrücklich verwendete befestigte primary/secondary/tertiary/minor/service-Quellen
begrenzen die Auswahl. Unbekannte/unbefestigte Beläge, Autobahnen, Wege und Gleise
werden nicht als geeignete Straßen erklärt. Das globale äußere Band der tatsächlichen
Verkehrsfläche hat höchstens 0,18 m XY-Breite; die Oberseite liegt vertikal 0,12 m
über dem gewählten Boden. Das sind Rezeptparameter, keine Norm-/Vermessungsangaben
und keine normale Materialdicke. Oberflächengradient höchstens 0,05.

Fuß-/Radquerungen und andere nicht geeignete Quellen erhalten Abstand; Endports,
Weltrand und bestehende Brücken-, Portal-, Wand- und tatsächliche Pfeilerflächen
bleiben ausgespart. Die Bauwerksprüfung verwendet konservative XY-Aussparungen,
auch wenn räumlich mehr möglich wäre. Quellzuordnung über Nähe und bestehende
Breiten umfasst ältere Eckfüllungen; daraus folgt keine exakte Bordlänge je Quelle.
Die bisherige Nutzung aller 3.259 Quellen erhält keine neue Freigabe.

## Verfahren und Gegenvergleich

Zuerst SIGGRAPH2008 Chen et al.: Quellgraph und Form trennen. Ergänzend
[Galin et al., Eurographics2010](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
S.8/Fig.17 optisch geprüft: parametrischer Querschnitt. Eigene Konstruktion mit
dem installierbaren `SupportVolumeBuilder`, kein kopierter Code oder SDK-Aufruf.
Das ursprüngliche Höhenfeld und das fein unterteilte Bodenmesh beschreiben hier
dieselbe affine Fläche. Deren Gleichheit wird an jedem tatsächlich ausgegebenen
ganzen Kontakt gegen das feine Mesh geprüft; es wird keine geringere Kontaktqualität
als Preis für die gröbere Konstruktion zugelassen.

Identische zwölf Kandidaten: 0,136,272,409,545,681,818,954,1090,1227,1363,1500.
Der letzte ist kleiner als 0,01 m² und wird bei beiden übersprungen.
Feines Mesh: 196.920 Höhenfaces, 1/11 gültige Konstruktionen; sechs konservative
Segmentbudgetabbrüche, vier nichtmannigfaltige Ergebnisse. Ursprüngliches Feld:
2.046 Höhenfaces, 10/11 gültig; Kandidat272 nichtmannigfaltig. Unterschiedliche
Triangulationen sind erlaubt, vollständiger Kontakt, Höhe und geschlossenes
positives Volumen werden für beide gleich geprüft. Kein globaler Optimalitätsbeweis.
Deshalb Feldkonstruktion für den Gesamtlauf; feines Mesh bleibt die unabhängige Referenz.

1.501 Kandidaten aus 1.468 geeigneten Quellbändern. Äußeres Band86.051,019515 m²,
nach Quellauswahl20.081,004206 m². Gesamtlauf: 256 angenommen; 93 wegen
`nonmanifold surface edge`, einer wegen Segmentbudget, 27 unter0,01 m²,
1.124 wegen optionalem Stück-/Zeitbudget ausgelassen. Tatsächlich erreicht wurde
das Stücklimit256. Grenzen: 45s Aufbau/Audit, 262.144 zusätzliche Faces,
1.024 Koordinaten je Footprint, Prozess3GiB/240CPU-s. Ein einzelner begrenzter
Kernaufruf kann das beobachtete Wandzeitlimit überlaufen. Originalquellen und jeder
lokale Grund bleiben im Bericht; fehlende Bordstücke entfernen keine alte Straße.

## Prüfung der tatsächlichen Exporte

Produzent `4e33f6f11b6f7dc919fd2180af358c03cb19c948`, sauberer Commit.
Unabhängiger Leser, Kosten und Renderer auf
`44b761f1722dc3c52d831f55b760de162ba86b2b`; Änderungen danach nur Dokumentation.
Der Leser konsumiert gespeicherte Körper/Kappen, baut sie nicht erneut auf.
Er prüft ganze affine Überlappungen zum feinen Boden in beiden Richtungen,
geschlossene Orientierung, Fläche×0,12 als Volumen, Kappen-/Shellbesitz,
volle Fußabdrücke, Verkehrs-/Bauwerks-/Portfreiraum, Breite und gegenseitige
Überdeckungsfreiheit. Maximale Abweichungen: Kontakt2,8422e-14 m,
Höhe4,5519e-15 m, Volumen4,2907e-10 m³. Diese Kontaktwerte gelten für Double;
Float32 prüft separat keine degenerierten/verkehrten neuen Faces. Keine Behauptung
derselben Kontaktgenauigkeit nach Float32-Konvertierung.

67 Quell-/Bibliothekskontrollen und 16 tatsächliche Blenderkontrollen terminal0
(0,194s bzw.6,029s); darunter offene Querung/Endports, geschlossener12cm-Körper,
verborgener innerer Bodenpeak und ausdrücklich geeignete Quellklassen.
Ruff, Format und Funktionslängen werden für die betroffenen Python-Dateien geprüft.
Keine native, Metal-/Vulkan-/A18- oder komplette Weltabnahme.

## Kosten und Bild

Linux, Python3.13.5, NumPy2.2.4, Shapely2.2.0/GEOS3.14.1,
ein BLAS-Thread; andere Agenten können gleichzeitig arbeiten.
Gesamt42,5693s, Peak504,906MiB; Feldvorbereitung9,436ms,
Aufbau samt Audit20,5339s. Die unveränderliche äußere Maske wird einmal vorbereitet.

Nur Kandidat1090 gelingt beiden Methoden: Die folgende isolierte Teilmessung
belegt ausdrücklich keinen allgemeinen Geschwindigkeitsfaktor für den Gesamtlauf.
Sie beginnt nach Eingabedekodierung/Indexvorbereitung, umfasst nur Konstruktion,
ohne Auswahl, unabhängiges Audit, Packen oder Rendern. Sieben normale Wiederholungen,
vorherige Ergebnisse jeweils freigegeben; jede Methode reproduziert ihre eigenen
gespeicherten Vertices/Indices exakt. Memray1.20.0 separat, ein instrumentierter Lauf.

| Teilmessung | Feiner Boden | Ursprüngliches Feld |
|---|---:|---:|
| Indexvorbereitung normal | 513,049ms | 5,026ms |
| Median Konstruktion normal | 8,834ms | 4,885ms |
| Ergebnis Vertices/Faces | 30/56 | 24/44 |
| Körper-/Kontaktarrays | 1.392/1.032B | 1.104/816B |
| Peak Prozess normal inkl. Eingaben | 349.118.464B | 95.735.808B |
| Allokationsereignisse | 42.848 | 14.306 |
| Kumulativ angeforderte Bytes | 4.171.087B | 1.520.051B |
| Getrackter Peak Heap | 179.020B | 62.512B |
| Instrumentierte Zeit | 84,855ms | 49,786ms |

Ereignisse enthalten realloc, kumulative Bytes sind kein Live-Speicher.
Interne vollständige Kopierbytes ungemessen. Zusätzliche explizite Darstellungskopien:
indexiertesXYZ3.667.104B und Ecknormalen1.833.552B; kompletter Arraypayload
58.740.000→63.029.520B. Kein Geräte-/Framebudgetnachweis.

Tatsächlich geöffnet: frisches regionales Vorher/Nachher1280×720 und lokale
Nachherprüfung1920×1080. Identische Paar-Kamera/Licht, gleicher Zielpunkt und
Blickrichtung wie #357; Bildspanne leicht1400→1100m reduziert, keine Nahaufnahme.
Blender5.2.2EEVEE, Mesa llvmpipeLLVM19.1.7, 16Samples, kein GPU-Cycles-Beleg.
Importreplay bestätigt alle tatsächlichen Positions-/Index-/Normalen-/Farbfelder.
MaterialJSON bleibt gepinnt; Borddarstellung verwendet vorhandenes lineares
concrete-Albedo. Lokaler Inspektionsshader besitzt weiterhin roughness0,86/metallic0,
keine behauptete vollständige native Betonmaterialübernahme.

720p-Pixelvergleich: 13.910/921.600 verändert(1,5093%), davon9.728 um mehr als1/255,
Maximum22/255, mittlererRGB-Kanalfehler0,055181/255. Straßenränder bleiben aus dieser
Höhe dezent; Gesamtbild weiterhin blass/schematisch. Kein großer sichtbarer
Qualitätssprung behauptet. Brücken und Anschlüsse bleiben lesbar, kein neuer
großer Darstellungsfehler erkennbar. Rohbilder bleiben lokal; nur master veröffentlicht
integrierte Places. Renderzeit720p21,952→21,605s, Peak2.331.452→2.351.848KiB;
1080p32,496s/2.555.908KiB. Das sind Offline-Inspektionskosten.

## Eingaben und frische Belege

Originale bleiben erhalten, alle Pfade im System-Temp. Ursprung48,21°N/16,37°E,
lokales Ost/Nord/Hoch in Metern, OpenFreeMap `20260927_080001_pt`,
Kacheln14/8938/5680,14/8939/5680,14/8939/5681; kein Quellrefresh.
Netz `/tmp/outshine-transport-network-303-cf33992d3`, Basis
`/tmp/outshine-transport-surface-357-final-b442d6c51`, Pfeiler
`/tmp/outshine-transport-support-library-332-v2`, ursprünglicher Bauwerksplan
`/var/tmp/outshine-transport-tunnel-portals-f68a060a4/build/experiments/transport`.

| Datei | SHA256 |
|---|---|
| Eingabe | `6d39b0165d18e8c3b466b38207d4291d564bcbd9a710628984bcacdb449f2a79` |
| Feiner Boden | `16cd7e8b774136dc50e70e9549d376630b4f7bb88e221f4b46527c30367627e5` |
| Höhenfeld | `20699c72258c3aae6e4a872d151241bc9d3cf8ce340a112e09e4847476206cac` |
| Alter Körper | `1ea4b2ed2eb343772f2c803c78ee0da21d372755010b8b7289754ade26618f41` |
| Vergleichsbericht | `b7b26c17052273a6ace843880d4654261b46c1dacd237527b37869b10100f810` |
| Tatsächliche Bordkörper | `4db10d8ede0e9ee0dcd7df19212841c94fd570de5ef3fea8b91f80fd32788431` |
| Neues Darstellungsmesh | `3cbb4f6ac7e0f555e09204d1e9c76fbff35ba8497519e3c59e012e215c8a21c9` |
| Vorher720p | `8974392517063fa36c8a5720c2e378aa3d4d08d326d65ff88e60df6b43e56121` |
| Nachher720p | `ce3d745c4659737c7f36300aab7ed5162e2ad7cfea415dd3b4241e8554e1f74d` |

Produkte `/tmp/outshine-transport-curbs-364-result-4e33f6f11`: comparison.json,
curbs.npz, gespeicherte Alternativen, verification.json, Szene-/Renderreceipts,
before.png/after.png/after-1080.png und pixels.log. Weitere Quell-/Pfeiler-/Materialpins
stehen vollständig im Vergleichsbericht und ausführbaren Eingabeprüfer.
Kosten `/tmp/outshine-transport-curbs-364-cost-44b761f17`: vier JSONs und zwei
Memraytraces. Terminale Phasenlogs `/tmp/outshine-transport-curbs-364-*-44b761f17.log`.
[Reproduktionsaufrufe](README.md#räumliche-straßenrandprofile-364) verwenden frische
explizite Ausgaben und eingefrorenen sauberen Commit. Die Quellpins ersetzen
keine benötigten Originaleingaben; diese werden samt aktuellem Vorherbild erhalten.
