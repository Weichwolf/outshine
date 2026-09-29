Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: engine, streaming, render
Tags: buildings, publication, visual

# Wien lädt seine vollständige Stadt innerhalb des Place-Budgets

## Ergebnis

Wien, CentralPark, Shibuya und Olympiaturm stehen aus gecachten Netzwerkquellen vollständig innerhalb
zehn Sekunden bereit. Die anschließende 360°-Drehung verliert keine Gebäude. Bestehende
Straßen und Terrain-Anschlüsse bleiben erhalten; keine Sichtweitenkürzung oder Place-Sonderpfade.

## Vorhandene Fähigkeit und tatsächlicher Engpass

Implizite Quellenaufträge wählen wieder Fine/Shell/Massed nach Entfernung. Explizite
Detailaufträge bleiben kameraunabhängig. Der persistente Gebäudeprodukt-Cache ist vom
Runtime-Pfad getrennt. Diese Reparatur allein löst das Ladeproblem der drei Städte nicht.
Fine erzeugt Dach-/Sockelumrisse nur einmal je Gebäudeform; ungenutzte Attika-Umrisse
entfallen bei geneigten Dächern. Das ändert keine Geometrie und ersetzt keine LOD-Auswahl.
Ganzkacheln bleiben bis zur atomaren Übernahme quellgültiger Zellen sichtbar; abgelöste
Jobs und übergebene CPU-Bakes werden freigegeben. Starre Pieces teilen den Vertexbuffer
mit der Darstellung voriger Positionen. Diese funktionierenden Verträge bleiben erhalten.

Die Zusammenfassung setzt acht Blöcke je Quellkachel voraus. Bei Wiens Zoom 14 ergeben
sich rund 1.629 m Kachelbreite und 101,8 m angenommener Fehler (Breite / 16).
Bei 720 Pixel Bildhöhe und 38,04° Bildwinkel beträgt die Brennweite etwa 1.044 Pixel;
101,8 m × 1.044 px / 1 px + 128 m Schutzabstand erlauben Massed erst ab etwa 106 km.
Damit erklärt `lumped=0` keinen defekten Zweig: Diese Regel entlastet die nähere Stadt kaum.
`StructureCellDetail` verlangt außerdem meist Fine, weil der ganze Zellumfang als
Fehler angenommen wird. Shell erzeugt inzwischen native Grundrisse, Dächer und Sockel;
der Name allein garantiert keine kleine Geometrie. Published-Byte-Summen sind kein Peak-RAM.

## Nächste Lieferung: funktionierende entfernungsabhängige LOD wiederherstellen

Jura muss entfernte Bebauung vor der Geometrieerzeugung vereinfachen und zusammenfassen;
Nahdetails entstehen nur bei nahen Gebäuden. Spätere Vegetation teilt dasselbe Framebudget.
Mesh-Zeichenarbeit erklärt die gemessene Spitze; Terrain- und Beleuchtungsvereinfachungen
beseitigen sie nicht. Keine neue Hierarchie vor Klärung der bestehenden Regression.

`a0f49bb7f` machte Refined von expliziten Zellprodukten abhängig. Deren Auswahl verwendet
den ganzen Zellumfang als Fehler und ersetzt damit die feinere Gebäudeauswahl im Quellbake.
Der ältere Generator wählte Shell anhand Architektur-/Quellauflösung und Massed anhand
Blockbreite. Diese Auswahl allein beseitigt Juras Spitze nicht: `4d9f4639d` ersetzte
am 28. September die günstigen Shell-Hüllen durch native Dach-/Sockelgeometrie.
Auswahl UND tatsächlich erzeugte Shell-Komplexität sind deshalb gemeinsam zu reparieren. `fe6910850` dokumentiert zugleich, weshalb reine Gebäude-Pixelgröße keine
zulässige Schranke für das Zusammenfassen weit auseinanderstehender Häuser ist.

1. Günstige native Shell-Hüllen sind implementiert; mit historischer Auswahl verschwindet
   Juras dauerhafte Framezeitspitze im wiederholten Diagnosevergleich, mit regulärer Auswahl
   kehrt sie zurück. Der nächste Schritt ist die geometrisch begründete Shell-Auswahl.
   Historische Architektur-/Quellauflösung ist kein Fehlerbeweis für ausgelassene Dachdetails;
   Bildänderungen und ein einzelner Ausreißer verhindern eine pauschale Qualitätsfreigabe.
2. Bestehenden Fine/Shell/Massed-Pfad reparieren. `StructureCellPlanner` darf früh gewählte
   entfernte Vereinfachung nicht pauschal in teure Fine-Zellen zurückverwandeln. Auswahl vor
   Detailgenerierung, quellgültige Produkte und atomare Ablösung gemeinsam erhalten.
