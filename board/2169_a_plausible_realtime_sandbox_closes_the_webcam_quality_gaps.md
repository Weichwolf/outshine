Type: feature
State: open
Area: world, render, generators, navigation
Tags: webcam, measured
Depends: 2188, 2092, 2101, 2111, 2128, 2129, 2137, 2138, 2140, 2144, 2145, 2152, 2155, 2166, 2167, 2168, 2170, 2171, 2172, 2173, 2174, 2175, 2176

# A plausible realtime sandbox closes the webcam quality gaps

## Auftrag und Abnahmemaßstab

Stand 2026-09-07, Renderer-Commit **12ceb790**. Ziel: eine plausible, annähernd fotorealistische
Open-World-Sandbox in Echtzeit aus **OSM, DEM, Zeit und Wetter**. Die Webcam ist visuelle
Referenz, kein Generatorinput und kein Auftrag zur Rekonstruktion ihres tatsächlichen Weltzustands.
Belegte Gelände-/Netz-/Gebäudedaten erhalten; unbekannte Bauformen, Artenmischung, Materialdetail,
Wolken und Population plausibel und deterministisch generieren. Keine Place-ID-Sondermodelle.
Zusätzliche Quelldaten wie Fototexturen, Photogrammetrie oder reale Gebäudemodelle sind keine Voraussetzung.

| Gegenstand | Maßstab |
|---|---|
| belegte Lage/DEM/OSM-Semantik | geographische und topologische Konsistenz innerhalb Quellunsicherheit |
| ungetaggte Architektur/Geologie/Vegetation | plausible generische Formen/Verteilungen und korrekte Konstruktion |
| Licht/Luft/Wasser/Material | physikalische Zusammenhänge, konsistente deklarierte Zeit/Wetterdaten |
| Boote/Fahrzeuge/Menschen/einzelne Wolken | eigene plausible Sandbox; kein Fotozähl-/Pixeloracle |
| Kamera | korrekt deklarierte Konventionen; Referenzpose kalibrieren, Restunsicherheit nennen |
| Navigation/NPC/Map | logisches 2D-Netz mit Ebenen/Verbindungen, unabhängig vom Render-Mesh |
| Kontakt/Darstellung | aus demselben räumlichen Alignment, mit getrennten LOD-/Fehlerverträgen |

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

## Implementierungsreihenfolge

`Depends` bezeichnet Voraussetzungen für die vollständige Abnahme, keine Sperre für unabhängige
Vorarbeiten. 2169 ist das übergeordnete Abnahme-WI; Kinder hängen nicht zurück von 2169 ab.

| Stufe | Arbeiten | Ergebnis |
|---|---|---|
| P0, belastbare Engine | 2188 API-/Lifecycle-Audit: zuerst 2190/2191, dann 2124 → 2130; 2170 Kamera/Datum; 2154 reproduzierbare Eingänge; 2124 Framepfad; 2132 Streaming; 2123 LOD | bewegte Kamera, begrenzte Residency und zurechenbare Bildfehler |
| P1, Bildgrundlage | 2166 Terrain einschließlich Seitenflächen → 2144 Nähte; 2171 MR; 2167 indirektes Licht; 2128 Schatten; 2152 GLSL | Gelände und einfache Oberflächen überzeugen bei konsistenter Beleuchtung |
| P2, gebaute Welt | 2173 Semantik; 2133 logisches Netz → 2175 Alignment/Bauwerke; 2121/2168 Kontakt/Körper; 2138 Gebäude; 2145 Ufer/Wassergeometrie | plausible Formen und räumliche Anschlüsse, befahrbare Brücken und Tunnel |
| P3, Darstellung vervollständigen | 2172 Wetter → 2140 Wolken; 2129 Reflexionen; 2137 Bodendetail; 2155 Kameraantwort | stimmige Atmosphäre, Wasser und Nahoberflächen |
| P4, Vegetation | 2111 isolierter Waldnachweis → Weltintegration; 2176 Arten/Ökologie | dichte, passende Bestände mit vollständiger Distanzleiter und Streaming |
| P5, Population | 2174 Sandbox-Population | belebte Welt auf tragfähiger Darstellung und Navigation |

Prioritätsentscheidung vom 2026-09-08: Die bisherige Vorziehung des Baumpfads war falsch.
Große Nah-Billboards und unpassende Kronen in den aktuellen Koerbersee-/Rosenheim-Bildern
sind ein abgelehnter Zwischenstand. Weitere Arten und Atlas-Hilfsfunktionen schließen die
grundlegenden Gelände-, Material-, Beleuchtungs- und Streaminglücken nicht. Deshalb zuerst
P0/P1; nächster fachlicher Prüfpunkt ist 2166 an Malcesines Seitenflächen, nach Klärung
der zugehörigen räumlichen Konventionen. Laufende uncommittete Vegetationsänderungen bleiben
als nicht abgenommener Arbeitsstand erhalten und sind vor Übernahme gesondert zu prüfen.

Vor Wiederaufnahme der Weltvegetation verlangt 2111 eine deklarierte großflächige ebene
Waldszene: gemeinsame Prototypen, Instancing, hierarchisches Culling, Nah-/Mittel-/Fern-LOD,
räumliches Laden und Freigeben. Fläche, Dichte, Sichtweite und Kamerafahrt vor dem Lauf
festlegen; PNGs, CPU/GPU-Framezeiten, Overdraw und Speicher einschließlich Spitzen prüfen.
Die unklare Flächenangabe „24ß km“ wird nicht als erfundene numerische Anforderung übernommen.
Erst der isolierte Nachweis erlaubt Integration und standortgerechten Artenausbau.

2092/2143 Bewegung und Dauerlauf begleiten jede Stufe, statt erst am Ende Leistung zu prüfen.
Stufen sind eine Reihenfolge der Integration, keine pauschale Sperre für notwendige
Abhängigkeitsreparaturen. Priorisierung und Änderungen daran verantwortet der implementierende
Engine-/C++-/GLSL-Spezialist anhand der Befunde.

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
