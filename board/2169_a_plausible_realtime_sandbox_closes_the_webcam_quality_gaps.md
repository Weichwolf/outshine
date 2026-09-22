Type: feature
State: open
Area: world, render, generators, navigation
Tags: webcam, measured
Depends: 2188, 2092, 2101, 2111, 2128, 2129, 2137, 2138, 2140, 2144, 2145, 2152, 2155, 2166, 2167, 2168, 2170, 2171, 2172, 2173, 2174, 2175, 2176, 2196, 2197, 2198, 2199, 2200, 2201, 2202, 2203, 2204, 2213

# A plausible realtime sandbox closes the webcam quality gaps

## Auftrag und Abnahmemaßstab

Visueller Ausgangsbefund vom 2026-09-07, Renderer **12ceb790**. Ziel: eine plausible, annähernd fotorealistische
Open-World-Sandbox in Echtzeit aus **OSM, DEM, Zeit und Wetter**. Die Webcam ist visuelle
Referenz, kein Generatorinput und kein Auftrag zur Rekonstruktion ihres tatsächlichen Weltzustands.
Belegte Gelände-/Netz-/Gebäudedaten erhalten; unbekannte Bauformen, Artenmischung, Materialdetail,
Wolken und Population plausibel und deterministisch generieren. Keine Place-ID-Sondermodelle.

| Gegenstand | Maßstab |
|---|---|
| belegte Lage/DEM/OSM-Semantik | geographische und topologische Konsistenz innerhalb Quellunsicherheit |
| ungetaggte Architektur/Geologie/Vegetation | plausible generische Formen/Verteilungen und korrekte Konstruktion |
| Licht/Luft/Wasser/Material | physikalische Zusammenhänge, konsistente deklarierte Zeit/Wetterdaten |
| Boote/Fahrzeuge/Menschen/einzelne Wolken | eigene plausible Sandbox; kein Fotozähl-/Pixeloracle |
| Kamera | korrekt deklarierte Konventionen; Referenzpose kalibrieren, Restunsicherheit nennen |
| Navigation/NPC/Map | logisches 2D-Netz mit Ebenen/Verbindungen, unabhängig vom Render-Mesh |
| Kontakt/Darstellung | aus demselben räumlichen Alignment, mit getrennten LOD-/Fehlerverträgen |

Volumetrisches Licht ist eine Kernkompetenz. Lichtführung, Schatten, Atmosphäre und
Farbgestaltung weltweit über Tages- und Jahreszeiten gemeinsam abnehmen; Details in 2172.

## SOLL/IST – visuelle Befunde

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

## Verbindliche Arbeitsreihenfolge

P0 priorisiert aktuelle rote Gates und belegte Lebensdauer-/Publikationsfehler.
clang-tidy bleibt auf **null**; offene Gesamtaudits sperren unabhängige Bildarbeit nicht.
Jetzt 2250 (Bodenrauheit), 2166 (Terrainvorhang lokalisieren und korrigieren),
2248 → 2247 (reproduzierbare Strukturprodukte) bearbeiten. Materialien und Licht
auf einfachen Szenen parallel zur gebauten Welt entwickeln. Places ohne Vegetation.

| Stufe | Arbeiten | Abnahme vor nächstem Ausbau |
|---|---|---|
| P0 Engine bereinigen | 2094 Gates; 2188 native API/Ownership/Lifecycle, 2190/2191 GPU, 2209 → 2194 Fehler/Allokation, 2124/2130/2132 Jobs und Streaming | null Tidy-Befunde; öffentliche Verträge dokumentiert und geprüft; relevante Tests und Negativkontrollen grün |
| P1 Materialien und Lichtgrundlage | 2216 Materialverträge, 2179 Filterorakel, 2152 GLSL; 2171 zunächst analytische Flächen; 2167 direkt/indirekt, 2128 Schatten, 2155 Kameraantwort | MR/BRDF/Farbräume/Normalen/Maßstab und Belichtung auf einfachen unabhängigen Szenen überzeugend |
| P2 Gelände und gebaute Welt | 2170 Pose/Datum; 2166/2144 Gelände; 2173 OSM; 2133 logisches Netz → 2121/2175 Anschlüsse/Bauwerke; 2138 Gebäude; 2145 Wasser/Ufer, 2129 Reflexionen; 2171 auf alle Generatoren übertragen | Städte/Landschaft ohne Pflanzen plausibel; Brücken/Tunnel/gestapelte Ebenen korrekt, Materialdetail und Nah-/Fernübergänge gut |
| P3 Atmosphäre und Himmel | 2172 gemeinsamer Zeit-/Wetter-/Luftzustand; 2167 weltweite Beleuchtungsabnahme; 2213 Sonne/Mond/Sterne | klare Luft, Dämmerung, Nacht, Jahreszeiten und Hemisphären konsistent; Geländeabschattung und Himmelshelligkeit stimmen |
| P4 Wolken | 2140 nutzt P3 für Dichte, Streuung, Verdeckung und Wolkenschatten | klar/bedeckt und Wetterwechsel zeitlich stabil; keine doppelte Atmosphärenkomposition |
| P5 Vegetation | 2111 isolierter ebener Waldnachweis → Weltintegration; 2176 Arten; 2137 krautige Vegetation/Unterwuchs | Instancing, Culling, vollständige LOD-Leiter, Overdraw, Streaming und Standortplausibilität belegt |
| P6 Population und Effekte | 2174 bewegte Sandbox-Population; 2137 Partikeleffekte; Audioausbau nach 2212 | auf konsistenten Navigations-/Simulationsverträgen aufbauende belebte Welt |

