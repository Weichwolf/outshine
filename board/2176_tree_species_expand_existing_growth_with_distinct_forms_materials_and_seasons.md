Type: feature
State: open
Parent: 2169
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

## Blattgenerator: Eingabevertrag vor Erweiterung

TreeSpecies liest Blattzahlen bisher ohne Bereichsprüfung; unbekannte leaf_kind
werden still Broad. TreeLeaf::Build löscht das alte Ergebnis vor jeder Prüfung.
BuildBlade berechnet (Segments + 1) * 3 in int, BuildPalmate 1 + 6 * (Segments + 1):
Überlauf vor der size_t-Konversion. Pinnate vervielfacht die Ausgabe nach Leaflets.
BuildNeedleShoot konvertiert Length / 0.0085 nach int vor dem Clamp auf 44..180;
Clamp nach einer nicht darstellbaren Konversion schützt nicht. PalmateSpread == 0
führt über pi/2/spread und 0 * inf zu NaN-Geometrie. uint32-Indizes sind ungesichert.

Vorhanden: zusammenhängende Meshpuffer, native Geometry-Übernahme, parametrische
Blätter und unabhängiger baryzentrischer Fehler-/Flächentest. Diese erhalten.
Entscheidung: ein gemeinsamer geprüfter Blattvertrag für JSON und direkte Aufrufe;
endliche fachliche Parameter, bekannte Form, begrenzte Stationen/Teilblätter und
abgeleitete Vertex-/Index-/Scratchkosten vor Allokation. Budgets ausdrücklich als
Enginegrenzen setzen und am vorhandenen Corpus prüfen; keine stillen Ersatzwerte.
Größen erst nach Prüfung konvertieren, Überläufe vor Multiplikation ausschließen.
LOD-Abstand und Flächenbudget als benannte Optionen statt austauschbarer float-Argumente.
Build liefert nodiscard expected und publiziert nur vollständige Kandidaten.
TreePrototype-Aufbau und GeometryAt müssen Fehler weiterreichen; leere Blattnetze
sind kein Ersatz für eine abgelehnte Geometrie. Keine pauschalen noexcept-Zusagen.
Verfahren: bestehende Kandidatenpublikation aus TreeSpecies::Parse und geprüfte
uint32-Kapazität aus TreeGeometry erweitern; kein neuer Morphologiealgorithmus.

Abnahme: alle Formen, kleinstes/größtes Budget, Budget+1, INT_MAX, NaN/Inf,
Nullspreizung, unbekannte Form und ungültige LOD-Optionen; Fehler erhalten alte
Vertex-/Indexdaten. Entfernte Validierung muss diese Tests brechen. Corpus bleibt
ladbar, gültige Blattnetze behalten Positionen/Normalen/UVs/Indizes. Geänderte gültige
Geometrie verlangt Place-PNG-Vergleich; reine Eingabeablehnung ist keine Bildabnahme.
Rinde/Wachstumsbudgets sind dadurch noch nicht abgesichert; WI 2194/2209 bleiben offen.
