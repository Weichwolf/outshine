Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends: 2188
Area: generators, render, world
Tags: weather, clouds, astronomy, seasons

# Weather, clouds and astronomy form one coherent environment

## Ergebnis und Ist
Himmel/Wolken, Sonne/Mond/Planeten/Sterne und Wind/Nässe/Schnee passend zu Ort/UTC.
SkyStage/Atmosphären-LUTs und Wetterdeklarationen bestehen; Live-Anschluss, Wolkenrenderer
und gemeinsamer Oberflächenzustand fehlen. Himmel mit 1/3–2/3 Bildanteil hat hohe Priorität.

## Besitzer und fehlender Vertrag
Depends 2188 bezeichnet ausschließlich den öffentlichen WeatherSnapshot: Ort/Höhe/UTC,
Einheiten, bekannte/fehlende Felder und Herkunft. Keine vollständige SDK-Migration nötig.
Wettererweiterung besitzt Open-Meteo/JSON-Normalisierung, Renderer Atmosphäre/Wolken,
Generatoren Umweltzustand. Cache/HTTP aus 2280 wiederverwenden, keine neue IO-Pipeline.
Zuerst Snapshot → begrenzte Wolkenschicht → kohärentes Bodenlicht im Place liefern;
Wolkenerprobung kann mit vorhandenen deklarierten Wetterwerten beginnen.

## Verfahren
- Sonne/Mond/Planeten aus UTC und Beobachterposition, inklusive Phasen, scheinbarer Größe/
  Helligkeit und Verdeckung. Venus/Merkur/Mars/Jupiter/Saturn sowie sichtbare Uranus/Neptun;
  fester lizenzierter Sternenkatalog mit Extinktion. Ephemeriden nicht je Pixel berechnen.
- Weltverankerte Wolkendichte mit Windadvektion, begrenzter Raymarch bei reduzierter Auflösung
  und temporaler Rekonstruktion. Dieselbe Dichte liefert Schatten/Himmelsfüllung.
  Radiance/Transmittanz einmal komponieren, ungültige History bei Änderungen verwerfen.
- Diffuse Füllung/weiche Schatten bei Bedeckung, höhen-/tiefenabhängiger Dunst/Nebel.
  Keine Foto-Wolkenrekonstruktion; unbekannte Wolkenstruktur bleibt prozedurale Annahme.
- Gemeinsamer Wind und Zustandsverlauf für Nässe/Regen, Schnee/Eis/Schmelze und Phänologie:
  Historie, Exposition und Untergrund statt bloßer Datums-/Höhenmaske. 2171/2145/2111
  konsumieren diesen Zustand. Wetterwechsel erzeugt keine unveränderte Weltgeometrie.
- Fehlende/alte Wetterwerte benennen, keine versteckte Ersatzquelle. Himmel/Wolken teilen
  das Bildbudget mit der Welt, statt allein nach ihrer Fläche eine feste Quote zu beanspruchen.

## Abnahme
Datierte Bilder für Bedeckung/Dunst/Nacht und Winter/Schmelze gewinnen Plausibilität;
Wolken und Bodenlicht stimmen zusammen. Bewegung bleibt stabil im AGENTS-Budget.
