# Webcam-Places und Infrastruktur-/Dichtebenchmarks

Der Standardkatalog enthält Rosenheim, Flensburg, DarmstadtWest, Wien, Husum, Feldkirch,
Malcesine und Koerbersee. `references.json` verknüpft Kamera, Archivaufnahme und Herkunft.
Bilder bleiben unter build/shots/reference/webcams/. Sie sind ausschließlich Vergleichsmittel.
Tokyo und Central Park prüfen dichte Städte. BaselBadischerBahnhof ergänzt Gleisfeld,
Straßen/Tunnel auf mehreren Ebenen. ZuerichHauptbahnhof ergänzt Bahn, Straßen und Fluss.
Beide Diagnosekameras sind frei gewählt, keine Webcam-Posen.

Jede .scenario-Datei ist ein normales Outshine-Szenario. Der Dateistamm ist der Name.
`outshine-client --places <Verzeichnis> places|shots|prepare|roundtrip` lädt den Katalog;
Standard ist src/assets/places. Neue Szenarien benötigen keinen Renderer-Sonderpfad.
Kataloge verlangen eine geodätische Kamera, feste Uhr und positive Rendergröße.

WI 2169 beschreibt Bildaufgaben, Auswahl und Kamerakalibrierung. Bestehende
Höhen/Pitch/FOV sind teilweise Schätzungen, keine bestätigte Fotoausrichtung. Flensburg
verwendet veröffentlichte Position/Bearing/Höhe und aus 57 Grad horizontalem Sektor bei
16:9 abgeleiteten vertikalen FOV; Pitch und Höhendatum sind noch offen. Wetter und Zeit
müssen für quantitative Vergleiche zusammenpassen; Archivbilder sind keine Wetterquelle.

Die übrigen erhaltenen Szenarien liegen unter src/assets/diagnostic-places und sind mit
--places explizit aufrufbar. Ihre bisherigen roten Befunde bleiben offen. Standardabnahme:
acht vollständige Welten ab vorbereitetem Quellcache, schnellstmöglicher Aufbau mit begründeten
Ladezielen je Szene, 60 Frames/360 Grad in <=1 s, p99 <=1000/60 ms. Internet-Erwerb separat messen.
`make prepare-place PLACE=Wien PREPARE_SECONDS=1800` bereitet Quellen vor;
`build/outshine-client shots --offline Wien` misst danach im frischen Prozess ohne Netzwerk.
Fehlende Cachebytes sind eine unerfüllte Voraussetzung, kein Beleg für zu langsamen Warmaufbau.
