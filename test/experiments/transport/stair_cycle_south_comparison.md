# Südlicher Radweg und wirkliche Außenprofile (#434)

Dieselbe begrenzte Versuchsfolge verarbeitet die dritte feste
[Planungseingabe](fixtures/stair-cycle-south-vienna.json). Sie ergänzt den ganzen
südlichen Radweg `39881120:51` (534,861 m) zum
[20-Quellenstand](stair_cycle_north_comparison.md). Bisherige Achsen, Priors,
Originalgeometrien, Eigenschaften und IDs bleiben erhalten; gemeinsame Höhen
und Terrain werden neu geplant. Keine neue Formulierung oder SDK-Abhängigkeit.

## Tatsächliche Kontakte und Datenbereich

Der wirkliche alte Endknoten 3198/3435 gehört zu `39881120:28`; sein ganzer
Querschnitt ist jetzt gebunden. Der weitere gemeinsame interne Quellvertex
2973/3073 ist kein Endpunkt des neuen Quellteils. Ein ausdrücklich getesteter
falscher Endanschluss bleibt abgelehnt. Gemeinsame zusätzliche Bandflächen
bleiben Planentscheidungen. Sieben wirkliche Außenprofile bleiben offen:
`4420992360:6` am Anfang, `32651280:3`, `39881120:23` und `39881120:51` beidseitig.
Kein gemessener Straßenknoten wird aus bloßer Nähe erfunden.

Der komplette rechteckige Terrainbereich braucht vier wirkliche Mapterhorn-Kacheln,
obwohl die ausgewählten Achsen allein die südöstliche Ecke nicht erreichen.
Die Aufnahme akzeptiert ein bis vier ausdrücklich benannte eindeutige Fenster
im begrenzten 2×2-Bereich des Quellframes. Zwei Reihen bekommen Dateinamen mit
X und Y; bisherige Zweifenster-Eingaben und ihre Hashes bleiben unverändert.
Doppelte/fremde Kacheln und fehlende benötigte Höhen bleiben Fehler.

Neue Rohbytes: `14/8937/5679`, 60344 B,
SHA256 `fdcdbcdc1a23efbf059e49c7b6877d360af14e7c4c18ea78c50d32ea45b7a800`;
`14/8938/5679`, 53812 B,
SHA256 `b17173dba20466cc1475ed8c267300ae15f0b5325a428f47c3c1320d20ad237d`.
Provider-URLs und feste Crop-Hashes stehen in der Eingabe. Original-PBF und beide
nördlichen DEM-Rohstände bleiben erhalten. Kein Erwerb bei normaler Reproduktion,
keine Extrapolation und keine vollständige kachelübergreifende OSM-Abnahme.

## Geprüfter Stand

Geometrie und Messungen auf `237bd38f50387fb552187b37c6095186c765a630`,
mit unabhängigem Leser desselben Commits: 39 Kontrollen in 41,040 s, terminal OK.
21 ganze Quellen, 21 gemeinsame flache Kontakte, sechs geschützte ganze
Endquerschnitte, drei Treppen/24 Stufen und sechs tatsächliche Unterführungen
≥2,519989 m in Float32. Verkehrskörper 5624 V/11276 F, alle Vertexlinks zyklisch.
Die sieben bisherigen Nord-Körper-/Höhen-/Darstellungsdateien wurden frisch
bytegleich erzeugt; alte Ost-/Nord-Eingabehashes bleiben eigene Regressionen.

Terrain 8517 Höhenpunkte/16710 Facetten, geschlossener Solid 17034 V/34064 F.
Alle 2884 ursprünglichen Bodenkontaktfacetten bleiben XYZ-bytegleich in Double
und Float32, gemessene ganze Kontaktabstände 0/0 m. Der allgemeine hergeleitete
Float32-Kontaktumschlag bleibt 0,590943 mm; kein globales Null-Überlappungsvolumen.
Erde maximal 0,664666918/0,665628756, konstruktiv 8,724059842/8,779035513
bei unveränderter Grenze 10; 32 konstruktive Facetten, 14,560742 m² projiziert.
Der wirkliche 5-m-Bereich ist in beiden Präzisionen geprüft. Vier ganze Crops und
8673 Punkte einschließlich aller endgültigen Terrainpunkte stimmen exakt gegen
den vollständigen Decode der vier retained Rohdateien. Auch die vierte Kachel
trägt wirkliche Terrainpunkte; fehlendes Fenster bleibt ein Gegenfall.

