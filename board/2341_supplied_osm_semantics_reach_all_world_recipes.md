Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: generators, data
Tags: osm, semantics, inventory, recipes

# Gelieferte OSM-Inhalte machen die Welt vielfältig und glaubwürdig

## Ergebnis und Ist
Die vorhandenen Daten bestimmen sichtbare Formen, Oberflächen, Nutzung und Belegung.
[Inventur](../doc/references/data/osm/inventory.md): 32 Standorte × 11 = 352 erfolgreich
abgerufene Kacheln; 16 Layer, 137 POI-Klassen und 476 beobachtete Unterklassenwerte.
Das ist eine breite nutzbare Grundlage, kein Nachweis weltweiter Vollständigkeit.
Quell-MVTs und Tags geladener nativer Layer werden gespeichert; weitere Layer und
Rezeptzuordnungen erreichen die Welt bisher nur teilweise. Neue Rezept-Shots fehlen.

## Besitzer und nutzbare Inhalte
OSM-Erweiterung besitzt Normalisierung, Zuordnung und räumliche Verknüpfung;
world konsumiert native Produkte. Keine Quellsemantik in Engine/Renderer.
| Daten | Sichtbare Verwendung | Besitzer |
|---|---|---|
| Gebäudegrundriss, Höhe/Unterkante, Farbe | Körper, Parts, einfache Dächer, Fassadenplan | 2173 |
| Verkehr: 26 Klassen, 21 Unterklassen | Straßen/Schienen/Wege, Ebenen, Brücken/Tunnel | 2281 |
| Landuse: 42 Klassen | Wohn-/Gewerbe-/Industrieflächen, Sport, Steinbruch, Damm | 2337, 2338 |
| Landcover: 7 Klassen, 26 Unterklassen | Wiese/Acker/Wald, Fels/Schutt, Sand/Nässe/Eis | 2337, 2171, zuletzt 2111 |
| Wasser und Wasserwege | Seen/Flüsse/Kanäle, Becken, Docks und Ufer | 2145 |
| POI und Aeroway | Barrieren/Möbel, Zugänge, Gewerbe, Sport, Flughafenobjekte | 2338, Nahfassaden 2173 |
Gemeinsame Primitive und Parametrierung statt eines Generators je Tagwert.

## Umsetzung und Priorität
1. Mit Gebäude/Form/Fassade beginnen, danach Boden/Material und Infrastruktur;
   Vegetation bleibt später. Fehlende Eigenschaften blockieren keine vorhandene Rezeptfamilie.
2. Alle gelieferten Quell-Layer/Attribute auf SSD erhalten. Relevante Felder erreichen
   ihr Produkt; ungenutzte Namen/Sprachdaten benötigen keine dauerhafte RAM-Residency.
   Klassen einer Rezeptfamilie, Nutzungsregel oder begründet Metadaten zuordnen.
3. Gelieferte Werte bevorzugen, fehlende Angaben stabil ergänzen. Grundriss, Schlankheit,
   Höhenintervalle, Nachbarschaft und Flächennutzung begrenzen heuristische Formfamilien.
   Sonderbauten/technische Anlagen sind eine offene Menge, keine abgeschlossene Beispielsammlung.
   Herkunft und Unsicherheit speichern; schwache Hinweise ergeben einen einfachen Ersatz.
4. Render-/Webcam-Vergleiche verbessern allgemeine Heuristiken. Keine Referenzbilder als
   Laufzeiteingaben, keine Place-Sondergeometrie. Silhouette, Öffnungen und Material an mehreren
   Standorten/Ansichten vergleichen; Gegenproben schützen andere Klassen vor Fehlzuordnung.
   Nutzung ist keine Bauform: ein POI kann
   nur ein Geschoss belegen. Räumliche Zuordnung und tatsächlichen Nutzungsumfang erhalten.
5. Zusätzliche Layer erst für integrierte Produkte dekodieren. Native Assethits laden denselben
   fertig ergänzten Plan wie Misses; Semantik gezielt versionieren. 2280 besitzt Speicherung.
6. Tatsächliche Features liefern zusätzliche Nah-/Fern-Shots je physischer Rezeptfamilie.
   Kontakte, Form, Öffnungen, Material und Übergänge prüfen; daraus Regeln korrigieren.

## Datenlücken und Abnahme
Gebäude exportieren Höhe/Unterkante/Farbe/hide_3d; weitere Nutzungs-/Dach-/Materialangaben
fehlen dort. Sonderklassen wurden nicht beobachtet. Plausible allgemeine Ergänzungen sind erlaubt;
fehlende Tags verlangen weder einen Quellenwechsel noch eine vorgeschaltete Beschaffungsphase.
Relevante gelieferte Werte werden genutzt. Heuristiken sind deterministisch, ihre Herkunft bleibt
klar. Keine Wohnfenster an schmalen Versorgungsschäften oder unbegründete Anlagenhäufung.
Pflicht-Places und zusätzliche Rezept-Shots nach AGENTS; sichtbarer Gewinn und Kosten zählen.
