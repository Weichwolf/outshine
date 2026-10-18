# Prozedurale, gespeicherte Rindenkarten

Python-Forschung #765, Gesamtform/Artenabnahme #682. Die metrischen Charts aus
#759 bleiben unverändert. Der neue Versuch erzeugt eigene vollständige
Höhen-, Pigment-, Rauheits- und Normalenkarten vor dem Rendering. Der Bildadapter
lädt zwei Texturen; er erzeugt keine Furchen oder Rauschfelder erneut.
Eine überzeugende Eichenrinde ist weiterhin nicht abgenommen.

## Lokale Referenzen und Entscheidung

Zuerst [Bloomenthal, SIGGRAPH 1985, Modeling the Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf)
nachgeschlagen; PDF-Seite 8 mit echtem Rindenstück, Abguss und Höhenkarte wurde
optisch geöffnet. Das Paper betont axiale und umlaufende physische Distanzen,
periodische Karten und die wechselnde Rinde dünner Äste. Seine digitalisierte
Rinde wird hier nicht verwendet. [Perlins solide stochastische Komposition](../../../doc/references/vegetation/siggraph/1985-image-synthesizer.pdf)
ist die bereits optisch geprüfte zusätzliche Grundlage.

Im lokalen `Oak_Hero_Field.spm` wurden Materialdateinamen geprüft und
`BaseBark_diffuse.png`/`BaseBark_height.png` tatsächlich geöffnet. Die Höhenkarte
beschreibt gröbere Längsformen; Farb-/Normalenkarten ergänzen feine Struktur.
Nur diese gestalterische Trennung wird übernommen. Keine Pixel, Reliefwerte,
Photogrammetrie, Fremdgeometrie oder Vendorformel fließen in die neuen Assets.
Die ebenfalls geöffnete `Oak_Bark_1.png` zeigt andere, glattere Rinde und ist
kein Beleg für das hier angestrebte grobe Relief. Keine Websuche.
Ghidras vorhandene UV-Untersuchung bleibt bei #35; dieser Versuch enthält
keinen neuen Binärlauf und behauptet keinen SpeedTree-Algorithmusnachbau.

## Eigener Aufbau und begrenzte Arbeit

`FurrowRecipe` legt Seed, physische Kachelgröße und Zahl der Längs-/Querfurchen
fest. Das geprüfte Beispiel besitzt 18 Längsfurchen und 180 endliche Querbrüche
auf 0,6×1,2 m. Ungleiche positive Abstände ersetzen ein regelmäßiges Raster.
Periodische Fourierprofile lassen Mittellinien, Breiten und Tiefen wandern.
Die unabhängige seitliche Auslenkung ist auf 0,22 des kleinsten Grundabstands
begrenzt; benachbarte Hauptlinien behalten mindestens 0,56 dieses Abstands.
Das sind gestaltete Furchen, keine Simulation von Rindenwachstum oder Bruchphysik.

Eine Furche verwendet den kompakten Querschnitt `K(q)=max(0,1-q²)³`;
schmale Rinne und dreifach breite Schulter werden mit 0,65/0,35 gemischt.
Einzelne Kernränder sind C2-stetig. Überlagerung durch Maximum kann zusätzliche
Grate erzeugen; eine überall C2-stetige Gesamthaut wird nicht behauptet.
Querbrüche liegen zwischen benachbarten Hauptlinien und haben endliche Länge.
Ein normiertes Chebyshevprofil bis Grad fünf versetzt ihre Mittellinie;
unterschiedliche Koeffizienten vermeiden identische Sinuswellen je Bruch.

Für aktive Querbruchpunkte gilt `|along|<1` und die Normalausdehnung ist
höchstens `1,9*Breite+|Biegung|`. Rotation dieses Rechtecks liefert die geprüfte
konservative Rasterbox. Länge und Minimalformat halten sie unter einer halben
Periode; die indizierten Pixelpositionen wiederholen sich nicht innerhalb eines
Auftrags. Modulo-Indizes übertragen Unterstützung über beide Kachelränder.
Hauptfurchen verwenden entsprechend konservative zeilenweise Pixelspannen.
Die dichte Kontrolle wertet dieselbe Funktion am gesamten Raster aus.
Kapazität/Format werden vor Plan-/Pixelaufbau geprüft: Achsen 32–4096,
höchstens 8.388.608 Pixel, bis 64 Hauptfurchen/1024 Querbrüche.

## Zusammengehörige Karten und Speicherung

