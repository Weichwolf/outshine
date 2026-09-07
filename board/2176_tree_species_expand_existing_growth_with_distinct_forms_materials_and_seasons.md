Type: feature
State: open
Area: generators, assets
Tags: webcam, measured
Depends: 2111, 2171

# Tree species expand existing growth with distinct forms, materials and seasons

## Vorhanden und Auftrag

Kein neuer Baumgenerator. `src/generators/flora/` enthält TreeGrower, TreeMesher,
TreeFoliage, TreeLeaf, TreePrototype und TreeSpecies. `src/assets/world/species/` enthält
31 JSON-Profile einschließlich Hecken, Sträuchern und Totholzvarianten; das sind nicht
31 unterschiedliche Baumarten. Growth/Leaf/Shading/Form sind bereits parametriert.
TreeSpecies::Shading hat Rindenfarbe/-Relief, Blatttint und Wind, aber keinen vollständigen
artspezifischen Metallic-Roughness-Vertrag. 2111 besitzt den fehlenden sichtbaren Placement-Pfad.

## Ausbau

1. Vorhandene Profile inventarisieren: botanischer Name, Wuchsform, Kronensilhouette,
   Blatt-/Nadelmorphologie, Rinde, Altersbereich und tatsächlich verwendete Regionsregel.
   Fehlende Formen ergänzen statt JSONs mit bloß anderer Farbe duplizieren.
2. Corpus zuerst: alpine Lärche/Legföhre und differenzierte Fichte/Tanne; mediterrane
   Zypresse, Schirmkiefer, Olive und immergrüne Eiche; städtische Platane sowie deutlich
   unterschiedliche Pappel-/Linden-/Ahorn-/Kastanienformen. Vorhandene Kiefer/Pappel sind
   zu spezifizieren, nicht unter anderem Namen doppelt anzulegen. Danach generische
   Formenfamilien für boreale, gemäßigte, trockene und tropische Regionen erweitern.
   Dies ist ein Ausbauvorschlag, keine botanische Identifikation einzelner Webcam-Bäume.
3. Art-/Altersparameter beeinflussen Stammverjüngung, Verzweigungsordnung/-winkel,
   Astquirle, Kronenbreite/-dichte, Blattstellung und Selbstbeschattung. Standortsbedingte
   Variation für Solitär/Wald/Allee/Schnittform, reproduzierbarer individueller Seed.
4. Rinde, Holz, Blattober-/unterseite, Nadeln und Totholz erhalten unterschiedliche
   dielektrische MR-Materialien aus 2171; Normal-/Roughness-Struktur in Weltmetern,
   Blatt-Coverage/DoubleSided und dünne Transmission ohne Metallglanz.
5. Phänologie: Austrieb, Sommer, Herbstfärbung, laublos, immergrün und sommergrüne Nadelbäume;
   Klima/Höhe/Breitengrad/Datum statt globalem Monats-Bool. Feuchte/Schnee/Wind aus 2172.
   Ohne Artenkarte plausible regionale Mischungen; OSM species/genus/leaf_type respektieren.
6. Prototypcache/Instancing und artspezifische LOD erhalten Silhouette/Kronen-Coverage;
   Ferndarstellung darf Fichte und Zypresse nicht zum gleichen Blob machen.

## Abnahme

- [ ] Artenatlas aus dem vorhandenen Generator: ganze Pflanze, Verzweigung, Rinde, Blatt,
      Jung/Alt und Jahreszeiten unter gleicher kalibrierter Beleuchtung; PNGs visuell öffnen.
- [ ] Unterscheidbarkeit über Kronenform und Morphologie, nicht nur Farbe; botanische
      Parameter vor Umsetzung mit benannten fachlichen Quellen belegen. Keine Zahl neuer
      JSON-Dateien als Qualitätsoracle.
- [ ] Koerbersee/Feldkirch/Wien/Olympiaturm/Malcesine erhalten plausible verschiedene Bestände;
      kein Anspruch auf genaue reale Artenverteilung. Andere Weltregion als Transferprobe.
- [ ] Dichteleiter mit 2092 messen; Mutation sämtlicher Arten zur selben Form muss das
      Morphologieoracle verletzen. Seed-/Tile-Reentry und saisonale Materialänderung prüfen.

Wahl: vorhandenen parametrischen Wachstumsbau erweitern; Unreal/RAGE sind visuelle
Vegetationsbenchmarks. Art, Material und Standort bleiben Daten hinter derselben Generator-API.
