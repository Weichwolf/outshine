# Tatsächlicher Pflanzenverbrauch der Transport-/Gebäudefreiräume (#568)

Stand 2026-10-17: Das vorhandene junge Pinus-SITE-Rezept konsumiert vorbereitete
Wiener Habitate, aktuelle Terrainkontakte und die tatsächlichen Gebäudeprodukte
aus #582 gemeinsam mit Transport. Kein neues Wuchsprogramm, keine Änderung an
gemeinsamer Bibliothek, nativer Engine oder SDK-Runtime. Entscheidung/Restabnahme:
[#568](https://github.com/Weichwolf/outshine/issues/568).

## Quellen, Form und räumliche Übergabe

Der ursprüngliche PBF enthält 384 Landcover-Features, hier 394 gültige Polygonteile:
55 `wood`, 333 `grass`, fünf `rock`, einen `sand`.
Originalfeatures, projizierte Originalgeometrien, IDs, Attribute und Part-Zuordnung
bleiben vollständig im Ledger. Die vorhandene `StandHabitat`-/`HabitatStand`-API
erzeugt begrenzte kanonische Standortvorschläge auf dem aktuellen Terrainbereich.
Region: untere Ecke (-821,-388) m, Seite 2048 m, expliziter Part-Regionsschlüssel,
Tiefe 0, Seed 718, höchstens 16 Sites/Part und 32768 Triangulationsdreiecke.
Part- und Quell-IDs binden die innerhalb eines Standes lokalen fünfteiligen Site-IDs.

Junge Pinus-Pflanzung auf gelieferten Gras-/Waldflächen ist eine ausdrücklich
prozedurale Wahl. Die Quelle beweist weder Baumposition noch Art, Alter oder Zahl.
Fels/Sand werden nicht als Habitat verwendet. Eine erste Wald-only-Probe erzeugte
688 Vorschläge; selbst die nächsten akzeptierten Wurzeln lagen ohne XY-Kandidaten
neben den Hindernissen. Dieser positive Lauf war keine schwierige Freiraumprobe.
Die endgültige Probe priorisiert deshalb Gras-/Waldquellen und ihre Sites nach
XY-Abstand zu den tatsächlichen Transport-/Gebäudevolumen; gleiche Daten/Form,
explizit andere Habitat-/Bedarfsauswahl, kein Zeit-/Qualitäts-Siegervergleich.

1024 Vorschläge, maximal 128 vollständige Prüfungen und vier akzeptierte Pflanzen:

| Ergebnis der tatsächlichen 124 Standortprüfungen | Anzahl |
|---|---:|
| Wurzelraum im tatsächlichen Pflanzenhöhenband gesperrt | 10 |
| Freier Wurzelpunkt, aber konservative Pflanzenhülle überschneidet Reservierung | 110 |
| Vollständige gemessene Pflanzenhülle frei | 4 |

Die 110 Fälle sind **keine** nachgewiesenen Ast-/Nadelkollisionen. Ein AABB um
alle tatsächlich erzeugten Vertices enthält auch leeren Raum und darf gültige
Pflanzen konservativ zurückstellen. Die vier akzeptierten Hüllen sind bereits
in XY disjunkt zu den Hindernissen: kein zusätzlicher tatsächlicher Unterführungs-
oder Affin-vs-Konstant-Freiraumgewinn behauptet. Kontrollfälle prüfen Höhenberührung
und eine Hülle über einem freien Wurzelpunkt separat.
Alle vier akzeptierten Pflanzen stammen in dieser Probe aus gelieferten Grasflächen.

34 Part-Ledger werden tatsächlich abgetastet, 30 weitere erhalten nur Vorschläge;
900 Sites bleiben ausdrücklich unangefordert. 324 weitere Parts erreichen die
Vorschlagsgrenze, sechs haben unpassende Habitatklasse. Es wird kein Original gelöscht
oder still repariert. Budget-/Geometriefehler sperren nur den örtlichen Standort.
Das ist ein gültiges begrenztes Teilprodukt, keine vollständige Vegetationsverteilung.

Die unveränderte Form aus `prepared().evaluate(seed=718)` und ihr vorhandener
physischer Emitter erzeugen **193 Achsen, 2289 Kurztriebe und 4578 Nadeln**.
Alle 2482 Holz- und 4578 Nadelinstanzen werden vollständig aus ihren vorhandenen
Prototypen/Posen expandiert: 289094 Vertices, 549948 Dreiecke.
Der vorhandene physische Leser bestätigt Eigentümer, Float64-Posen und Anheftungen;
maximaler beobachteter Anheftungsfehler 4,440892098500626e-16 m.
Gemessene lokale Grenzen: X [-1,0243986447;0,9713281866],
Y [-1,0212087981;1,0823739476], Z [0;3,2744471689] m.
Eine einmal erzeugte gemeinsame Form begründet die Abfragen aller vier Sites.

`StandHabitat.prepare(..., support=resolved_support)` erhält den ursprünglichen
Quellpolygonbereich und den mit diesem tatsächlichen Höhenband bereinigten Support.
`RootSite`/`site_input` und `prepared(SITE).evaluate(outputs=("placement","plant"))`
erzeugen anschließend die wirkliche Platzierung. Die ausgegebene Form muss genau
der zuvor gemessenen Form entsprechen. Das bisherige lokale SITE-Datum
`ellipsoid0-flat-study-no-DEM` bleibt unverändert und ausdrücklich lokal.
Ein pro Site benannter Frame und `transport-flat-plant-to-current-terrain/1`
übersetzen Z danach um den tatsächlichen Rootkontakt im gemeinsamen Wiener Frame.
XY bleibt erhalten; Vertikalstellung ist Welt-Up. Alter Pilot-Datumstext wird nicht
als tatsächliches Welt-Höhendatum ausgegeben.

Terrainface/-normale, World-Z, lokale Platzierung und beide Frames sind gespeichert.
Quellen, Geometrie und Höhen aller **5986 Transport- plus 65 tatsächlich erzeugten
Gebäude-Reservationen** werden über verifizierte Eingabeprodukte gebunden.
Einzelkontakt und freie AABB beweisen weder volle Gelände-/Wurzelpassung noch
Kronenpackung, Wachstumsdynamik oder exakte Körperüberschneidung.
Das gemeinsame `pipeline.prepare` mit echten Fachcallbacks und die öffentliche
affine API bleiben separate offene Schritte; hier konsumiert die reale Sequenz
die bereits erzeugten räumlichen Produkte.

## Prüfungen und Kosten

Produzent/Quell- und installierter Gegenlauf: `5e8ecafd07c4061150656e4c473e2f024ed02679`.
Frische vollständige Leser: `824a65de57882bc8bc75f65b8e4c91501f31c9a2` nach unabhängigem
Buildings-Merge. Alle 400 Formdateien sind quell-/installiert bytegleich.
Standorte, SITE-Produkte und sämtliche Original-/Auslassungsdaten stimmen nach
Entfernen ausschließlich der Abfragesekunden auch mit dem profilierten Lauf exakt überein.
Frische Leser emittieren die vollständige Form erneut, vergleichen alle Vertices,
Dreiecke, Materialzuordnung, Rezept-/Produktdateien und tatsächlichen Site-Produkte.
Alle 120 Zurückstellungen werden mit demselben Ergebnis reproduziert; der Leser
erzwingt Vollscan statt Produzentenindex. Alle akzeptierten Hüllen werden zusätzlich
aus sämtlichen tatsächlich transformierten Pflanzenvertices erneut abgeleitet.
Die allgemeinen GEOS-XY-/Nichtleer-Topologiegrenzen aus dem
[Präzisionsbericht](stair_occupancy_precision.md) bleiben offen.

66 betroffene Quellkontrollen (57 Pflanzen-/Übergabe-/Bibliothekskontrollen plus
neun Renderer-/Farb-/Mehrkörperkontrollen) und ein gemeinsamer installierter
66er Lauf bestehen. 205 Bibliotheksmodule sind unverändert/quellgleich zum frischen
#580-Wheel. Ruff/Format und Funktionen ≤120 Zeilen; native Gates folgen der Integration.

Linux x86_64/i5-1135G7, Python 3.13, NumPy 2.5.4, Shapely 2.2.0.
Quell-/installierter Lauf unter `nice -n 10`, Desktop-/Fremdlast, teilweise eigene
parallele Prüfungen. Eingabeladung getrennt 4,176/4,186 s, vollständige Form
2,090/2,120 s, Habitat/Standort/Abfragen 1,705/1,643 s; RSS-Peaks
227553280/227463168 B. Kein Warm-Asset-, Frame-, GPU- oder Gerätebeleg.
Prototyp-Vertices/-Dreiecke belegen 13537632 B, Blattflächenflags weitere 549948 B;
physische Einzelprototypen/Posen und Python-/GEOS-Heap kommen hinzu.

Memray 1.20.0, Python-Allokatortracking, eigener normal priorisierter Prozess auf
`824a65de5`: **45213987 Allokationen, 6932259921 allokierte Bytes,
164546253 B getrackter Peak**. Voller Prozess einschließlich Imports, ursprünglicher
Quellen-/Terrainladung, Prototyp, Standortprüfungen und Exporte; keine ungemessene
Kopie als null ausgewiesen. Interne kopierte GEOS-Bytes bleiben unbekannt.
Hoher Gesamtaufwand, keine Optimierungs-/Runtime-Abnahme; instrumentierte Zeiten
sind keine normale Latenz.

## Tatsächliches 3D-Bild und Reproduktion

Zwei vollständige **1280×720**-EEVEE-Bilder auf `2db6a43a1`, beide geöffnet und ganz
dekodiert. Base plus vier vollständige Pflanzen: insgesamt 2279739 Dreiecke.
Der erste Einobjektversuch scheiterte ausdrücklich an der bestehenden Farbdaten-
Kapazität; sein Prozessstatus wird nicht als Bildbeleg verwendet.
Die endgültige Szene importiert fünf getrennte unveränderte Körper, jeweils unter
der unveränderten Grenze von 1048576 Farbflächen/Objekt. Jeder Import prüft
Vertices/Topologie, Float32-Positionen, eigene Normalen und Farbattribute.
Der bisherige Einobjektpfad delegiert unverändert; keine Inhaltskürzung.
Float32-/Subpixelorgane können im Renderer kollabieren. Dieser vollständige
Diagnosedetailstand ist kein sinnvoller Fern-LOD oder Laufzeitbudgetnachweis.
llvmpipe: 7,893/32,982 s, RSS-Peaks 1816436736/2961018880 B.
`pixels.py`: **14/921600** Pixel ändern sich, sechs um mehr als 1/255.
Gesamtbild bleibt blass/lückenhaft; kein deutlicher Bildgewinn, keine Veröffentlichung.
Nach dieser Übergabeprobe hat sichtbare Transportnetz-Vervollständigung Vorrang
vor weiteren unsichtbaren Diagnoseausbauten.

Private Daten ausschließlich `/tmp/outshine-transport-plants-a76914d07/`.
Originale/Transport-/Gebäudekette und Herstellerabhängigkeiten besitzt der
[Architekturbericht](stair_consumer_comparison.md), das unveränderte Wuchsprogramm
[Vegetationsbericht](../vegetation/recipe_plant.md). Keine SDK-/Fremdcode-Runtime ergänzt.

| Artefakt | SHA256 |
|---|---|
| `final/comparison.json` | `51ab1a10c42975ffe5f53f38fd6951d0b0a6ec63cc2c09017589cf2f04405420` |
| `final/prototype/complete.npz` | `c4b6da222245ce52fca9967dcd79105ac69c247978ea3fa537aebb340661fefc` |
| `fullscan-final.json` | `a9d32796728320684a16d3f2b02d62b6730d86cbb96f7179243b4c327314a79a` |
| `fullscan-installed.json` | `43bc211c7393b2a851df8ccad521a2fc7ab35ecd74a969f0f2942bf1f71a5546` |
| `equivalence.json` | `d055aad6bdd4c5eb51921f9222aef224638abb501bbbcffff2804e24e0e458fd` |
| `process-stats.json` | `8892a67b62f0a3bef024032e7673b102cc7ed408313b72c02ac1d12c392e0f07` |
| `display-bodies/after.png` | `165b77ef8f454a7f11d7dd18b48f0ff5773b1625ad2ecb0f796626ed3633a8be` |

Module `stair_plant_experiment`, `stair_plant_verify`, `stair_plant_display` jeweils
mit `--help`: explizit frische `/tmp`-Ausgaben, verifizierte Transport-/Gebäudeprodukte
und frischer Gebäudeleser erforderlich. `PYTHONPATH` enthält `lib/src`, `lib/tests`,
`test/experiments/{transport,buildings,vegetation,assets}`. Produzent: CPU 180 s /
AS 4 GiB, Aufruf zusätzlich Wall 240 s; Memray optional, Blender für Bilder.
Keine native Aufnahme, weltweite Nutzung oder vollständige Fachabnahme behauptet.