Drei feste periodische Koeffizientengitter liefern Makrofärbung, Korn und
Feinstruktur. SciPys kubisches `map_coordinates(..., mode="grid-wrap")` wertet
sie aus. Gefaltete Kornbänder ergänzen die Kruste; Vertiefungen, Pigment und
Rauheit teilen die Furchenquelle. Farbe enthält keine berechnete Beleuchtung.
Maßstab/Färbung sind gestaltete Werte ohne botanische Messkalibrierung.

Aus derselben Höhe in Metern entstehen periodische zentrale Ableitungen mit
expliziten Texelabständen und die normierte Tangentennormale `(-dh/dx,-dh/dz,1)`.
Die PNGs besitzen umgekehrte Arrayzeilen für nach oben laufendes Blender-V.
Texelzentren sind periodische Abtastpunkte: Erste und letzte Zeile sind
benachbart, keine doppelt gespeicherte identische Randlinie.

| Gespeichertes Produkt | Format |
|---|---|
| Pigment | RGB8, linear |
| Normale / Rauheit | RGBA8, XYZ / Alpha, unabhängige Kanäle |
| Höhe für spätere Nutzung | R16, Skala 12 mm |
| Beschreibung | `procedural-furrow-bark-1`, Rezept, Maße, Orientierung, Dateihashes |

Der Adapter setzt `Non-Color` und für die gepackte Karte `CHANNEL_PACKED`;
damit verändert Rauheitsalpha die Normalenkanäle nicht. Metallic ist null.
Nur Pigment und Normalen/Rauheit werden beim Schattieren gelesen. Die gespeicherte
Höhe wird hier nicht zur Verformung verwendet. Es entsteht kein neuer nativer
Cache-/Materialvertrag; dies sind lokale vollständige Versuchsassets.

## Korrektheit und vollständige Kosten

Acht neue Prüfungen umfassen eine vollständige unabhängige skalare 32×32-Auswertung
und weitere 120 Punkte dreier Seeds,
bitgleiche dichte/indizierte Raster verschiedener Seeds und extremer Seitenverhältnisse,
beide periodischen Ränder, analytische metrische Normalen/Skalierung,
unabhängige Normalenableitung im wiederholten Feld, Speicher-/Formatvorprüfung
und PNG-Rundreise einschließlich Orientierung und gepackter Rauheit.
Die Normalenquantisierung bleibt im geprüften Fall unter 0,4°; Höhenfehler
bleibt innerhalb einer halben R16-Stufe zuzüglich Float-Rundung.
42 betroffene Kontrollen bestehen einschließlich der unveränderten C1-/Alt-Rindenverfahren.

Der vollständige Rastervergleich verwendet **1024×2048** (Breite×Höhe), nicht
die kleinen Prüfraster. Alle acht frischen Auswertungen einschließlich separater
Profile liefern Höhenhash `69cf521b811af33139290f0bae6edb8816e6664f011cc5475f619b860d0804e2`.

| Reine Höhenauswertung | Indizierte Spannen | Dicht |
|---|---:|---:|
| Kandidatenpunkte | 1.691.364 | 415.236.096 |
| Drei alternierende frische Läufe, Median | 0,208 s | 45,486 s |
| Memray-Allokationsanfragen | 123.293 | 112.342 |
| Kumulativ angeforderte Bytes | 421.074.190 | 92.639.839.843 |
| Verfolgter Heap-Peak | 56.897.653 B | 219.649.905 B |
| Höchster Prozess-RSS der Zeitläufe | 75.476.992 B | 242.446.336 B |

Die indizierte Variante hat **mehr** kleine Allokationsanfragen, aber wesentlich
kleinere temporäre Felder. Keine pauschal bessere Allokationszahl behauptet.
Ein bekannter Kopieranteil ist das Lesen/Zurückschreiben des alten Höhenfelds
über fortgeschrittene NumPy-Indizes: 16 Payloadbytes je Kandidat, hier
27.061.824 gegen 6.643.777.536 B. Das sind keine gemessenen Speicherbusbytes;
weitere temporäre Rechenfelder bleiben in den Memray-Anforderungen enthalten.
Der Ausgabehöhenraster belegt jeweils 16.777.216 B. Hasherzeugung kopiert ihn
nach dem verfolgten Bereich zusätzlich. Modulimporte sind davor geladen;
interne Erstimporte während der Auswertung bleiben in beiden Profilen enthalten.

