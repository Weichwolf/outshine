# Webcam-Places und Transport-/Dichtebenchmarks

Zwölf wechselnde Ansichten dienen der visuellen Beurteilung. Orte, Kameras und Inhalte
folgen dem aktuellen Bedarf; sie sind keine feste Vorgabe oder weltweite Abnahme.
Transport, Gebäude und Bodenmaterialien bilden zuerst die Qualitätsbasis.

| Aktueller Schwerpunkt | Ansichten | Vegetation |
|---|---|---|
| Natur: Wald, Gebirge, Boden | Jura, Koerbersee | an |
| Gemischt: Stadt/Natur, Ufer, Parkrand | CentralPark, Feldkirch, Malcesine | an |
| Infrastruktur und Boden ohne Baumdeckung | Wien, Tokyo, Rosenheim, Flensburg, Husum, BaselBadischerBahnhof, ZuerichHauptbahnhof | aus |

Dies ist die aktuelle Arbeitsauswahl, keine repräsentative Stichprobe der Erde.
Geografisch andere Gegenbeispiele ergänzen und ersetzen sie bei neuen Verfahren.
Die Naturansichten prüfen den vorhandenen Vegetationsstand, ohne den Schwerpunkt
von Transport, Gebäuden und Bodenmaterialien zu verschieben.
`references.json` erhält Webcam-Herkunft und Archivvergleiche, auch für archivierte Orte.
Referenzen bleiben unter build/shots/reference/webcams/, aktuelle Kontrollbilder lokal
unter build/shots/places/. In doc/ und README werden vorerst keine Bilder veröffentlicht.

Jede .scenario-Datei ist ein normales Outshine-Szenario. Der Dateistamm ist der Name.
`outshine-client --places <Verzeichnis> places|shots|prepare|roundtrip` lädt den Katalog;
Standard ist src/assets/places. Neue Szenarien benötigen keinen Renderer-Sonderpfad.
Kataloge verlangen eine geodätische Kamera, feste Uhr und positive Rendergröße.

WI [#5](https://github.com/Weichwolf/outshine/issues/5) beschreibt Bildaufgaben, Auswahl und Kamerakalibrierung. Bestehende
Höhen/Pitch/FOV sind teilweise Schätzungen, keine bestätigte Fotoausrichtung. Flensburg
verwendet veröffentlichte Position/Bearing/Höhe und aus 57 Grad horizontalem Sektor bei
16:9 abgeleiteten vertikalen FOV; Pitch und Höhendatum sind noch offen. Wetter und Zeit
müssen für quantitative Vergleiche zusammenpassen; Archivbilder sind keine Wetterquelle.

Die übrigen erhaltenen Szenarien liegen unter src/assets/diagnostic-places und sind mit
--places explizit aufrufbar. Ihre bisherigen roten Befunde bleiben offen. Geräteabnahme:
vollständige vorbereitete Welten, schnellstmöglicher Aufbau mit begründeten Ladezielen je Szene,
genau Ziel-fps Frames/360 Grad in einer Sekunde, p99 <=1000/Ziel-fps ms. A18-Pro-Ziel:
1280×720/30; 1280×720/60 ist ein getrenntes Vergleichsprofil. Internet-Erwerb separat messen.
`make prepare-place PLACE=Wien PREPARE_SECONDS=1800` bereitet Quellen vor;
`build/outshine-client shots --offline Wien` misst danach im frischen Prozess ohne Netzwerk.
Fehlende Cachebytes sind eine unerfüllte Voraussetzung, kein Beleg für zu langsamen Warmaufbau.
