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

`buildings.modelling` enthält den übernommenen Scope-Kern: `Frame`, `Scope`,
`Relative`, `t`, `r`, `s` und `translate`. Er hält die dokumentierten CGA-Achsen
Ost/Auf/Süd intern; `cga_to_enu`/`enu_to_cga` konvertieren Positionen und Normalen
an der gemeinsamen ENU-Grenze, ohne Dreieckswinding oder Höhenbezug zu ändern.
Alle Gebäudeexperimente verwenden diese Implementierung. Der verbundene Modellierkern
enthält außerdem `Shape`, `Geometry`, `Face`, `Material`, `Asset`, `Texture`,
`Branch`, `Part` und `Floating`: Komponenten, Extrusion, Unterteilung, Offset,
Pult-/Firstdächer, UV-/Materialbearbeitung, Projektionen und statische Asset-Platzierung.
`Shape.rotate_scope` richtet nur den Scope neu aus und erhält die sichtbare Geometrie.
Die vorhandenen Gegenfälle/Referenzen bleiben unter `test/experiments/buildings/`.
Konstruktive Gebäudepläne und Rezepte liegen weiterhin dort (#51); viele dokumentierte
CityEngine-Fähigkeiten und Teilverträge sind noch offen (#34).

```sh
.venv/bin/python -m pip install -e './lib[assets]'
```

`assets` installiert Trimesh/Pygltflib für den optionalen OBJ/glTF/GLB-Import.
Der Grundkern benötigt NumPy, Shapely und Pillow. Importiert er nur eigene Shapes,
lädt er die Asset-Decoder nicht. Intern bleiben CGA-Achsen ausdrücklich benannt;
`Shape.from_polygon` übernimmt einen ENU-XY-Grundriss und erzeugt ihn in CGA-XZ
mit der angegebenen lokalen Höhe. Gemeinsame Ausgabe an Transport/Vegetation/world
wird mit `cga_to_enu` für Positionen/Normalen konvertiert. Quellen/IDs sind der
Auftrag des übergeordneten Gebäudeplans; ein Shape allein ist kein Gebäude-Assetvertrag.
