Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, world, engine
Tags: roads, bridges, tunnels, topology

# Roads and structures form a continuous usable transport network

## Ergebnis und Ist
Straßen/Wege/Gehwege, Bahn/Tram und Brücken/Tunnel mit richtigen Ebenen und Anschlüssen.
Alignment, Corridors, RoadMesher/RoadSurfaceBuilder und Terrain-Deformation bestehen und
bleiben erhalten. MVT-Mittelachsen/Ebenen müssen normalisiert, komplexe Anschlüsse ergänzt werden.

## Besitzer und nächste Lieferung
OSM-Adapter besitzt Quellsemantik, world/navigation das logische Netz; generators/road
Profile/Geometrie, Terrain die Kontaktdeformation. Vorhandene Linien/Produkte reichen für
den nächsten Schritt. Erst einen realen Stadt-/Hafenanschluss samt Brücke bis zum Bild liefern;
keine vollständige Quellenmigration oder Hockenheim-Runde als Vorbedingung.
Native `EarthworkKind::Clearance`-Aufträge schneiden nur Terrain oberhalb ihrer Grenze;
sie füllen nie Boden und sperren keine tiefere Becken-Vertiefung. `Corridors::AppendTerrainStamps`
und `AppendJunctionTerrainStamp` erzeugen sie für Brückenspannen und erhöhte Knoten;
`EarthworkPress::BidsTerrain` trennt sie von tatsächlichen Kontakten. Widerlager und
Anschlussrampen behalten ihre Kontakte. Husums künstlicher Bodenriegel unter der Brücke
ist im Runtime-Bild beseitigt; breite Uferböschungen bleiben bei 2145 offen.
Nächster Schritt: Wasserfreiraum bereits beim Entwurf bestimmen, bevor Endhöhen, Rampen
und Knoten entstehen. `MarksWaterCrossing` wird derzeit erst beim Pflastern aufgerufen;
dadurch können Knoten auf Bodenhöhe bleiben, obwohl ihr Überbau danach angehoben wird.
Überbau, Auflager und Geländer anschließend aus demselben durchgehenden Profil erzeugen.

## Verfahren
- Gelieferte Klasse, Breite/Spuren, Oberfläche, Brücke/Tunnel/Ebene normalisieren. MVT ist
  kein vollständiger Routinggraph: Randsegmente/IDs vereinigen, keine Kreuzung aus bloßer Nähe.
  Fehlende Topologie ausdrücklich behandeln, belegte Ebenen erhalten.
- Ein Alignment je zusammenhängender Kette; Schultern, Bord/Gehweg und Knoten davon ableiten.
  Segmentwellen/Nähte an der Ursache beheben, keine zweite Straßenpipeline hinzufügen.
- Brücken mit Überbau/Pfeilern/Widerlagern und Anschlussprofil; Tunnel mit Portal/Freiraum.
  Nur tatsächliche Kontakte stempeln Boden. Gemeinsamer Bezug zu Gebäuden und Wasser aus 2145.
- Markierungen, Geländer, Signale und Beleuchtung aus Klassenparametern; Wiederholungen
  instanzieren. 2336 begrenzt Nahgeometrie, das logische Netz bleibt erhalten. 2171 beleuchtet
  metrische Straßenbaustoffe; Quellarchive sind kein dauerhafter Bestandteil des Netzes.

## Abnahme
Durchgehende reale Straße samt korrekt angeschlossener Brücke ohne Gelände-/Wasserwände,
Lücken oder verlorene Ebenen. Bestehende Straßenqualität erhalten oder verbessern;
Netzfunktion und Bild separat prüfen, gemessene Arbeit/Bytes innerhalb AGENTS-Budget.
