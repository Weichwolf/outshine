# Native Places und Szenariovergleiche

Orte, Kameras und Inhalte richten sich nach der Bildfrage. Wald, Parkrand und
Stadtgrün profitieren von Ansichten mit Vegetation. Diagnoseansichten machen
Gelände-, Gebäude- und Verkehrskontakte sichtbar. Für Phase 1 stehen die
[Python-/Blender-Ansichten](../../../../lib/tests/fixtures/vienna/) zur Verfügung.
`references.json` erhält Webcam-Herkunft und Archivvergleiche, auch für archivierte Orte.
Datierte Webcam-Aufnahmen sind die erste Bildreferenz. Vergleiche wiederholte Ansichten
über Tageszeiten, Wetter und Jahreszeiten und gleiche die Kamera vor einem Geometrievergleich ab.
Referenzen bleiben unter build/shots/reference/webcams/, aktuelle Kontrollbilder lokal
unter build/shots/places/. In doc/ und README werden vorerst keine Bilder veröffentlicht.

Jede .scenario-Datei ist ein normales Outshine-Szenario. Der Dateistamm ist der
Testname; Outshine selbst kennt keinen Katalog. Der Testtreiber verlangt eine
geodätische Kamera, feste Uhr und positive Rendergröße.

```sh
make shots PLACE=Wien
make prepare-place PLACE=Wien PREPARE_SECONDS=1800
make bench BENCH_PROFILE=a18 BENCH_ARGS=Wien
python3 -B test/scripts/places.py --catalog test/outshine/data/places Wien Tokyo
```

`shots` und der direkte Treiber sind standardmäßig offline; `prepare-place` lädt
fehlende Quellen online, baut fehlende Assets und nimmt anschließend die Szene auf.
`PLACES_CATALOG` wählt einen anderen Katalog, `OUTSHINE_CACHE_DIR` den Quellcache.
`SHOTS_ARGS` übergibt zusätzliche Treiberparameter, etwa `--check-budget`.
`make bench` misst Stillstand, Drehung, Gehen und Flug offline mit drei Wiederholungen.
Eine vollständige Drehung aktualisiert das Place-Bild. Ohne Ortsfilter nimmt der
Benchmark den gesamten Katalog. `BENCH_ARGS` wählt Orte oder Treiberparameter.
Mit `--online` darf der Vorlauf fehlende Quellen laden. Die drei gemessenen Läufe
bleiben offline. Der Ergebnisbericht enthält den Vorlauf separat.
Pro Ort bleibt das neueste erfolgreiche Hash-PNG unter build/shots/places erhalten.
Private Berichte/Logdateien liegen im System-Tempverzeichnis. CPU/GPU-p99 und
Vollständigkeit stehen im unveränderten nativen Benchmarkbericht; eine erfolgreiche
Aufnahme allein bestätigt das Framebudget nicht.

WI [#5](https://github.com/Weichwolf/outshine/issues/5) beschreibt Bildaufgaben, Auswahl und Kamerakalibrierung. Bestehende
Höhen/Pitch/FOV sind teilweise Schätzungen, keine bestätigte Fotoausrichtung. Flensburg
verwendet veröffentlichte Position/Bearing/Höhe und aus 57 Grad horizontalem Sektor bei
16:9 abgeleiteten vertikalen FOV; Pitch und Höhendatum sind noch offen. Wetter und Zeit
müssen für quantitative Vergleiche zusammenpassen; Archivbilder sind keine Wetterquelle.

Weitere Szenarien stehen über `--catalog test/outshine/data/diagnostic-places` zur
Verfügung. Beurteile Bildqualität, Vollständigkeit, Aufbau und Framezeit auf dem
A18 Pro im gewählten Vergleichsprofil. Quelldownload und Cacheaufbau separat
erfassen.