Separater vollständiger Skriptlauf für Plan, Karten, PNG-Export und Importe
(Memray ab Skriptstart, ohne vorgeschalteten Interpreter-/CLI-Start):
1.929.874 Allokationsanfragen, 1.958.083.101 kumulative Bytes,
466.942.994 B Heap-Peak. Das ist ein anderer Umfang als die Rastertabelle.
Die vier Float32-Semantikfelder belegen 67.108.864 B; drei fertige PNGs
8.336.152 B. Reiner Raster-/Kartenaufbau im uninstrumentierten Erstlauf:
0,186/1,185 s, Export separat; keine Profilzeit als Normalzeit verwenden.

Ein weiterer vollständiger Neuaufbau reproduziert alle drei PNG-Dateien exakt.
Ein vorbereiteter Python-Leseversuch für die zwei tatsächlich konsumierten
Texturen: 0,216 s, 14.680.064 B dekodierte Arrays, ohne GPU-Upload und Importe.
Frische Rendererprozesse blockieren Plan-/Raster-/Kartenerzeugung ausdrücklich
und zeichnen trotzdem aus den gespeicherten Dateien. Das ist keine allgemeine
Outshine-Lade-, Frame- oder Gerätezusage.

Python 3.13.5, NumPy 2.5.4, SciPy 1.18.1, Memray 1.20.0; geteilter Linux-Host.
Zeitworker mit normaler Priorität, einige erste Läufe neben nice10-Softwarerenderern.
Arbeitszählung und bitgleiche Inhalte tragen die Entscheidung; verrauschte
Hostzeiten sind keine plattformübergreifende Geschwindigkeitsgarantie.

## Tatsächliches Bild und verbleibende Lücke

Vier geöffnete Bilder zeigen denselben gekrümmten Prüfkörper mit bisherigem
Zellmaterial und neuen Karten, jeweils Front und umlaufende UV-Schließstelle.
Je 2560×1440, 32 EEVEE-Samples, gleiches Studiolicht, 24.832 Dreiecke.
Die neue Oberfläche zeigt wesentlich feinere Kruste und Längsfurchen; besonders
Querbrüche wirken weiterhin zu eingeschnitten und die Grundstruktur zu gleichartig.
Das trägt den lokalen Materialversuch, **keine überzeugende Eichenfreigabe**.
Ein weiterer teurer Ganzbaumversuch ist damit noch nicht begründet.

Die Probe hat konstanten Radius 0,6/(2π) m und Mittellinienlänge 1,2 m;
ihre Kreisbiegung 0,15 rad ergibt höchstens rund 1,2 % axiale Metrikabweichung
zwischen innerer/äußerer Oberfläche und Mittellinienchart. Kein Taper-/Gabelbeweis.
Endkappen behalten ausdrücklich separates glattes Material. Keine Geometrie-
oder Silhouettenverformung, kein Wind/Schatten-/LOD-/Native-/Gerätegate.
Blender 5.2.2 LTS verwendet llvmpipe/LLVM19.1.7, keinen belegten RTX-/Cycles-Lauf.
`pixels.py` bestätigt Änderung nur am Körper; Pixeländerung ist kein Qualitätsmaß.

Weitere Arbeit braucht verzweigte Furchen mit weniger gleichförmiger Kruste,
Altersübergänge, taper-/gabelgerechte Metrik und gefilterte Normalen/Rauheit.
Diese Darstellung wird nicht als Artenstandard in den Katalog übernommen.
Alle Bilder und Assets bleiben ausschließlich unter `/tmp`.

## Reproduktion

Aus dem Checkout mit `.venv` und
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation:test/experiments/render`.
`PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1` setzen;
private XDG-/Mesa-/Profilerausgaben ausschließlich nach `/tmp` legen.

```sh
.venv/bin/python test/experiments/vegetation/furrow_probe.py --output /tmp/outshine-vegetation-furrows/assets
.venv/bin/python -W error -m unittest furrow_checks
.venv/bin/python test/experiments/vegetation/furrow_cost.py --method indexed --output /tmp/outshine-vegetation-furrows/cost-indexed
.venv/bin/python test/experiments/vegetation/furrow_cost.py --method dense --output /tmp/outshine-vegetation-furrows/cost-dense
EGL_PLATFORM=surfaceless DISPLAY= LIBGL_ALWAYS_SOFTWARE=1 .venv/bin/python test/experiments/vegetation/render_furrow_bark.py --inputs /tmp/outshine-vegetation-furrows/assets --mode furrows --view seam --output /tmp/outshine-vegetation-furrows/shot
```

Für Profile `furrow_cost.py --profile` ergänzen und mit `python -m memray stats`
auswerten. Für die Bildkontrolle `--mode cells`/`--view front` verwenden.
Jedes Ausgabeziel muss neu sein. Keine Normalprüfung erzeugt Repository-Orakel.
