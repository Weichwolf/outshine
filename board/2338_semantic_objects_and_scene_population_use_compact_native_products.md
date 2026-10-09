Type: feature
State: open
Architecture: planned
Priority: P1
Parent: 2169
Depends:
Area: generators, world, render
Tags: objects, population, instances

# Compact procedural objects make city and harbour scenes feel occupied

## Ergebnis und Ist
Technische Aufbauten, Kräne/Masten/Geländer/Leitungen und plausible Autos/Boote/Segel.
Native Geometry/Material/Instanzen und glTF bestehen; Objekt-/Belegungsgenerator fehlt.
Exakte Foto-Belegung ist unbekannt und kein Rekonstruktionsversprechen.

## Besitzer und nächste Lieferung
OSM-Adapter liefert vorhandene Klassen-/Flächenhinweise, generators/objects Form/Belegung,
world native Assets/Instanzen, Renderer Ausgabe. Erste Klasse anhand tatsächlich gelieferter
Semantik wählen, Eingabe-/Produktvertrag dort konkretisieren; daher `planned`.
Vorhandene native Produkte reichen zum Einstieg; kein pauschaler API-/Quellen-Blocker.
Zuerst eine erkennbare technische Objektklasse im Stadt-/Hafenbild integrieren.

## Verfahren
- [Inventur](../doc/references/data/osm/inventory.md)/2341 bestimmen gelieferte Klassen und
  fehlende Sonderklassendaten. Rezepte für Windräder/Solarparks/Kräne/Masten, Flughafenteile,
  Straßenmöbel/Barrieren, ÖPNV/Parken und Sport/Spiel teilen primitive/instanzierte Rohlinge.
  Industrieflächen belegen keine bestimmte technische Anlage. Zusätzliche Places prüfen
  jede physische Rezeptfamilie, nicht nur das Vorhandensein ihrer Tags.
- Gelieferte Maße/Richtung/Material/Kontakt übernehmen; Unbekanntes stabil und plausibel
  ergänzen. Primitive/Profile für Masten/Ausleger/Kräne, Poller/Geländer/Antennen und
  generische Fahrzeug-/Bootshüllen. Wiederholungen instanzieren, dünne Leitungen begrenzen.
- Terrain/Wasser/Freiraum und Zugänge bei Platzierung respektieren. Seeds/Identität über
  LOD stabil; 2336s Fernbeiträge statt Nahmesh. Gebäude 2173, Straßenmarkierungen 2281,
  Pflanzen 2111 besitzen ihre eigenen Klassen; keine duplizierten Generatoren.
- Geteilte Rohlinge/Belegungspläne über 2280s Assetcache laden; nur Misses erzeugen sie.
  Nahdetails budgetiert aus fertigen Parametern ergänzen, keine neue Quellenpipeline.
- Park-/Liegeflächen erzeugen plausible Belegung. Statische Objekte brauchen keinen fertigen
  Solver; bewegliche Varianten später über 2136. Segel/Flaggen teilen 2172s Wind und Bounds.

## Forschungsgrundlage
[GPU-Driven Rendering](../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf)
([Primärquelle/Einordnung](../doc/references/README.md)): wiederholte Bauteile als geteilte
Assets/Instanzen, Sichtbarkeit über Cluster-Bounds. Dünne Fernobjekte als gefilterte Beiträge;
keine Einzelteil-Draws oder vollständigen Nahmeshes vor der budgetierten Auswahl.

## Abnahme
Stadt-/Hafenbild gewinnt erkennbare Objekte mit richtigen Kontakten/Maßen, ohne unplausible
Zufallsbelegung. Form bleibt über Entfernung stabil, zusätzliche Zeit/Bytes budgetiert.