3. Kleinere Runtime-Schranken entstehen beim Generator aus begrenzten Änderungen an
   denselben Gebäudeformen. Gemeinsame Flächen, entfernte Aufbauten, Dachüberstände,
   Sockel und konkave Grundrisse vollständig einschließen; unbekannte Fälle bleiben Fine.
   WI 2312 prüft diese Schranken unabhängig an nativen Fine/Shell-Paaren. Den bestehenden
   vollständigen Paarvergleich nicht pro Ladezelle aktivieren: Auf echten Jura-Eingaben
   überschreitet er das Preload-Budget. Keine zusätzliche Fine-Erzeugung nur zum Nachweis.
   Quellidentität, Terrain und Transformationspräzision bleiben Teil der Runtime-Gültigkeit.
4. Jura ist der erste vollständige Durchstich; danach Wien, CentralPark, Shibuya und Olympiaturm.
   Gebäude, markante Höhen, Zwischenräume, Materialien und Straßen bleiben erhalten.
   Vorhandenes Massed mittelt Höhen: Bildvergleich muss insbesondere Hochpunkte und
   Hangstaffelung prüfen. Eine neue räumliche Hierarchie ist vorerst nicht freigegeben.

## Vier durchgängige Darstellungsstufen

Der gemeinsame Vertrag besitzt bereits Fine/Shell/Massed/Skyline. Der Gebäude-Bake
und die Zellauswahl akzeptieren jedoch höchstens Massed; Skyline ist dort nicht angebunden.
Keine zusätzliche parallele LOD-API. Die vier gewünschten sichtbaren Stufen müssen
Generator, Anfrage, Publikation und Auswahl durchgängig abbilden: Nahdetails, vereinfachte
Einzelobjekte, einfache Hüllen/Kronen und zusammengefasste Siedlungs-/Waldmassen.
Die heutige Enum-Semantik passt dazu nicht vollständig; Zuordnung vor Runtime-Änderungen
explizit entscheiden. Vegetation nutzt denselben Vertrag, wird aber weiterhin zuletzt umgesetzt.

## Besitzer und unveränderliche Verträge

`StructureBuildQueue` besitzt Aufträge und begrenzte Worker-Arbeit; `BuildingField` die
quellqualifizierten Eingaben; `TilePieces` aktive Produkte und Auswahl; `SubjectResidency`
die GPU-Reserven. `GroundPublication` besitzt Kandidat und Veröffentlichung.
Quellrevision, Terrain-Zertifikat und Produktidentität gelten bis zur Aktivierung.
Veraltete Ergebnisse dürfen weder neue Produkte ersetzen noch gültige Gebäude entfernen.
Frustum-Culling reduziert Zeichenarbeit, nicht die notwendige Rundum-Verfügbarkeit.
Nur Netzwerkquellen werden persistent gecacht; vorhandene Cachedateien bleiben erhalten.
CPU-/GPU-Produkte und temporäre Überlappung haben begrenzte Besitzer und Freigabegrenzen.
Unified Memory nicht doppelt zählen; hoher Verbrauch rechtfertigt keinen Bedarf.

## Abnahme und Widerlegung

`make format`, betroffene Generator-/Queue-/Planer-Suites, `LINT_JOBS=2 make lint` und
`JOBS=1 make suite SUITE=outshine/integration/places`. Alle tatsächlichen Hash-PNGs öffnen
und mit erhaltenen Vorherbildern vergleichen. Kostenänderungen bei denselben Quellen und
Ansichten nachweisen; Host und A18 Pro getrennt beurteilen. Logs im System-Tempverzeichnis.
Die Lieferung scheitert an fehlenden Gebäuden, schlechteren Straßen/Materialien/Silhouetten,
verlorenen OSM-Eigenschaften, überschrittenen Place-Budgets oder unbewiesenen Fehlerschranken.

## Gemessener Warmstart-Engpass: Gebäude-Meshing

Das kapazitätsabhängige Leeren trivialer Weld-Tabellen ist durch Epoch-Wechsel ersetzt.
Im Folgeprofil dominiert Site::Index/Corner; Wien bleibt beim vollständigen Preload rot.
BuildingMesh sucht neue Positions-/Corner-Schlüssel vor Emplace nochmals mit Find.
Den vorhandenen Emplace-Rückgabevertrag verwenden: Index aus dem Ergebnis übernehmen,
Corner-Vertex ausschließlich bei tatsächlicher Einfügung anhängen. Reihenfolge, Schlüssel,
Geometrie, LOD, Quellen und Fehlerweitergabe erhalten; keine neue Container-API.
FlatMap-/Building-Suites, unveränderte Place-Bilder und vollständiges Lint prüfen.
Der Warmstart entscheidet über den Nutzen; diese lokale Reparatur ersetzt keine LOD-Lösung.
