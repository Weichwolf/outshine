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
Weltweite prozedurale Sandbox vom Orbit bis zum Grashalm, optisch an GTA5/RDR2 orientiert.
Default: minimalistischer Solarpunk mit Bauhaus-/Art-déco-Prägung; gemeinsamer Look in 2155.
Zuerst größtmögliche Webcam-Annäherung im Hardwarebudget; später physikalische Interaktion,
LLM-NPCs, JS/HTML-CSS, Spatial Audio und Save/Load/Replay. Bestehende Renderer, native Assets,
Importe, Straßenprofile und Terrain-Deformation sind die Basis. Kein pauschaler Neustart.
Der aktuelle Place-Ladepfad liefert noch keine vollständige, budgetgerechte Welt.
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
Dies ist der fortlaufende Katalog, keine Behauptung eines abgeschlossenen Code-Reviews.
Jeder Befund bleibt beim Featurebesitzer; dort stehen Ursache, Ersatz und Bildabnahme.
| Priorität | Defizit | Zielpfad und Besitzer |
|---|---|---|
| P0 | Tokyo erzeugt noch 2,36 Mio. Dreiecke; Fernbedarf nicht global | Plan → projizierter Bedarf → Hülle/Cluster/Impostor, 2336 |
| P0 | Cache enthält Quellen statt fertiger Game-Assets; Bewegung wiederholt Aufbau | Assetbedarf → Cachehit laden / Miss generieren, räumlicher Index/LOD, 2280/2336 |
| P0 | Rasterarbeit bleibt teuer; HiZ greift bei Drehung nicht | Hierarchische Sichtauswahl, vordere Coverage, begrenzte aktuelle Arbeit, 2340 |
| P0 | CP/Tokyo Prozesspeaks 2,68/3,01 GB; GPU-Anteil noch unbewiesen | Nur benötigte Darstellungen/Attribute resident, geteilte Eingaben, 2336/2188 |
| P0 | Geometrie-/Material-Replay ist noch nicht im Stadtpfad integriert | Unbeleuchtete Fernfelder, gültige Tiefe/Parallaxe, aktuelle Beleuchtung, 2336/2171 |
| P0 | Wasserpegel/Ufer fluten Gebäude oder bilden falsche Stufen | Zusammenhängende Gewässergeometrie und Terrainkontakt, 2145 |
| P1 | Sonderklassen/Parts/Dachformen fehlen oder werden falsch interpretiert | Gelieferte Semantik statt Wohnhausannahmen, 2173/2338 |
| P1 | Boden/Fels/Fassaden bleiben flach und repetitiv | Maßstäbliche Form, gemeinsame Materialkomposition, 2337/2171 |
| P1 | Licht/Schatten/Reflexion bleiben schwach; viele lokale Lichter fehlen | Aktuelles regionales Licht mit kohärenter Kameraantwort, 2155 |
| P1 | Brücken/Tunnel und komplexe Straßenanschlüsse fehlen | Native Ebenen/Kontakte bei erhaltener Straßenqualität, 2281 |
| P1 | Engine/world kennen Quellsemantik; Erweiterungen umgehen öffentliche Grenzen | Fachplanung in Generatoren, generische native Welt, 2188 |
| P1 | GPU-Passzeiten fehlen; Encoding/Warten ersetzen keine GPU-Messung | Kompakte direkte Kostenmessung, 2339 |
| später | Wälder bleiben POC; Wetter/Wolken und bewegte Sandbox fehlen | Nach Gebäude/Terrain/Straßen: 2111, 2172, 2136 |

Typische Last ist eine bewegte Spielwelt: Kamera, Figuren/Fahrzeuge, Wind, lokale Lichter und
Wolkenschatten. Statische Form/Material dürfen wiederverwendet werden; ihre fertige Beleuchtung
ist dadurch nicht statisch. Die Place-Drehung ist ein Integrationsfall, kein Ersatz für diese Last.

## Priorität und Lieferung
| Reihenfolge | WI | Nächster sichtbarer Nutzen |
|---|---|---|
| 1 Gebäude | 2173, 2336, 2171 | Vollständige Formen/Höhen, Dächer, lesbare Fassaden; Großstadt-LOD |
| 2 Terrain | 2337, 2171 | Plausibles Relief, Fels-/Bodenmaterial statt grüner Kunststoffflächen |
| 3 Infrastruktur | 2281, 2145, 2338 | Straßen/Brücken/Sonderbauwerke und korrekte Ufer/Wassergeometrie |
| 4 Vegetation | 2111 | Vollständige performante Wälder bis zu räumlichem Nahlaub |
| 5 Wolken | 2172 | Wetterhimmel und kohärentes Bodenlicht im zuvor freigemachten Budget |
| Danach Sandbox | 2136 | Kräfte/Kontakte/Gelenke, NPC/JS/LLM, Ton, Persistenz |

2280 (indizierter Asset-Cache/Laden), 2188 (öffentliche Grenzen) und 2155 (Licht) werden mit diesen sichtbaren
Lieferungen integriert. Gemessene Einsparungen schaffen das Wolkenbudget; keine Lockerung
von Profil, Sichtweite, Inhaltsvollständigkeit oder Framebudget.

## Reihenfolge und Zuständigkeit
2280 und 2336 sind aktiv: vollständiger Ladeablauf und einfache Gebäude mit Fernclustern.
Zuerst Wien beim aktuellen Bildstand unter 10 ms pro Frame einschließlich p99 bringen;
2336/2340 besitzen Repräsentation/Arbeitsauswahl. Keine neuen Bildfeatures vor diesem Nachweis.
Dann Gebäude, Terrain, Infrastruktur, Vegetation und schließlich Wolken ausbauen.
Der SpeedTree-Qualitätsmaßstab zieht Vegetation nicht vor.
2188 migriert dabei
nur die benötigten öffentlichen Grenzen. Keine komplette SDK-Neufassung vor dem ersten Bild.
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
Hockenheim-Runden/Physikausbau sind kein erster visueller Meilenstein.

## Forschungsgrundlage
[Engine-weite Recherche](../doc/references/README.md) ordnet SIGGRAPH und verwandte
Primärquellen jedem Feature zu. Empfehlungen sind Integrationsaufträge, keine Abnahme.

## Abnahme
Alle acht vollständigen Bilder mit datierten Referenzen vergleichen; Formen, Materialien,
Licht/Wetter, Stabilität und Kosten getrennt bewerten. Client-API und AGENTS-Gate verwenden.
`src/assets/places`/`client/PlaceCamera` besitzen Szenario/Pose; Webcam-Referenzen bleiben in
`build/shots/reference/webcams`. Kamerakalibrierung ersetzt keine fehlende Geometrie.
Rundum-Abdeckung und Datenherkunft belegen; ein Sichtweitenparameter allein beweist sie nicht.
