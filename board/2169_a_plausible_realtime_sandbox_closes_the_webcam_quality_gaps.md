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

## Priorität und Lieferung
| Rang | WI | Nächster sichtbarer Nutzen |
|---|---|---|
| P0 zuerst | 2280 | Wien erreicht vollständig den Screenshot; kein endloses Warten |
| P0 integriert | 2336 | Detailbedarf vor Erzeugung; Fernstadt ohne Daten-/Geometrieexplosion |
| P0 gezielt | 2188 | Derselbe funktionierende Weltpfad für Builtins und externe Generatoren |
| P0 Geometrie | 2145, 2281, 2173 | Richtige Pegel/Kontakte, durchgehende Straßen/Brücken, vollständige Gebäude |
| P1 Bildgewinn | 2155, 2171 | Kohärentes Licht und lesbare Materialien auf vorhandener Geometrie |
| P1 Bildgewinn | 2172, 2337, 2338 | Wolken/Wetter, Felsrelief, technische Objekte und Belegung |
| P1 aktiv | 2111 | Vollständige performante Wälder, standortgerechte Baumformen und Nahlaub |
| P3 Sandbox | 2136 | Kräfte/Kontakte/Gelenke, NPC/JS/LLM, Ton, Persistenz |

## Reihenfolge und Zuständigkeit
2280, 2336 und 2111 sind aktiv: vollständiger Ladeablauf, einfache Gebäude mit Fernclustern
und integrierte Waldvegetation. Der Vegetations-POC wird zum vollständigen Waldpfad ausgebaut.
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
Central Park/Tokyo bleiben dichte Ladebenchmarks; historische Places bleiben Diagnosen.
Hockenheim-Runden/Physikausbau sind kein erster visueller Meilenstein.

## Abnahme
Alle acht vollständigen Bilder mit datierten Referenzen vergleichen; Formen, Materialien,
Licht/Wetter, Stabilität und Kosten getrennt bewerten. Client-API und AGENTS-Gate verwenden.
`src/assets/places`/`client/PlaceCamera` besitzen Szenario/Pose; Webcam-Referenzen bleiben in
`build/shots/reference/webcams`. Kamerakalibrierung ersetzt keine fehlende Geometrie.
Rundum-Abdeckung und Datenherkunft belegen; ein Sichtweitenparameter allein beweist sie nicht.
