Type: feature
State: active
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: render, engine, client
Tags: lighting, shadows, hdr, presentation

# Light and camera response make existing world geometry convincing

## Ergebnis und Ist
Kohärentes Tages-/Nachtlicht, räumliche Schatten/Reflexion und stabile Belichtung.
SceneRenderer/SkyStage, LightVisibility/Irradiance, HDR/TemporalResolve/Tonemap bestehen;
Weltwirkung und Kameraantwort sind unzureichend oder nicht am Place belegt.
subjectLighting prüft derzeit die gemeinsame Lichtliste je Fragment und vergleicht
Sonnentiefe mit einem einzelnen ungefilterten Shadow-Lookup. Das erklärt weder alle
Bildfehler noch gemessene Kosten; beide Pfade sind konkrete Ausbaupunkte.
Die Umgebungsspekularantwort verwendet derzeit denselben gemittelten Himmels-/Bodenanteil
wie Diffuse; Reflexionsrichtung und Roughness fehlen. Das lässt Glas/Metall flach erscheinen.

## Besitzer und nächste Lieferung
Renderer besitzt Licht/Pässe/History, Client Kamera/Pacing; PlaceCamera/Referenzkatalog
Pose/FOV/UTC und Kalibrierung. Mit vorhandenen Inputs zuerst eine Stadt- und Bergansicht
über Himmelsfüllung, Sonnenschatten und Belichtung verbessern. Kein Quellen-/SDK-Blocker.
2172 ergänzt später denselben Lichtzustand um Wolken und Wetter.

## Aktueller Ausbau: gemeinsame Umgebungsreflexion
- Gemeinsame Himmelsabfrage für Hintergrund und Reflexion, einschließlich Horizont und
  Texelzentren; Sonnenprojektion am Zenit bleibt endlich. Erst diesen vorhandenen Vertrag
  anschließen, dann MediumRadiance → EnvironmentSpec → Gebäude/Terrain/Wasser integrieren.
- Renderer besitzt einen GPU-vorgefilterten GGX-Atlas: sieben Roughness-Stufen, 64² nutzbare
  Texel je Stufe, je ein Randtexel. RGBA16F: 66 × 66 × 7 × 8 = 243936 Byte (238,2 KiB).
  Deterministische begrenzte Samples; Octaeder-Ränder korrekt fortsetzen, zwei gefilterte
  Abfragen interpolieren Roughness. Das Layout vermeidet neue Cubemap-Subresource-Verwaltung.
- BRDF-Split-Sum aus demselben GGX/Smith-Modell wie Direktlicht; vorhandene Tabellenerzeugung
  erweitern. Sonne bleibt getrenntes Direktlicht, nicht doppelt in Reflexionen rechnen.
  Welt-Up/Sonnenrichtung explizit im typisierten CPU/GPU-Lichtvertrag, keine Lichtindexannahme.
- Atlas hängt an Medium, Sonnenstand, Augenhöhe und Quellprodukt. Submission/Invalidierung
  und Abschluss-Fence in bestehende Weltvorbereitung integrieren; kein Aufwärmframe.
  Erst Himmel und gemittelter Boden: fehlende lokale Weltreflexion bleibt eine benannte Lücke.
  Wolken/Nachtkörper und später SSR/Probes ergänzen dieselbe Lichtwelt. Keine Objektsonderfarben.
- Filament `ef1a133d`, `surface_light_indirect.fs`/`CubemapIBL.cpp` und UE4/Frostbite-Kursnotizen
  liefern Vergleichsmodelle. GPU-Bild, Rauheitsverlauf, Energie und Kosten entscheiden.

## Verfahren
### Gemeinsamer Look
- Default ist minimalistischer Solarpunk: klare Volumen/Raster aus Bauhaus, gezielte
  gestufte Formen/Reliefs und rhythmische Akzente aus Art déco. Wenige Materialfamilien,
  konsistente Proportionen und gezieltes Detail statt gleichförmiger Zufallsdekoration.
  Belegte Gebäudeform/-farbe bleibt maßgeblich; kein weltweiter Stilumbau realer Orte.
- Grau/Beige für Beton/Putz, warme Erdtöne, abgestufte Grüntöne und ruhige Blautöne als
  gemeinsame Palette aller Generatoren. Varianten folgen demselben Materialkatalog (2171).
  Höhe/Umfang eines Details folgt seiner Bildwirkung; Silhouetten nicht pauschal vereinfachen.
