Type: feature
State: open
Architecture: planned
Priority: P0
Area: world, render, generators, navigation
Tags: webcam, measured
Depends: 2188, 2092, 2101, 2111, 2128, 2129, 2137, 2138, 2140, 2144, 2145, 2152, 2155, 2166, 2167, 2168, 2170, 2171, 2172, 2173, 2174, 2175, 2176, 2196, 2197, 2198, 2199, 2200, 2201, 2202, 2203, 2204, 2213

# A coherent realtime sandbox closes the webcam quality gaps

## Verbindliches Ziel

Studio-Look zwischen Animation und Realismus gemäß AGENTS.md; Solarpunk-2050 als
Default, belegte OSM-Formen und physikalische Verträge haben Vorrang. Keine fotografische
Rekonstruktion, Place-Sondermodelle oder Kameraorte als Generatorparameter.
Provider liefern OSM/DEM/Zeit/Wetter; deterministische Generatoren ergänzen plausible
Formen, Materialien und Population. Webcam-Paare prüfen Bildkohärenz und Größenordnung.
Navigation und Kontakt teilen Raumreferenzen mit Darstellung, bleiben von deren LOD unabhängig.

## Aktuelle Place-Abnahme 2026-09-28

4844c3196: alle zehn Places ohne Vegetation über den öffentlichen Client geprüft.
Full lint/tidy/API PASS, 256/256 Tidy-Einheiten; 17 fokussierte Prüfungen PASS.
Komplette Places-Suite: 38/44 PASS, 4 TIMEOUT, 2 UNPREPARED; Hockenheim separat PASS.
Refined 7/10: DarmstadtWest, Rosenheim, Husum, Feldkirch, Malcesine, Koerbersee,
Hockenheimring. Alle PNGs geöffnet: `build/shots/places-refined-4844c3196/`.
Wien: 6144 Frames; normal noch 40/43 Gebäude-Tiles, zweite Variante in Earthworks mit
49/49 akzeptiert. Graz/Olympiaturm überschreiten 120 s; kein grünes Gesamt-Gate.
2315 beseitigt echte Timeout-Orphans; nach jeder Eskalation ist der alte Client beendet.
Logs: /tmp/outshine-repair-4844c3196-{full-places,full-lint,gate-results}.log.
Host p95/p99: Darmstadt 19.3496/19.8103 ms, Feldkirch 34.8442/36.7897 ms,
Hockenheim 16.1216/17.2067 ms. Kalter Aufbau eingeschlossen, keine A18-Aussage.
Graz-Probe: 8.0 GB Peak; der beseitigte Stempel-Scan fehlt, Metal waits dominieren jetzt.
Kleinere Tabellen beweisen weder Stadt-Speichergewinn noch tragfähige Laufzeit (2247/2228).
Als Nächstes Publikations-/Vorbereitungs- und Residency/Upload-Kosten sowie Runtime-LOD
reparieren (2311/2312/2298/2247/2228); dann Terrain-Falten, Kanten, Fassaden und Licht.
Darmstadt pixelidentisch; Feldkirch hat zwei Bildstände bei gleichen Dreieckszahlen:
Quellen/Ladezustand/Temporalphase prüfen, keine unbelegte Regressionsursache behaupten.
Malcesine bleibt Faltenvorhang, Koerbersee glatt, Husum gezackt, Fassaden repetitiv.
Hockenheim wirkt wie eine Karte; Nähe/Bewegung bleiben eigene Abnahmen.
Drei frische Playable-Diagnosen geöffnet: `build/shots/places-playable-4844c3196/`.
Wien große Nahwand, Graz fast ganz verdeckt, Olympiaturm unvollständig; keine Abnahme.
Vegetation zuletzt; Hockenheim nur erster Integrationstest. ALLE Places bei JEDEM Code-Gate.

## Historischer Bildbefund (2026-09-07, Renderer 12ceb790)

