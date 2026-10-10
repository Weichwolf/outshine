Type: feature
State: active
Architecture: ready
Priority: P0
Parent:
Depends:
Area: client, world, generators, render, gameplay
Tags: vision, webcam, sandbox

# Outshine becomes a believable worldwide open-world sandbox

## Ergebnis und Ausgangspunkt
Plausible, funktionale Erdinterpretation vom Orbit bis zum Grashalm, optisch an GTA5/RDR2 orientiert.
OSM/DEM/Wetter liefern Verteilung, Großformen und Parameter; prozedurale Rezepte ergänzen sie.
Default: minimalistischer Solarpunk mit Bauhaus-/Art-déco-Prägung; Look in 2155.
Zuerst größtmögliche Webcam-Annäherung im Hardwarebudget; später physikalische Interaktion,
LLM-NPCs, JS/HTML-CSS, Spatial Audio und Save/Load/Replay. Bestehende Renderer, native Assets,
Importe, Straßenprofile und Terrain-Deformation sind die Basis. Kein pauschaler Neustart.
Der aktuelle Place-Ladepfad liefert noch keine vollständige, budgetgerechte Welt.
Ziel auf A18 Pro: hohe Bildqualität bei 480p30, vorbereitete Places warm unter zehn Sekunden.
720p60 bleibt Vergleichsmessprofil; eine Sekunde Laden ist eine Optimierungs-Challenge, kein Gate.
AGENTS besitzt Arbeitsregeln/Abnahme; technische Verträge stehen bei ihrem Featurebesitzer.

## Pflicht-Places
| Place | foto-webcam.eu/webcam/ | Bildziel |
|---|---|---|
| Wien | wien | Vollständige Stadt, Donau/Brücken, Fernverbände |
| Flensburg | flensburg | Korrekte Küste/Pegel, Backstein/Kirchturm, Hafen |
| Rosenheim | rosenheim | Dächer/Sonderbauten, Alpen, Nachtlicht |
| DarmstadtWest | darmstadt-west | Räumliche Fassaden, diffuse Stadttiefe |
| Husum | husum-hafenklappbruecke | Kai/Giebel, Brücke/Wasser, Nässe |
| Feldkirch | feldkirch | Altstadt/Fluss/Tal, Hanganschlüsse |
| Malcesine | malcesine | See/Felsufer, Spiegelung/Dunst |
| Koerbersee | koerbersee | Grate/Schnee/Schmelze, See/Wolken |

Tokyo (historische Shibuya-Kamera) und Central Park sind zusätzliche harte Dichtebenchmarks.
Alle zehn Places teilen Profil, Bildauftrag und Ziel unter 10 ms pro Frame beim aktuellen Umfang;
Objektzahl darf GPU-Arbeit bei gleicher Bildwirkung nur gering beeinflussen. Keine Inhaltskürzung.

