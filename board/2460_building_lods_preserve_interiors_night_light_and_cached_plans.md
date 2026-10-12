Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, render, engine, world
Tags: buildings, interiors, lod, cache, lighting, performance

# Building LODs preserve functional interiors and coherent night light

## Ergebnis und fehlender Vertrag
Besitzer: Gebäudeagent/Python; 2420 liefert stabile Räume/Portale/Geschosse samt tatsächlicher Geometrie.
2280 besitzt Cache, 2336 Bedarf, 2171 Materialien, 2155 Licht; nativ mit Besitzern integrieren.
Kameraunabhängiger Rohling enthält Körper-/Raum-/Öffnungsplan und benötigte vorbereitete Produkte.
Treffer ohne Quellenaufbereitung/Join/Rohling-Aufbau; Nahdetails daraus nach Bedarf.
Hüllen/Repeats sind quellenfrei ladbar; funktionale Innenraum-/Lichtprodukte und deren Kosten fehlen.
Aktiver Python-Vergleich: gespeicherte Raum-/Öffnungs-IDs vor teurer Detail-/Schattenarbeit räumlich auswählen.
Wiener Alpha- und Frustum-Nahkontrollen überschritten 600 s; Kamerafehler auf Beleuchtungszentrum statt Öffnungsziel entdeckt.
Diese Nahbilder/Kosten belegen keine korrekte Sichtauswahl; Showcase kennzeichnet betroffene Fenster-/Eingangs-/Kompositbilder ungültig.
Korrigierter Renderer übernimmt dieselbe Basis wie Bedarf, prüft tatsächliche Rotation/Projektion und protokolliert beide Zentren getrennt.
Opake Hülle/Raumwelt erhalten; Kontrollläufe ohne Glas sowie mit getrenntem Fill-Schatten prüfen den eigentlichen Engpass.
Auswahl/Schattendarstellung bleiben ohne passenden Bild- und Kostenbeleg unfreigegeben; keine höhere Laufgrenze.
Beide korrigierten Wiener Kontrollläufe überschreiten 600 s; Fill-Schattenabschaltung brachte keinen Kostenbeleg.
`perf`: 567 User-Space-Stichproben/3 s, 95,77 % JIT und 4,23 % Mesa; konkrete Shaderziele unaufgelöst, keine Python-Erzeugungszeit.
[`verify_render_camera.py`](../test/experiments/buildings/verify_render_camera.py): zehn reale Blender-Szenenkameras, unabhängige Zielprojektion und Lichtausrichtung; ursprünglicher Fehler als Gegenfall.
[`opening_inspection.py`](../test/experiments/buildings/opening_inspection.py): perspektivische Workbench-Fenster-/Eingangskontrollen aus denselben Cacheplänen.
Glaspaneele ausschließlich für die Raum-/Öffnungskontrolle ausblenden; gespeichertes Vollmesh samt Quelle/IDs unverändert.
Workbench ersetzt weder Esri-Taglichtvergleich noch Render-LOD; Material-/Glas-/Nachtabnahme bleibt offen.

## Detail, Funktion und Nacht
- Nah durch Fenster: echte Wandtiefe/Böden/Decken/Trennwände und räumlicher Hintergrund passender Räume.
  Begehbarkeit/Portale verwenden denselben Plan; keine widersprüchliche zweite Raumwelt.
- Sicht-/Projektions-/Portalbedarf vor teurer Expansion; unsichtbare Schatten-/Lichtwirkung berücksichtigen.
  Vollständige grobe Hülle/Schatten erhalten, versteckte Feindetails nicht pauschal bauen.
- Fern: vereinfachte Silhouette/Öffnungs-/Innenraumdarstellung mit geprüftem Übergang zum Nahdetail.
  Räumlicher Eindruck, mittlere Helligkeit und Beleuchtungszustand bleiben kohärent.
- Räume mit stabilen IDs und Nutzung/Belegung/Lichtzustand; zusammengehörige Fenster stimmen überein.
  Warme/kühle Quellen, dunkle/belegte Räume, Verdeckung/Vorhänge und Außenwirkung nachts vergleichen.
  Raumgeometrie und veränderliche Spiel-/Lichtzustände getrennt; kein Licht im Asset festbacken.
- Bewegung/Drehung/Eintritt/Zustandsänderung/Cachehit/Fehler prüfen; Rundum-Abdeckung renderbereit.
  Logische Räume/Kollision/Verkehrswege bleiben unabhängig vom Render-LOD.

## Messung und Abnahme
Verfahren zuerst Python, gleiche Quellen/Ansichten/Qualität; Kosten und Bildfehler getrennt messen.
Erwerb/Rohling/Treffer/Auswahl/Expansion/Speicher, Median/Peaks/Wachstum und Einheiten nennen.
Tag-/Nacht-Nah/Gegen/Fern, keine neu gewürfelten Räume/Lichter oder verschlossenen Zugänge.
Vollständiger Rohling → frischer Offline-Prozess; Form/Plan/Material/Übergänge erhalten.
Wien/Tokyo/Central Park: Dichte; Python/Blender belegen keine nativen Frames oder A18 Pro.
Budgetende liefert gültige gröbere Darstellung samt Grund; Qualitätsgrenzen bleiben verbindlich.
Gate nach AGENTS/2169/2280/2336; native Kollisions-/Lichtintegration und Gerätebeleg offen.

## Verfahren
[F-shade, EGSR 2010](../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf): kompakte Fassaden/wahlfreier Zugriff.
Innenraum-/Parallaxverfahren gegen echte Nahgeometrie vergleichen; 2400 liefert Esri-Komponenten-/Referenzbelege.
[Stochastic Transparency, TVCG 2011](../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf): Alpha-/Schatten-Sampling erzeugt Varianz; Seiten 1/3 optisch geprüft.
Kein Beleg für die konkrete Blender-Körnung; kontrollierte Geometrie-/Schattenläufe müssen die Ursache unterscheiden.
