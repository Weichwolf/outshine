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

## Priorität und Lieferung
| Reihenfolge | WI | Nächster sichtbarer Nutzen |
|---|---|---|
| 1 Gebäude | 2173, 2336, 2171 | Vollständige Formen/Höhen, Dächer, lesbare Fassaden; Großstadt-LOD |
| 2 Terrain | 2337, 2171 | Plausibles Relief, Fels-/Bodenmaterial statt grüner Kunststoffflächen |
| 3 Infrastruktur | 2281, 2145, 2338 | Straßen/Brücken/Sonderbauwerke und korrekte Ufer/Wassergeometrie |
| 4 Vegetation | 2111 | Vollständige performante Wälder bis zu räumlichem Nahlaub |
| 5 Wolken | 2172 | Wetterhimmel und kohärentes Bodenlicht im zuvor freigemachten Budget |
| Danach Sandbox | 2136 | Kräfte/Kontakte/Gelenke, NPC/JS/LLM, Ton, Persistenz |

2280 (Laden), 2188 (öffentliche Grenzen) und 2155 (Licht) werden mit diesen sichtbaren
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

2280 besitzt Erwerb/Cache und den vollständigen Ladeablauf, 2336 räumlichen Bedarf/LOD,
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