## Katalog der Defizite und ihres Zielpfads
Kostenreview 2026-10-07: aktuelle Offline-Places und Code, kein A18-Pro-/Internetnachweis.
Messdetails/Einheiten bei 2280; Verfahrensentscheidung bei 2336. Weitere Befunde beim Besitzer.
| Priorität | Defizit | Zielpfad und Besitzer |
|---|---|---|
| P0 | Zehn native Treffer ohne Quellenbytes/Providerdecode; Zwischenfelder bleiben | Fertige Region-/Kontaktprodukte vor Feldassembly laden, 2280 |
| P0 | Feine DEM-/OSM-Anfragen vor möglicher Sichtbarkeit | Grobe DEM-Eltern → Rundum-Horizont → benötigte Kinder, 2336 |
| P0 | Native Höhensnapshots sparen 25–32 MiB Nahplatzierung; CP/Tokyo je 2 MiB Kamerahöhe bleiben | Aktive Kontakte/übrige Produkte vor Quellenarbeit laden, 2280 |
| P0 | Frischer Trefferprozess: Peak-Footprint 4,08 GiB, RSS 1,37 GiB | Produktkopien, Upload-Scratch und Treiberreserve getrennt erklären, 2280/2339 |
| P1 | Native Mip-Karten entpacken 248 statt 620 MiB; RAM-/GPU-Kosten bleiben unvollständig erklärt | Bedarf vor Decode, gezielte Produktversion, Tokyo-Footprint zuordnen, 2280/2336 |
| P1 | Native Geometriephase CP/Tokyo 2,17/2,87 s; Teilkosten unbekannt | CPU/Allokation/Upload/Fence getrennt messen und Spitzen ersetzen, 2339/2188 |
| P1 | Tokyo erzeugt 2,36 Mio. Gebäudedreiecke; ferne Auswahl nur kachelweise | Hierarchische Coverage, Hülle/Cluster/Impostor vor Emission, 2336 |
| P1 | Erstframes/Fence-Spitzen, GPU-Passzeiten fehlen | Readiness/Upload-Lebensdauer, direkte Backendmessung, 2340/2339 |
| P1 | Wasser/Ufer, Sonderklassen/Parts, komplexe Bauwerke fehlerhaft | Native Pegel/Kontakte, Semantik und Ebenen, 2145/2173/2281/2338 |
| P1 | Breiter OSM-Lieferumfang erreicht die Welt nur teilweise | Vorhandene Klassen in Formen, Materialien und Belegung integrieren; fehlende Angaben plausibel ergänzen, 2341 |
| P1 | Fels/Fassaden/Licht bleiben flach und repetitiv | Gemeinsame Form/Material-/Lichtkomposition, 2337/2171/2155 |
| P1 | Öffentliche Erweiterungen umgehen dieselbe Bedarf-/Produktkette | Generische Welt, Generatorplanung, native Verträge, 2188 |
| später | Wälder POC; Umwelt und bewegte Sandbox fehlen | Nach Gebäude/Terrain/Infrastruktur: 2111, 2172, 2136 |

Typische Last ist eine bewegte Spielwelt: Kamera, Figuren/Fahrzeuge, Wind, lokale Lichter und
Wolkenschatten. Statische Form/Material dürfen wiederverwendet werden; ihre fertige Beleuchtung
ist dadurch nicht statisch. Die Place-Drehung ist ein Integrationsfall, kein Ersatz für diese Last.

## Priorität und Lieferung
| Reihenfolge | WI | Nächster sichtbarer Nutzen |
|---|---|---|
| 1 Infrastruktur | 2281, 2145, 2338 | Zusammenhängende Straßen, Schienen und Wege; Brückenebenen, Profile und sichere Kontakte |
| 2 Gebäude | 2173, 2336, 2171 | Vollständige Formen/Höhen, Dächer, lesbare Fassaden; Großstadt-LOD |
| 3 Terrain | 2337, 2171 | Plausibles Relief, Fels-/Bodenmaterial statt grüner Kunststoffflächen |
| 4 Vegetation | 2111 | Vollständige performante Wälder bis zu räumlichem Nahlaub |
| 5 Wolken | 2172 | Wetterhimmel und kohärentes Bodenlicht im zuvor freigemachten Budget |
| Danach Sandbox | 2136 | Kräfte/Kontakte/Gelenke, NPC/JS/LLM, Ton, Persistenz |

2280 (indizierter Asset-Cache/Laden), 2188 (öffentliche Grenzen) und 2155 (Licht) werden mit diesen sichtbaren
Lieferungen integriert. Gemessene Einsparungen schaffen das Wolkenbudget; keine Lockerung
von Profil, Sichtweite, Inhaltsvollständigkeit oder Framebudget.