| Place | SOLL als Plausibilitätsreferenz | IST im neuen PNG | zuständige WIs |
|---|---|---|---|
| DarmstadtWest | strukturierte rote/graue Dachlandschaft, diffuse helle Fassaden, Baumzwischenräume | repetitive Prismen, leere Fassaden, kaum Vegetation, klarer Himmel | 2173, 2138, 2171, 2111, 2167, 2172 |
| Wien | Brücke mit lesbarer Konstruktion, bewachsene Insel, spiegelndes Wasser, Luftperspektive | dünnes Brückenband, kahle Insel, dunkle uniforme Wasserfläche, flache Stadt | 2133, 2175, 2145, 2129, 2111, 2172 |
| Rosenheim | gegliederte Stadt, markante ferne Grate, tiefer gestaffelte Luft | uniforme Dach-/Wandmassen, gerundete Bergformen, fehlende Kronen | 2166, 2173, 2138, 2171, 2111 |
| Husum | gebaute vertikale Kaikante, verschiedene Fronten, reflektierendes Hafenwasser | helle gezahnte Böschung, durchgehende helle Bänder, uniformes Wasser, grobe Bauformen | 2121, 2145, 2175, 2138, 2129, 2171 |
| Olympiaturm | Sportfelder, niedrige Baukomplexe, gegliederte Wohnbauten, hohe Kronendeckung | sehr hohe uniforme Blöcke, kahle Freiflächen, fehlende Feld-/Straßendetails | 2173, 2111, 2176, 2138, 2137, 2175 |
| Graz | Hügel rechts der Mitte, differenzierte Industrie/Stadt, schlanke hohe Bäume | Hügel weit links, auffällige helle Hangflächen, uniforme Blöcke, kahle Stadt | 2170, 2166, 2173, 2138, 2176 |
| Koerbersee | gegliederte felsige Seitenflächen, alpine Baumgruppen, Wiesen, spiegelnder See | grobes aufgeblähtes Relief, große Farbpatches, keine lesbaren Bäume, dunkler See | 2166, 2171, 2111, 2176, 2129, 2167 |
| Malcesine | gegliederte Felswand, bewachsene Halbinsel, strukturierte Wasserreflexion | senkrechter Faltenvorhang/Zähne, kahle Landzunge, fast konstante Wasserfläche | 2166, 2144, 2145, 2171, 2176, 2129 |
| Feldkirch | bewaldete Hänge, eingebundene Fluss-/Straßenräume, gegliederte Stadt | abrupte Nahwand rechts, tiefer Ufergraben, kahle Hänge, uniforme Gebäude | 2170, 2121, 2166, 2145, 2175, 2176, 2138 |

Sichtbefund ist hoch sicher; allein daraus abgeleitete Ursache bleibt Hypothese. Bei Graz/
Feldkirch wurden keine geschätzten Kameraoffsets eingetragen: Pose/Datum und finale Geländeform
müssen zuerst auseinandergehalten werden (2170). Tunnel, Nahfassaden, Schatten unter Brücken,
Nacht und bewegte NPCs sind durch Außen-Standbilder nicht abgedeckt.

## Prioritäten und echte Blocker

| Priorität | Nächste Arbeit | Blocker und Grenze |
|---|---|---|
| P0 | Rote Gates und bestätigte Lebensdauer-/Publikationsfehler | Nur betroffener Pfad gesperrt; keine globale Auditblockade |
| P0 | Terrain-Provenienz 2310/2311; CPU-Verfeinerung 2313 → Runtime 2312 → Gebäude-LOD 2298 | Keine kleinere Schranke ohne vollständigen nativen Oberflächenbeweis |
| P0 | 2092: zeitlich getaktete Hockenheim-Fahrt und übereinstimmende Qualitäts-/Kostentraces | CPU/Fence-Zeit ersetzt keine GPU-Passzeit und keinen A18-Nachweis |
| P1 | 2172: Wetterzustand; danach Wolken/Licht/Materialien | 2111 Vegetation folgt zuletzt; Weltkronen benötigen atomare Publikation |
| P1 | 2140: erste volumetrische Wolkenschicht; 2167/2171 Licht und Materialien | Wolken benötigen verbindlichen Wetter-/Kompositionsvertrag, keine fertige Vegetation |
| P1 | Gelände, OSM-Bauwerke, Wasser und räumliche Anschlüsse | Nur tatsächliche gemeinsame Quellen-/Kontaktverträge blockieren |
| P1 | 2314: gemeinsames Qualitätsbudget aus gemessenen Leitern ableiten | Erst Kosten/Qualität messen; kein vorgezogener generischer Solver |
| P2 zuletzt | 2111: Vegetation, Artenvielfalt und Unterwuchs | Place-Abnahmen laufen ab sofort, nicht erst hier |

