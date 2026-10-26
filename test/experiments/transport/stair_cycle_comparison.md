# Östliche Radwegschleife am Wiener Treppenverbund (#434)

Der Versuch erweitert den [18-Quellenstand](stair_outer_comparison.md) um den ganzen
ursprünglichen Radweg `openfreemap:14/8937/5678:transportation:32651280:3`.
Beide bisherigen östlichen Außenprofile von `39881120:34` erhalten ganze gemeinsame
Kappen. Ein Kontakt besitzt einen ursprünglichen MVT-Punkt, der andere ist ein
explizit gewählter Bandkontakt bei 0,130762 m Achsabstand. Die Originalachse wird
nicht auf einen erfundenen Quellknoten gesnappt. Nur Quellkoordinaten zu verbinden
verfehlt diesen tatsächlichen Bandkonflikt und wird abgelehnt.

Alle bisherigen Achsen und Priors bleiben erhalten, einschließlich der schon zuvor
gewählten Faltauffahrt. Die gemeinsamen Höhen werden für das größere Netz neu
geplant; der vorige Körper bleibt eigene unveränderte Regression, kein behauptet
bytegleicher neuer Körper. Drei Treppen und sechs tatsächliche Unterführungen
behalten ihre Bedingungen. Drei frühere und zwei neue äußere Profile bleiben offen;
das Stationsende `indoor=1, level=0` ist kein gewöhnlicher Erdbodenanschluss.
Globale Quellfreigabe bleibt leer, native Übernahme offen.

Die neue Achse reicht in den PBF-Kachelbuffer über den östlichen DEM-Rand.
Zwei ausdrücklich erhaltene reale DEM-Fenster werden nach ihrer tatsächlichen
Kachel ausgewählt. Kein Clamping/Extrapolieren der ursprünglichen Höhenkachel und
kein behaupteter kachelübergreifender OSM-Netznachweis. Die neuen Terrainflächen
werden nach dem [kontinuierlichen Böschungsverfahren](stair_blend_comparison.md)
geplant und mit ganzen ursprünglichen Flächen sowie tatsächlichem Float32 geprüft.
Mengen beziehen sich weiterhin nur auf das affine DEM-Modell.

Verfahren: Chen et al. SIGGRAPH 2008 (Netz/Form trennen), Galin et al. Eurographics
2010 (Profile/Bauwerke/Terrain), Desbrun et al. SIGGRAPH 1999 (Membranenergie),
bereits optisch geprüfte Primärquellen unter [Transportreferenzen](../../../doc/references/transport/README.md).
Eigene Geometrie-/Optimierungszusammenstellung; kein Fremdcode oder SDK in der Runtime.

Die [Planungseingabe](fixtures/stair-cycle-vienna.json) enthält keine gelösten Höhen.
[Aufrufe und Pakete](README.md). Gespeicherte Exporte werden ohne erneuten Solver,
CDT oder Körperbau geprüft. Normale DS-/IPM-Zeiten und separat profilierte
Allokationen müssen dieselben ganzen Qualitätsbedingungen bestehen; ein
Fristabbruch hat keine gültige Ausgabemediane. Kein nativer/Geräte-/Framefaktor.

## Geprüfte örtliche Lieferung

Grundcode/Kontrollen `64dd34371`, vollständige frische Erzeugung `f9271d883`,
unabhängiger gespeicherter Leser und Kosten `71112e1ab`. Die beiden späteren
Korrekturen betreffen Kameraaufruf und Übergabe der Treppenbesitzer an den Leser;
Netz, Terrain, Solverbedingungen und feste Eingänge bleiben unverändert.
17 Kontrollen (sechs neue, elf bisherige Böschungsprüfungen) bestehen auf dem
eingefrorenen Grundcode: 54,968 s unter paralleler Last. Ruff/Format, Funktionen
≤120 Zeilen und Diffprüfung bestehen. `make lint-docs` besteht ausschließlich
für AGENTS/offline; keine nativen Gates ausgeführt.

Neuer ganzer Radweg: 388,057145 m, vorhandenes Profil 2 m / Gradientengrenze 0,06.
19 Quellteile, 17 gemeinsame flache Kontakte, drei Treppen mit 24 Stufen,
4824 Körpervertices / 9668 Facetten. Alle vier früheren/neuen Außenquerschnitte
sind ganz durch tatsächliche gemeinsame Kappen gedeckt, Double und Float32.
Die zwei räumlich getrennten neuen Kappen haben unterschiedliche Höhen und werden
deshalb als eigene zusammenhängende Komponenten geprüft. Alle sechs tatsächlichen
Float32-Unterführungen haben mindestens 2,519989 m; alle Vertexlinks sind zyklisch.
Originalinventar: 1657 Linienteile / 71 Flächen, davon 19 lokale Kandidaten;
1638 Linien und 71 Flächen bleiben ausdrücklich ungeprüft mit IDs und Gründen.

Die Original-DEM-Bytes bleiben SHA256 `67a97b8f…`; östliche Kachel 14/8938/5678,
63642 B, SHA256 `f0588282420d4654f76e742d68813e3d5fecca792a1158770a64d0d062d5fee7`.
Beide Ausschnitte und das Rezept sind aus den erhaltenen vollständigen Rohbytes
bytegleich neu aufgenommen. 6397 echte Punkte einschließlich des ganzen vorläufigen
Terrainplans stimmen exakt mit dem unabhängigen vollständigen `Elevation`-Decode
überein, größter Fehler 0 m; kein neuer Online-Erwerb in der normalen Reproduktion.

