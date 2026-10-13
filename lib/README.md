# Outshine Python reference libraries

Gemeinsame Arbeit: [#47](https://github.com/Weichwolf/outshine/issues/47).

`test/experiments/` untersucht Verfahren und ihre Qualität/Kosten. Bewährte Verfahren
werden in dieses installierbare Paket übernommen. Der Integrationsagent setzt sie
unter `src/` maschinennah um und prüft ihre native Bildwirkung und Kosten.

```sh
.venv/bin/python -m pip install -e ./lib
.venv/bin/python -m unittest discover -s lib/tests -v
```

Die öffentliche API liegt unter `outshine_ref`. Fachmodule `transport`, `buildings`
und `vegetation` importieren gemeinsame Verträge und Geometrieverfahren.
`pipeline.prepare` verbindet ihre Planungsfunktionen in dieser Reihenfolge.
Kerne importieren weder Experimente noch Renderer, Showcases oder fremde Checkouts.

Alle Produkte verwenden denselben benannten rechtshändigen lokalen Rahmen:
X Ost, Y Nord, Z oben, Meter. Geometrien sind XY; Höhenintervalle beziehen sich auf
denselben lokalen Z-Ursprung. Die Quellaufbereitung bindet Projektion und Datum vor
Aufruf der Bibliothek. Esri-Scopes und pflanzeneigene Achsen werden an Fachgrenzen umgerechnet.

Transport liefert nachvollziehbare `FootprintAdjustment`-Produkte und bindende
`Reservation`-Freiräume. Buildings erhält die angepassten Grundrisse; die unveränderten
Quellen bleiben daneben verfügbar. Neue Teile behalten ihre Quelle und erhalten
eindeutige Teilidentitäten. Höhenkontakte und bloße Randberührung sind keine Volumenüberlappung.
`Hint` enthält freiwillige Gestaltungshinweise; er ersetzt keine bindende Reservierung.
Vegetation erhält Reservierungen und Hinweise beider Vorgänger.

Pläne bleiben kameraunabhängig. Die Bibliotheksbasis enthält noch keine vollständigen
Fachsolver, Rezepte, LOD-Ausgabe oder nativen Exportformate. Planungsfunktionen werden
von den Fachbibliotheken schrittweise übernommen; Detailausgabe folgt gesondert.
`Mesh` besitzt unveränderliche Float64-Positionen und Uint32-Dreiecksindices als
erste Referenzbuffer; Material-, UV-, Normalen- und native Präzisionsverträge folgen
mit konkreten Verfahren. Dies ist noch keine Runtime-Asset-ABI.

Das erste übernommene Verfahren `geometry.conform_triangles` stammt aus dem
Transportexperiment: vorhandene Schnittstellenvertices in Dreieckskanten einsetzen,
ohne Koordinaten oder belegte Fläche zu ändern. Die zugehörigen Gegenfälle prüfen
Mehrfachschnitte, echte Lücken, Transformationen und unveränderte Eingaben.

Die Tests arbeiten lokal. Sie lesen oder verifizieren keine GitHub Issues.

Die Vegetationsbibliothek exportiert `vegetation.Needle`: geschlossene, spitze
Organflächen mit elliptischen, rhombischen oder Halbkreis-/Kreissektorquerschnitten,
kontrollierter Biegung/Twist und analytischen Normalen. Gemeinsame Nah-/Grobpunkte
bleiben erhalten. Pflanzenlokaler Rahmen: X quer, Y längs, Z normal, Meter; dieser
Rahmen wird erst bei Weltplatzierung in ENU umgerechnet. `mesh()` liefert eigene
Float64-Positionen, UVs, Parameter und Ecknormalen sowie Int32-Dreiecke als Dictionary.
Harte Kanten und Deckel behalten getrennte Ecknormalen. Unteraufgelöste, falsch
orientierte Facetten werden abgelehnt; feste Unterteilungen beweisen keine LOD-Deckung.
Material, Wind, vollständige Pflanzen und Weltplatzierung bleiben Experimentarbeit.

```python
from outshine_ref.vegetation import Needle

organ = Needle(section="sector", sector_count=5, aspect=1.0)
near = organ.mesh(32, 16)
coarse = organ.mesh(16, 8)
```

Quellen, Gegenfälle und Bild-/Kostenbelege: [#45](https://github.com/Weichwolf/outshine/issues/45),
[#46](https://github.com/Weichwolf/outshine/issues/46), Übernahme [#52](https://github.com/Weichwolf/outshine/issues/52).

`buildings.modelling` enthält den übernommenen Scope-Kern: `Frame`, `Scope`,
`Relative`, `t`, `r`, `s` und `translate`. Er hält die dokumentierten CGA-Achsen
Ost/Auf/Süd intern; `cga_to_enu`/`enu_to_cga` konvertieren Positionen und Normalen
an der gemeinsamen ENU-Grenze, ohne Dreieckswinding oder Höhenbezug zu ändern.
Alle Gebäudeexperimente verwenden diese Implementierung. Shape-/Geometrieoperationen,
Dächer und konstruktive Gebäudepläne liegen weiterhin in den Experimenten (#51).
Die Scope-Übernahme allein ist keine vollständige CityEngine-Bibliothek.
