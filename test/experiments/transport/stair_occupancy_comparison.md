# Höhenabhängige räumliche Übergabe (#568)

Stand 2026-10-17: Der Python-Versuch liefert Reservierungsvolumen aus allen
65 gespeicherten Wiener Transportquellen einschließlich ihrer 124 Stützen.
Das Verfahren und seine Verbraucherprüfungen liegen ausschließlich im Experiment.
Der öffentliche Bibliotheksvertrag bleibt unverändert; die Fachentscheidung
besitzt [#568](https://github.com/Weichwolf/outshine/issues/568).
Vegetation und Buildings haben ausdrücklich zugestimmt; masters Antwort steht
noch aus. Diese fehlende Antwort wird nicht als Zustimmung behandelt.

## Geometrie und Übergabe

`AffineOccupancy` ist ein unveränderlicher experimenteller `Reservation`-Untertyp.
Über einer gültigen Polygon-/MultiPolygon-Domäne P liegen zwei lokale affine
Flächen L und U: `z0 + gx*(x-origin_x) + gy*(y-origin_y)`.
Der gemeinsame Plan-Frame einschließlich Höhendatum ist verbindlich.
Endliche positive Höhenintervalle und L<U auf der ganzen geschlossenen Domäne
werden geprüft. Die gespeicherte Höhenhülle dient der Vorprüfung.

Für das Abfrageband H=[h0,h1] lautet die positive Konfliktfläche
`P ∩ {L<h1} ∩ {U>h0}`. Die Rückgabe ist deren polygonaler Abschluss;
leere Ergebnisse sind leere Polygone. `overlaps` verlangt positive Schnittfläche.
Reine Höhen-/XY-Berührung bleibt frei; `covers(Point)` allein genügt nicht.
Zwei Halbebenen schneiden zunächst nur das konvexe Begrenzungsrechteck von P.
Dieser kleine Schnitt verwendet exakte rationale Arithmetik der Eingabe-Binärzahlen.
Der anschließende GEOS-Schnitt erhält Löcher und getrennte Komponenten; Linienreste
werden entfernt. Seine XY-Vertices und Booleschen Operationen verwenden binary64.
Eine allgemeine exakte GEOS-/Topologiegarantie für beliebig dünne Regionen ist
damit **nicht** bewiesen. 200 unabhängige rationale Dreieckskontrollen stimmen
bis höchstens 1e-10 m² absoluter Flächenabweichung überein.

Aus den unveränderten tatsächlichen Ober-/Unterseiten entstehen 4852 Straßen-
und 1134 Stützenfacetten. Alle zugeordneten Kappen haben identische XY-Vertexmengen.
Verkehrsfreiraum: 2,5 m für die gewählten Wege/Radwege, 4,5 m für die gewählten
kleinen Straßen; gemeinsame Facetten verwenden das Maximum ihrer Quellprofile.
Stützen erhalten ihren tatsächlichen Körper ohne zusätzlichen Verkehrsfreiraum.
Alle 5986 Facetten und sämtliche Quellen-/Stützen-Owner werden exportiert.
Ein frischer Leser prüft jede Fläche gegen die ursprünglichen XYZ-Vertices,
ihre Quellzuordnung und das Rezept. Keine Körper oder Terrainpatches werden geändert.
Rationale Vertexprüfungen begrenzen die konservative Erweiterung der Höhenflächen
auf maximal **2,8728413860010476e-14 m**; affine Interpolation erhält diese Grenze
im Dreiecksinneren. Diese Höhenaussage ersetzt keine Genauigkeitsgrenze für GEOS-XY.

Der originale, versionierte OpenFreeMap-Payload enthält 41 Building-Features.
28 sind für diesen Versuch verwendbar: Polygonquelle, Provider-Höhen und ein
Original-DEM-Anker im selben Höhendatum. Zehn ungültige Geometrien, zwei `hide_3d`
und ein Anker außerhalb des deklarierten DEM-Kontexts bleiben mit vollständigem
Originalfeature und Grund im Export. Es findet keine stille Geometriereparatur statt.
Zwei der 28 Footprints werden um zusammen **40,792421875565196 m²** gekürzt.
ID, Part, Attribute und Höhen bleiben erhalten; getrennte Restflächen dürfen
als MultiPolygon derselben Part bestehen. Budget-/numerische Fehler stellen
nur den betroffenen Footprint konservativ zurück, mit Grund; sie geben keinen ungeprüften
Raum frei. Programmierfehler bei Methoden-/Kapazitätsargumenten werden weitergereicht.

`pipeline.prepare` wird mit echten Quellen/Footprints und diagnostischen
Buildings-/Vegetation-Callbacks ausgeführt. Buildings erhält die angepassten
Footprints, Vegetation beide Reservierungsprodukte. Das sind Schnittstellenprüfungen,
keine Architektur- oder Wuchsrezepte. 128 ausdrücklich diagnostische, bodennahe
Prüfvolumen unter tatsächlichen Brücken verwenden je 1,2 m Höhe und 0,2 m Breite.
34 bleiben gegenüber Transport frei; bloßer Projektionsabzug würde alle 128 sperren.
Konstante **einzelne Facettenhöhenhüllen** sperren ebenfalls 94 dieser Proben:
Hier wird kein zusätzlicher realer Freiraumgewinn gegenüber dieser feineren
Alternative behauptet. Schräge Teilkonflikte werden durch separate Kontrollen belegt.

## Kosten und Bilder

Gleiche 156 Footprint-/Probe-Eingaben, 5986 Volumen, identischer Geometriehash
`40b93f14c27822a62f744bec1e7b3fc87491120ec8dc665166e934468f48acae`:
Zeiten umfassen Abfrage, Footprint-Restflächen und Vergleichshash; vorbereitete
Eingaben und Indexaufbau liegen außerhalb der Abfragemessung.

| Verfahren | Median, 3 normale Läufe | Memray-Allokationen | allokierte Bytes |
|---|---:|---:|---:|
| Vollscan, rationaler Rechtecktest | 158,680 ms | 1124670 | 107023838 |
| Index, rationaler Rechtecktest | 104,666 ms | 815827 | 76117035 |
| Vollscan, zusätzliche Vollflächenprüfung | 103,014 ms | 514310 | 52436755 |
| Index, zusätzliche Vollflächenprüfung | 44,547 ms | 205227 | 21516491 |

Die Vollflächenprüfung speichert zwei nach außen gerundete Grenzwerte je Volumen:
max L und min U. Liegt H streng darüber/darunter, überlappt die ganze Domäne;
der rationale Rechtecktest entfällt. Alle übrigen Fälle behalten denselben Schnitt.
Beide Varianten bearbeiten dieselben 441 XY-/257 Höhenkandidaten; der Vollscan
prüft 933816 Reihen, der Index liefert 441 Kandidaten. Interne Baumknotenbesuche
sind nicht gemessen. Exportierte Volumen, Footprints und Probe-Ergebnisse vor/nach
der Optimierung sind bytegleich. Keine geringere Qualität oder Eingabemenge.

Vorbereitung aus gespeichertem JSON einschließlich Validierung/Index: 1,706→1,814 s.
Letzter Prozess-RSS-Peak 132214784 B; getrackter Index-Abfragepeak 163912 B,
ohne die vorbereiteten Eingaben. Referenzarray: 47888 B, serialisierte Volumen:
3353523 B. Das sind keine gemessenen kopierten Geometriebytes; vollständige interne
GEOS-Kopien sind weiter ungemessen. Instrumentierte Zeiten werden nicht als normale
Latenz verwendet. Host: i5-1135G7, vier sichtbare Threads, rund 16 GB RAM;
Desktop-/fremde Hintergrundlast, beim letzten Vergleich paralleler Wheel-Aufbau.
Keine Geräte-/Framebudgetaussage. Memray 1.20.0 mit Python-Allokatortracking.

Erzeugung einschließlich geprüfter Eingabeladung/Übergabe/Export: 8,2683 s,
Prozess-RSS-Peak 151900160 B. Zwei frische vollständige **1280×720**-3D-Ansichten
mit identischem, aus beiden Geometrien berechnetem Gesamtausschnitt sind geöffnet.
Blender/EEVEE auf llvmpipe: 10,273/9,811 s. Kamera-/Mesh-/PNG-Hashes und vollständige
PNG-Dekodierung geprüft; der erste zu enge Kameraausschnitt wurde verworfen.
`test/scripts/pixels.py`: nur **31/921600** Pixel ändern sich, davon 20 um mehr
als 1/255. Kein großer sichtbarer Generatorfortschritt behauptet. Darstellung bleibt
blass/sparse; außerhalb des gelieferten Terrainprodukts erscheinen Quellenprismen
ohne Terrainunterlage. Diese Prismen zeigen Provider-Footprints/Höhen, keine
Buildings-Architektur. Bilder bleiben privat unter `/tmp`; keine Galerie wird geliefert.

## Herkunft und Reproduktion

Produzent/Quellleser: `a6fbf16a399aa308b464546b905972f93d96e6e6`.
Neues Wheel, installierter Leser, Kosten und Renderings nach unabhängigem
Vegetation-Merge: `b01fe8d28254e4f0919bed596c9b501279e8464d`.
Zuvor 176 Quell-/176 installierte Prüfungen auf `231fab4dc`; nach der
Vollflächen-/Kameraänderung 22 betroffene Quell-/22 installierte Prüfungen grün.
205 Python-Paketmodule stimmen mit dem frischen Wheel bytegenau überein.
Ruff/Format und alle Funktionen ≤120 Zeilen. Bibliothek/native Gates unverändert;
native Integration und öffentliche Übernahme bleiben offen.

Private Belege: `/tmp/outshine-transport-occupancy-9e6347892/`.
`current/` hält die langsame Referenz, `final/` die überprüften aktuellen Produkte;
`cost/`, `cost-final/` halten Rohtraces und Statistik. Wichtige SHA256:

| Artefakt | SHA256 |
|---|---|
| `final/comparison.json` | `20b1ae82bd14132e2898b8238f3b8924b17ea00eea3371003a00a25a8ab6e945` |
| `final/volumes.json` | `e1372915c8cb8a38fcfa35b56452a5c215607bda1813c423ff24ad171dc6f05d` |
| installierter Leser | `4b0b95459e1a7f4d19f3dd3ab84386adf9617f2bf94da2b805726e2e05a8ec33` |
| frisches Wheel | `cbff10e82ea48f759aebfdfcbb3841051739690b2e99cc7edb3ce6bbbc29bb2b` |
| Nachher-PNG | `6998da746a17c0844efab5307d1a6e34316fe29aec090352aaebd077369b4884` |

Abhängigkeiten: bestehende NumPy-/Shapely-/Pillow-/MVT-Umgebung; Memray optional
für Allokationsbelege, Blender für Diagnosebilder. Kein SDK-Code/SDK in der Runtime.
Pinned Original-PBF im Repository: `test/outshine/data/transport/wien-junction-14-8937-5678.pbf`
(SHA256 `97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`).
Volles Original-DEM [Mapterhorn 14/8937/5678](https://tiles.mapterhorn.com/14/8937/5678.webp),
56194 B, SHA256 `67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177`.
Die geprüfte Eingabekette besitzt [stair_seed_comparison.md](stair_seed_comparison.md).
Frische Ausgabepfade verwenden; Variablen zeigen ausschließlich unter `/tmp`:

```sh
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/stair_occupancy_experiment.py --resident "$RESIDENT" --prior-module "$MODULE55" --prior-module "$MODULE57" --prior-module "$MODULE65" --dem "$FULL_DEM" --output "$OCCUPANCY"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/stair_occupancy_verify.py "$OCCUPANCY"
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/stair_occupancy_cost.py "$OCCUPANCY" --output "$COST" --profile
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" test/experiments/transport/stair_occupancy_display.py "$OCCUPANCY" --scene "$MODULE65/display/after-overview-scene.json"
```

Recherche zuerst im vorhandenen SIGGRAPH-Paper
[Interactive Procedural Street Modeling](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
Seite 2 einschließlich Figure 2 geöffnet: Trennung von Graph/Geometrie,
kein Beweis dieser Schnittabfrage. Ergänzende Primärquelle:
Bengt-Olaf Schneider, [Accelerating Polygon Clipping](https://diglib.eg.org/bitstreams/4258713d-2168-4dec-9046-e61f58e1c6a5/download),
§1.2/Figure 1.2 auf PDF-Seite 3 ebenfalls optisch geprüft. Verwendet werden
die Halbebenenidee und eine eigene Herleitung/Implementierung; kein Fremdcode.