- Palette bei Vorbereitung in OKLCH über Helligkeit, Chroma und Farbton abstimmen;
  Flächenanteil, Hell-Dunkel-Hierarchie und wenige Akzente zusammen beurteilen. Wahrnehmungs-
  abstand ist kein Schönheitsbeweis. Kompatibilitätsmodelle liefern Vergleichshypothesen.
  Für Licht/Materialmischung in lineares RGB überführen, keine Paletteoptimierung je Fragment.
- Ein gemeinsamer HDR-/Belichtungs-/Ausgabepfad hält Materialien zusammen. Kein nachträgliches
  Einfärben einzelner Objekte; Nässe, Nacht und Jahreszeit bleiben physikalisch plausibel.

### Licht und Kamera
- Kamera gegen Landmarken/Relief kalibrieren; falsche Gebäude nicht durch Pose kaschieren.
  Gerichtete Schatten nach Bildwirkung staffeln, Kontakt und Fernwelt erhalten. Lokale
  Lichter/Schatten bündeln; Himmel füllt Schatten ohne globale Überbelichtung.
- Roughness-gefilterte Weltreflexion mit Sichtbarkeit und vollständigen Mips; Wasser nutzt
  dieselbe Lichtwelt. Emissive Fenster/Straßenlichter fern als kompakte Beiträge, keine
  Detail-/Schattenarbeit je Fenster. Bloom ersetzt keine Geometrie.
- HDR/Farbraum/Belichtung zeitlich stabil; konsistente Tiefe/Bewegung und Frame-Ursprung.
  Disocclusion, neue Produkte und Ursprungswechsel invalidieren betroffene History.
- Arbeit pro tatsächlich benötigtem Pass/Extent; keine ungenutzten Renderressourcen.
  Render-/Ausgabemaß und Zielrate getrennt; Profile aus AGENTS, kein stiller Qualitätswechsel.
- SDL-Submission/Ressourcenwechsel auf zuständigem Thread, begrenzte Frames in Flight,
  Deadline-/Event-Warten statt Busy-Wait. OS erhält CPU-Zeit, GPU-Freigabe nach letzter Nutzung.

## Forschungsgrundlage
[Color Compatibility](../doc/references/presentation/siggraph/2011-color-compatibility.pdf)
und [Oklab/OKLCH](https://bottosson.github.io/posts/oklab/): gemeinsame Palette vorbereiten,
in Stadt-/Bergbildern und bei wechselndem Licht prüfen. Kein universelles Harmoniegesetz.
[Frostbite-PBR](../doc/references/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf),
[TAA-Übersicht](../doc/references/presentation/cgf/2020-temporal-antialiasing-survey.pdf)
([Primärquellen/Einordnung](../doc/references/README.md)): Lichtgrößen/IBL/Belichtung zusammen
kalibrieren. Schatten nach projizierter Wirkung staffeln; Bias gegen Kontaktverlust prüfen.
History anhand Tiefe, Bewegung und Produktgültigkeit validieren; flimmerfreie Unschärfe ist
kein Bildgewinn. Bewegtes Wasser/Laub und Ursprungssprünge gesondert integrieren.
[Moment Shadow Mapping](../doc/references/lighting/i3d/2015-moment-shadow-mapping.pdf):
zuerst stabile Tiefenprojektion/Bias und begrenztes PCF; Momente nur bei belegtem Gesamtgewinn
einschließlich Blur/Mips/Bytes. Autoren-Errata anwenden. 2048² × 8 Byte = 32 MiB allein für Momente.
[Clustered Shading](../doc/references/lighting/hpg/2012-clustered-shading.pdf):
bei vielen lokalen Nachtlichtern räumliche Listen, Sonne separat; Überlauf explizit behandeln.
[Screen-Space-DDA](../doc/references/lighting/jcgt/2014-efficient-screen-space-rays.pdf):
IBL bleibt Grundreflexion, gültige SSR-Treffer ergänzen sie; Step-Limit, Tiefe/Dicke und
Disocclusion prüfen. Spiegelung darf bei Kameradrehung nicht einfach verschwinden.

## Abnahme
Datierte klare/bedeckte Stadt-/Bergbilder gewinnen Tiefe und Materiallesbarkeit; Morgen,
Abend und Nacht erhalten plausible Helligkeit. Bewegung ohne Geisterbilder/Belichtungssprünge.
Host/GPU/Bytes getrennt messen, kein Geräteversprechen aus Desktop-Messungen.
Beton, Boden, Vegetation und Himmel wirken als zusammenhängender Look; Detailverzicht
erhält Charakter/Lesbarkeit. Palette und Kontakt unter Sonne, Wolken, Nässe und Nacht prüfen.
