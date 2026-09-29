# Acht Webcam-Places

Der Standardkatalog enthält Rosenheim, Flensburg, DarmstadtWest, Wien, Husum, Feldkirch,
Malcesine und Koerbersee. `references.json` verknüpft Kamera, Archivaufnahme und Herkunft.
Bilder bleiben unter build/shots/reference/webcams/. Sie sind ausschließlich Vergleichsmittel.

Jede .scenario-Datei ist ein normales Outshine-Szenario. Der Dateistamm ist der Name.
`outshine-client --places <Verzeichnis> places|shots|prepare|roundtrip` lädt den Katalog;
Standard ist src/assets/places. Neue Szenarien benötigen keinen Renderer-Sonderpfad.
Kataloge verlangen eine geodätische Kamera, feste Uhr und positive Rendergröße.

WI 2324 beschreibt Bildaufgaben und Auswahl; WI 2170 kalibriert Kameras. Bestehende
Höhen/Pitch/FOV sind teilweise Schätzungen, keine bestätigte Fotoausrichtung. Flensburg
verwendet veröffentlichte Position/Bearing/Höhe und aus 57 Grad horizontalem Sektor bei
16:9 abgeleiteten vertikalen FOV; Pitch und Höhendatum sind noch offen. Wetter und Zeit
müssen für quantitative Vergleiche zusammenpassen; Archivbilder sind keine Wetterquelle.

Die übrigen erhaltenen Szenarien liegen unter src/assets/diagnostic-places und sind mit
--places explizit aufrufbar. Ihre bisherigen roten Befunde bleiben offen. Standardabnahme:
acht vollständige Welten, <=10 s Preload, 60 Frames/360 Grad in <=1 s, p99 <=1000/60 ms.
