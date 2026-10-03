Type: feature
State: active
Architecture: ready
Priority: P0
Parent:
Depends:
Area: client, world, generators, render, gameplay
Tags: vision, webcam, sandbox

# Outshine becomes a believable worldwide open-world sandbox

## Ergebnis
Eine zusammenhängende datengetriebene Welt von Orbit/Flug bis zum Nahdetail, optisch an
GTA5/RDR2 orientiert. Zuerst größtmögliche Webcam-Annäherung im Hardwarebudget; danach
physikalische Interaktion, LLM-NPCs, JavaScript, HTML/CSS, Spatial Audio und Save/Load/Replay.
Technische Grenzen bestimmen den Look; Licht/Material/Schatten vor Pixelzahl. AGENTS
besitzt Quellenpräferenzen, Arbeitsregeln und Budgetabnahme; 2188 die gemeinsamen Verträge.
Vorhandene Engine/Import-/Straßen-/Terrain-/Renderpfade sind Basis, kein Neuanfang.

## Acht Pflicht-Places
| Place | foto-webcam.eu/webcam/ | Bildziel |
|---|---|---|
| Rosenheim | rosenheim | Dächer/Sonderbauten, Alpen, Nachtlicht |
| Flensburg | flensburg | Backstein/Kirchturm, Hafen, korrekte Küste |
| DarmstadtWest | darmstadt-west | Dachlandschaft/Fassaden, diffuse Stadttiefe |
| Wien | wien | Vollständige Stadt, Donau/Brücken, Fernverbände |
| Husum | husum-hafenklappbruecke | Kai/Giebel, Brücke/Wasser, Nässe |
| Feldkirch | feldkirch | Altstadt/Fluss/Tal, Hanganschlüsse |
| Malcesine | malcesine | See/Felsufer, Spiegelung/Dunst |
| Koerbersee | koerbersee | Grate/Schnee/Schmelze, See/Wolken |

## Weg und Featurebesitz
| Priorität | Lieferung / konkreter Schwerpunkt | WI |
|---|---|---|
| P0 jetzt | Schnelle austauschbare Quellen → residenter erster Place, zuerst Wien | 2280 |
| P0 integriert | Öffentliche Adapter/Provider/Generatoren → generische native Welt | 2188 |
| P0 | Distanz-/Fehlerbedarf vor Arbeit; Fernverbände, Flug/Orbit ohne Löcher | 2336 |
| P0 | Straßen/Bahn/Wege, Kreuzungen, Brücken/Tunnel/Markierungen | 2281 |
| P0 | Richtige Gewässerkörper/Pegel/Ufer und Terrainkontakte | 2145 |
| P0 | Vollständige Gebäude/Höfe/Parts, Dächer und räumliche Nahfassaden | 2173 |
| P1 | Metrische gefilterte Baustoffe, Glas und Alterung | 2171 |
| P1 | Weltlicht/Schatten/Nacht, Reflexion, HDR/Kamera/History/Pacing | 2155 |
| P1 | Wolken/Atmosphäre/Wetterzustand, Sonne/Mond/Planeten/Sterne | 2172 |
| P1 | Gerichtetes Felsrelief, Risse/Schutt/Nahboden | 2337 |
| P1 | Technische Objekte und plausible Autos/Boote/Szenenbelegung | 2338 |
| P2 zuletzt | Standortgerechter Fernwald bis Äste/Blätter/Grashalm | 2111 |
| P3 | Gemeinsame Physik/Commands, NPC/JS/UI, Ton und Persistenz | 2136 |

## Arbeitsfähige Reserve und Integration
2280 und 2188 sind aktive Kinder. Zuerst brauchbare Quellen bis zum vollständigen
Wien-Bild, mit notwendigen öffentlichen Grenzen; 2336 ist die nächste Reserve gegen
Detail-/Datenexplosion. Quellen-, Produkt- und Zeichenbedarf getrennt halten.
2281/2145/2173 erhalten und verbessern vorhandene Geometrie; P1 gewinnt Bildqualität
auf dieser Welt, ohne auf Vegetation zu warten. Kinder nennen nur konkrete fehlende
Verträge in Depends; ein großer Architektur-WI ist keine pauschale Wartebarriere.
Central Park/Tokyo sind zusätzliche dichte Ladebenchmarks, historische Places bleiben
Diagnosen. Hockenheim-Runden/Physikausbau sind kein erster visueller Meilenstein.

## Abnahme
Alle acht echte Bilder mit passenden datierten Referenzen vergleichen; Formen,
Materialien, Licht/Wetter, Stabilität und Kosten getrennt bewerten. Quelle/UTC/Kamera
→ Inputs → Generator → Welt → Bild/Ton/Simulation → nächster Zustand lückenlos integrieren.
`src/assets/places`/`client/PlaceCamera` besitzen Szenarien/Kamera; Referenzen unter
`build/shots/reference/webcams` erhalten. Kamera-/Datum-/Zeitkalibrierung repariert keine
falsche Geometrie. Pflichtgate/Budgets aus AGENTS; Fotos sind keine Generatorinputs.
Keine exakte Rekonstruktion unbekannter Details oder Gerätequalität aus einem Entwurf behaupten.
