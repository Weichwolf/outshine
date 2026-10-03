Type: feature
State: open
Architecture: planned
Priority: P1
Parent: 2169
Depends: 2188, 2280
Area: generators, world, render
Tags: objects, scene-population, instances

# Semantic objects and plausible occupancy complete the scene

## Ergebnis und Ist
Technische Aufbauten/Kräne/Masten/Geländer/Leitungen und plausible Autos/Boote/Segel
verdichten Stadt/Hafen. Native Geometry/Material/Instanzen und glTF-Import bestehen;
allgemeiner Objekt-/Belegungsgenerator fehlt. Tatsächliche Foto-Belegung ist unbekannt.

## Besitzer und fehlende Verträge
2280 liefert normalisierte verfügbare Objekt-/Flächensemantik, 2188 öffentliche Inputs/
Produkte/Detail. OSM-Adapter besitzt Quellattribute, `generators/objects` Form-/Belegungs-
grammatik, world Assets/Instanzen/Bounds/Herkunft; Render Ausgabe. Produzenten-/Klassen-
vertrag vor Implementierung konkretisieren, daher `planned`. Zuerst eine belegte technische
Klasse im Stadt-/Hafenbild liefern; Gebäude 2173, Straßenmarkierungen 2281, Pflanzen 2111.

## Verfahren und Invarianten
- Gelieferte Klasse/Maße/Richtung/Material/Kontakt übernehmen; unbekannte Angaben folgen
  stabiler Klassenpolicy mit generischer Provenienz. Keine erfundenen Quell-Tags/Ortsmodelle.
- Primitive/Profile zusammensetzen für Mast/Ausleger/Kran, Poller/Geländer/Antennen und
  generische Fahrzeug-/Bootshüllen; wiederholte Teile instanzieren. Dünne Leitungen als
  begrenzte Kurven, subpixelige Beiträge gefiltert statt unbegrenzter Tessellation/Overdraw.
- Seeds/Identität/Maße über Detailwechsel stabil; 2336s Fernformen/Beiträge statt Nahmesh.
  Terrain-/Wasserkontakt, Freiraum, Zugang und native Netze bei Platzierung respektieren.
- Park-/Liegeflächen erzeugen plausible Belegung mit benannter Unsicherheit, keine exakten
  Foto-Fahrzeuge. Statische Belegung braucht keinen fertigen Solver oder Fernsimulation.
- Bewegliche Formen werden Entities mit Kollision und später 2136s Kräften/Commands.
  Segel/Flaggen/Leitungen teilen 2172s Wind, konservative bewegte Bounds und Schatten.

## Abnahme
Ein Hafen-/Stadtbild gewinnt erkennbare Objekte mit richtigen Kontakten/Maßen und
stabiler Ferndarstellung. Quellbefund und Ergänzung unterscheidbar; öffentliche Pipeline
und Kostenmessung belegen dieselben sichtbaren Produkte.