## Reihenfolge und Zuständigkeit
2342/2343 liefern den ebenen Vektorplan samt Nutzungsbilanz und gemeinsamen Bauteilen/Ports.
2344 liefert Profile, 2345 Ebenenrezepte; jeweiliger Vertrag vor Ausbau, Python-Bilder vor nativer Übertragung.
Plausibilität, Ästhetik und Funktion im Budget entscheiden; ungelöste lokale Teile werden ausgewiesen.
2280/2336/2340 liefern dafür Cache, Bedarf und Arbeitsauswahl;
2188 migriert nur benötigte öffentliche Grenzen. Danach Gebäude/Terrain, Vegetation im freien
Budget und Wolken zuletzt. GPU-Passzeiten und A18-Pro-Budget bleiben offen.
Ein günstiges Messfenster schließt kein Gate; sichtbare Qualität folgt dem integrierten Gewinn.
Zuerst Vorbereitungsstillstand und unnötige Arbeit beseitigen, dann verbleibende Kosten messen.
Orbit-Verfeinerung folgt der Bodenhierarchie; sie blockiert keine nahe Stadtverbesserung.
Geometriefehler sofort beheben. Licht/Material/Fels dürfen mit vorhandenen Inputs beginnen,
ohne auf den Abschluss anderer WIs zu warten. Nach jeder vollständigen Lieferung den nächsten
Bildverlust wählen; keine Folge ausschließlich interner Reparaturen als Fortschritt ausgeben.

2280 besitzt Erwerb und fertige, räumlich indizierte Assets aller Generatoren; Cachetreffer
laden ohne Neubau. 2336 besitzt räumlichen Bedarf/LOD,
2188 öffentliche Erweiterungsgrenzen. Die übrigen WIs besitzen jeweils Form, Material,
Licht, Umwelt oder Simulation. `Depends` bezeichnet nur den ausdrücklich genannten fehlenden
Teilvertrag, nicht den Abschluss eines gesamten WI. Keine versteckten Abhängigkeiten.
Weitere historische Places bleiben Diagnosen; Tokyo/Central Park gehören zum Render-Gate.
Der globale CPU-Terrainmesh ist entfernt; vier Places bleiben pixelgleich. Feine Quellen,
Stadt-Ladezeit und Speicher überschreiten weiterhin die angestrebte Arbeitsmenge (2336/2280).

## Forschungsgrundlage
[Engine-weite Recherche](../doc/references/README.md) ordnet SIGGRAPH und verwandte
Primärquellen jedem Feature zu. Empfehlungen sind Integrationsaufträge, keine Abnahme.

## Abnahme
Pflicht: Weltpipeline mit nativen Assets/Cache, DEM/OSM, Umwelt und Offline-Places samt Budgets.
glTF/Animation/Khronos-Bildreferenzen bleiben optionale Kompatibilitätsprüfung; gemeinsame
Shaderbindungen, CPU/GPU-Layouts und Ressourcenlebensdauer bleiben verbindlich.
Alle acht vollständigen Bilder mit datierten Referenzen vergleichen; Formen, Materialien,
Licht/Wetter, Stabilität und Kosten getrennt bewerten. Client-API und AGENTS-Gate verwenden.
`src/assets/places`/`client/PlaceCamera` besitzen Szenario/Pose; Webcam-Referenzen bleiben in
`build/shots/reference/webcams`. Kamerakalibrierung ersetzt keine fehlende Geometrie.
Rundum-Abdeckung und Datenherkunft belegen; ein Sichtweitenparameter allein beweist sie nicht.
Native Places laden warm in frischen Offline-Prozessen; Erdstraßen haben eigene obere Flächen,
Brücken behalten geschlossene Körper. Weggelassene Unterflächen sparen Geometrie und Speicher.
Fundament-/Beckenränder, gemeinsame Höhenprofile, Gleisfelder und Ebenen bleiben bei 2281/2145 offen.
2281 plant erst kachelweise das Vektornetz, daraus konstruktive 3D-Rezepte; Gestaltung folgt.
Basel Badischer Bahnhof und Zürich HB ergänzen die Diagnosen. Erstaufbau fordert übermäßig
feine Terrainfelder (2280/2336); GPU-Passzeiten und A18-Pro-Budgets bleiben bei 2340/2339 offen.
