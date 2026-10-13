Type: feature
State: active
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
Aktiver begrenzter Vergleich: vorhandene Raum-/Öffnungsmeshes mit Mitsuba 3 LLVM/RGB auf CPU, gleiche Kameras/1920×1080.
[Primärdokumentation](https://github.com/mitsuba-renderer/mitsuba3), [SIGGRAPH Asia 2019](https://rgl.epfl.ch/publications/NimierDavidVicini2019Mitsuba2): Renderer-Vektorisierung; Seiten 1/2 optisch geprüft.
[Quellenbeleg](../doc/references/buildings/siggraph/2019-mitsuba2-retargetable-forward-inverse-renderer.reference.json): PDF lokal, Zitat/Link/Hash versioniert. CPU-Referenzmessungen unten; kein Runtime-Rendererwechsel.

## Übernahme aus den Transportexperimenten
Review von `~/Git/outshine/test/experiments/transport/` am 2026-10-13, Stand dd95f208b:
`transport_coupled_roadbed_products` prüft gespeicherte Produkte gegen ausgegebene Dreiecke,
erhält sämtliche Quellanfragen samt begründeten Auslassungen und bereits bewiesene Verbindungen.
`transport_vehicle_sweep` begrenzt Bewegungsfehler geometrisch; Diagnose-Samples ersetzen keinen Sweep-Beleg.
Gebäude besitzen bereits Mesh-Tür-/Raum-/Treppenprüfungen in 2420. Die zusätzliche Pflicht hier:
- Dieselben Zugang-/Raum-/Geschoss-IDs nach Laden des vollständigen Produkts erneut gegen sein Mesh prüfen.
- Detailwechsel erhalten jeden vorher gültigen Zugang und Geschossweg; fehlende Fälle mit Grund ausweisen.
- Renderkamera, Materialbindung und Lichtzustand aus dem geladenen Produkt prüfen; Bilder allein beweisen keine Begehbarkeit.
- Bewegung mit endlicher Prüffigur samt Kopf-/Bodenfreiheit prüfen; kontinuierliche Fehlerschranke ausdrücklich nennen.
Aktuell kein Nachweis für die gesamte Kette Cache → Detailwechsel → kontinuierliche Begehung.
Mitsuba-Kanalprüfung: RGB-Renderarray hat neun Kanäle, Integratorliste zehn; benannte Film-Layer verwenden.
Entwicklungsbild 1920×1080/16 spp: 59,10 s CPU-Pfadverfolgung + 5,24 s OIDN, zwei Threads;
64-spp-Kontrolle: 232,60 s CPU; gleicher Raum/Kamera/Licht/Seed, Rohbild weiterhin körnig.
16-spp-OIDN gegen 64-spp-Rohbild: lineare RMSE 0,039 statt 0,086, mittlere Energieabweichung 0,37 %.
Beide optisch geprüft; keine rauschfreie Referenz/zeitliche Abnahme. Sieben physikalische Showcasekontrollen auf 03c0f5354 optisch geprüft, alle 1920×1080/32 spp.
CPU-Pfadverfolgung 16,40–170,16 s, Filter 5,14–9,54 s, zwei Threads; vollständige 2.520/47.956 Gebäude-Dreiecke.
Material-Overrides vier Cachemeshes/21 Messpaare: p50 0,071–0,152 ms, Baken 1,50–43,99 ms; gleiche aufgelöste Werte.
Konstruktion/finale Ausgabe/Frames ausgeschlossen; Importressourcen bleiben erhalten. Vollständige Shader-/Texturbindung offen.
f097edd46 ergänzt metrische Fassadenplatten mit prozeduralen Farb-/Rauheits-/Normalkarten; Quellfarbe erhalten.
Vier Cachegebäude: UV-Kantenlängen/Geometrie/Semantik geprüft; Diagnose expandiert Ecken, keine native LOD-Abnahme.
Zwei Wiener Nachtbilder streuen trotz gleichem Seed auch mit altem Code: PNG-RMSE 0,68/0,95 auf 0–255.
Alt/neu 0,67/0,88; optisch keine neue Strukturabweichung. Sampling-Ursache/zeitliche Stabilität bleiben offen.
Pakete: Mitsuba 3.9.1/Dr.Jit 1.5.0 in .venv; OIDN 2.5.1 extern unter ~/Downloads/oidn-2.5.1.x86_64.linux.
[OIDN-Dokumentation](https://www.openimagedenoise.org/documentation.html); [SIGGRAPH 2017](../doc/references/buildings/siggraph/2017-recurrent-monte-carlo-denoising-autoencoder.reference.json), Seiten 1/2 optisch geprüft.
Räumlicher Filter; kein Beleg für den rekurrenten Paper-Algorithmus oder zeitliche Stabilität.

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
