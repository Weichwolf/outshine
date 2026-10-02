Type: feature
State: open
Architecture: planned
Priority: P1
Parent: 2169
Depends: 2188
Area: import, generators, world, engine, render
Tags: objects, scene-population, instances

# Semantic objects and plausible occupancy complete the scene

## Ergebnis und belegter Iststand
Stadt/Hafen zeigen technische Aufbauten, Kräne/Masten, Geländer/Leitungen und plausible
Autos/Boote/Segel. OSM-Semantik, native Geometry/Material/Instanzen und glTF-Import bestehen;
ein angeschlossener allgemeiner Objekt-/Belegungsgenerator fehlt. Fotos zeigen bewegliche
Objekte, deren momentane Position die erlaubten Eingaben nicht belegen können.

## Besitzer und nächster vollständiger Schritt
Import übersetzt Original-Nodes/Ways/Relations/Tags in native Objektbeschreibungen.
Generatoren besitzen kompakte Klassen-/Formgrammatiken; world hält Assets/Instanzen mit
Bounds/Herkunft. Engine besitzt Planung/Publikation, render Ausgaberesourcen. 2188 liefert
noch den öffentlichen gepinnten Input-/Produkt-/Detailvertrag. Objekte verwenden dieselbe
Registrierung wie Gebäude und externe Generatoren. Neue Besitzer unter generators/objects
vor Implementierung anhand tatsächlicher Produzenten festlegen; deshalb `planned`.
Zuerst eine belegte technische Klasse in einem vollständigen Stadt-/Hafen-Place liefern.
Straßenquerschnitte/Markierungen bleiben 2281; Gebäudeformen 2173; Vegetation 2111.

## Verfahren und Invarianten
- assetclass, Abmessungen, Richtung, Material und Kontakt/Erhöhung nach OSM übernehmen;
  unbekannte Angaben folgen stabiler Klassenpolicy mit generischer Provenienz.
- Zusammengesetzte primitive/profilebasierte Formen: Mast/Ausleger/Kran, Poller/Geländer,
  Antennen/Schornsteine, generische Fahrzeug-/Bootshülle. Wiederholte Teile teilen Geometrie.
  Dünne Leitungen als begrenzte Kurvensegmente; subpixelige Beiträge gefiltert integrieren,
  statt unbegrenzt Tessellation/Overdraw zu erzeugen. Keine fotografischen Ortsmodelle.
- Maßstab, Seeds und Identität bleiben über Distanzen stabil. Detailhierarchie aus 2336
  konsumieren; entfernte Objekte werden kompakte Formen/Beiträge, keine Nahgeometrie.
- Parkplätze/Spuren und Hafen-/Liegeflächen erlauben plausible stabile Belegung mit
  benannter Unsicherheit. Keine Behauptung, das Foto-Fahrzeug sei tatsächlich dort.
  Freiraum, Terrain-/Wasserkontakt, OSM-Zugang und logische Netze respektieren.
- Bewegliche Objekte sind Entities mit Renderform/Kollision und später 2136s Kräften/
  Steuerevents. Animation/NPC-Aktionen umgehen keine Physik. Statische Belegung erfordert
  keinen vollständigen dynamischen Solver oder heimliches Simulieren jedes Fernobjekts.
- Segel, Flaggen und Leitungen konsumieren denselben Wind wie Wetter/Vegetation;
  begrenzte geometrische Bewegung, Schatten und konservative bewegte Bounds stimmen überein.

## Abnahme
Ein Hafen-/Stadtbild gewinnt erkennbare Objekte mit richtigen Kontakten, Maßen und
stabiler Ferndarstellung. Originalklasse und plausible Ergänzung bleiben unterscheidbar.
Öffentliche Generierung, sichtbare Welt und Zeit-/Bytekosten belegen dieselben Produkte.
