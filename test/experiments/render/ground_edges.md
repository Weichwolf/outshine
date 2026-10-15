# Metrische Bodenkontakte — #173

Ziel: natürliche Materialkanten mit vorhandenen Rezeptbreiten und gemeinsamem Metallic-Roughness.
Die nativen Klassen bleiben funktional; neue Bilddaten dürfen keine Quellen verdrängen.

## Verfahren

`outshine_ref.materials.filtered_edge_weight` integriert die kubische Maske über einen
1D-Box-Footprint entlang der Randnormalen. Innenintervalle verwenden den exakten kubischen
Mittelwert. Gespiegelte Randfälle vermeiden die Subtraktion fast gleicher Stammfunktionen.
Float32, komplementäre Gewichte; keine exakte Integration gekrümmter/zweidimensionaler Pixel.

`MaterialRegion/coalesce_regions` vereinigt gleiche Material-/Ranggruppen mittels GEOS nur,
wenn der Rang in der Eingabe ein einziges Material bezeichnet. Mehrdeutige Ränge bleiben
unverändert. Lokaler Rahmen und Quell-IDs bleiben erhalten; vollständige Quellen bleiben separat.

## Reale Eingaben und Gegenfälle

SHA-gepinnte z14-Landcover-/Landuse-Polygone aus dem Inventar 20261009, aus vorhandenem
Client-Quellcache kopiert. Kein Netzwerkerwerb. Tokyo-Pin fehlt hier im lokalen Cache;
das sagt nichts über den separat vollständigen nativen Tokyo-Place aus.
Unbekannte Quellklassen bleiben dokumentiert. Für die Bündelung gelten die vorhandenen nativen
Vorlagen/Ränge einschließlich `sites`→`land`-Fallback; keine zusätzliche Nutzungsheuristik.

| Kachel | Nutzbare Quellen → Regionen | Mitgliedschaftsproben | Grob: Kandidaten/Zelle | Grob: Referenzen/Zelle |
|---|---:|---:|---:|---:|
| Central Park | 277 → 6 | 2108 | 3 | 347 |
| Feldkirch | 134 → 10 | 1128 | 6 | 98 |
| Wien | 757 → 10 | 1656 | 6 | 195 |

Feinraster 16 m, Grobstress 64 m mit denselben detaillierten z14-Quellen. Der Stress ist keine
Messung des tatsächlichen z11-Providers. Metrischer Bedarf: 1,5 m Rezeptmaximum plus halbe
Rasterzelle als deklarierter Footprintfall, also 9,5/33,5 m. Alle geprüften Halo-Randabfragen
stimmen gegen vollständige GEOS-Grenzen; die Stichprobe ist kein Welt-/GPU-Nachweis.

Ohne Bündelung erreicht Wien im Grobstress 40 Quellen/Zelle. Nur die 1,5-m-Bandbreite ohne
Pixel-Footprint reicht ebenfalls nicht: Randhinweise fehlen außerhalb dieses kleinen Halos.
Bündelung vermeidet mehr Quellen-Slots, Central Park benötigt jedoch 347 Referenzen in einem
Seed. Das passt nicht in das alte 8-Bit-Referenzfeld.

Native Entscheidung: expliziter 16-Byte-Seed mit vollständigem Referenzzähler statt dichter
8-Bit-Kodierung. 32 Quellen/Zelle bleiben; Arbeitslimit pro Zelle bleibt aus dem bisherigen
Maximum 32 × 255 = 8160 Referenzen abgeleitet. Größen/Offsets an CPU/GPU verbindlich prüfen.
Native Quantisierung, Winding, Zell-/Kachelgrenzen, Ränge, Ressourcen/Cache und echte Bildkosten
sind vor Freigabe zu verifizieren. Ein gültiger Python-Verbund allein schließt #173 nicht.

## Wiederholen

Python ≥3.11; Bibliotheksabhängigkeiten sowie `mapbox-vector-tile==2.2.0` und `matplotlib==3.11.2`.

```sh
PYTHONPATH=lib/src python -W error -m unittest discover -s lib/tests -p 'test_material_*.py' -v
PYTHONPATH=lib/src python test/experiments/render/ground_edges.py --prepare-from-cache "$HOME/Library/Application Support/outshine/outshine/sources"
PYTHONPATH=lib/src python test/experiments/render/ground_edges.py --cell-m 64 --output build/experiments/render/ground-edges/coarse
```

Ergebnisse/Inputs liegen unter `build/experiments/render/ground-edges/`; der einzelne
Kontrollplot zeigt die Maske, kein natives Place-Rendering. Primärquellen und native Abnahme in #173.