Stadt, Wald, Infrastruktur, Himmel und Wolken teilen dasselbe Gesamtframebudget.
Eine Großstadt und ein Wald müssen dieselbe Zeitobergrenze einhalten; kein künstliches
Auffüllen freier Zeit. Keine festen Familienquoten. Sichtbarer Beitrag, Kosten und
zeitliche Stabilität entscheiden. Himmel mit etwa 1/3–2/3 Bildanteil ist ein Kerninhalt;
Wolken verändern auch Bodenlicht und Schatten. Alte P3–P5-Wartefolge ist aufgehoben.
Depends dieses Gesamt-WI nennt Voraussetzungen der vollständigen Abnahme, keine
Startblocker seiner Kinder. Reihenfolge steht in Priority und der kleinen Reserve 2188.

## Place-Abnahmen

| Place | Abnahme-WI | Schwerpunkt |
|---|---|---|
| DarmstadtWest | [2196](2196_darmstadtwest_meets_the_webcam_visual_acceptance.md) | Dachlandschaft und diffuse Stadttiefe |
| Wien | [2197](2197_wien_meets_the_webcam_visual_acceptance.md) | Flussinseln, tragende Brücke und Stadtpanorama |
| Rosenheim | [2198](2198_rosenheim_meets_the_webcam_visual_acceptance.md) | Stadt vor einer gestaffelten Alpenkulisse |
| Husum | [2199](2199_husum_meets_the_webcam_visual_acceptance.md) | Gebaute Hafenkante und lebendiges Wasser |
| Olympiaturm | [2200](2200_olympiaturm_meets_the_webcam_visual_acceptance.md) | Sportcampus, Wohnstaffelung und dichtes Stadtgrün |
| Graz | [2201](2201_graz_meets_the_webcam_visual_acceptance.md) | Bahnraum, Industrie und bewaldeter Stadthügel |
| Koerbersee | [2202](2202_koerbersee_meets_the_webcam_visual_acceptance.md) | Alpine Geländeformen, Bergsee und standortgerechte Vegetation |
| Malcesine | [2203](2203_malcesine_meets_the_webcam_visual_acceptance.md) | Gegliederte Steilfelsen über mediterranem Seeufer |
| Feldkirch | [2204](2204_feldkirch_meets_the_webcam_visual_acceptance.md) | Altstadt im bewaldeten Tal mit eingebundenem Flussraum |

## Abnahme

- [ ] Alle neun Webcam-/Client-Paare selbst öffnen; Ursachen, verbleibende Grenzen,
      verwendete Quellen, Kamera, Wetter, Digest und Kosten festhalten.
- [ ] Bewegung sowie Nah-, Unter-, Tunnel- und mehrstöckige Ansichten prüfen;
      mindestens ein unabhängiger Ort/Seed pro relevanter Generatorfamilie.
- [ ] Materialien: Metallic-Roughness, BRDF/Farbräume, Maßstab und Normalen;
      Tag/Nacht, klar/bedeckt/Regen/Nebel/Schnee und Jahreszeiten gemeinsam bewerten.
- [ ] 720p60 auf Apple A18 Pro, 8 GB: 1000/60 = 16,666… ms Gesamtframe;
      p50/p95/p99, Warmstand/Kaltstart/Bewegung, Speicher und Rückstau getrennt berichten.
      Keine Addition isolierter p99 und kein Ersatz durch residente Standbilder.
- [ ] LOD ändert weder logisches Netz noch Kontakt; Fehlerschranken und Qualitätsverluste
      bleiben sichtbar. Relevante Orakel, API-Dokumentation und vollständiges Lint grün.

Historische Bildidentitäten stehen in Git; PNGs unter build/shots/reference/,
Logs im System-Tempverzeichnis. Unbekannte Wolkenpositionen sind kein Foto-Pixeloracle.

Partikel/Feuer/Rauch/Niederschlag bleiben späterer Ausbau mit nativen Emittern,
begrenzter Lebenszeit/Population/Uploads, Replay und vollständiger Freigabe. Erst einen
eigenständigen ausführbaren WI aktivieren; kein paralleles Wolken-/Mediumsystem in 2137.
