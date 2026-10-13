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