Terrain: 6266 Höhenvertices / 12272 Facetten, geschlossener Außensolid
12532 Vertices / 25060 Facetten. Alle 2292 ursprünglichen Bodenfacetten sind ganz
abgedeckt; keine inneren Höhensprünge/Nullflächen, vollständige Double-/Float32-
Brückenunterseiten geprüft. Erdgradient maximal 0,664961 / 0,666086; konstruktiv
9,998000 / 9,998481 bei unveränderter Grenze 10. 40 konstruktive Facetten,
14,6519 m² projiziert / 21,5913 m² Oberfläche, weiterhin vollständig innerhalb 5 m.
Hergeleitetes Float32-Kontaktband 0,659009 mm, ganze gemessene Extrema
−17,201711 / +13,407565 µm. Kein exaktes Null-Überlappungsvolumen behauptet.
Affines DEM-Modell: 1357,4948 m³ Auftrag, 8864,8578 m³ Abtrag, Bilanzrest <1e−12 m³;
keine Vermessungswerte oder zusätzlichen ganzen Cut-/Fill-Körper.

Der gespeicherte Leser besteht ohne Solver/CDT/Körperbauer; ursprüngliche Quellen,
ganze gemeinsame Kappen, alle flachen Kontaktflächen, Treppenmonotonie, Unterseiten,
Terrain und tatsächlich dargestellte Geometrie/Normalen/Farben werden geprüft.
Vier frische PNGs vollständig dekodiert, an aktuelle Szenen-/Mesh-/Helperhashes und
exakte Importarrays gebunden und tatsächlich geöffnet: 1280×720, gleiche Kameras,
Farben und Licht je Paar, Gesamtübersicht plus 420-m-Ausschnitt. Pixeländerungen
15,8111 % / 38,1375 % einschließlich der erweiterten Terrainfläche, keine Qualitätsskala.
EEVEE 5.2.2 LTS / llvmpipe-CPU, 16 Samples: vor/nach Übersicht 14,29/14,38 s,
Ausschnitt 15,25/16,26 s. Keine Galerie oder Bilddateien unter `doc/`.

## Gleiche Qualitätsbedingungen und getrennte Kosten

Linux x86_64, Intel i5-1135G7 mit zwei sichtbaren Kernen/vier logischen CPUs;
Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0/GEOS 3.14.1,
Clarabel 0.11.1. Keine eigenen Render-/Testprozesse während der normalen,
nacheinander ausgeführten Kostenmessungen; gemeinsamer Host nicht exklusiv.
Beide LP-Methoden bekommen denselben festen Plan, alle Klassen-/Kontakt-/Freiraum-/
Float32-Bedingungen und dasselbe 5-s-Limit für den Solver. Matrixaufbau plus Solve
nach Import/Decode/Layout und einem Aufwärmlauf werden gemessen; tatsächliche
Körperprüfung danach außerhalb der Zeit. Keine Grenze angehoben.

| Phase / Ergebnis | HiGHS dual simplex | HiGHS interior point |
|---|---:|---:|
| Normale Ausgabeprüfung | 7 gültige, bytegleiche Höhenfolgen | Zeitlimit, keine Ausgabe |
| Normale Matrix-/Solvezeit | Median 2775,52 ms | 6137,11 ms abgelehnter Versuch |
| Normale Prozess-Peaks | 289 714 176 B | 218 693 632 B |
| Separat profilierte Zeit | 10620,59 ms, gültig | 12947,69 ms, abgelehnt |
| Separat profilierte Prozess-Peaks | 299 978 752 B | 228 999 168 B |
| Separate Allokationsereignisse | 7 843 900 | 7 300 011 |
| Separate kumulierte Anforderungen | 1 146 244 539 B | 1 179 270 161 B |
| Separater getrackter Heap-Peak | 229 740 459 B | 112 388 816 B |

4421 LP-Variablen, 97222 Bedingungen, 287752 Nichtnullen; gespeicherte Sparsearrays
3 470 712 B. Bekannte explizite XY-Quantisierung/Kopie 52 440 B je Vorbereitung,
gültige Höhenfolge 17 480 B. Memray 1.20.0 mit Python-/nativen Traces lief separat;
Anforderungsbytes sind keine eindeutigen/live Bytes, interne Kopiersummen ungemessen.
IPM bekommt keine gültige Ausgabemediane und keinen Beschleunigungsfaktor. Die
Terrainplanung ist nicht Teil dieses LP-Vergleichs. Keine nativen/Geräte-/Frame-
oder Gesamtgeneratorfaktoren. Ganzer frischer Aufbau bis vor Display: 33,76 s,
Prozess-Peak 275208 KiB, neben fremden Renderprozessen; einzelner Forschungslauf.

Ein früherer Aufbau unter eigenen Tests und fremden Rendern erreichte dasselbe
5-s-Limit und wurde vollständig verworfen. Ein weiterer Lauf bestand die Geometrie,
scheiterte aber am Kameraadapter; erst die vollständige frische Erzeugung samt
unabhängigem Lesen/Bildexport zählt als Lieferung. Keine bloßen Exitcodes als Abnahme.

Originalprodukte: System-Temp `outshine-transport-stair-cycle-f9271d883`, Kosten
`outshine-transport-stair-cycle-cost-71112e1ab`; darin `comparison.json`,
`verification.json`, `render-completeness.json` und getrennte normale/Profilergebnisse.
Quellen-/Produkt-/Prüfer-Commits und vollständige Artefakthashes bleiben dort erhalten.
Der vorige 18-Quellen-/Terrainstand bleibt unveränderte eigene Regression.
Übrige Quellen/Ebenen, Fahr-/Gehhüllen, Radien, Statik, allgemeine Selbstschnittfreiheit
und native Übernahme bleiben offen; #434 und der vollständige Transportauftrag bleiben aktiv.
