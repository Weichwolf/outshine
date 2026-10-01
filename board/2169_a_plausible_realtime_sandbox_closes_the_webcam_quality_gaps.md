Type: feature
State: active
Architecture: ready
Priority: P0
Parent:
Depends:
Area: client, world, generators, render, gameplay
Tags: vision, webcam, sandbox

# Outshine becomes a believable data-driven open-world sandbox

## Ergebnis und erster Meilenstein
Die Welt wirkt vollständig, zusammenhängend und glaubwürdig. Zuerst nähere ich acht
Places an reale Webcams über Tages-/Jahreszeiten und Wetter an. Danach wachsen daraus
physikalisch konsistente bewegliche Systeme, LLM-NPCs, JavaScript, HTML/CSS, Interaktion
und Audio. GTA5/RDR2 prägen die Optik; technische Grenzen bestimmen den erreichbaren Look. Regeln/Budgets stehen in
AGENTS.md; dieses Board beschreibt den Weg. RDR2/GTA5 sind Qualitätsmaßstäbe.

| Place | foto-webcam.eu/webcam/ | Entscheidende Bildwirkung |
|---|---|---|
| Rosenheim | rosenheim | Dächer, Sonderbauten, gestaffelte Alpen, Nachtlicht |
| Flensburg | flensburg | Backstein, Kirchturm, Hafen und korrekte Küste |
| DarmstadtWest | darmstadt-west | Dachlandschaft, Fassaden und diffuse Stadttiefe |
| Wien | wien | Vollständige dichte Stadt, Donau, Brücken, Fernverbände |
| Husum | husum-hafenklappbruecke | Kai, Giebel, Brücke, Hafenwasser und Nässe |
| Feldkirch | feldkirch | Altstadt, Fluss, Tal und saubere Hanganschlüsse |
| Malcesine | malcesine | See, Felsufer, Spiegelung und Dunst |
| Koerbersee | koerbersee | Grate, Schnee/Schmelze, Bergsee und Wolken im Relief |

## Arbeitsweg nach Priorität
| Phase | Lieferung | Kinder | Zusammenhang |
|---|---|---|---|
| P0 jetzt | Gemeinsames Engine-Design und vollständige Originalwelt | 2188, 2280 | Verträge, paralleler Erwerb, begrenzte Ingestion und Residency gemeinsam liefern |
| P0 | Ferndarstellung ohne Löcher oder Datenmengenexplosion | 2336 | Native Detailhierarchie begrenzt Arbeit vor Geometrie |
| P0 | Richtige Straßen, Bauwerke, Gelände und Gewässer | 2281, 2173, 2145 | Vorhandene Formen erhalten, reale Geometriefehler beseitigen |
| P1 | Lesbare Materialien, kohärentes Licht, Wetter und Himmel | 2171, 2155, 2172 | Mit vorhandenen Oberflächen liefern; kein Warten auf Vegetation |
| P2 zuletzt | Vegetation vom Fernwald bis zum Grashalm | 2111 | Standortdaten und gemeinsame Detail-/Residency-Verträge |
| P3 | Physikalische, programmierbare und bevölkerte Sandbox | 2136 | Gemeinsame Physik/Commands für JS, UI, LLM-NPCs und bewegliche Systeme |

## Arbeitsfähige Reserve
2188 und 2280 sind aktiv: grundlegendes Design und die aktuelle Ladepipeline. Danach den
Projektions-/Fehlervertrag in native Detailplanung überführen; native Formen in
2173/2281 und Baustoffe 2171 können ihre vorhandenen Inputs unabhängig verwenden.
2336s globale Grobquelle und 2145s Küstenabschluss brauchen noch belegte Quellenverträge.
2136 besitzt den Ausbau der gemeinsamen Physik und asynchronen NPC-Entscheidungen. Reihenfolge ist
Priorität; `Depends` nennt nur den fehlenden konsumierten Vertrag, keine pauschale Gesamtabnahme.

## Abnahme und Zuständigkeit
`src/assets/places` und `client/PlaceCamera` besitzen den Katalog und die Aufnahme.
Kamera, FOV, Pose, Höhendatum, Zeit und Quellenfit getrennt von Weltfehlern prüfen;
unbekannte Kalibrierung bleibt benannt. Referenzmanifest und vorhandene Archivbilder
unter build/shots/reference/webcams bleiben erhalten. Fotos liefern keine Weltgeometrie.
Alle acht Bilder persönlich vergleichen; Form, Material, Licht/Wetter und Kosten getrennt
bewerten. Der nächste Schritt muss Bildgewinn oder einen konkreten Bildblocker liefern.
Historische Places bleiben ausdrücklich aufrufbare Diagnosen, kein erweitertes Pflichtgate.
Hockenheim-Runden und Physik-Ausbau warten auf Phase P3; vorhandene Fähigkeiten erhalten.
Der spätere Maßstab reicht nahtlos vom Planeten über Flug bis zur unmittelbaren Umgebung.
Kein Kind kann durch Tests allein fehlende Runtime-Inhalte oder Bilder für fertig erklären.
