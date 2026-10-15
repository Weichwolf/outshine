# Vorbereitete Vegetationseingaben

`species-pilot-1.json` ist ein eigener kompakter Artdaten-Pilot aus WCVP16:
30 akzeptierte Arten, 31 Anfragen, 2.068 Vorkommensangaben; 66.103 Byte.
CC-BY-3.0-Herkunft, Original-IDs, Zustand und Anpassungsbeschreibung sind enthalten.
Es sind keine optisch abgenommenen Pflanzenrezepte. Vertrag und Quellen:
[species_catalog.md](../../../../species_catalog.md), Issue #226.

## Blattoptik

`leaf-optics-table-1.json` ist ein eigenes versioniertes Generatorprodukt aus
der geprüften PROSPECT-D-Referenzrechnung von [PR126](https://github.com/Weichwolf/outshine/pull/126).
Das installierte Wheel enthält die Tabelle und `leaf-optics-provenance-1.json`.
Die Provenanz nennt Parameter, Einheiten, Herkunft, SHA256 und fünf unabhängig
spektral berechnete Prüfpunkte. Blender und Spektralbibliotheken werden beim Laden
nicht benötigt. Die allgemeine Python-Paketbasis benötigt weiterhin ihre normalen Abhängigkeiten.

```python
import json
from importlib.resources import files
from outshine_ref.vegetation import LeafOpticsTable

source = files("outshine_ref.vegetation") / "data" / "leaf-optics-table-1.json"
table = LeafOpticsTable.from_recipe(json.loads(source.read_text()))
rt = table.sample([[45.0, 0.2]])
```

33×33×6 Float64-Werte: erste Achse Chlorophyll, zweite Anthocyanin, jeweils
µg/cm². Beide Achsen sind gleichmäßig in `log1p` verteilt, Grenzen 80 und 10.
Die Kanäle sind `R.r, R.g, R.b, T.r, T.g, T.b` in linearem sRGB.
Die rohen Tabellenwerte werden zuerst bilinear interpoliert; anschließend folgt
die gemeinsame Gamut-/Energiekopplung aus `leaf_optics.gamut_map`.
Einzelne Kanäle vor der Interpolation unabhängig zu klemmen ändert den Vertrag.

Die Rechnung integriert 400–780 nm in 1-nm-Schritten mit CIE1931 2° und D65.
Feste Größen: Strukturparameter N=1,3 (kein Brechungsindex), Carotinoide 10 µg/cm²,
Wasser 0,012 cm, Trockenmasse 0,010 g/cm², Braunpigment 0 und Alpha 40°.
Reflexion und Transmission bleiben getrennt und unbeleuchtet; Blattseiten sind
symmetrisch. Das Produkt ist keine artspezifische oder winkelabhängige Kalibrierung.
Die Prüfpunkte liefern keinen globalen Interpolationsfehlerbeweis.

Native Integration und weitere Energiebudgets besitzt [#134](https://github.com/Weichwolf/outshine/issues/134).
Coverage, Normal-/Rauheitsmomente und komplette Materialfelder bleiben eigene Produkte.
