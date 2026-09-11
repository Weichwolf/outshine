Type: feature
State: active
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

Behobene Ursache: ungeprüfte Blattzahlen führten zu int-Überlauf vor size_t-
Konversion, unbeschränkter Vervielfachung und Konversion vor Clamp. Nullspreizung
lieferte NaN-Geometrie; Build konnte keinen Fehler melden und löschte den Altstand.

Vorhanden: zusammenhängende Meshpuffer, native Geometry-Übernahme, parametrische
Blätter und unabhängiger baryzentrischer Fehler-/Flächentest. Diese erhalten.
Implementiert: ein gemeinsamer geprüfter Blattvertrag für JSON und direkte Aufrufe;
endliche fachliche Parameter, bekannte Form, begrenzte Stationen/Teilblätter und
abgeleitete Vertex-/Index-/Scratchkosten vor Allokation. Budgets ausdrücklich als
Enginegrenzen setzen und am vorhandenen Corpus prüfen; keine stillen Ersatzwerte.
Größen erst nach Prüfung konvertieren, Überläufe vor Multiplikation ausschließen.
LOD-Abstand und Flächenbudget als benannte Optionen statt austauschbarer float-Argumente.
Build liefert nodiscard expected und publiziert nur vollständige Kandidaten.
TreePrototype-Aufbau und beide GeometryAt-Pfade reichen Fehler weiter; leere Blattnetze
sind kein Ersatz für eine abgelehnte Geometrie. Keine pauschalen noexcept-Zusagen.
Verfahren: bestehende Kandidatenpublikation aus TreeSpecies::Parse und geprüfte
uint32-Kapazität aus TreeGeometry erweitern; kein neuer Morphologiealgorithmus.

Abnahme: alle Formen, kleinstes/größtes Budget, Budget+1, INT_MAX, NaN/Inf,
Nullspreizung, unbekannte Form und ungültige LOD-Optionen; Fehler erhalten alte
Vertex-/Indexdaten. Entfernte Validierung muss diese Tests brechen. Corpus bleibt
ladbar, gültige Blattnetze behalten Positionen/Normalen/UVs/Indizes. Geänderte gültige
Geometrie verlangt Place-PNG-Vergleich; reine Eingabeablehnung ist keine Bildabnahme.
Rinde/Wachstumsbudgets sind dadurch noch nicht abgesichert; WI 2194/2209 bleiben offen.

Gewählte Blattbudgets [SET]: 4..128 Segmente, 0..16 Leaflets (0 verwendet fünf),
8192 Vertices und 32768 Indizes vor LOD. Broad: 3*(n+1) Vertices, 12*n Indizes;
Pinnate multipliziert mit 2*Leaflets+1 und addiert 4/6 für die Achse. Palmate hat
sechs Ringe, Needle höchstens 180 Nadeln. Nutzdaten maximal 256 KiB Vertexwerte
(8192*8*4) plus 128 KiB Indizes (32768*4); Containerkapazität, Scratch, alter Stand
und Instanzexpansion kommen hinzu. Kein gemessenes Frame-/Gesamtspeicherbudget.
Shape-Validierung und expected-Publikation sind implementiert; erzeugte Attribute
müssen endlich, Normalen einheitlich sein. OOM-Vertrag bleibt in 2194/2209.

## Numerische Artdeklaration vor Wachstum
Vor Parse-Publikation werden alle gelesenen Zahlen typ- und darstellbarkeitsgeprüft; kein
String/Null als Default, keine Brüche für Zähler, keine float-/int-Überläufe. Seed
bleibt uint32 ohne int-Zwischenschritt; bisherige Blatt-Zahlenprüfung konsolidiert.
Wachstumszähler erhalten explizite Enginegrenzen: 64 Leader/Whorlzweige/Trunkseiten,
4096 Trunkschritte/Whorlabstand, Order 0..8. Bole-/Break-Anteile und OrderLen in [0,1]
verhindern unzulässige Schritt-Konversionen. 31 Profile bleiben zulässig; keine
Änderung gültiger Wachstumsarithmetik. Fehler behalten vorherige Art und Definition.
Abnahme: Typen, Grenzen, Brüche, sehr große Zahlen, voller Seedbereich, Recovery und
Corpus. Queue-/Blatt-/Scratch-Gesamtbudget, Grower-Publikation und abgeleitete Float-
Überläufe bleiben offen; sichere Parse-Repräsentation beweist kein Echtzeitbudget.
474 Checks sowie drei Blatt-/Wachstums-/Materialregressionen grün; Altimplementierung
scheitert. Alle 31 Profile akzeptiert. Keine Aussage zu Gesamtkosten maximaler Eingaben.
