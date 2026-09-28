Type: feature
State: open
Architecture: planned
Priority: P2
Parent: 2169
Area: generators, assets
Tags: webcam, measured
Depends:

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

## Implementierte Eingabe-/Wachstumsverträge

TreeSpecies-Parse und direkte TreeLeaf-Aufrufe prüfen endliche Zahlen, Formen, Typen,
Konversionen und abgeleitete Kosten vor Allokation. Keine String/Null-Defaults oder
Bruchzähler; uint32-Seed ohne int-Zwischenschritt. Fehler erhalten vorherige Art/Geometrie.
TreeLeaf-Build liefert expected und publiziert nur vollständige Kandidaten; Prototype
und GeometryAt reichen Fehler weiter. Attribute endlich, Einheitsnormalen erforderlich.

| Grenze [SET], kein gemessenes Gesamtspeicherbudget | Vertrag |
|---|---|
| Blatt | 4..128 Segmente; 0..16 Leaflets, 0 verwendet fünf; 8192 Vertices/32768 Indizes vor LOD |
| Broad/Pinnate | 3*(n+1) Vertices, 12*n Indizes; Pinnate Faktor 2*Leaflets+1 plus Achse 4/6 |
| Palmate/Needle | sechs Ringe / höchstens 180 Nadeln |
| Blattnutzdaten | 8192*8*4 = 256 KiB plus 32768*4 = 128 KiB; Kapazität/Scratch/Altstand/Instanzen zusätzlich |
| Wachstum | 64 Leader/Whorlzweige/Trunkseiten; 4096 Trunkschritte/Whorlabstand; Order 0..8 |
| Anteile | Bole/Break/OrderLen in [0,1], keine unzulässige Schritt-Konversion |

Historisch 474 Eingabechecks, drei Regressionen und alle 31 Profile grün. Grenz+1,
INT_MAX, NaN/Inf, Nullspreizung, Formen/LOD, Recovery und volle Seeds bleiben Orakel.
Keine Aussage zu Kosten maximaler Eingaben; Rinde/OOM unter 2194/2209.

GrowOnce trennt Queue, Einzeltrieb, Richtung, Verzweigung und Abschluss. Spawn darf
Queue reallokieren; Tip bleibt lokale Kopie, Pass-Kontext geliehen. FrameFrom verwendet
zwei Kreuzprodukte. Historisch 180 Checks; max |dot(Dir,Up)| 0.0053 → 1.23e-7 bei
459253 Nodes, 31 Profile. Reine Phasentrennung bewahrte 49540826 Snapshot-Bytes;
Tannen-GLB über öffentlichen Client pixelgleich und PNGs geöffnet.
Unbefriedigende Tannensilhouette bleibt; keine Arten-/Rinden-/Lichtqualitätsabnahme.
Queue-/Scratch-Gesamtbudget und atomare Grower-Fehlerpublikation bleiben offen.

## Priorität statt Scheindependenz

P2 nach 2111s funktionierendem nativen Waldkern. Vorhandene Prototyp-/Materialverträge
tragen isolierte Artenarbeit; weder komplette Weltstreaming-Abnahme 2111 noch alle
Materialfamilien 2171 sind technische Startblocker. Wetter/Phänologie benötigt später
2172s konkreten Snapshot-Vertrag, keine eigene Wetterinterpretation.
