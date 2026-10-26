# Forschungsgrundlage für Outshine

Stand: 2026-10-27. Primärquellen: Autoren, Forschungsgruppen, Kursveranstalter und Normgeber.
Papers, Kurse und Produktdokumentation sind als solche bezeichnet; Links und lokale PDFs bleiben erhalten.
Die Einträge sind die kanonische Relevanzbewertung nach [#919](https://github.com/Weichwolf/outshine/issues/919),
keine Machbarkeitsbestätigung und keine neue Architekturfreigabe.

**Prüfstand: 70 von 71 Einträgen bewertet.** Alle Bereiche sind eingeordnet;
ein Normalenartikel bleibt wegen fehlender Volltext-/Abbildungsprüfung ausdrücklich
unter „Bewertung ausstehend“, ohne Relevanzurteil.
Bewertete Einträge folgen absteigend dem aktuellen Nutzen innerhalb jeder Stufe:

| Relevanz | Bedeutung |
|---|---|
| Hoch | Direkt benötigtes Verfahren für aktuelle Engpässe oder Bildlücken |
| Mittel | Nützlicher Baustein mit konkretem Anpassungs- oder späterem Integrationsbedarf |
| Gering | Spezialfall oder gemessen ungünstiger Ansatz im aktuellen Pfad |
| Nicht relevant | Veröffentlichtes Verfahren passt nicht zu Eingaben, Ziel oder Kosten |

Maßstab sind vollständige bewegte Welten bis 240 km, soweit physikalisch sichtbar,
bei 1280×720/60 einschließlich p99 auf A18 Pro/Metal und i5-1135G7/Iris Xe/Vulkan ([#5](https://github.com/Weichwolf/outshine/issues/5)).
Beide Ziele nutzen schnelle SSD und Unified Memory: kleine Rundum-Metadaten → Sicht-/Spielbedarf
→ passende unabhängig lesbare fertige Produkte. Eltern bleiben bis vollständiger Kindabdeckung.
Render-, Funktions- und Simulationsbedarf bleiben getrennt ([#10](https://github.com/Weichwolf/outshine/issues/10),
[#12](https://github.com/Weichwolf/outshine/issues/12), [#915](https://github.com/Weichwolf/outshine/issues/915),
[#2](https://github.com/Weichwolf/outshine/issues/2)); verdeckte Schatten-/Spielwirkung zählt weiterhin.
IO-Latenz/Batchgröße, Decode, Kopien, geteilte Bandbreite, physische Residency und GPU-/Updatearbeit
messen; gemeinsame Seiten einmal zählen, Zero-Copy nicht voraussetzen. Historische HDD-/VRAM-
Modelle und publizierte Zeiten nicht als aktuelle Gerätebelege übernehmen.

Der Bedarfsfilter verwirft unbenötigte Nutzdaten vor Laden/Decode und GPU-Nutzung;
passende fertige LODs begrenzen danach räumliche Frequenzen und Arbeit. Coverage,
mittlere Lichtantwort und Kontakte erhalten; erforderliche Interaktionsdaten separat laden.
A18 Pros 8 GB sind der geteilte Systemspeicher, kein Assetbudget. Physische Peaks samt
komprimierten/entpackten Daten, Übergängen, Renderzielen und Scratch zählen; gemeinsame
Seiten einmal erfassen. Spätes Aussortieren gibt den vorherigen Ladepeak nicht zurück.

## Gemeinsamer Engine-Referenzkern

Die fünf Grundlagen tragen unterschiedliche Schritte derselben Pipeline; weitere
Einträge bleiben gezielte Fachreferenzen. Auswahl, Original-/Abbildungsprüfung und
Abstimmung: [#1225](https://github.com/Weichwolf/outshine/issues/1225#issuecomment-6410933180).
Sie begründen Verfahren, keine vollständige Bestands- oder Geräteabnahme.

| Schritt | Grundlage und übernommener Baustein | Grenze |
|---|---|---|
| Fertige räumliche Produkte | [Unreal DDC / World Partition / HLOD](engine/unreal/prepared-assets.md): regenerierbare Produkte, Zellen und vorbereitete Proxies | Runtime-Miss→Generator ist unsere Anpassung; dauerhafte Rezepte/Weltentscheidungen separat sichern |
| Bedarf vor Content-IO | [Cesium Native Selection](engine/visibility-driven-demand.md#cesium-native-selection): Hierarchie/SSE und verfügbare Ersatzabdeckung | Fallback und Rundumabdeckung explizit wählen; geometrischer Fehler ist kein Material-/Funktionsfehler |
| Residente Renderauswahl | [GPU-Driven Rendering Pipelines 2015](geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf): Instanz-/Clusterwahl, Kompaktion, indirekte Submission | Spart vorheriges IO/Decode/Upload nicht automatisch; gemeinsame Metal-/Vulkan-Verträge |
| Bewegliches Höhenfeld | [Geometry Clipmaps 2004](terrain/siggraph/2004-geometry-clipmaps.pdf): toroidale Wiederverwendung und begrenzte Randupdates | Bei Überlast fällt Feindetail zurück; kein allgemeines Gebäude-/Transport-LOD |
| Physikalische Bildantwort | [Frostbite PBR, Revision 3](lighting/siggraph/2014-frostbite-pbr-course-notes.pdf): Material-/Licht-/Kameraeinheiten und unabhängiger Referenzvergleich | Geometrie-SSE ersetzt keine Materialfehlerprüfung; historische Pass-/Pufferkosten neu messen |

Worldpublikation, Simulation, Audio, JS und Persistenz-Lebensdauer besitzen
[#1230](https://github.com/Weichwolf/outshine/issues/1230),
[#1231](https://github.com/Weichwolf/outshine/issues/1231) und
[#1232](https://github.com/Weichwolf/outshine/issues/1232); die fünf Quellen
decken sie nicht vollständig ab. Generatoren bleiben ein breites Forschungsfeld,
ohne vorgeschriebene Solver-/Grammatikfamilie. Refactor und gemeinsame Abnahme
der gesamten aktuellen Codebasis gehen neuer Featureintegration voraus (#1233/#1320).

## PDFs und Nutzung

Alle vorhandenen PDFs liegen nach Bereich/Konferenz direkt unter `doc/references/` und sind versioniert.
Die kanonischen Downloadlinks stehen bei jeder Quelle. Fehlende Volltexte sind im Eintrag benannt.
Kein Fremdcode, Texturpaket oder Trainingsdatensatz wird übernommen. Optisch geprüfte Seiten und
terminale Doc-Prüfungen stehen beim jeweiligen Bewertungs-PR; Diagnosebilder bleiben lokal.

Fachbesitz: [Vegetation #1](https://github.com/Weichwolf/outshine/issues/1),
[Transport #11](https://github.com/Weichwolf/outshine/issues/11),
[Straßen/Wasser #3](https://github.com/Weichwolf/outshine/issues/3),
[Terrain #13](https://github.com/Weichwolf/outshine/issues/13),
[Kontakte #14](https://github.com/Weichwolf/outshine/issues/14).
Die Relevanzgruppen nennen weitere konkrete Verbraucher am jeweiligen Eintrag.

## Relevanz: Hoch

- **GigaVoxels: Ray-Guided Streaming for Efficient and Detailed Voxel Rendering**,
  Crassin et al., I3D 2009.
  [Quelle](https://www-sop.inria.fr/reves/Basilic/2009/CNLE09/CNLE09.pdf) ·
  [PDF](geometry/i3d/2009-gigavoxels-ray-guided-streaming.pdf).

  - **Relevanz: Hoch — Streaming / #10, #12, #915.**
  - **Einsatz:** Sicht-/LOD-Traversierung; Requests erst bei Bedarf; Elternersatz; gebündelte
    Misses; Auswahl vor Fine-IO/Decode.
  - **Grenzen:** Keine Voxelwelt-Entscheidung; Feedback-/Baumzugriffs-/Miss-Kosten; historische
    HDD-/VRAM-Stufen nicht auf SSD/UMA übertragbar.

- **Conservative Visibility Preprocessing using Extended Projections**, Durand / Drettakis /
  Thollot / Puech, SIGGRAPH 2000.
  [Publikation](https://doi.org/10.1145/344779.344891) ·
  [Autoren](https://people.csail.mit.edu/fredo/PUBLI/Sig2000/index.htm) ·
  [PDF](geometry/siggraph/2000-conservative-visibility-extended-projections.pdf).

  - **Relevanz: Hoch — Verdeckung / #12, #915.**
  - **Einsatz:** Konservative Kamerazell-Verdeckung vor Detail-IO; Occluder-Innenprojektion,
    Ziel-Außenprojektion, konservative Tiefe.
  - **Grenzen:** Nur qualifizierte opake Flächen; keine AABB-/Lochschluss-/veralteten
    Bewegungsoccluder; Vorberechnung, Zellwechsel, Invalidierung; unbekannt bleibt sichtbar.

- **Geometry Clipmaps: Terrain Rendering Using Nested Regular Grids**, Losasso / Hoppe,
  SIGGRAPH 2004. [Quelle](https://hhoppe.com/geomclipmap.pdf) ·
  [PDF](terrain/siggraph/2004-geometry-clipmaps.pdf).

  - **Relevanz: Hoch — Terrain / #13, #10, #915.**
  - **Einsatz:** Verschachtelte Gitter; toroidale Wiederverwendung; grob-vor-fein; begrenzte
    Randupdates bei Bewegung; unabhängig lesbare Höhen-/Materialstufen.
  - **Grenzen:** SSD-Latenz, Decode, UMA-Schreibbandbreite; Höhenfeld statt allgemeinem
    Gebäude-/Straßen-LOD; keine automatische Krümmungs-/Verdeckungs-/Fehlergarantie.

- **Simplification Envelopes**, Cohen et al., SIGGRAPH 1996.
  [Quelle](https://www.cs.umd.edu/gvil/papers/simp_env.pdf) ·
  [PDF](geometry/siggraph/1996-simplification-envelopes.pdf).

  - **Relevanz: Hoch — Formfehler / #12, #14, #915.**
  - **Einsatz:** Begrenzende Hüllen; überprüfbarer räumlicher LOD-Fehler; Eltern-/Kindauswahl und
    Kontaktverträglichkeit.
  - **Grenzen:** Topologie-/Regularitätsannahmen; Aufbaukosten; Distanznähe allein weder sicherer
    Occluder noch Befahrbarkeit.

- **GPU-Driven Rendering Pipelines**, Haar / Aaltonen, SIGGRAPH-Kurs 2015.
  [Quelle](https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf) ·
  [PDF](geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf).

  - **Relevanz: Hoch — Renderauswahl / #48, #844, #12.**
  - **Einsatz:** Instanz-/Cluster-Culling; kompakte Listen; indirekte Submission; grobe vorhandene
    Occluder als Bedarfshilfe.
  - **Grenzen:** GPU-Culling nach Upload spart kein Fine-IO; Tiefenpass, Reprojektion, Kompaktion,
    Änderungen auf Metal/Vulkan messen; alte PS4-Zahlen/unsichere History keine SSD-/UMA-Freigabe.

- **Procedural Modeling of Buildings**, Müller et al., SIGGRAPH 2006.
  [Quelle](https://peterwonka.net/Publications/pdfs/2006.SG.Mueller.ProceduralModelingOfBuildings.final.pdf) ·
  [PDF](buildings/siggraph/2006-procedural-modeling-of-buildings.pdf).

  - **Relevanz: Hoch — Gebäudeplan / #8, #33, #915.**
  - **Einsatz:** Hierarchischer Formplan: Grundriss, Körper, Dach, wiederholte Module; gemeinsame
    Stufen; Vielfalt ohne Einzelgeometrie je Instanz.
  - **Grenzen:** Gelieferte Klasse/Höhe plus begrenzte Ergänzungen; kein vollständiger
    CGA-Interpreter nötig; Hierarchie allein ohne lesbare LOD-Seiten-/Formfehlergarantie.

- **Grammar-based Encoding of Facades**, Haegler et al., EGSR 2010.
  [Quelle](https://peterwonka.net/Publications/pdfs/2010.EGSR.Haegler.GrammarBasedEncoding.pdf) ·
  [PDF](buildings/egsr/2010-grammar-based-encoding-of-facades.pdf).

  - **Relevanz: Hoch — Fassaden / #6, #8, #33.**
  - **Einsatz:** Kompakte Fassadenregeln; wiederholte Elemente; eigene prozedurale Produkte;
    Paper-Dekodierung pro Pixel mit Zusatzpuffern.
  - **Grenzen:** Regeltiefe, Divergenz, Scratch, Filterung; Nahsilhouette/Laibungen/Räume als
    Geometrie; keine Fotofassaden oder Übernahme des CUDA-/VRAM-Pfads.

- **Synthetic Texturing Using Digital Filters**, Feibush / Levoy / Cook, SIGGRAPH1980.
  [Quelle](https://graphics.stanford.edu/papers/texture-filtering/) ·
  [PDF](materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf).

  - **Relevanz: Hoch — Pixelfilter / #6, #306, #899.**
  - **Einsatz:** Gemeinsame Integration von projizierter Pixelfläche, sichtbarem Polygonanteil und
    Texturfilter; Coverage-/Subpixelreferenz; unabhängiges Polygon-Clipping-Orakel.
  - **Grenzen:** Kein historischer Scanline-/Dateiladepfad; Referenzkorrektheit ohne Beleg für
    günstigen Runtime-Filter oder unabhängig ladbare Fernprodukte.

- **Frequency Domain Normal Map Filtering**, Han / Sun / Ramamoorthi / Grinspun,
  SIGGRAPH / TOG 2007. [Autorenquelle](https://www.cs.columbia.edu/cg/normalmap/) ·
  [PDF](vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf).

  - **Relevanz: Hoch — Normalenfilter / #6, #422, [#44](https://github.com/Weichwolf/outshine/issues/44).**
  - **Einsatz:** Normalenverteilung statt Mittelrichtung; Glanzstruktur bei Verkleinerung;
    Blatt-/Relief-Mips und Fernmaterialien.
  - **Grenzen:** SH-/vMF-Koeffizienten, Fits, Sampling; eigene Fehlerbelege für schmale/mehrgipflige
    Verteilungen und korrelierte Farbe/Materialien; kein universeller kleiner GGX-Ersatz.

- **Realistic Modeling and Rendering of Plant Ecosystems**, Deussen et al., SIGGRAPH 1998.
  [Quelle](https://algorithmicbotany.org/papers/ecosys.sig98.pdf) ·
  [PDF](vegetation/siggraph/1998-plant-ecosystems.pdf).

  - **Relevanz: Hoch — Bestände / #1, #12, #915.**
  - **Einsatz:** Bestandsplan, Pflanzenform, Darstellung getrennt; geteilte Prototypen/Instanzen;
    kompakte Bestands-/Fernprodukte vor Nahdetail.
  - **Grenzen:** Quantisierung/Artenmischung gegen Deckung/Lichtantwort; Offline-Bestand kein
    Echtzeitbeleg; Wind, Schatten, Overdraw, Übergangspeaks.

- **A Scalable and Production Ready Sky and Atmosphere Rendering Technique**, Hillaire,
  EGSR 2020. [Quelle](https://sebh.github.io/publications/egsr2020.pdf) ·
  [PDF](atmosphere/egsr/2020-production-ready-atmosphere.pdf).

  - **Relevanz: Hoch — Atmosphäre / #7, #4.**
  - **Einsatz:** Kompakte Transmission-/Mehrfachstreuungs-/Himmels-/Luftperspektiv-LUTs; Boden,
    Flug, Orbit; vorhandene SkyStage; physikalisch begrenzte Fernsicht.
  - **Grenzen:** LUT-Auflösung/Updates und Sonnen-/Geländeschatten gegen Referenz; approximierte
    Streuung; Wetterparameter weder gemessene Aerosole noch Wolkenlösung.

- **Surface Gradient-Based Bump Mapping Framework**, Mikkelsen, JCGT 9(3), 2020.
  [Quelle](https://jcgt.org/published/0009/03/04/paper.pdf) ·
  [PDF](materials/jcgt/2020-surface-gradient-bump-mapping.pdf).

  - **Relevanz: Hoch — Bump-Komposition / #6.**
  - **Einsatz:** Gemeinsamer Gradientenraum für UV-/Projektionsrelief; einmalige
    Normalenrekonstruktion; vorhandene Materialkomposition.
  - **Grenzen:** Lokalkonstante Triplanar-Gewichte als Näherung; keine
    Silhouetten-/Kontakt-/Normalenvarianz-/GGX-Filterlösung; Detailtextur-/Samplebudget.

- **Moving Frostbite to Physically Based Rendering**, Lagarde / de Rousiers,
  SIGGRAPH-Kurs 2014, Kursnotizen Revision 3 (2015).
  [Quelle](https://seblagarde.wordpress.com/wp-content/uploads/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf) ·
  [PDF](lighting/siggraph/2014-frostbite-pbr-course-notes.pdf).

  - **Relevanz: Hoch — Lichtkalibrierung / #4, #6.**
  - **Einsatz:** Gemeinsame Kalibrierung von BRDF-Energie, Rauheit, Lichtgrößen, Exposition; flache
    Tag-/Nachtantwort und dunkles Glas; getrennte Referenz-/Runtime-Prüfung.
  - **Grenzen:** Keine unveränderte Passkettenübernahme; IBL, HDR-/Schattenziele und gleichzeitige
    Lebensdauer auf beiden Geräten messen.

- **Real Shading in Unreal Engine 4**, Karis, SIGGRAPH-Kurs 2013.
  [Quelle](https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf) ·
  [PDF](materials/siggraph/2013-unreal-engine-4-shading.pdf).

  - **Relevanz: Hoch — GGX und IBL / #4, #6.**
  - **Einsatz:** GGX; kompakte Materialparameter; vorgefiltertes Environment/Split-Sum; vorhandener
    gemeinsamer Shader; weniger BRDF-Auswertungen durch Parameterblend.
  - **Grenzen:** Parameterblend keine exakte Ergebnismischung; Split-Sum und n=v als Näherungen;
    keine direkten Roughness-Remaps für IBL; Atlas-/Mipbytes und Updates.

- **A Survey of Temporal Antialiasing Techniques**, Yang / Liu / Salvi, CGF / Eurographics 2020.
  [Autoren](https://research.nvidia.com/labs/rtr/publication/yang2020survey/) ·
  [Verlags-PDF](https://diglib.eg.org/server/api/core/bitstreams/53732e70-b64d-46f4-bbae-865eb7673a35/content) ·
  [PDF](presentation/cgf/2020-temporal-antialiasing-survey.pdf).

  - **Relevanz: Hoch — Bewegungsstabilität / #4, #16.**
  - **Einsatz:** Reprojektion, History-Prüfung, Rekonstruktion; LOD-Wechsel, neue Geometrie, Wind,
    Wasser, Ursprungssprünge; ungültige History verwerfen.
  - **Grenzen:** Clamping: Geisterbilder/Detailverlust; zusätzliches RGBA16F bei 1280×720: 7,03125
    MiB reine Texeldaten ohne Padding/weitere Puffer; keine automatische Raster-/Assetersparnis.

- **Clustered Deferred and Forward Shading**, Olsson / Billeter / Assarsson, HPG 2012.
  [Quelle](https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf) ·
  [PDF](lighting/hpg/2012-clustered-shading.pdf).

  - **Relevanz: Hoch — Lokale Lichtauswahl / #4.**
  - **Einsatz:** Räumliche Tiefencluster; kleine Fragmentlisten für tausende lokale Lichter;
    Nacht-/Innenraumlicht; einfacher Pfad bei kleinen Lichtmengen.
  - **Grenzen:** Aufbau, Listenbytes, Überlauf, Änderungen; keine Asset-Verdeckung vor IO oder
    Schattenkarte pro sichtbarem Fenster.

- **Real-time Realistic Rendering and Lighting of Forests**, Bruneton / Neyret, Eurographics 2012.
  [Publikation](https://doi.org/10.1111/j.1467-8659.2012.03016.x) ·
  [Autoren-PDF](https://maverick.inria.fr/Publications/2011/BN11a/article.pdf) ·
  [PDF](vegetation/eg/2012-real-time-forests.pdf).

  - **Relevanz: Hoch — Fernvegetation / #1, #12, #899.**
  - **Einsatz:** Bestandsdeckung und Blick-/Lichtkorrelation für Fernwald; geteilte Prototypen;
    bedarfsabhängige Seeds/Coverage statt Einzelbaum-/Blattarbeit.
  - **Grenzen:** Z-Felder, Tabellen, Artenmischung samt Bytes/Bandbreite; keine ungeprüfte
    Vollübernahme; Wind/Änderungen/Simulation separat; Dichtekarte kein opaker Occluder.

## Relevanz: Mittel

- **Surface Simplification Using Quadric Error Metrics**, Garland / Heckbert, SIGGRAPH 1997.
  [Quelle](https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf) ·
  [PDF](geometry/siggraph/1997-quadric-error-metrics.pdf).

  - **Relevanz: Mittel — Geometrie-LOD / #12, #899, #915.**
  - **Einsatz:** Quadrikpriorisierte lokale Kontraktionen; vorgefertigte Mesh-LODs.
  - **Grenzen:** Paaraggregation kann getrennte Teile verbinden; Zusatzregeln für
    Straßenkontakte/Verdeckung; kein kontinuierlicher Hausdorff-/Coveragebeweis; Fine-Aufbau vor
    Reduktion spart keinen Miss-Cook.

- **Procedural Generation of Roads**, Galin / Peytavie / Maréchal / Guérin, Eurographics 2010.
  [Quelle](https://perso.liris.cnrs.fr/egalin/Articles/2010-roads.pdf) ·
  [PDF](transport/eurographics/2010-procedural-generation-of-roads.pdf).

  - **Relevanz: Mittel — Straßenprofile / #3, #14, #18.**
  - **Einsatz:** Gemeinsame Trajektorie, Querschnitt, Geländeanpassung;
    Brücken/Tunnel/Einschnitt/Aufschüttung als eigene Modelle; funktionale Kontakte und kohärente
    Renderstufen.
  - **Grenzen:** Anisotrope Pfadsuche kein OSM-Netzersatz; Abtastung/Höhenschwellen ohne Garantie
    für Knotenanschlüsse, Durchfahrt oder kreuzungsfreie Geometrie.
  - **Bezüge:** [Codevorbilder und Grenzen](transport/README.md).

- **Interactive Procedural Street Modeling**, Chen et al., SIGGRAPH 2008.
  [Quelle](https://peterwonka.net/Publications/pdfs/2008.SG.Chen.InteractiveProceduralStreetModeling.pdf) ·
  [PDF](transport/siggraph/2008-interactive-procedural-street-modeling.pdf).

  - **Relevanz: Mittel — Transportplan / #11, #18.**
  - **Einsatz:** Graphbearbeitung getrennt von Straßengeometrie; lokale Bearbeitung/Muster als
    Planungsbausteine.
  - **Grenzen:** Tensorfeld-Neuerzeugung kein Ersatz gelieferter OSM-Verbindungen/Ebenen/Profile;
    eigene Brücken-/Tunnel-/Kontaktverträge; unnötiges Vollnetzwachstum vor Fernprodukten.

- **Instant Architecture**, Wonka et al., SIGGRAPH 2003.
  [Quelle](https://peterwonka.net/Publications/pdfs/2003.SG.Wonka.InstantArchitecture.high.pdf) ·
  [PDF](buildings/siggraph/2003-instant-architecture.pdf).

  - **Relevanz: Mittel — Gebäuderezepte / #8, #33.**
  - **Einsatz:** Split-/Kontrollgrammatik; Attributzuordnung; begrenzte Rezeptwahl aus gelieferten
    Körpern/Nutzung; geteilte Fassadenmodule.
  - **Grenzen:** Keine Rekonstruktion unbekannter Fassaden; Regelwahl-/Modulaufwand; Außenform,
    Dach, begehbare Teile im gemeinsamen späteren Gebäudeplan.

- **Mesh Arrangements for Solid Geometry**, Zhou et al., SIGGRAPH 2016.
  [Quelle](https://www.cs.columbia.edu/cg/mesh-arrangements/) ·
  [PDF](buildings/geometry/2016-mesh-arrangements-for-solid-geometry.pdf).

  - **Relevanz: Mittel — Baukörper / #8, #88.**
  - **Einsatz:** Exakte Schnitte; Zell-/Winding-Klassifikation; robuste Baukörper-Booleans;
    Vergleich lokal begrenzter Supportregionen/Überlagerungen.
  - **Grenzen:** Rationale Koordinaten/Arrangement-Aufbau teuer; Ausgaberundung separat prüfen;
    nicht je Fassade/Fernasset erforderlich.
  - **Bezüge:** [#88](https://github.com/Weichwolf/outshine/issues/88).

- **Billboard Clouds for Extreme Model Simplification**, Décoret et al., SIGGRAPH 2003.
  [Autoren](https://graphics.cs.yale.edu/publications/billboard-clouds-extreme-model-simplification) ·
  [PDF-Quelle](https://graphics.cs.yale.edu/sites/default/files/bc03_0.pdf) ·
  [PDF](vegetation/siggraph/2003-billboard-clouds.pdf).

  - **Relevanz: Mittel — Fernrepräsentation / #1, #899.**
  - **Einsatz:** Geometrisch gewählte Ebenen; Kandidat für eigene prozedurale Fernprodukte.
  - **Grenzen:** Ebenenabweichung ohne
    Alpha-/Mip-/GGX-/Tiefen-/Disokklusions-/Schattenfehlergarantie; Texturleerraum/Overdraw,
    Wind-/Lichtänderungen können Meshgewinn aufheben.

- **Maximum Mipmaps for Fast, Accurate, and Scalable Dynamic Height Field Rendering**,
  Tevs / Ihrke / Seidel, I3D 2008.
  [Autorenquelle](https://projects.tevs.eu/www.tevs.eu/files/i3d08_lowres.pdf) ·
  [PDF](terrain/i3d/2008-maximum-mipmaps.pdf).

  - **Relevanz: Mittel — Exakte Terrain-Strahlreferenz / #1237, #515.**
  - **Einsatz:** Maximum-Hierarchie; leere Höhenbereiche überspringen; dynamischer Aufbau;
    Referenz für selektive exakte Abfragen. PDF-Seiten 3/5 optisch geprüft.
  - **Grenzen:** Bilineare Paperfläche statt nativer Dreiecke; LOD-Heuristik keine eigene
    0,5px-Schranke. Lokaler #1237-Gewinn gegen Vollscan kein Renderergewinn: unabhängiger
    Raster billiger; kein direkter nativer Port empfohlen. Hierarchiebytes/Traversal extra;
    alte GPUzeiten kein A18-/Iris-/Welt-/p99-Beleg, keine Fine-IO-Vermeidung.
  - **Bezüge:** [#1239](https://github.com/Weichwolf/outshine/pull/1239) ·
    [#1237](https://github.com/Weichwolf/outshine/issues/1237) ·
    [#515](https://github.com/Weichwolf/outshine/issues/515).

- **LEAN Mapping**, Olano / Baker, I3D 2010, korrigierte Autorenfassung.
  [Autorenquelle](https://userpages.cs.umbc.edu/olano/papers/lean/) ·
  [PDF](vegetation/i3d/2010-lean-mapping.pdf).

  - **Relevanz: Mittel — Filterbare Neigungsstatistik / [#44](https://github.com/Weichwolf/outshine/issues/44), #422.**
  - **Einsatz:** Linear filterbare erste/zweite Neigungsmomente im gemeinsamen Tangentialraum;
    Vergleich vorbereiteter Material-LODs.
  - **Grenzen:** Fünf Momente, Formate/Mips, Rekonstruktion; Beckmann-Gaußmodell/endliche Neigungen;
    kein allgemeiner GGX-/Mikroverdeckungs-/mehrgipfliger Glanzersatz.

- **Layered Depth Images**, Shade / Gortler / He / Szeliski, SIGGRAPH 1998.
  [Publikation](https://doi.org/10.1145/280814.280882) ·
  [Autoren-PDF](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/Shade-SG98.pdf) ·
  [PDF](vegetation/siggraph/1998-layered-depth-images.pdf).

  - **Relevanz: Mittel — Parallaxe / [#23](https://github.com/Weichwolf/outshine/issues/23), #899.**
  - **Einsatz:** Mehrere Tiefentreffer; verdeckte Flächen für neue Ansichten; Vergleich gegen flache
    Mattes.
  - **Grenzen:** Schichtbytes/Sampling; begrenzte Wiederverwendung durch feste Ansicht/Abtastung;
    eigene unbeleuchtete PBR-Felder brauchen Material-/Normalfilter und gültige
    Disokklusionsunterstützung.

- **Relief Texture Mapping**, Oliveira / Bishop / McAllister, SIGGRAPH 2000.
  [Publikation](https://doi.org/10.1145/344779.344947) ·
  [Autoren-PDF](https://www.inf.ufrgs.br/~oliveira/pubs_files/RTM_low_res.pdf) ·
  [PDF](geometry/siggraph/2000-relief-texture-mapping.pdf).

  - **Relevanz: Mittel — Parallaxe / #899.**
  - **Einsatz:** Geometrisch verankerte Tiefe; Prewarp; prüfbarer Baustein eigener Reliefprodukte.
  - **Grenzen:** Ein Höhenwert/Texel ohne beliebige Schichtung; Silhouette/neue Flächen/PBR separat;
    eigener Planarschnitt #917 wegen Bild-/Gesamtkosten verworfen; kleine Pixelparallaxe kein
    Welttiefenbeleg.
  - **Bezüge:** [#917](https://github.com/Weichwolf/outshine/issues/917).

- **Procedural Noise using Sparse Gabor Convolution**, Lagae et al., SIGGRAPH 2009.
  [Quelle](https://graphics.cs.kuleuven.be/publications/LLDD09PNSGC/LLDD09PNSGC_paper.pdf) ·
  [PDF](materials/siggraph/2009-sparse-gabor-noise.pdf).

  - **Relevanz: Mittel — Gerichtete Materialfelder / #6, #13.**
  - **Einsatz:** Steuerbares Spektrum; anisotrope Filterung aus Mapping-Jacobian; Fels/Holz ohne
    große Einzeltexturen; gegebenenfalls fertige Stufen cachen.
  - **Grenzen:** Kernel-/Nachbarschafts-/Ableitungskosten gegen einfachere gefilterte Noise; neue
    Frequenzen durch nichtlineare Farb-/Materialabbildung; kein DEM-/Geologienachweis.

- **Crafting a Next-Gen Material Pipeline for The Order: 1886**, Neubelt / Pettineo,
  SIGGRAPH-Kurs 2013.
  [Quelle](https://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf) ·
  [PDF](materials/siggraph/2013-the-order-material-pipeline.pdf).

  - **Relevanz: Mittel — Materialrezepte / #6.**
  - **Einsatz:** Wiederverwendbare Materialtemplates; Parameter-/Maskenkomposition; prozedurale
    Baustoffe, Schmutz, Nässe.
  - **Grenzen:** Werkzeug-/Foto-/Bake-Pipeline kein Produktvertrag; keine einzigartigen Maps je
    Objekt; Detailkarten, Runtime-Layer, Normalenfilter mit Speicher-/Samplebudget und gültigen
    Übergängen.

- **Physically Based Shading at Disney**, Burley, SIGGRAPH-Kurs 2012.
  [Quelle](https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf) ·
  [PDF](materials/siggraph/2012-disney-physically-based-shading.pdf).

  - **Relevanz: Mittel — Materialfamilien / #6.**
  - **Einsatz:** Gemessene Reflexionen; wenige Parameter; Vergleich von Metall, Dielektrikum,
    Beschichtung, Fasern; vorhandenen GGX-/Energievertrag erhalten.
  - **Grenzen:** Künstlerische Bedienbarkeit/unphysikalische Parameter keine Kalibrierung;
    Zusatzlobes nur für konkrete Oberflächen mit Filter-/Kostenbeleg.

- **Precomputed Radiance Transfer for Real-Time Rendering in Dynamic,
  Low-Frequency Lighting Environments**, Sloan / Kautz / Snyder, SIGGRAPH 2002.
  [Quelle](https://www.microsoft.com/en-us/research/publication/precomputed-radiance-transfer-real-time-rendering-dynamic-low-frequency-lighting-environments/) ·
  [PDF](lighting/siggraph/2002-precomputed-radiance-transfer.pdf).

  - **Relevanz: Mittel — Aggregierte Lichtantwort / #4, #451.**
  - **Einsatz:** Geometrischer Transfer getrennt vom aktuellen niederfrequenten Umgebungslicht;
    vorbereitete Antworten geteilter Prototypen/Fernbestände.
  - **Grenzen:** Samplevektoren/-matrizen, Vorberechnung, Änderungen; formgebundener
    Transfer/räumlich glattes Licht; Verformung, scharfe lokale Schatten, GGX-Spitzen separat.

- **Real-Time Polygonal-Light Shading with Linearly Transformed Cosines**,
  Heitz / Dupuy / Hill / Neubelt, SIGGRAPH / TOG 2016.
  [Autorenquelle und PDF](https://eheitzresearch.wordpress.com/415-2/).
  [PDF](lighting/siggraph/2016-linearly-transformed-cosines.pdf).

  - **Relevanz: Mittel — Flächenlicht und Referenzintegrale / #4, [#422](https://github.com/Weichwolf/outshine/issues/422).**
  - **Einsatz:** Analytisches Kosinus-Randintegral; LTC-Fits für spätere polygonale Flächenlichter.
  - **Grenzen:** BRDF-/Texturlichtfit-Fehler; Horizont-Clipping/Verdeckung zusätzlich; eigener
    GGX-Kernvergleich nutzt nur Randintegration und scheitert an Resttermen: weder Gesamt-LTC
    verwerfen noch Shaderport ableiten.
  - **Bezüge:** [GGX-Kernvergleich](../../test/experiments/render/relief_finite.md).

- **Moment Shadow Mapping**, Peters / Klein, ACM SIGGRAPH I3D 2015.
  [Autoren/Errata](https://momentsingraphics.de/I3D2015.html) ·
  [korrigierte Autorenfassung](https://momentsingraphics.de/Media/I3D2015/MomentShadowMapping.pdf) ·
  [PDF](lighting/i3d/2015-moment-shadow-mapping.pdf).

  - **Relevanz: Mittel — Filterbare Schatten / #4, #16.**
  - **Einsatz:** Vier Tiefenmomente; breite Filter als PCF-Alternative; Vergleich gegen vorhandenes
    Reverse-Z-PCF.
  - **Grenzen:** 2048×2048 mit vier 16-Bit-Kanälen: 32 MiB reine Texeldaten; Mips/Tiefen-/Blurziele
    nach Lebensdauer; Quantisierung, Bias, Light Bleeding; Lookup/alte GTX-Zeiten ohne
    UMA-Peakgewinnbeleg.

- **Efficient GPU Screen-Space Ray Tracing**, McGuire / Mara, JCGT 2014.
  [Quelle](https://jcgt.org/published/0003/04/04/) ·
  [PDF-Quelle](https://jcgt.org/published/0003/04/04/paper.pdf) ·
  [PDF](lighting/jcgt/2014-efficient-screen-space-rays.pdf).

  - **Relevanz: Mittel — Lokale Reflexionen / #4, #3.**
  - **Einsatz:** Perspektivkorrekte DDA; weniger doppelte Tiefenpixelabfragen; begrenzte
    Wasser-/Glasreflexionen neben IBL.
  - **Grenzen:** Schrittlänge/-zahl, Dicke; fehlende Flächen außerhalb Bild/hinter erster Tiefe;
    zusätzliche Tiefenschichten kosten; kein räumlicher Weltsichtbarkeitsersatz.

- **Creation and Rendering of Realistic Trees**, Weber / Penn, SIGGRAPH 1995.
  [Publikation](https://doi.org/10.1145/218380.218427) ·
  [PDF-Quelle](https://courses.cs.duke.edu/cps124/fall02/resources/p119-weber.pdf) ·
  [PDF](vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf).

  - **Relevanz: Mittel — Geteilte Baumrezepte / #1.**
  - **Einsatz:** Astordnungen, Elternprofile, kontrollierte Formvarianz; parametrisierbare
    Vergleichsformen; begrenzte gemeinsame Prototypen; ferne Bestände direkt aggregieren.
  - **Grenzen:** Rekursion/Organwachstum; Formparameter ohne
    Lichtkonkurrenz-/Botanik-/Deckungs-/Laufzeitfreigabe; keine vollständige Fernausmodellierung.

- **Self-organizing Tree Models for Image Synthesis**, Pałubicki et al., SIGGRAPH / TOG 2009.
  [Autoren](https://algorithmicbotany.org/papers/selforg.sig2009.html) ·
  [PDF-Quelle](https://algorithmicbotany.org/papers/selforg.sig2009.small.pdf) ·
  [PDF](vegetation/siggraph/2009-self-organizing-trees.pdf).

  - **Relevanz: Mittel — Kronenform / #1.**
  - **Einsatz:** Knospen, Raum-/Lichtkonkurrenz, Wachstumssignale; natürliche lokale Astarchitektur;
    wenige gemeinsame Prototypen gegen direkte Formrezepte vergleichen.
  - **Grenzen:** Wachstumsiterationen und Licht-/Raumfelder; keine Simulation jedes Fernbaums;
    Einzelbaumwachstum ohne kompaktes Fernwaldprodukt oder begrenzte Weltaufbauzeit.

- **Modeling Trees with a Space Colonization Algorithm**, Runions / Lane / Prusinkiewicz,
  Eurographics Workshop on Natural Phenomena 2007.
  [Autoren](https://algorithmicbotany.org/papers/colonization.egwnp2007.html) ·
  [PDF-Quelle](https://algorithmicbotany.org/papers/colonization.egwnp2007.pdf) ·
  [PDF](vegetation/eg/2007-space-colonization.pdf).

  - **Relevanz: Mittel — Kronenplan / #1.**
  - **Einsatz:** Attraktoren, Internodien, freier Kronenraum; Einfluss-/Entfernungsradien für
    Nachbarschaft/Abbruch; begrenzte Nahpläne bei gleichen Eingaben.
  - **Grenzen:** Nachbarsuche, Iterationen, Stillstand protokollieren; keine fertigen Blätter oder
    Fernwaldlösung ohne Einzelbaum-/Organarbeit.

- **Physically Based Real-Time Translucency for Leaves**, Habel / Kusternig / Wimmer,
  EGSR 2007.
  [Quelle](https://www.cg.tuwien.ac.at/research/publications/2007/Habel_2007_RTT/Habel_2007_RTT-Preprint.pdf) ·
  [PDF](vegetation/egsr/2007-real-time-leaf-translucency.pdf).

  - **Relevanz: Mittel — Blattlicht / #1, #6.**
  - **Einsatz:** Vorberechnete Dünnschicht-Streuung; kompakte gerichtete Basis; getrennte
    Reflexion/Transmission; eigene Blattparameter und instanzierbare Lichtantwort.
  - **Grenzen:** Keine fotografische Mess-/Texturakquisition; Vorbereitungsdaten, Dicke, Schatten,
    Fernfilterung; Blattmodell ohne Bestandsdeckungslösung.

- **Physically Guided Animation of Trees**, Habel / Kusternig / Wimmer, Eurographics 2009.
  [Autoren](https://www.cg.tuwien.ac.at/research/publications/2009/Habel_09_PGT/) ·
  [PDF](vegetation/eg/habel-2009-physically-guided-animation.pdf).

  - **Relevanz: Mittel — Wind / #1, #41.**
  - **Einsatz:** Verjüngte Balken; hierarchische Verformung; vorbereitete spektrale Bewegung;
    gemeinsame Astparameter statt Vollphysik je Baum.
  - **Grenzen:** Einspannung, Radiusabhängigkeit, Kräfte kalibrieren; modaler
    Aufbau/Instanzspeicher; eigenes erstes Verschiebungsfeld keine kalibrierte Balkendynamik oder
    vollständige Windlösung.
  - **Bezüge:** [#41](https://github.com/Weichwolf/outshine/issues/41).

- **Stochastic Transparency**, Enderton / Sintorn / Shirley / Luebke, TVCG 2011,
  erweiterte Fassung der I3D-Arbeit 2010.
  [Quelle](https://research.nvidia.com/sites/default/files/pubs/2011-08_Stochastic-Transparency/stochtransp-tvcg.pdf) ·
  [PDF](vegetation/tvcg/2011-stochastic-transparency.pdf).

  - **Relevanz: Mittel — Vegetationsdeckung / #1, #6, #12.**
  - **Einsatz:** Stochastische Samplemasken; Alpha-Mischung im Erwartungswert; begrenzter Speicher;
    gleicher Schatten-/Deckungsvergleich gegen Alpha-Test/Dither.
  - **Grenzen:** Einzelframe-Rauschen; MSAA, Akkumulation, Bewegung samt Zeit/Bandbreite/Stabilität;
    kein automatischer UMA-Gewinner/konservativer Occluder; Castaño-Alpha-Mips bei festem Cutoff,
    kleine diskrete Mips ohne stets exakt gleiche Deckung.
  - **Bezüge:** [Computing Alpha Mipmaps, Castaño](https://www.ludicon.com/castano/blog/articles/computing-alpha-mipmaps/).

- **Computer-Generated Residential Building Layouts**, Merrell / Schkufza / Koltun, SIGGRAPH Asia 2010.
  [Quelle](https://vladlen.info/papers/architecture.pdf) ·
  [PDF](buildings/siggraph/2010-computer-generated-residential-building-layouts.pdf).

  - **Relevanz: Mittel — Innenräume / #8, #31.**
  - **Einsatz:** Gemeinsame Optimierung von Raumprogramm, Adjazenzen, Geschossumrissen;
    Anschlüsse/Zugänglichkeit als Vergleichskriterien.
  - **Grenzen:** Viele stochastische Schritte/Formänderungen; OSM-Außenkörper als Randbedingung; nur
    angeforderte Nah-/Funktionsprodukte, keine Vollinnenräume fürs Fernrendern.
  - **Bezüge:** [#31](https://github.com/Weichwolf/outshine/issues/31).

- **Interactive Furniture Layout Using Interior Design Guidelines**, Merrell et al., SIGGRAPH 2011.
  [Autorenquelle](https://graphics.stanford.edu/~pmerrell/furnitureLayout2.pdf) ·
  [PDF](buildings/siggraph/2011-interactive-furniture-layout.pdf).

  - **Relevanz: Mittel — Nutzbare Räume / #8, #174.**
  - **Einsatz:** Objekt-/Aktivitätsflächen; Konfigurationsraum; erreichbare Nutzungsziele; begrenzte
    Einrichtungssuche; freie Wege/Interaktionsflächen erhalten.
  - **Grenzen:** Visual-LOD kein Ersatz funktionaler Daten; Original-GPU-MCMC/Kosten nicht
    vorausgesetzt; Designregeln weder Baurecht noch Welt-Begehbarkeitsbeleg.
  - **Bezüge:** [#174](https://github.com/Weichwolf/outshine/issues/174).

- **Finding the Maximum Area Axis-Parallel Rectangle in a Polygon**, Daniels / Milenkovic / Roth,
  Autorenpreprint Oktober 1993, später Computational Geometry 1997.
  [Quelle](https://cogcomp.seas.upenn.edu/papers/maaprJ.pdf) ·
  [PDF](buildings/geometry/1993-maximum-area-axis-parallel-rectangle.pdf).

  - **Relevanz: Mittel — Innenkerne / #30, #31.**
  - **Einsatz:** Randkontakte/Zerlegung; innenliegende Rechtecke auch bei Löchern; gültige
    angeschlossene Platzierung fester Treppen-/Kernmaße ausreichend.
  - **Grenzen:** Flächenoptimierung bei fester Achse; keine globale Optimalität des eigenen
    begrenzten Platzierers oder Lösung beliebig gedrehter Raumprogramme.
  - **Bezüge:** [#30](https://github.com/Weichwolf/outshine/issues/30) · [#31](https://github.com/Weichwolf/outshine/issues/31).

- **Wave Particles**, Yuksel / House / Keyser, SIGGRAPH 2007.
  [Quelle](https://www.cemyuksel.com/research/waveparticles/waveparticles.pdf) ·
  [PDF](water/siggraph/2007-wave-particles.pdf).

  - **Relevanz: Mittel — Nahwasser / #3, #14, #7.**
  - **Einsatz:** Begrenzte Partikel/Wellenfront-Überlagerung; lokale Körper-/Uferwirkung ohne
    weltweite Fluidsimulation; Nahbedarf begrenzen, Fernwellen getrennt.
  - **Grenzen:** Dynamische Aufspaltung/Nachbarschaft/Reflexion; linearisiertes Modell ohne
    Quellpegel-/Küstenabschluss-/Strömungs-/Wasser-Bett-Kontaktlösung.

- **L-systems and Beyond**, Prusinkiewicz (Organizer), SIGGRAPH Course 2003.
  [Autorenquelle](https://algorithmicbotany.org/papers/sigcourse.2003.pdf) ·
  [PDF](vegetation/siggraph/2003-l-systems-and-beyond.pdf).

  - **Relevanz: Mittel — Pflanzenrezepte / #1, #36.**
  - **Einsatz:** SIGGRAPH-Kurs; lageabhängige Profile/Teilblätter an gemeinsamen Achsen; begrenzte
    eigene Farn-/Grasrezepte; geteilte Prototypen ohne Vollwachstum je Standort.
  - **Grenzen:** Kein Vollinterpreter nötig; botanische Richtigkeit, Organkosten, Fernaggregation
    separat.
  - **Bezüge:** [#36](https://github.com/Weichwolf/outshine/issues/36).

- **Developmental Models of Plant Architecture**, Prusinkiewicz / Lindenmayer / Hanan,
  SIGGRAPH 1988. [Autorenquelle](https://algorithmicbotany.org/papers/developmental.sig1988.pdf) ·
  [PDF](vegetation/siggraph/1988-developmental-models-of-plant-architecture.pdf).

  - **Relevanz: Mittel — Wuchsform / #1, #105.**
  - **Einsatz:** Entwicklungsregeln/Tropismus; Achsen/Verzweigung; vorbereitete räumliche
    Pflanzenpläne/lageabhängige Formen; Wachstum nur angeforderter gemeinsamer Prototypen.
  - **Grenzen:** Eigene Tangentenprojektion/ODE als begrenzte Konstruktion; heuristischer Tropismus
    ohne mechanischen Gewichts-/Steifheitssolver oder Lichtkonkurrenz.
  - **Bezüge:** [#105](https://github.com/Weichwolf/outshine/issues/105).

- **An Image Synthesizer**, Perlin, SIGGRAPH 1985.
  [Primärquelle](https://doi.org/10.1145/325334.325247) ·
  [PDF](vegetation/siggraph/1985-image-synthesizer.pdf), CMU-Kursarchiv.

  - **Relevanz: Mittel — Prozedurale Materialien / #6, #35.**
  - **Einsatz:** Räumliche Noise/Komposition; kompakte Materialrezepte, etwa Rinde in physischen
    Astkoordinaten; kleine Parameter statt Fototexturen.
  - **Grenzen:** Neue Hochfrequenzen durch Nichtlinearität/Oktaven; Pixelfilter/fertige
    Materialstufen nötig; eigene Achsfelder/Blender-Noise kein Originalcode-Nachbau.
  - **Bezüge:** [#35](https://github.com/Weichwolf/outshine/issues/35).

- **European Atlas of Forest Tree Species**, San-Miguel-Ayanz et al. (Hg.), JRC 2016,
  DOI [10.2788/038466](https://doi.org/10.2788/038466).
  Artspezifische Kapitel für [Picea abies](vegetation/botany/2016-european-atlas-picea-abies.pdf),
  [Pinus sylvestris](vegetation/botany/2016-european-atlas-pinus-sylvestris.pdf),
  [Pinus cembra](vegetation/botany/2016-european-atlas-pinus-cembra.pdf) und
  [Larix decidua](vegetation/botany/2016-european-atlas-larix-decidua.pdf).
  [JRC-Primärquelle](https://forest.jrc.ec.europa.eu/en/european-atlas/atlas-data-and-metadata/).

  - **Relevanz: Mittel — Botanisches Referenzwerk / #1, #131.**
  - **Einsatz:** Vier Artenkapitel; Wuchsform, Nadeln, Rinde, Habitat; geteilte Artenrezepte:
    Einzel-/Zweier-/Fünfernadeln, Lärchen-Kurztriebe; Schweizer Namen/IDs am 14.10.2026 geprüft.
  - **Grenzen:** Fotos nur Vergleich; europäische Kapitel ohne Weltabdeckungs-/Allometrienachweis;
    Bestandsverteilung, Formfehler, Fernprodukte separat.
  - **Bezüge:** [#131](https://github.com/Weichwolf/outshine/issues/131) · [Fichte](https://www.infoflora.ch/de/flora/picea-abies.html) · [Wald-Föhre](https://www.infoflora.ch/de/flora/pinus-sylvestris.html) · [Arve](https://www.infoflora.ch/de/flora/pinus-cembra.html) · [Europäische Lärche](https://www.infoflora.ch/de/flora/larix-decidua.html).

- **Leaf area index of boreal forests: Theory, techniques, and measurements**,
  Chen et al., JGR 1997, DOI [10.1029/97JD01107](https://doi.org/10.1029/97JD01107).
  [Autorenkopie](https://professorpaul.com/publications/chen_et_al_1997.pdf),
  [PDF](vegetation/botany/1997-leaf-area-index-of-boreal-forests.pdf).

  - **Relevanz: Mittel — Bestandsdeckung / #1, #179.**
  - **Einsatz:** Halbe grüne Gesamtfläche je Grundfläche; getrennte tatsächliche/wirksame LAI und
    Klumpung; Flächenziele/aggregierte Lichtantwort statt Blattzahl.
  - **Grenzen:** Nominale Prototypfläche keine gemessene Bestands-LAI; Klumpung, Blick, Projektion:
    LAI allein weder opake Deckung noch sichere Verdeckung.
  - **Bezüge:** [NASA-Produktdefinition](https://access.earthdata.nasa.gov/collections/C2222147000-LPCLOUD).

- **Modeling the Mighty Maple**, Bloomenthal, SIGGRAPH 1985.
  [Autorenquelle](https://www.unchainedgeometry.com/JBloom/pdf/SIG85-ModelingMightyMaple.pdf) ·
  [PDF](vegetation/siggraph/1985-modeling-the-mighty-maple.pdf).

  - **Relevanz: Mittel — Astflächen / #1, #46.**
  - **Einsatz:** Glatte räumliche Achsen/variable Querschnitte; generalisierte Zylinder; geteilte
    Ast-/Nadelformen; stabile Rahmen statt Frenet-Singularitäten.
  - **Grenzen:** Gerade Teile/Wendepunkte; dichte Ausgabe begrenzen, Stufen vorbereiten; keine
    fotografischen Rinden-/Blatttexturen.
  - **Bezüge:** [#46](https://github.com/Weichwolf/outshine/issues/46).

- **Global and Local Deformations of Solid Primitives**, Barr, SIGGRAPH 1984.
  [Primärmetadata](https://authors.library.caltech.edu/records/99pd2-73028) ·
  [DOI](https://doi.org/10.1145/964965.808573) ·
  [PDF](vegetation/siggraph/1984-global-local-deformations.pdf), Universitätskopie.

  - **Relevanz: Mittel — Deformation / #1, #81.**
  - **Einsatz:** Jacobi-Matrix; inverse-transponierte/Kofaktor-Normalenabbildung; kohärente eigene
    Rinden-/Astdeformation; Ableitungen gegen Differenzen/Normalen gegen unabhängige lineare Lösung.
  - **Grenzen:** Singularitäten/Invertierbarkeit; kein Boolean-Weld oder allgemeiner
    Kollisionsnachweis; geteilte Vorbereitung statt Wiederholung je Instanz.
  - **Bezüge:** [#81](https://github.com/Weichwolf/outshine/issues/81).

- **Computation of Rotation Minimizing Frames**, Wang / Jüttler / Zheng / Liu, TOG 2008.
  [Autorenpreprint](https://www.ag.jku.at/pubs/2007wjzl.pdf) ·
  [PDF](vegetation/tog/2008-rotation-minimizing-frames-preprint.pdf), ungesetzte Vorabfassung.

  - **Relevanz: Mittel — Räumliche Rahmen / #1, #27, #103.**
  - **Einsatz:** Doppelreflexion; twistarme Rahmen regulärer Kurven; Vergleich gegen
    Tangentendrehung gleicher Ast-/Frondachsen: Fehler, Vorbereitung, sichtbare Querschnitte;
    geteilte Rahmen.
  - **Grenzen:** Differentialnormalen passend zur tatsächlichen Kurvenabbildung; lokale Regularität
    ohne Kollisions-/Kraftnachweis.
  - **Bezüge:** [#27](https://github.com/Weichwolf/outshine/issues/27) · [#103](https://github.com/Weichwolf/outshine/issues/103).

- **Efficient Implementation of Marching Cubes' Cases with Topological Guarantees**,
  Lewiner / Lopes / Vieira / Tavares, JGT 2003.
  [Autorenquelle](https://thomas.lewiner.org/pdfs/marching_cubes_jgt.pdf) ·
  [PDF](vegetation/jgt/2003-marching-cubes-topological-guarantees.pdf), Autorenpreprint.

  - **Relevanz: Mittel — Implizite Gabeln / #1, #28.**
  - **Einsatz:** Flächen-/Innerentests für mehrdeutige Zellfälle; Vergleich gemeinsamer
    Astgabeloberflächen.
  - **Grenzen:** Topologiegarantie nur für trilineare Gitterinterpolation, nicht unbekannte
    kontinuierliche Fläche; Abtast-/Speicher-/Kontinuitätsgrenzen; kein Vollgitter je Pflanze oder
    Pflichtzwischenschritt für Fernprodukte.
  - **Bezüge:** [#28](https://github.com/Weichwolf/outshine/issues/28).

- **Interval Arithmetic and Recursive Subdivision for Implicit Functions and
  Constructive Solid Geometry**, Duff, SIGGRAPH 1992.
  [Primärquelle im MIT-Kursarchiv](https://fab.cba.mit.edu/classes/S62.12/docs/Duff_interval_CSG.pdf) ·
  [PDF](vegetation/siggraph/1992-interval-arithmetic-csg.pdf).

  - **Relevanz: Mittel — Feldbedarf / #1, #28.**
  - **Einsatz:** Intervallgrenzen; rekursive Gebiete; unbetroffene Blöcke vor teurer Feldabtastung
    verwerfen; lokale implizite Gabeln.
  - **Grenzen:** Einschluss aller Operationen samt Rundung, keine bloßen Punktproben; begrenzte
    Rekursion/Unterteilung/Scratch; keine kostenlose Weltverdeckung oder szenentiefenunabhängige
    Laufzeit.
  - **Bezüge:** [#28](https://github.com/Weichwolf/outshine/issues/28).

- **Interactive and Robust Mesh Booleans**, Cherchi / Pellacini / Attene / Livesu,
  SIGGRAPH Asia / TOG 2022. [Autorenquelle](https://arxiv.org/abs/2205.14151) ·
  [PDF](vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf), Autorenpreprint.

  - **Relevanz: Mittel — Gemeinsame Oberflächen / #1, #29, #88.**
  - **Einsatz:** Konforme Oberflächenzerlegung; Innen-/Außenklassifikation; robuste Vergleiche
    vollständiger Ast-/Baukörperflächen; begrenzte vorbereitete Eingaben.
  - **Grenzen:** Exakte/implizite Schnittkoordinaten mit Aufbau-/Speicherkosten; Manifold mit
    eigenem Verfahren; Bibliotheksgarantien ohne Ersatz für lokale Prüfung, organische Ausrundung,
    gültige gerundete Ausgabe.
  - **Bezüge:** [#29](https://github.com/Weichwolf/outshine/issues/29).

- **Embree: A Kernel Framework for Efficient CPU Ray Tracing**, Wald / Woop / Benthin /
  Johnson / Ernst, SIGGRAPH / TOG 2014.
  [Autorenquelle](https://www.sci.utah.edu/~wald/Publications/2014/embree/embree.pdf) ·
  [PDF](vegetation/siggraph/2014-embree-ray-tracing.pdf).

  - **Relevanz: Mittel — CPU-Referenzabfragen / #1, #29.**
  - **Einsatz:** BVH-/SIMD-Strahlabfragen gegen skalare Elternflächenabfragen;
    Astnormalen/Referenzbilder; geteilte statische Beschleunigung.
  - **Grenzen:** Aufbau, Updates, Primitivkopien, Peak; damaliges x86 ohne A18-Faktorbeleg,
    Bibliothekspflicht oder günstiges GPU-Streaming.
  - **Bezüge:** [#29](https://github.com/Weichwolf/outshine/issues/29).

- **Improving Noise**, Perlin, SIGGRAPH2002.
  [Autorenquelle](https://mrl.cs.nyu.edu/~perlin/paper445.pdf) ·
  [PDF](clouds/siggraph/2002-improving-noise.pdf).

  - **Relevanz: Mittel — Prozedurale Felder / #6, #7, #447.**
  - **Einsatz:** Verbesserte Gittergradienten/glattere Interpolation; weniger Ableitungsartefakte;
    kompakte Material-/Wolkendichtefelder.
  - **Grenzen:** Relative/große Weltkoordinaten und Advektion numerisch prüfen; Glattheit keine
    Pixelfilterung; Oktaven-/Sample-/Texturbandbreitengrenzen; keine native Präzisionsabnahme.
  - **Bezüge:** [#447](https://github.com/Weichwolf/outshine/issues/447).

- **The Real-time Volumetric Cloudscapes of Horizon Zero Dawn**, Schneider / Vos,
  SIGGRAPH-Kurs 2015.
  [Quelle](https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf) ·
  [PDF](clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf).

  - **Relevanz: Mittel — Wolken / #7.**
  - **Einsatz:** Geteilte 3D-Noise; Höhen-/Formprofile; Raymarching in sphärischer Wolkenschicht;
    plausible Wolken aus deklariertem Wetter ohne globale Fluidsimulation.
  - **Grenzen:** Dichte-/Lichtproben, Bandbreite, Schatten, zeitliche Stabilität; historische
    PS4-Zahlen/verkleinerter Planetenradius weder Geräteabnahme noch physikalische Skalenvorgabe.

- **Animating Rotation with Quaternion Curves**, Shoemake, SIGGRAPH 1985.
  [Quelle](https://doi.org/10.1145/325165.325242) ·
  [PDF](animation/siggraph/1985-rotation-quaternion-curves.pdf).

  - **Relevanz: Mittel — Kamera/Animation / #2, #811.**
  - **Einsatz:** Quaternionprodukt; sphärische Interpolation; analytische Kamerarotation aus
    Framezeit statt gespeicherter Ansichten.
  - **Grenzen:** Vorzeichen, kurze Bögen, Matrix-/Multiplikationskonventionen gegen unabhängige
    Achsrotationen; keine Gelenk-/Kontaktsteuerung; eigener Kameraweg ohne Splines.

- **XPBD: Position-Based Simulation of Compliant Constrained Dynamics**, Macklin et al.,
  Motion in Games 2016. [Quelle](https://mmacklin.com/xpbd.pdf) ·
  [PDF](physics/mig/2016-xpbd.pdf).

  - **Relevanz: Mittel — Nahe flexible Körper / #2, #41.**
  - **Einsatz:** Zeitschrittbezogene Compliance; fortgeschriebene Multiplikatoren; vergleichbarere
    Steifheit als PBD; begrenzte Seil-/Stoff-/Pflanzenkontakte.
  - **Grenzen:** Kein allgemeiner Weltphysikersatz; Iterationen, Substeps, Kontakte, Fehler;
    Konvergenzkosten; notwendige Funktions-/Simulationsdaten unabhängig vom Render-LOD.

- **Synthesizing Sounds from Rigid-Body Simulations**, O'Brien / Shen / Gatchalian,
  ACM SIGGRAPH Symposium on Computer Animation 2002.
  [Quelle](https://jamesobrien.com/papers/Obrien-SSR-2002-07/Obrien-SSR-2002-07.pdf) ·
  [PDF](audio/sca/2002-rigid-body-sound-synthesis.pdf).

  - **Relevanz: Mittel — Kontaktklang / #2.**
  - **Einsatz:** Kontaktkräfte; vorbereitete elastische Moden; wenige geteilte Moden/analytische
    Zeitantworten; Vergleich für Materialklänge.
  - **Grenzen:** Kleine lineare Schwingungen, Geometrie-/Materialparameter, Modalaufbau; keine
    Vollmesh-Eigenanalyse je Kontakt; Stimmenzahl, hörbarer Gewinn, Spatial-Audio-Kosten separat.

## Relevanz: Gering

- **Tutorial 15: Publish web scenes**, Esri 2013, Produktdokumentation.
  [Quelle](https://resources.arcgis.com/en/help/pdf/cityengine/10.2/tutorial_15_publish_web_scenes.pdf) ·
  [PDF](buildings/esri/2013-interior-floors-web-scenes.pdf).

  - **Relevanz: Gering — Produktdokumentation / #8, #30.**
  - **Einsatz:** Produktdokumentation; Geschosslayer/Schnittansichten als visueller
    Innenraumvergleich.
  - **Grenzen:** Webexport/Bedienung vorhandenen Beispiels, kein automatischer
    Raumplaner/Gültigkeitsbeweis; keine ArcGIS-/Webscene-Produktpipeline;
    Layout-/Zugänglichkeitsquellen aussagekräftiger.
  - **Bezüge:** [#30](https://github.com/Weichwolf/outshine/issues/30).

- **Leaf anatomy of Pinus thunbergii**, Ghimire et al., KJPT 2014.
  [Primärquelle](https://doi.org/10.11110/kjpt.2014.44.2.91) ·
  [PDF](vegetation/botany/2014-pinus-thunbergii-leaf-anatomy.pdf).

  - **Relevanz: Gering — Botanische Nahform / #1, #46.**
  - **Einsatz:** Mikroskopische Querschnitte; flache adaxiale/gekrümmte abaxiale Nadelseiten; eigene
    Sektorflächen als Nahformreferenz.
  - **Grenzen:** Nur ausreichend nahe sichtbar; keine fernen Nadelquerschnitte; artspezifisch statt
    universeller Koniferenform; keine Zellmodellierung/Texturübernahme nötig.
  - **Bezüge:** [#46](https://github.com/Weichwolf/outshine/issues/46).

- **Projection onto the Probability Simplex**, Wang / Carreira-Perpiñán, arXiv 2013.
  [Autorenquelle](https://arxiv.org/abs/1309.1541) ·
  [PDF](vegetation/math/2013-probability-simplex-projection.pdf).

  - **Relevanz: Gering — Mathematischer Teilbaustein / #1, #28.**
  - **Einsatz:** Endliche Sortierprojektion; nichtnegative Gewichte fester Summe; eigene
    symmetrische Feldmischung; unabhängige QP-/Gradientenprüfung.
  - **Grenzen:** Optimalität nur des Projektionsproblems, nicht Astgeometrie/Weltplanung/Kosten;
    kein Vegetations-/SpeedTree-Verfahren; nur bei benötigter Mischung.

- **Modeling and visualization of leaf venation patterns**, Runions et al., SIGGRAPH 2005.
  [Autoren-PDF](https://www.algorithmicbotany.org/papers/venation.sig2005.pdf) ·
  [PDF](vegetation/siggraph/2005-leaf-venation.pdf).

  - **Relevanz: Gering — Blatt-Nahdetail / #1, #42.**
  - **Einsatz:** Attraktoren/Entfernungsregeln; Adernetze/verbundene Radien; wenige geteilte
    Blattrezepte/Materialfelder.
  - **Grenzen:** Eigenes offenes Netz mit fester Kontur ohne Wachstum/geschlossene Maschen; keine
    Wachstumssuche je Blattinstanz; Subpixeladern filtern statt Geometrie/Vollsolver.
  - **Bezüge:** [#42](https://github.com/Weichwolf/outshine/issues/42).

- **DeepMimic: Example-Guided Deep Reinforcement Learning of Physics-Based Character Skills**,
  Peng et al., SIGGRAPH 2018.
  [Quelle](https://xbpeng.github.io/projects/DeepMimic/DeepMimic_2018.pdf) ·
  [PDF](animation/siggraph/2018-deepmimic.pdf).

  - **Relevanz: Gering — NPC-Spezialsteuerung / #2.**
  - **Einsatz:** Referenzbewegungen/Training; physikbasierte Skills, lokale Gelenkmotor-Ziele;
    spätere Forschungsoption für Nahcharakter-Vergleich.
  - **Grenzen:** Kein fertiger allgemeiner NPC; lizenzierte Referenzen, Training, Inferenz,
    Körper-/Kontaktsimulation; zuerst begrenzte ausführbare Steuerung; LLM-Ziel keine gültige
    Renderpose.

- **LDI Tree: A Hierarchical Representation for Image-Based Rendering**, Chang / Bishop / Lastra, SIGGRAPH 1999.
  [Publikation](https://doi.org/10.1145/311535.311571) ·
  [Universitätsarchiv](https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/chang99.pdf) ·
  [PDF](geometry/siggraph/1999-ldi-tree.pdf).

  - **Relevanz: Gering — Räumliche Tiefenhierarchie / #899.**
  - **Einsatz:** Hierarchische Abtastung als Vergleichsmathematik.
  - **Grenzen:** Eigener orthographischer Gittervergleich #902 bei Coverage/Materialgrenzen/Speicher
    schlechter als Originale; dieses Profil nicht portieren/weiterpacken; andere Hierarchien nur mit
    neuem Bild-/Kostenbeleg; kein Gegenbeweis aller LDI-Varianten.
  - **Bezüge:** [#902](https://github.com/Weichwolf/outshine/issues/902).

- **Taming the Shadow Terminator**, Chiang / Li / Burley, SIGGRAPH Talks 2019.
  [Autorenquelle](https://www.yiningkarlli.com/projects/shadowterminator.html) ·
  [PDF](vegetation/siggraph/2019-taming-shadow-terminator.pdf), korrigierte Autorenfassung.

  - **Relevanz: Gering — Schattennormalen-Spezialfall / #29, #4.**
  - **Einsatz:** Gezielter Schattenfaktor für geometrische/abweichende Schattierungsnormalen;
    Diagnose bei entsprechendem Fehler.
  - **Grenzen:** Keine fehlende Geometrie/grobe Schattenkarte repariert; modellierte
    Mikroverschattung kein echter Relieftransport; EEVEE-Positionsoffsets als anderes Verfahren,
    kein Übernahmebeleg.

## Relevanz: Nicht relevant

- **Stereo Magnification: Learning view synthesis using multiplane images**,
  Zhou / Tucker / Flynn / Fyffe / Snavely, SIGGRAPH 2018.
  [Publikation](https://doi.org/10.1145/3197517.3201323) ·
  [Autoren-PDF](https://tinghuiz.github.io/papers/siggraph18_mpi_lowres.pdf) ·
  [PDF](geometry/siggraph/2018-stereo-magnification.pdf).

  - **Relevanz: Nicht relevant — Gelernte Ansichtsrekonstruktion / #899.**
  - **Einsatz:** Geometrische Verankerung/Alpha-Komposition als Vergleichsmathematik.
  - **Grenzen:** Gesamtverfahren mit Stereo-Fotos, trainierter Rekonstruktion, RGB-Schichten
    unpassend zu prozeduralen Assets/PBR/Wetter; keine Modell-/Welt-/Residenzfreigabe.

- **Color Compatibility From Large Datasets**, O'Donovan / Agarwala / Hertzmann,
  SIGGRAPH / ACM TOG 2011.
  [Autoren](https://www.dgp.toronto.edu/~donovan/color/) ·
  [Quelle](https://www.dgp.toronto.edu/~donovan/color/colorcomp.pdf) ·
  [PDF](presentation/siggraph/2011-color-compatibility.pdf).

  - **Relevanz: Nicht relevant — Gelernte Palettenpräferenz / #4, #6.**
  - **Einsatz:** Fünffarben-Palettenpräferenzen als Gegenstand; separater Ottosson-Autorenartikel zu
    Oklab/OKLCH für Diagnose/UI-Abstände.
  - **Grenzen:** Datensatz-/Benennungsbias statt physikalischer Material-/Lichtwerte; keine
    vorgegebene Stilpalette, Trainingsdaten oder Modelle übernehmen; Beleuchtung/Materialmischung in
    linearem RGB.
  - **Bezüge:** [Oklab/OKLCH, Ottosson 2020](https://bottosson.github.io/posts/oklab/).

## Bewertung ausstehend

Ein Eintrag ohne Relevanzurteil; fehlenden Volltext nicht durch Abstract oder
Implementierungswissen ersetzen.

- **Computing Vertex Normals from Polygonal Facets**, Thürmer / Wüthrich, JGT 1998.
  [Primärabstract](https://doi.org/10.1080/10867651.1998.10487487).

  - **Relevanz:** unbekannt; kein Urteil.
  - **Prüfstand:** Volltext-/Abbildungsprüfung offen; Verlags-PDF 403; nur Primärabstract
    zur Eckwinkelgewichtung.
  - **Grenzen:** eigene Differentialnormalenmittelung kein Volltextbeleg; Max 1999 nur
    textlich gelesen, lokaler PDF-Abruf ohne Dokumentdaten, optische Prüfung offen;
    Oberflächenapproximation versus Unterteilungserhaltung noch zu prüfen.
  - **Bezüge:** [#40](https://github.com/Weichwolf/outshine/issues/40) · [Autorenpreprint](https://escholarship.org/uc/item/7657d8h3) · [#919](https://github.com/Weichwolf/outshine/issues/919).

## Weitere Primärquellen und Fachnotizen

### Assets und Sichtbedarf — WI [#10](https://github.com/Weichwolf/outshine/issues/10) / [#12](https://github.com/Weichwolf/outshine/issues/12)

[Vorbereitete Assets, räumliche Pakete und Residency](engine/unreal/prepared-assets.md)
fasst Unreal DDC, World Partition/HLOD, Texture Streaming und Geometry Clipmaps zusammen.
Die Notiz verlinkt die Primärquellen, grenzt deren Übertragung ab und trennt SSD-Gültigkeit
von RAM-/GPU-Residency und budgetierten Laufzeitdetails. Epic-Referenzen sind Online-Dokumentation;
das Clipmaps-Paper liegt bereits als PDF unter Terrain/SIGGRAPH.

[Hierarchischer Bedarf vor Download und Geometrie](engine/visibility-driven-demand.md)
prüft konkrete MapLibre-/CesiumJS-Codepfade und ergänzt die getrennte
Cesium-Native-Originalprüfung samt Fallback-/Ladegrenzen.

### Gebäudeeingaben — WI [#8](https://github.com/Weichwolf/outshine/issues/8) / [#17](https://github.com/Weichwolf/outshine/issues/17)

[OSM-Lieferinventur und Rezeptplan](data/osm/inventory.md), WI [#17](https://github.com/Weichwolf/outshine/issues/17):
32 gezielte Standorte, alle gelieferten Layer, vollständiger beobachteter Klassenkatalog.
Lieferlücken von Rezept-/Integrationslücken unterscheiden; keine globale Vollständigkeitsbehauptung.

### Klang — WI [#2](https://github.com/Weichwolf/outshine/issues/2)

- [Begrenztes Spatial Audio: geprüfter Steam-Audio-/Resonance-Audio-Code](audio/bounded-spatial-audio.md).
  Native Abfragen wiederverwenden; grobe gemeinsame Umgebung, begrenzte Stimmen/Hallbusse.
  Cubemap/GPU sind zu messende Experimente, keine von der Quellenzahl unabhängige Simulation.

### Bereiche mit geeigneteren Primärquellen

| Bereich / WI | Grundlage | Entscheidung für Outshine |
|---|---|---|
| Quellen/HTTP/Cache — [#10](https://github.com/Weichwolf/outshine/issues/10) | [HTTP-Semantik, RFC 9110](https://www.rfc-editor.org/rfc/rfc9110) | libcurl; paralleles begrenztes IO, Status-/Retry-/Abbruchverträge; Quellbytes separat, Assetbedarf → Treffer laden / Miss generieren; räumlicher Assetcache mit LOD/Residency |
| Weltweite Auswahl — [#12](https://github.com/Weichwolf/outshine/issues/12) | [Cesium Native: Auswahl](engine/visibility-driven-demand.md#cesium-native-selection) | SSE vor Content-IO; Ersatzabdeckung, `forbidHoles` und erreichte Zielqualität unterscheiden; Blick-Culling getrennt von Rundum-Residency |
| Import/Material/Animation — [#9](https://github.com/Weichwolf/outshine/issues/9) / [#6](https://github.com/Weichwolf/outshine/issues/6) / [#2](https://github.com/Weichwolf/outshine/issues/2) | [Khronos glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) | Material-/Transformkonventionen als Referenz; kein glTF-Import/-Export im Produkt |
| Backend/Submission — [#9](https://github.com/Weichwolf/outshine/issues/9) / [#4](https://github.com/Weichwolf/outshine/issues/4) | [SDL3 GPU](https://wiki.libsdl.org/SDL3/CategoryGPU) | Historischer SDL3_GPU-Bezug; aktuell native Metal-/Vulkan-Verträge in [#48](https://github.com/Weichwolf/outshine/issues/48) |
| JS/UI/LLM — [#2](https://github.com/Weichwolf/outshine/issues/2) | [ECMAScript](https://tc39.es/ecma262/) und vorhandene HTML/CSS-Teilmenge | Begrenzte Commands/Events, lokale ausführbare Steuerung, keine Netzwerkabhängigkeit im Tick |
| Navigation — [#2](https://github.com/Weichwolf/outshine/issues/2) | [Recast/Detour](https://recastnav.com/md_Docs_2__1__Introduction.html) | Begehbare Flächen und lokale Pfadkorridore; Straßengraph für Verkehr, eigene Flug-/Wassermodelle statt einer universellen 2D-Navigation |
| Astronomie — [#7](https://github.com/Weichwolf/outshine/issues/7) | [IAU SOFA](https://www.iausofa.org/cookbooks) | Zeit-/Koordinatenkonventionen und Beobachterkorrekturen prüfen; Astronomie aus UTC/Ort, nicht als Live-Datenabhängigkeit |
| Parser/Mathematik/Geometrie/Physik | [Systembibliotheken](../dependencies.md) | Bewährte Implementierungen kapseln; keine Library-Typen in Welt, ABI oder Saveformat |
| Save/Replay, Telemetrie, Budgets — [#2](https://github.com/Weichwolf/outshine/issues/2) / [#9](https://github.com/Weichwolf/outshine/issues/9) / [#5](https://github.com/Weichwolf/outshine/issues/5) | Eigene Zustands-/Lebensdauerverträge im WI | Versionierte atomare Zustandsübergänge, aufgezeichnete externe Events; Literatur begründet keine Vollständigkeit |

Offene Implementierungsfragen bleiben im zuständigen WI: Quellsemantik/Sonderbauten,
globale Abdeckung, konservative Formfehler, Schattenqualität, Reflexionssichtbarkeit,
bewegliche Kontakte und lokale NPC-Navigation. Eine Quellenliste schließt diese Features nicht.
