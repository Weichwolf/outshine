# Webcam-Places und Transport-/Dichtebenchmarks

Zwölf wechselnde Ansichten dienen der visuellen Beurteilung. Orte, Kameras und Inhalte
folgen dem aktuellen Bedarf; sie sind keine feste Vorgabe oder weltweite Abnahme.
Transport, Gebäude und Bodenmaterialien bilden zuerst die Qualitätsbasis.

| Aktueller Schwerpunkt | Ansichten | Vegetation |
|---|---|---|
| Natur: Gebirge und Boden | Jura, Koerbersee | aus |
| Gemischt: Stadt/Natur, Ufer, Parkrand | CentralPark, Feldkirch, Malcesine | aus |
| Infrastruktur und Boden ohne Baumdeckung | Wien, Tokyo, Rosenheim, Flensburg, Husum, BaselBadischerBahnhof, ZuerichHauptbahnhof | aus |

Dies ist die aktuelle Arbeitsauswahl, keine repräsentative Stichprobe der Erde.
Geografisch andere Gegenbeispiele ergänzen und ersetzen sie bei neuen Verfahren.
Alle zwölf Szenarien bleiben vorerst ohne Vegetation. So sind Transport, Gebäude,
Bodenmaterialien und ihre Kontakte frei sichtbar. Das ist ein Diagnoseprofil;
die vollständige Welt einschließlich Vegetation bleibt das Ausbau- und Budgetziel.
`references.json` erhält Webcam-Herkunft und Archivvergleiche, auch für archivierte Orte.
Referenzen bleiben unter build/shots/reference/webcams/, aktuelle Kontrollbilder lokal
unter build/shots/places/. In doc/ und README werden vorerst keine Bilder veröffentlicht.

Jede .scenario-Datei ist ein normales Outshine-Szenario. Der Dateistamm ist der
Testname; Outshine selbst kennt keinen Katalog. Der Testtreiber verlangt eine
geodätische Kamera, feste Uhr und positive Rendergröße.

```sh
make shots PLACE=Wien
make prepare-place PLACE=Wien PREPARE_SECONDS=1800
python3 -B test/scripts/places.py --catalog test/outshine/data/places Wien Tokyo
```

`shots` und der direkte Treiber sind standardmäßig offline; `prepare-place` lädt
fehlende Quellen online, baut fehlende Assets und nimmt anschließend die Szene auf.
`PLACES_CATALOG` wählt einen anderen Katalog, `OUTSHINE_CACHE_DIR` den Quellcache.
`SHOTS_ARGS` übergibt zusätzliche Treiberparameter, etwa `--check-budget`.
Pro Ort bleibt das neueste erfolgreiche Hash-PNG unter build/shots/places erhalten.
Private Berichte/Logdateien liegen im System-Tempverzeichnis. CPU/GPU-p99 und
Vollständigkeit stehen im unveränderten nativen Benchmarkbericht; eine erfolgreiche
Aufnahme allein bestätigt das Framebudget nicht.

WI [#5](https://github.com/Weichwolf/outshine/issues/5) beschreibt Bildaufgaben, Auswahl und Kamerakalibrierung. Bestehende
Höhen/Pitch/FOV sind teilweise Schätzungen, keine bestätigte Fotoausrichtung. Flensburg
verwendet veröffentlichte Position/Bearing/Höhe und aus 57 Grad horizontalem Sektor bei
16:9 abgeleiteten vertikalen FOV; Pitch und Höhendatum sind noch offen. Wetter und Zeit
müssen für quantitative Vergleiche zusammenpassen; Archivbilder sind keine Wetterquelle.

Die übrigen erhaltenen Szenarien liegen unter test/outshine/data/diagnostic-places;
der Treiber wählt sie mit `--catalog test/outshine/data/diagnostic-places`. Ihre bisherigen roten Befunde bleiben offen. Geräteabnahme:
vollständige vorbereitete Welten, schnellstmöglicher Aufbau mit begründeten Ladezielen je Szene,
genau Ziel-fps Frames/360 Grad in einer Sekunde, p99 <=1000/Ziel-fps ms. A18-Pro-Ziel:
1280×720/60 einschließlich p99. Internet-Erwerb separat messen.
Fehlende Cachebytes sind eine unerfüllte Voraussetzung, kein Beleg für zu langsamen Warmaufbau.
