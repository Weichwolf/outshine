Type: bug
State: active
Priority: P0
Architecture: ready
Area: engine, world
Parent: 2169
Depends:

# Eine geladene Welt bleibt bei Drehung und Bewegung stabil

## Ergebnis

Die vollständige Welt bleibt rund um die Kamera resident. Drehen ändert Sichtbarkeit,
Bewegen ergänzt nur neu benötigte Regionen und Detailstufen. Unveränderte Frames lösen
keine erneute Ingestion, Kompaktierung oder Generierung aus. Geänderte Quellen betreffen
nur ihre Produkte und direkten Randabhängigkeiten; alte vollständige Produkte bleiben
bis zum gültigen Ersatz sichtbar. Sichtweite und Straßenqualität bleiben erhalten.

## Vorhandene Fähigkeit und offene Kosten

Worker-Bakes, geteilte Quelldaten und Terrain-Felder sind vorhanden. DEM-Höhenabfragen
melden Pending statt den Frame auf Netzwerk-IO warten zu lassen. Wasseraufnahme hält
bereits geprüfte Höhen und veröffentlicht erst vollständige Tiles; ihre Arbeit ist begrenzt.
`GroundStack::Restand` merkt abgeschlossene Klassen-, Vektor- und Footprint-Revisionen.
Gleiche vollständige Eingaben überspringen Ingestion und shrink-to-fit; Quellen-Polling
und Gesamt-Speichergrenze bleiben aktiv. Close/Open verwirft den Abschlussstempel.

Offen: `Grounds` fragt über `RingWanted` und `Focuses` auch ohne Quellenänderung erneut
Terrain-Abdeckung und Speicherbestände ab. Rezentrieren und neue Ergebnisse können noch
weltweite Aufbau-/Uploadarbeit auslösen. Eine schnelle stationäre Ansicht beweist weder
begrenzte Arbeit bei Bewegung noch das Budget aller Blickrichtungen.

## Architektur und Umsetzung

1. `GroundStack` besitzt Quellaufnahme und Revisionen; `GroundPublication` besitzt
   Kandidat und atomare Aktivierung. Provider pollen weiter, ohne fertige Inhalte erneut
   aufzubauen. Quellen-, Terrain-, Footprint- und Projektionsänderungen bleiben getrennt.
2. `Laying` übernimmt abgeschlossene Workerprodukte aus unveränderlichen Eingaben.
   Coverage-Anfragen nur bei relevanten Änderungen wiederholen; Pending und Ankunft müssen
   Fortschritt auslösen. Keine pauschale Abkürzung anhand unveränderter Kameraposition.
3. Terrain, Straßen und Wasser invalidieren geänderte Regionen plus Randabhängigkeiten.
   LOD ändert Darstellung, nicht logische Netze oder Physik. Bestehende Render-/Kontakt-
   Raumreferenzen bleiben konsistent; Terrain- und Quellenzertifikate bleiben verbindlich.
4. Cluster-/Instanzbereiche resident halten und nur geänderte Bereiche hochladen.
   Übernahme, Uploads, Queue und temporäre Überlappung begrenzen. Abgelöste Generationen
   dürfen weder publizieren noch unbegrenzt Ressourcen behalten.
5. Speichergrenzen gelten auch bei unveränderten semantischen Revisionen: Terrain-Pool und
   Workerprodukte können unabhängig wachsen. Zählkosten durch Besitzer-Akkumulation lösen,
   falls gemessen relevant; niemals die Gesamtgrenze durch einen Kamera-Schnellpfad umgehen.

`Tasks` besitzt Queue und Ergebnislebensdauer. Queue-Sättigung, unbekannte/konsumierte
Handles und Shutdown brauchen definierten Abschluss. Ein unbewiesener Leak ist kein
Grund für einen Umbau; begrenzte Übergabe und gemessene Kosten entscheiden die Lieferung.
Nur Netzwerkquellen werden persistent gecacht, keine generierten Produkte.

## Abnahme und Widerlegung

`make format`, betroffene GroundStack-/Streaming-/Publikations-Suites,
`LINT_JOBS=2 make lint` und alle Places über outshine-client. Tatsächliche PNGs öffnen.
Nach maximal zehn Sekunden vollständigem Preload: 60 Frames, 360° in höchstens einer
Sekunde, p99 höchstens 1000/60 ms. Stationäre Wiederholung, neue Quelldaten und Ortswechsel
prüfen; Arbeit beim Tilewechsel muss von Änderungen statt vom gesamten Weltring abhängen.
Fehlende Inhalte, schlechtere Straßen, alte Veröffentlichungen, blockierendes IO oder
unbegrenztes Speicherwachstum widerlegen die Lieferung. Host und A18 Pro getrennt abnehmen.