Fünf frische, vollständig decodierte und tatsächlich geöffnete 1280×720-PNGs:
gleiche Vorher-/Nachher-Kamera je Übersicht und ganzer Südquelle, dazu der ganze
Brückenverbund bei 420 m Kameraspanne. Tatsächliche Mesh-/Szenen-/Normal-/Farb- und
Importbindung geprüft. Die vergrößerte Terrainfläche verändert die Bilddeckung;
Pixeländerungen sind kein Qualitätsmaß auf identischer Geometrie. Schmale Wege
bleiben in der Übersicht schwach lesbar; der zusätzliche Blick zeigt ihre Anschlüsse.
Blender 5.2.2 LTS EEVEE/llvmpipe, kein Outshine-/GPU-/Gerätebeleg.

Originalinventar 1657 Linienteile/71 Flächen; 21 Kandidaten, 1636 Linien und alle
71 Flächen mit ursprünglichen IDs/Eigenschaften/Nichtverwendungsgründen erhalten.
56 Linien haben mögliche Bandüberlappungen, nicht automatisch physische Konflikte.

## Gleiche Qualität und gemessene Kosten

Fester neuer Plan, beide Varianten Solved/49 Iterationen; sieben gültige Fits
je Variante plus ein Warmfit. Selektiv 39 statt 13826 zusätzliche Kegel,
7072 tatsächliche Solvervariablen nach Eliminieren von 2250 festen Höhen.
Zeilen 71341/112702, A-Nichtnullen 89767/141947; dieselbe Energie und sämtliche
Klassen-/Kontakt-/Exportgrenzen. Größte Höhenabweichung 0,565 µm.

Matrixaufbau/Solve-Median selektiv/vollständig 3916,761/5371,412 ms;
Prozess-Peak 158642176/193122304 B. Lokaler i5-1135G7, zwei physische/vier
logische CPUs, 16 GiB RAM; normale Priorität, jeweils ein eigener Worker.
Während dieser Messreihe liefen wechselnd andere Fachrenderings, beobachtet
u. a. drei Building-Renderer. Daher kein isolierter Beschleunigungsfaktor oder
Vergleich mit früheren Messfenstern. Zählerersparnis und gültige Ausgabe sind
davon getrennt; kein Frame-/A18-/RTX-Nachweis.

Getrennte Memray-1.20.0-Läufe: 3676915/4095546 Allokationsereignisse,
426466117/496467884 angeforderte B, 54696139/84773722 B getrackter Heap-Peak.
Profilierte Phasen 8731,520/10499,964 ms, Prozess-Peaks 167530496/202309632 B;
keine unverfälschten Laufzeiten. Die acht Sekunden gelten dem Solver, nicht
inklusive Python-Matrixaufbau/Profilierung. Explizite XY-Kopie 204408 B,
Substitutionsarrays 223728 B; interne Gesamt-Kopierbytes bleiben unbeobachtet.
Eigene Limits 180 CPU-s/3 GiB, LP 5 s und Terrain 8 s/100 Iterationen unverändert.
Keine globale Optimalität aus dem Status abgeleitet.

Aktuelle Originalprodukte, Bilder, terminale Logs und Gegenfälle:
`/tmp/outshine-transport-stair-south-237bd38f5/`;
Kosten/Profiler: `/tmp/outshine-transport-stair-south-cost-237bd38f5/`.
Diese lokalen Ausgaben sind Belege, keine Laufzeit- oder feste Testabhängigkeit.

## Reproduktion und Abgrenzung

[Aufrufe](README.md). Dieselbe Kontaktband-Zerlegung, selektive Float32-Reserve,
Membranenergie und feste Höhenelimination wie im Nordvergleich; dort stehen
SIGGRAPH-Primärquellen, eigene Ableitung und die bisherigen visuellen Gegenfälle.
Der unabhängige Leser prüft gespeicherte tatsächliche Körper und beide Präzisionen.
Solverstatus allein ist keine Abnahme. Kostenvergleiche verwenden gleiche feste
Vernetzung, Ziele, Intervalle, Qualitätsgrenzen und bisherige Ressourcenlimits.

Keine gemeinsame/lib/native/Reservations-API geändert. Der #483-Pilot bleibt
separat; eine weitere Straßeninstanz erzeugt keinen Katalogeintrag.
`admitted_source_ids=[]`. Übrige Quellen/Ebenen, Fahr-/Gehhüllen, Knickradien,
Handläufe/Statik, allgemeine Selbstschnittfreiheit und native Übernahme bleiben
offen. #434 und der vollständige Transportauftrag bleiben aktiv.
