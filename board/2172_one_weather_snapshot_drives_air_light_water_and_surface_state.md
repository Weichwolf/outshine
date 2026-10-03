Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends: 2188, 2280
Area: generators, render, world
Tags: weather, clouds, astronomy, seasons

# Weather and astronomy drive a coherent sky and world

## Ergebnis und Ist
Atmosphäre/Wolken, Sonne/Mond/Planeten/Sterne und Wind/Nässe/Schnee passend zu Ort/UTC.
SkyStage/Atmosphären-LUTs und Wetterdeklarationen bestehen; Open-Meteo-Anschluss,
Wolkenrenderer und gemeinsamer Materialzustand fehlen. Wolken sind bei 1/3–2/3 Himmel
im Bild eine zentrale Lieferung, kein Restbudget-Effekt.

## Besitzer und fehlende Verträge
2188 liefert öffentlichen WeatherSnapshot mit Ort/Höhe/UTC/Einheiten/Gültigkeit/Herkunft;
2280 normalisierten Live-/Archivinput. Internes world/weather/WeatherProvider ohne diese
Metadaten ersetzen. Wettererweiterung besitzt Provider/Adapter, Generatoren Dichte/
Zustand, Render Integration/History. Welt hält native Umweltprodukte, keine HTTP-Abfragen.
Zuerst Snapshot → begrenzte Wolkenschicht → kohärentes Bodenlicht bis zum Place liefern.

## Verfahren und Invarianten
- Astronomie aus UTC/Beobachterposition: Richtung/Phase/scheinbare Helligkeit/Verdeckung
  von Sonne/Mond und sichtbaren Planeten, einschließlich Venus/Merkur/Mars/Jupiter/Saturn
  und situationsabhängig Uranus/Neptun. Fester lizenzierter Sternenkatalog, Extinktion;
  keine zusätzliche Live-Quelle oder wiederholte Ephemeridenarbeit je Pixel.
- Weltverankerte Wolkendichte/Windadvektion, begrenzter Raymarch in reduzierter Auflösung
  mit temporaler Rekonstruktion. Quellenwechsel/Disocclusion verwerfen History.
  Radiance/Transmittanz einmal komponieren; dieselbe Dichte liefert Schatten/Himmelsfüllung.
- Bedeckte Stadt: diffuse Füllung/weiche Schatten. Klarer Berg: räumliche Luftschichten;
  Nebel folgt Höhe/Wetter und Tiefe statt globalem Bildfilter. Wolkenform bleibt plausibel.
- Regen/Nässe, Schnee/Eis/Schmelze und Phänologie aus Historie/Exposition/Untergrund,
  nicht nur Datum oder weißer Höhenmaske. 2171/2145/2111 konsumieren denselben Zustand;
  Wetterwechsel baut keine unveränderten Gebäude neu. Himmel/Wolken teilen Budget.
- Fehlende/alte Providerwerte bleiben explizit; Einheiten/Gültigkeit erhalten. Keine
  Foto-Wolkenrekonstruktion, verborgene Ersatzquelle oder unbelegte Wettergenauigkeit.

## Abnahme
Bedeckt/Dunst/Nacht und Winter/Schmelze verbessern datierte Places. Wolken beleuchten
Boden kohärent, Himmelobjekte besitzen plausible Positionen; Bewegung bleibt stabil.