`Parent` bezeichnet Zugehörigkeit, `Depends` fachliche Voraussetzungen der vollständigen
Abnahme. Die Tabelle priorisiert Arbeit; keine künstlichen Depends-Ketten nur für Reihenfolge.
Insbesondere Materialgrundlagen benötigen keine fertige Stadt, Himmelskörper keine Wolken.
2171s komplette Generatorabnahme folgt erst nach P2; deren Materialkern beginnt in P1.
2169 ist die Gesamtabnahme; Kinder hängen nicht auf 2169 zurück.

2092/2143 messen Bewegung, Framezeiten und Speicher bei jeder Stufe. API-/Datenverträge,
Streaming, LOD und Feature-Schalter gehören zur Grundlage, nicht in eine späte Optimierung.
Plausibler gestalteter Look zählt; technische Geometrie-/Lichtfehler bleiben Fehler.
Vor Weltvegetation: deklarierte ebene Waldfläche mit Dichte/Sichtweite/Kamerafahrt,
CPU/GPU/Overdraw/Speicherspitzen und geöffneten PNGs. Kein Weltvegetationsausbau vorher.

## Verbindliche Abnahmen je Kamera

RDR2/GTA5 (PS4) bestimmen die visuelle Baseline; die folgenden offenen Kinder konkretisieren
sie anhand der selbst geöffneten Webcams. Kein vorhergesagter exakter RAGE-Render.

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

Alle Kinder müssen bestehen. Fotoartefakte, Logos, exakte Population und Wolkenpositionen
sind kein Ziel. PNG-Abnahme erfolgt über den Client, mit kalibrierter Kamera und deklarierter
Zeit/Wetterlage; kein Foto-Pixeloracle. Zusätzlich Bewegung, Unter-/Nahansichten und Transfer
auf andere Seeds/Orte prüfen. Materialqualität bedeutet Khronos MR einschließlich BRDF,
Farbräumen, Rauheit, Normalen und plausiblen Texturmaßstäben; kein bloßes Grundfarbenfeld.

## Globale Abnahme

- [ ] Alle neun Paare erneut visuell öffnen, jeweils Gesamtbild und ursachenspezifischen
      Ausschnitt/AOV; Befund, noch offene Grenze, Digest und Kosten festhalten. Kein SSIM/
      Pixelgleichheitsziel gegen unabhängig erzeugte Population oder unbekannte Webcam-Grade.
- [ ] Kamera-/DEM-Korrespondenzen getrennt von generischer Material-/Formqualität prüfen.
      Zusätzliche Nah-/Seit-/Unter-/Tunnelansichten und Dreiebenen-Verkehrsfixture nach 2175.
- [ ] Kein Generator endet bei Fallback-RGB: 2171s Material-Coverage über alle Geometrieklassen.
      2176 erweitert den vorhandenen Baumgenerator; 2111s Placement-Fix allein genügt nicht.
- [ ] Tag/Nacht, klar/bedeckt/Regen/Nebel/Schnee und Jahreszeiten; mindestens ein nicht für
      den Fotovergleich angepasster Ort/Seed je relevanter Generatorfamilie als Transferprobe.
- [ ] 720p60 auf dem Projektziel Apple A18 Pro gemäß 2092, nicht nur residenter Stillrender.
      1000/60 = 16,666… ms; jede neue Stufe mit CPU/GPU/Bytes und Gesamtframe messen.
      Noch keine belastbaren Einzelpass-Etats: zuerst Profiling, keine addierten isolierten p99.
- [ ] Logisches Netz unverändert bei Render-LOD/Unsichtbarkeit; räumlicher Kontakt und
      Darstellung versioniert konsistent. Bekannte rote Orakel/Lint bleiben offen.

Historische Renderreihen und vollständige Bildidentitäten stehen in Git. Aktuelle
PNG-Referenzen: `build/shots/reference/terrain-20260908/`. Logs im System-Tempverzeichnis.
