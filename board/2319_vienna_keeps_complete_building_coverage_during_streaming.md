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

Wien, CentralPark und Shibuya stehen aus gecachten Netzwerkquellen vollständig innerhalb
zehn Sekunden bereit. Die anschließende 360°-Drehung verliert keine Gebäude. Bestehende
Straßen und Terrain-Anschlüsse bleiben erhalten; keine Sichtweitenkürzung oder Place-Sonderpfade.

## Vorhandene Fähigkeit und tatsächlicher Engpass

Implizite Quellenaufträge wählen wieder Fine/Shell/Massed nach Entfernung. Explizite
Detailaufträge bleiben kameraunabhängig. Der persistente Gebäudeprodukt-Cache ist vom
Runtime-Pfad getrennt. Diese Reparatur allein löst das Ladeproblem der drei Städte nicht.
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

## Nächster vollständiger Schritt

1. `StructureBake` und `BuildingMesh` getrennt nach Quellenprodukt und Zellprodukt betrachten:
   Welche Geometrie wird für dieselbe Stadt mehrfach erzeugt und welche bleibt resident?
   Fine/Shell/Massed an identischen Quelldaten vergleichen; Generierungszeit, Dreiecke,
   Upload und belegte Silhouetten-/Oberflächenabweichung bestimmen. Keine weitere Cachekampagne.
2. Vorhandene quellgültige Produkte innerhalb ihrer nachgewiesenen Qualitätsgrenzen
   wiederverwenden. `StructureCellPlanner` darf teure Fine-Zellen nur verlangen, wenn die
   vorhandene Darstellung das projizierte Fehlerbudget tatsächlich nicht nachweislich erfüllt.
   Kleinere Schranken benötigen die Runtime-Übertragung aus WI 2312; CPU-Beweise allein reichen nicht.
3. Entfernte Zusammenfassung vor Geometrieerzeugung an nachgewiesene räumliche Fehler binden.
   Die derzeitige Blockgröße nicht willkürlich verkleinern oder Grenzwerte erhöhen.
   Gebäudehöhen, Silhouetten, Öffnungen und Materialwirkung getrennt erhalten und prüfen.
   Fehlender Nachweis erhält den konservativen Fallback und bleibt als Kostenblocker offen.

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
