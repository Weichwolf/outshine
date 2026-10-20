# Forschungsgrundlage für Outshine

Stand: 2026-10-20. Primärquellen: Autoren, Forschungsgruppen, Kursveranstalter und Normgeber.
Papers, Kurse und Produktdokumentation sind als solche bezeichnet; Links und lokale PDFs bleiben erhalten.
Die Einträge sind die kanonische Relevanzbewertung nach [#919](https://github.com/Weichwolf/outshine/issues/919),
keine Machbarkeitsbestätigung und keine neue Architekturfreigabe.

**Prüfstand: 69 von 70 Einträgen bewertet.** Alle Bereiche sind eingeordnet;
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
  **Relevanz: Hoch — Streaming / #10, #12, #915.**
  Sicht-/LOD-Traversierung fordert fehlende Knoten erst bei tatsächlichem Bedarf an;
  Eltern liefern gröbere Darstellung, Requests werden zusammengefasst. Das ist unser
  Hebel vor Fine-IO/Decode, keine Entscheidung für eine Voxelwelt. Feedback, Baumzugriffe
  und Miss-Latenz mitbezahlen; HDD-/getrennte-VRAM-Stufen nicht auf SSD/UMA übertragen.

- **Conservative Visibility Preprocessing using Extended Projections**, Durand / Drettakis /
  Thollot / Puech, SIGGRAPH 2000.
  [Publikation](https://doi.org/10.1145/344779.344891) ·
  [Autoren](https://people.csail.mit.edu/fredo/PUBLI/Sig2000/index.htm) ·
  [PDF](geometry/siggraph/2000-conservative-visibility-extended-projections.pdf).
  **Relevanz: Hoch — Verdeckung / #12, #915.**
  Konservative Verdeckung für eine ganze Kamerazelle kann Detailanforderungen schon
  vor dem Laden ausschließen. Innenprojektion des Occluders, Außenprojektion des Ziels
  und konservative Tiefe gemeinsam prüfen. Nur vorhandene qualifizierte opake Flächen
  zählen; AABBs, zugeschlossene Löcher oder veraltete bewegte Occluder reichen nicht.
  Vorberechnung, Zellwechsel und Invalidierung kosten Arbeit; unbekannt bleibt sichtbar.

- **Geometry Clipmaps: Terrain Rendering Using Nested Regular Grids**, Losasso / Hoppe,
  SIGGRAPH 2004. [Quelle](https://hhoppe.com/geomclipmap.pdf) ·
  [PDF](terrain/siggraph/2004-geometry-clipmaps.pdf).
  **Relevanz: Hoch — Terrain / #13, #10, #915.**
  Verschachtelte Gitter, toroidale Wiederverwendung, grob-vor-fein und begrenzte
  Randupdates passen zu Rundumabdeckung bei Bewegung. Fertige Höhen-/Materialstufen
  unabhängig lesen; SSD-Latenz, Decode und UMA-Schreibbandbreite trotzdem bezahlen.
  Höhenfeldverfahren: kein allgemeines Gebäude-/Straßen-LOD, keine automatische
  Erdkrümmungs-, Verdeckungs- oder konservative Fehlergarantie.

- **Simplification Envelopes**, Cohen et al., SIGGRAPH 1996.
  [Quelle](https://www.cs.umd.edu/gvil/papers/simp_env.pdf) ·
  [PDF](geometry/siggraph/1996-simplification-envelopes.pdf).
  **Relevanz: Hoch — Formfehler / #12, #14, #915.**
  Begrenzende Hüllen liefern das Prinzip für überprüfbare räumliche LOD-Fehler statt
  bloßer Samples. Benötigt bei Eltern-/Kindauswahl und Kontaktverträglichkeit.
  Die publizierte Konstruktion hat Topologie-/Regularitätsannahmen und Aufbaukosten;
  Distanznähe allein macht ein Ersatzmesh weder zum sicheren Occluder noch fahrbar.

- **GPU-Driven Rendering Pipelines**, Haar / Aaltonen, SIGGRAPH-Kurs 2015.
  [Quelle](https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf) ·
  [PDF](geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf).
  **Relevanz: Hoch — Renderauswahl / #48, #844, #12.**
  Instanz-/Cluster-Culling, kompakte Listen und indirekte Submission vermeiden Arbeit
  an unsichtbarer Geometrie. Vorhandene grobe Occluder können den Bedarfsentscheid
  unterstützen; GPU-Culling nach dem Upload spart vorheriges Fine-IO nicht.
  Tiefenpass, Reprojektion, Listen/Indexkompaktion und Änderungen auf Metal/Vulkan
  messen; alte PS4-Zahlen und unsichere History sind keine Freigabe für SSD/UMA.

- **Procedural Modeling of Buildings**, Müller et al., SIGGRAPH 2006.
  [Quelle](https://peterwonka.net/Publications/pdfs/2006.SG.Mueller.ProceduralModelingOfBuildings.final.pdf) ·
  [PDF](buildings/siggraph/2006-procedural-modeling-of-buildings.pdf).
  **Relevanz: Hoch — Gebäudeplan / #8, #33, #915.**
  Ein hierarchischer Formplan verbindet Grundriss, Baukörper, Dach und wiederholte
  Module; passende Stufen können aus demselben Plan entstehen. Das adressiert
  Gebäudevielfalt ohne neue Einzelgeometrie je Instanz. Regeln benötigen gelieferte
  Klasse/Höhe und begrenzte Ergänzungen; ein vollständiger CGA-Interpreter ist
  nicht nötig. Hierarchie allein garantiert weder ladbare LOD-Seiten noch Formfehler.

- **Grammar-based Encoding of Facades**, Haegler et al., EGSR 2010.
  [Quelle](https://peterwonka.net/Publications/pdfs/2010.EGSR.Haegler.GrammarBasedEncoding.pdf) ·
  [PDF](buildings/egsr/2010-grammar-based-encoding-of-facades.pdf).
  **Relevanz: Hoch — Fassaden / #6, #8, #33.**
  Wiederholte Fassadenelemente als kompakte Regeln statt ausmodellierter Einzelteile
  speichern. Geeignet für eigene prozedurale Fassadenprodukte; das Paper dekodiert
  Regeln pro Pixel mit zusätzlichen Puffern. Regeltiefe, Divergenz, Scratch und
  Filterung mitmessen. Nahsilhouette, Laibungen und Räume brauchen eigene Geometrie;
  Fotofassaden und historischer CUDA-/VRAM-Pfad werden nicht übernommen.

- **Synthetic Texturing Using Digital Filters**, Feibush / Levoy / Cook, SIGGRAPH1980.
  [Quelle](https://graphics.stanford.edu/papers/texture-filtering/) ·
  [PDF](materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf).
  **Relevanz: Hoch — Pixelfilter / #6, #306, #899.**
  Projizierte Pixelfläche, sichtbarer Polygonanteil und Texturfilter gemeinsam
  integrieren: konkrete Referenz für Coverage und unterpixelgroße Materialdetails.
  Unser Polygon-Clipping bleibt unabhängiges Orakel; den historischen Scanline-/
  Dateiladepfad nicht übernehmen. Ein korrekter Referenzfilter beweist noch keinen
  günstigen Runtime-Filter oder selbstständig ladbare Fernprodukte.

- **Frequency Domain Normal Map Filtering**, Han / Sun / Ramamoorthi / Grinspun,
  SIGGRAPH / TOG 2007. [Autorenquelle](https://www.cs.columbia.edu/cg/normalmap/) ·
  [PDF](vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf).
  **Relevanz: Hoch — Normalenfilter / #6, #422, [#44](https://github.com/Weichwolf/outshine/issues/44).**
  Die Verteilung der Normalen statt nur ihrer mittleren Richtung erhält die
  Glanzstruktur unter Verkleinerung. Direkter Maßstab für Blatt-/Relief-Mips und
  Fernmaterialien. SH-/vMF-Approximationen kosten Koeffizienten, Fits und Sampling;
  schmale oder mehrgipflige Verteilungen sowie korrelierte Farbe/Materialien
  brauchen eigene Fehlerbelege. Kein universeller kleiner GGX-Ersatz daraus.

- **Realistic Modeling and Rendering of Plant Ecosystems**, Deussen et al., SIGGRAPH 1998.
  [Quelle](https://algorithmicbotany.org/papers/ecosys.sig98.pdf) ·
  [PDF](vegetation/siggraph/1998-plant-ecosystems.pdf).
  **Relevanz: Hoch — Bestände / #1, #12, #915.**
  Bestandsplan, Pflanzenform und Darstellung trennen; Prototypen und Instanzen
  teilen, statt überall Organprodukte aufzubauen. Grundprinzip für kompakte
  Bestands-/Fernprodukte und späteres Nahdetail. Quantisierung und Artenmischung
  gegen Deckung/Lichtantwort prüfen. Offline-Bestände sind kein Echtzeitbeleg;
  Wind, Schatten, Overdraw und physische Übergangspeaks bleiben eigene Kosten.

- **A Scalable and Production Ready Sky and Atmosphere Rendering Technique**, Hillaire,
  EGSR 2020. [Quelle](https://sebh.github.io/publications/egsr2020.pdf) ·
  [PDF](atmosphere/egsr/2020-production-ready-atmosphere.pdf).
  **Relevanz: Hoch — Atmosphäre / #7, #4.**
  Kompakte LUTs für Transmission, Mehrfachstreuung, Himmel und Luftperspektive
  verbinden Boden-/Flug-/Orbitansichten. Konkrete Grundlage für die vorhandene
  SkyStage und physikalisch begrenzte Fernsicht. LUT-Auflösung, Updates und
  Sonnen-/Geländeschatten gegen Referenz prüfen; das Modell approximiert die
  Streuung. Wetterparameter sind keine gemessene Aerosolverteilung oder Wolkenlösung.

- **Surface Gradient-Based Bump Mapping Framework**, Mikkelsen, JCGT 9(3), 2020.
  [Quelle](https://jcgt.org/published/0009/03/04/paper.pdf) ·
  [PDF](materials/jcgt/2020-surface-gradient-bump-mapping.pdf).
  **Relevanz: Hoch — Bump-Komposition / #6.**
  Reliefbeiträge aus mehreren UVs/Projektionen im gemeinsamen Gradientenraum
  mischen und die Normale einmal rekonstruieren; Grundlage der vorhandenen
  Materialkomposition. Lokalkonstante Triplanar-Gewichte sind eine Näherung.
  Gradientenmischung ersetzt weder Silhouette/Kontakte noch Normalenvarianz- und
  GGX-Filterung; zusätzliche Detailtexturen und Samples separat budgetieren.

- **Moving Frostbite to Physically Based Rendering**, Lagarde / de Rousiers,
  SIGGRAPH-Kurs 2014, Kursnotizen Revision 3 (2015).
  [Quelle](https://seblagarde.wordpress.com/wp-content/uploads/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf) ·
  [PDF](lighting/siggraph/2014-frostbite-pbr-course-notes.pdf).
  **Relevanz: Hoch — Lichtkalibrierung / #4, #6.**
  BRDF-Energie, Rauheitskonventionen, Lichtgrößen und Kameraexposition gemeinsam
  kalibrieren; direkt gegen flache Tag-/Nachtantwort und dunkles Glas verwenden.
  Referenzintegration und Energieprüfungen von Runtime-Näherungen trennen.
  Der Kurs ist keine unverändert zu übernehmende Passkette: IBL-Produkte, HDR-/
  Schattenziele und deren gleichzeitige Lebensdauer auf beiden Geräten messen.

- **Real Shading in Unreal Engine 4**, Karis, SIGGRAPH-Kurs 2013.
  [Quelle](https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf) ·
  [PDF](materials/siggraph/2013-unreal-engine-4-shading.pdf).
  **Relevanz: Hoch — GGX und IBL / #4, #6.**
  GGX, kompakte Materialparameter und vorgefiltertes Environment mit Split-Sum
  stützen den bestehenden gemeinsamen Shader. Parameterblend spart mehrere
  BRDF-Auswertungen, entspricht aber nicht exakt einer Mischung ihrer Ergebnisse.
  Split-Sum und n=v beim Environment-Filter sind eigene Näherungen; direkte
  Roughness-Remaps nicht auf IBL übertragen. Atlas-/Mipbytes und Updates zählen.

- **A Survey of Temporal Antialiasing Techniques**, Yang / Liu / Salvi, CGF / Eurographics 2020.
  [Autoren](https://research.nvidia.com/labs/rtr/publication/yang2020survey/) ·
  [Verlags-PDF](https://diglib.eg.org/server/api/core/bitstreams/53732e70-b64d-46f4-bbae-865eb7673a35/content) ·
  [PDF](presentation/cgf/2020-temporal-antialiasing-survey.pdf).
  **Relevanz: Hoch — Bewegungsstabilität / #4, #16.**
  Reprojektion, History-Prüfung und Rekonstruktion zusammen behandeln; entscheidend
  bei LOD-Wechseln, neuer Geometrie, Wind, Wasser und Ursprungssprüngen.
  Ungültige History verwerfen; Clamping kann Geisterbilder/Detailverlust nur abwägen.
  Ein zusätzliches RGBA16F-Bild bei 1280×720 hat bereits 7,03125 MiB Texelnutzdaten
  ohne Padding/weitere Puffer. TAA spart nicht automatisch Raster- oder Assetarbeit.

- **Clustered Deferred and Forward Shading**, Olsson / Billeter / Assarsson, HPG 2012.
  [Quelle](https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf) ·
  [PDF](lighting/hpg/2012-clustered-shading.pdf).
  **Relevanz: Hoch — Lokale Lichtauswahl / #4.**
  Räumliche Tiefencluster ordnen den benötigten tausenden lokalen Lichtern kleine
  Fragmentlisten zu; direkter Baustein für Nacht-/Innenraumlicht. Aufbau, Listenbytes,
  Überlauf und dynamische Änderungen begrenzen und messen; kleine Lichtmengen
  dürfen den einfachen Pfad behalten. Dies wählt Lichtarbeit, liefert aber weder
  Asset-Verdeckung vor IO noch eine Schattenkarte pro sichtbarem Fenster.

- **Real-time Realistic Rendering and Lighting of Forests**, Bruneton / Neyret, Eurographics 2012.
  [Publikation](https://doi.org/10.1111/j.1467-8659.2012.03016.x) ·
  [Autoren-PDF](https://maverick.inria.fr/Publications/2011/BN11a/article.pdf) ·
  [PDF](vegetation/eg/2012-real-time-forests.pdf).
  **Relevanz: Hoch — Fernvegetation / #1, #12, #899.**
  Bestandsdeckung und Blick-/Lichtkorrelation statt Einzelbaum-/Blattarbeit für den
  Fernwald; gemeinsame Prototypen und bedarfsabhängige Seeds/Coverage liefern einen
  konkreten Verbraucher. Z-Felder, vorberechnete Tabellen und Artmischung samt
  Bytes/Bandbreite prüfen; keine ungeprüfte Vollübernahme. Wind, Änderungen und
  Simulation sind zusätzlich zu lösen; eine Dichtekarte ist kein opaker Occluder.

## Relevanz: Mittel

- **Surface Simplification Using Quadric Error Metrics**, Garland / Heckbert, SIGGRAPH 1997.
  [Quelle](https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf) ·
  [PDF](geometry/siggraph/1997-quadric-error-metrics.pdf).
  **Relevanz: Mittel — Geometrie-LOD / #12, #899, #915.**
  Quadriken priorisieren lokale Kontraktionen und erlauben vorgefertigte Mesh-LODs.
  Allgemeine Paaraggregation kann getrennte Teile verbinden: für Straßenkontakte
  und Verdeckung nur mit zusätzlichen Einschränkungen. Kein kontinuierlicher
  Hausdorff-/Coveragebeweis; Fine-Aufbau vor späterem Reduzieren spart keinen Miss-Cook.

- **Procedural Generation of Roads**, Galin / Peytavie / Maréchal / Guérin, Eurographics 2010.
  [Quelle](https://perso.liris.cnrs.fr/egalin/Articles/2010-roads.pdf) ·
  [PDF](transport/eurographics/2010-procedural-generation-of-roads.pdf).
  **Relevanz: Mittel — Straßenprofile / #3, #14, #18.**
  Trajektorie, Querschnitt und Geländeanpassung gemeinsam auswerten; Brücken,
  Tunnel, Einschnitt und Aufschüttung als eigene Modelle ableiten. Nützlich für
  funktionale Kontakte und kohärente Renderstufen. Anisotrope Pfadsuche ersetzt
  unser vorhandenes OSM-Netz nicht. Abtastung und Höhenschwellen allein sichern
  keine Knotenanschlüsse, Durchfahrt oder kreuzungsfreie Baugeometrie.
  Weitere Bezüge:
  [Codevorbilder und Grenzen](transport/README.md).

- **Interactive Procedural Street Modeling**, Chen et al., SIGGRAPH 2008.
  [Quelle](https://peterwonka.net/Publications/pdfs/2008.SG.Chen.InteractiveProceduralStreetModeling.pdf) ·
  [PDF](transport/siggraph/2008-interactive-procedural-street-modeling.pdf).
  **Relevanz: Mittel — Transportplan / #11, #18.**
  Graphbearbeitung von späterer Straßengeometrie trennen; lokale Bearbeitung und
  Muster sind als Planungsbausteine nützlich. Die Tensorfeld-Neuerzeugung ersetzt
  keine gelieferten OSM-Verbindungen, Ebenen oder Profile. Brücken/Tunnel und
  funktionale Kontakte brauchen eigene Verträge; vollständiges Netz-Wachstum
  vor jedem geladenen Fernprodukt wäre unnötige Arbeit.

- **Instant Architecture**, Wonka et al., SIGGRAPH 2003.
  [Quelle](https://peterwonka.net/Publications/pdfs/2003.SG.Wonka.InstantArchitecture.high.pdf) ·
  [PDF](buildings/siggraph/2003-instant-architecture.pdf).
  **Relevanz: Mittel — Gebäuderezepte / #8, #33.**
  Split-/Kontrollgrammatik und Attributzuordnung strukturieren wiederholte Fassaden.
  Nützlich zur begrenzten Rezeptwahl aus gelieferten Körpern und Nutzung, mit
  geteilten Modulen. Keine Rekonstruktion unbekannter realer Fassaden; Regelwahl
  und Modulbestand kosten Aufbau/Speicher. Der spätere Gebäude-Formplan muss
  Außenform, Dach und begehbare Teile gemeinsam besitzen.

- **Mesh Arrangements for Solid Geometry**, Zhou et al., SIGGRAPH 2016.
  [Quelle](https://www.cs.columbia.edu/cg/mesh-arrangements/) ·
  [PDF](buildings/geometry/2016-mesh-arrangements-for-solid-geometry.pdf).
  **Relevanz: Mittel — Baukörper / #8, #88.**
  Exakte Schnitte und Zell-/Winding-Klassifikation begründen robuste Boolesche
  Baukörperoperationen. Für Supportregionen und schwierige Überlagerungen mit
  lokal begrenzten Eingaben vergleichen. Rationale Schnittkoordinaten und
  Arrangement-Aufbau können teuer sein; Rundung zum Ausgabemesh gesondert prüfen.
  Keine notwendige Operation je Fassade oder Fernasset.
  Weitere Bezüge:
  [#88](https://github.com/Weichwolf/outshine/issues/88).

- **Billboard Clouds for Extreme Model Simplification**, Décoret et al., SIGGRAPH 2003.
  [Autoren](https://graphics.cs.yale.edu/publications/billboard-clouds-extreme-model-simplification) ·
  [PDF-Quelle](https://graphics.cs.yale.edu/sites/default/files/bc03_0.pdf) ·
  [PDF](vegetation/siggraph/2003-billboard-clouds.pdf).
  **Relevanz: Mittel — Fernrepräsentation / #1, #899.**
  Mehrere geometrisch gewählte Ebenen sind ein konkreter Kandidat für eigene
  prozedurale Fernprodukte. Geometrische Ebenenabweichung deckt Alpha-/Mipmap-,
  GGX-, Tiefen-/Disokklusions- und Schattenfehler nicht ab; Texturleerraum/Overdraw
  und laufende Wind-/Lichtänderungen können den Meshgewinn aufheben.

- **LEAN Mapping**, Olano / Baker, I3D 2010, korrigierte Autorenfassung.
  [Autorenquelle](https://userpages.cs.umbc.edu/olano/papers/lean/) ·
  [PDF](vegetation/i3d/2010-lean-mapping.pdf).
  **Relevanz: Mittel — Filterbare Neigungsstatistik / [#44](https://github.com/Weichwolf/outshine/issues/44), #422.**
  Erste/zweite Neigungsmomente im gemeinsamen Tangentialraum linear filtern;
  nützlicher Vergleich für vorbereitete Material-LODs. Fünf Momente, deren Format/
  Mips und die Rekonstruktion kosten Speicher/Bandbreite. Beckmann-Gaußmodell und
  endliche Neigungen sind Voraussetzungen; GGX, verdeckte Mikroflächen und
  mehrgipflige Glanzverteilungen sind dadurch nicht allgemein korrekt vertreten.

- **Layered Depth Images**, Shade / Gortler / He / Szeliski, SIGGRAPH 1998.
  [Publikation](https://doi.org/10.1145/280814.280882) ·
  [Autoren-PDF](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/Shade-SG98.pdf) ·
  [PDF](vegetation/siggraph/1998-layered-depth-images.pdf).
  **Relevanz: Mittel — Parallaxe / [#23](https://github.com/Weichwolf/outshine/issues/23), #899.**
  Mehrere Tiefentreffer erhalten verdeckte Flächen für neue Ansichten und begründen
  den Vergleich gegen flache Mattes. Zusätzliche Schichten kosten Speicher/Sampling;
  feste Ansicht und Abtastung begrenzen Wiederverwendung. Eigene unbeleuchtete
  PBR-Felder brauchen Material-/Normalfilterung und gültige Disokklusionsunterstützung.

- **Relief Texture Mapping**, Oliveira / Bishop / McAllister, SIGGRAPH 2000.
  [Publikation](https://doi.org/10.1145/344779.344947) ·
  [Autoren-PDF](https://www.inf.ufrgs.br/~oliveira/pubs_files/RTM_low_res.pdf) ·
  [PDF](geometry/siggraph/2000-relief-texture-mapping.pdf).
  **Relevanz: Mittel — Parallaxe / #899.**
  Geometrisch verankerte Tiefe und Prewarp liefern einen prüfbaren Baustein für
  eigene Reliefprodukte. Ein Höhenwert pro Texel vertritt keine beliebig geschichtete
  Welt; Silhouette, neue Sichtflächen und aktuelles PBR gesondert prüfen.
  Der verwandte eigene Planarschnitt in [#917](https://github.com/Weichwolf/outshine/issues/917)
  bleibt wegen Bild-/Gesamtkosten verworfen; seine kleine Pixelparallaxe beweist keine Welttiefe.

- **Procedural Noise using Sparse Gabor Convolution**, Lagae et al., SIGGRAPH 2009.
  [Quelle](https://graphics.cs.kuleuven.be/publications/LLDD09PNSGC/LLDD09PNSGC_paper.pdf) ·
  [PDF](materials/siggraph/2009-sparse-gabor-noise.pdf).
  **Relevanz: Mittel — Gerichtete Materialfelder / #6, #13.**
  Steuerbares Spektrum und anisotrope Filterung aus dem Mapping-Jacobian passen
  zu Fels-/Holzstruktur ohne große individuelle Texturen. Kernel-/Nachbarschafts-
  und Ableitungskosten gegen einfachere gefilterte Noise messen, gegebenenfalls
  fertige Stufen cachen. Nichtlineare Farb-/Materialabbildungen können neue
  Frequenzen erzeugen; Noise ersetzt weder DEM-Form noch belegte Geologie.

- **Crafting a Next-Gen Material Pipeline for The Order: 1886**, Neubelt / Pettineo,
  SIGGRAPH-Kurs 2013.
  [Quelle](https://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf) ·
  [PDF](materials/siggraph/2013-the-order-material-pipeline.pdf).
  **Relevanz: Mittel — Materialrezepte / #6.**
  Wiederverwendbare Materialtemplates und Parameter-/Maskenkomposition passen
  zu prozeduralen Baustoffen, Schmutz und Nässe. Die publizierte Werkzeug-/Foto-/
  Bake-Pipeline ist kein Produktvertrag für Outshine. Einzigartige Maps je Objekt
  vermeiden; Detailkarten, Runtime-Layer und Normalenfilter benötigen ein
  gemessenes Speicher-/Samplingbudget und gültige Übergänge.

- **Physically Based Shading at Disney**, Burley, SIGGRAPH-Kurs 2012.
  [Quelle](https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf) ·
  [PDF](materials/siggraph/2012-disney-physically-based-shading.pdf).
  **Relevanz: Mittel — Materialfamilien / #6.**
  Gemessene Reflexionen und wenige Parameter helfen beim Vergleich von Metall,
  Dielektrikum, Beschichtung und Fasern. Der Kurs priorisiert auch künstlerische
  Bedienbarkeit und erlaubt unphysikalische Parameter; das ist keine verbindliche
  Materialkalibrierung. Vorhandenen GGX-/Energievertrag erhalten, zusätzliche
  Lobes nur für einen konkreten Oberflächentyp mit Filter-/Kostenbeleg ergänzen.

- **Precomputed Radiance Transfer for Real-Time Rendering in Dynamic,
  Low-Frequency Lighting Environments**, Sloan / Kautz / Snyder, SIGGRAPH 2002.
  [Quelle](https://www.microsoft.com/en-us/research/publication/precomputed-radiance-transfer-real-time-rendering-dynamic-low-frequency-lighting-environments/) ·
  [PDF](lighting/siggraph/2002-precomputed-radiance-transfer.pdf).
  **Relevanz: Mittel — Aggregierte Lichtantwort / #4, #451.**
  Geometrischen Transfer vom aktuellen niederfrequenten Umgebungslicht trennen;
  Baustein für vorbereitete Antworten gemeinsamer Prototypen/Fernbestände.
  Transfervektoren/-matrizen je Sample, Vorberechnung und Änderungen kosten
  Speicher/Arbeit. Der vorbereitete Transfer ist an die Form gebunden und setzt
  räumlich glattes Licht voraus; Verformung, lokale scharfe Schatten und GGX-
  Spitzen benötigen zusätzliche Behandlung.

- **Real-Time Polygonal-Light Shading with Linearly Transformed Cosines**,
  Heitz / Dupuy / Hill / Neubelt, SIGGRAPH / TOG 2016.
  [Autorenquelle und PDF](https://eheitzresearch.wordpress.com/415-2/).
  [PDF](lighting/siggraph/2016-linearly-transformed-cosines.pdf).
  **Relevanz: Mittel — Flächenlicht und Referenzintegrale / #4, [#422](https://github.com/Weichwolf/outshine/issues/422).**
  Analytisches Kosinus-Randintegral ist ein wiederverwendbarer mathematischer
  Baustein; LTC-Fits ermöglichen später polygonale Flächenlichter. Der BRDF-Fit
  und die Approximation texturierter Leuchten haben eigene Fehler; Horizont-
  Clipping und Verdeckung bleiben zusätzlich nötig. Der [GGX-Kernvergleich](../../test/experiments/render/relief_finite.md)
  verwendet nur Randintegration und scheitert weiterhin an seinen Resttermen;
  daraus weder den vollständigen LTC-Ansatz verwerfen noch einen Shaderport ableiten.

- **Moment Shadow Mapping**, Peters / Klein, ACM SIGGRAPH I3D 2015.
  [Autoren/Errata](https://momentsingraphics.de/I3D2015.html) ·
  [korrigierte Autorenfassung](https://momentsingraphics.de/Media/I3D2015/MomentShadowMapping.pdf) ·
  [PDF](lighting/i3d/2015-moment-shadow-mapping.pdf).
  **Relevanz: Mittel — Filterbare Schatten / #4, #16.**
  Vier Tiefenmomente erlauben breite Filter als Alternative zu PCF. Bei 2048×2048
  benötigen vier 16-Bit-Kanäle bereits 32 MiB reine Texeldaten; Mips, Tiefenziel
  und Blur-Ziele kommen je tatsächlicher Lebensdauer hinzu. Quantisierung,
  Moment-Bias und Light Bleeding gegen vorhandenes Reverse-Z-PCF prüfen.
  Ein Lookup oder alte GTX-Zeiten belegen keinen Gewinn im gemeinsamen UMA-Peak.

- **Efficient GPU Screen-Space Ray Tracing**, McGuire / Mara, JCGT 2014.
  [Quelle](https://jcgt.org/published/0003/04/04/) ·
  [PDF-Quelle](https://jcgt.org/published/0003/04/04/paper.pdf) ·
  [PDF](lighting/jcgt/2014-efficient-screen-space-rays.pdf).
  **Relevanz: Mittel — Lokale Reflexionen / #4, #3.**
  Perspektivkorrekte DDA vermeidet mehrfache Abfragen derselben Tiefenpixel;
  sinnvoll für begrenzte Reflexionen auf Wasser/Glas neben dem vorhandenen IBL.
  Schrittlänge, maximale Schritte und Flächendicke verursachen Fehler/Kosten.
  Außerhalb des Bilds oder hinter der ersten Tiefe fehlen Flächen; zusätzliche
  Tiefenschichten sind nicht kostenlos. Kein Ersatz für räumliche Weltsichtbarkeit.

- **Creation and Rendering of Realistic Trees**, Weber / Penn, SIGGRAPH 1995.
  [Publikation](https://doi.org/10.1145/218380.218427) ·
  [PDF-Quelle](https://courses.cs.duke.edu/cps124/fall02/resources/p119-weber.pdf) ·
  [PDF](vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf).
  **Relevanz: Mittel — Geteilte Baumrezepte / #1.**
  Astordnungen, Elternprofile und kontrollierte Formvarianz liefern direkt
  parametrisierbare Vergleichsformen. Begrenzte gemeinsame Prototypen erzeugen;
  rekursive Verzweigung kann die Organanzahl stark vergrößern. Formparameter
  ersetzen keine Lichtkonkurrenz, botanische Freigabe oder belegte Deckungs-/
  Laufzeitgrenze. Ferne Bestände direkt aggregiert statt vollständig ausmodellieren.

- **Self-organizing Tree Models for Image Synthesis**, Pałubicki et al., SIGGRAPH / TOG 2009.
  [Autoren](https://algorithmicbotany.org/papers/selforg.sig2009.html) ·
  [PDF-Quelle](https://algorithmicbotany.org/papers/selforg.sig2009.small.pdf) ·
  [PDF](vegetation/siggraph/2009-self-organizing-trees.pdf).
  **Relevanz: Mittel — Kronenform / #1.**
  Knospen, Raum-/Lichtkonkurrenz und Wachstumssignale erzeugen natürliche lokale
  Astarchitektur. Für wenige gemeinsame Prototypen gegen direkte Formrezepte
  vergleichen. Wachstumsiterationen und Licht-/Raumfelder kosten Arbeit und Speicher;
  keine Simulation jedes fernen Baums. Einzelbaumwachstum liefert noch kein
  kompaktes Fernwaldprodukt und keine begrenzte Welt-Aufbauzeit.

- **Modeling Trees with a Space Colonization Algorithm**, Runions / Lane / Prusinkiewicz,
  Eurographics Workshop on Natural Phenomena 2007.
  [Autoren](https://algorithmicbotany.org/papers/colonization.egwnp2007.html) ·
  [PDF-Quelle](https://algorithmicbotany.org/papers/colonization.egwnp2007.pdf) ·
  [PDF](vegetation/eg/2007-space-colonization.pdf).
  **Relevanz: Mittel — Kronenplan / #1.**
  Attraktoren lenken neue Internodien in freien Kronenraum; Einfluss- und
  Entfernungsradien bestimmen Nachbarschaft und Abbruch. Konkreter Vergleich
  begrenzter Nahpläne bei gleichen Eingaben. Nachbarsuche, Iterationen und
  Stillstand protokollieren. Das Verfahren erzeugt weder fertige Blätter noch
  einen Fernwaldpfad ohne Einzelbaum-/Organarbeit.

- **Physically Based Real-Time Translucency for Leaves**, Habel / Kusternig / Wimmer,
  EGSR 2007.
  [Quelle](https://www.cg.tuwien.ac.at/research/publications/2007/Habel_2007_RTT/Habel_2007_RTT-Preprint.pdf) ·
  [PDF](vegetation/egsr/2007-real-time-leaf-translucency.pdf).
  **Relevanz: Mittel — Blattlicht / #1, #6.**
  Dünnschicht-Streuung vorberechnen und in kompakter gerichteter Basis auswerten;
  Reflexion und Transmission bleiben getrennt. Vergleich für eigene prozedurale
  Blattparameter und instanzierbare Lichtantwort. Die fotografische Mess-/Textur-
  akquisition wird nicht übernommen. Vorbereitungsdaten, Blattdicke, Schatten und
  gefilterte Fernantwort prüfen; ein Blattmodell löst keine Bestandsdeckung.

- **Physically Guided Animation of Trees**, Habel / Kusternig / Wimmer, Eurographics 2009.
  [Autoren](https://www.cg.tuwien.ac.at/research/publications/2009/Habel_09_PGT/) ·
  [PDF](vegetation/eg/habel-2009-physically-guided-animation.pdf).
  **Relevanz: Mittel — Wind / #1, #41.**
  Verjüngte Balken und hierarchische Verformung mit vorbereiteter spektraler
  Bewegungsantwort verbinden. Vergleich für gemeinsame Astparameter statt
  Vollphysik je Baum. Einspannung, Radiusabhängigkeit und Kräfte kalibrieren;
  modaler Aufbau und Instanzzustand brauchen Speicher. Unser erstes gemeinsames
  Verschiebungsfeld ist noch keine kalibrierte Balkendynamik oder vollständige Windlösung.
  Weitere Bezüge:
  [#41](https://github.com/Weichwolf/outshine/issues/41).

- **Stochastic Transparency**, Enderton / Sintorn / Shirley / Luebke, TVCG 2011,
  erweiterte Fassung der I3D-Arbeit 2010.
  [Quelle](https://research.nvidia.com/sites/default/files/pubs/2011-08_Stochastic-Transparency/stochtransp-tvcg.pdf) ·
  [PDF](vegetation/tvcg/2011-stochastic-transparency.pdf).
  **Relevanz: Mittel — Vegetationsdeckung / #1, #6, #12.**
  Stochastische Samplemasken liefern Alpha-Mischung im Erwartungswert bei
  begrenztem Speicher. Gegen Alpha-Test/Dither mit gleichem Schatten-/Deckungsziel
  vergleichen. Ein Frame bleibt verrauscht; MSAA, Akkumulation und Bewegung kosten
  Zeit/Bandbreite und brauchen Stabilitätsprüfung. Kein automatischer Gewinner
  auf UMA und kein konservativer opaker Occluder.
  Weitere Bezüge:
  [Computing Alpha Mipmaps, Castaño](https://www.ludicon.com/castano/blog/articles/computing-alpha-mipmaps/).
  Castaños verlinkter Autorenartikel behandelt Alpha-Mips bei festem Cutoff;
  kleine diskrete Mips erlauben nicht immer exakt gleiche Deckung.

- **Computer-Generated Residential Building Layouts**, Merrell / Schkufza / Koltun, SIGGRAPH Asia 2010.
  [Quelle](https://vladlen.info/papers/architecture.pdf) ·
  [PDF](buildings/siggraph/2010-computer-generated-residential-building-layouts.pdf).
  **Relevanz: Mittel — Innenräume / #8, #31.**
  Raumprogramm und Adjazenzen gemeinsam mit Geschossumrissen optimieren;
  Anschlüsse/Zugänglichkeit sind konkrete Vergleichskriterien für eigene Layouts.
  Der stochastische Optimierer verändert auch Formen und benötigt viele Schritte;
  OSM-Außenkörper bleiben unsere Randbedingung. Nur angeforderte Nah-/Funktions-
  produkte erzeugen; vollständige Innenräume sind kein Fernrenderbedarf.
  Weitere Bezüge:
  [#31](https://github.com/Weichwolf/outshine/issues/31).

- **Interactive Furniture Layout Using Interior Design Guidelines**, Merrell et al., SIGGRAPH 2011.
  [Autorenquelle](https://graphics.stanford.edu/~pmerrell/furnitureLayout2.pdf) ·
  [PDF](buildings/siggraph/2011-interactive-furniture-layout.pdf).
  **Relevanz: Mittel — Nutzbare Räume / #8, #174.**
  Objekt- und Aktivitätsflächen, Konfigurationsraum und erreichbare Nutzungsziele
  liefern konkrete Kriterien für begrenzte Einrichtungssuche. Freie Wege und
  Interaktionsflächen erhalten; Visual-LOD ersetzt diese Daten nicht. Der originale
  GPU-MCMC-Optimierer und dessen Kosten werden nicht vorausgesetzt; Designregeln
  sind weder Baurecht noch Beleg einer vollständig begehbaren Welt.
  Weitere Bezüge:
  [#174](https://github.com/Weichwolf/outshine/issues/174).

- **Finding the Maximum Area Axis-Parallel Rectangle in a Polygon**, Daniels / Milenkovic / Roth,
  Autorenpreprint Oktober 1993, später Computational Geometry 1997.
  [Quelle](https://cogcomp.seas.upenn.edu/papers/maaprJ.pdf) ·
  [PDF](buildings/geometry/1993-maximum-area-axis-parallel-rectangle.pdf).
  **Relevanz: Mittel — Innenkerne / #30, #31.**
  Randkontakte und Zerlegung helfen, vollständig innenliegende Rechtecke auch
  bei Polygonen mit Löchern zu begründen. Für feste Treppen-/Kernmaße genügt
  eine gültige Platzierung mit Anschluss. Das Paper optimiert Fläche bei fester
  Achsrichtung; daraus folgt keine globale Optimalität unseres begrenzten
  Platzierers oder eine Lösung für beliebig gedrehte Raumprogramme.
  Weitere Bezüge:
  [#30](https://github.com/Weichwolf/outshine/issues/30) ·
  [#31](https://github.com/Weichwolf/outshine/issues/31).

- **Wave Particles**, Yuksel / House / Keyser, SIGGRAPH 2007.
  [Quelle](https://www.cemyuksel.com/research/waveparticles/waveparticles.pdf) ·
  [PDF](water/siggraph/2007-wave-particles.pdf).
  **Relevanz: Mittel — Nahwasser / #3, #14, #7.**
  Lokale Wellenfronten durch begrenzte Partikel und ihre Überlagerung darstellen;
  geeigneter Vergleich für Körper-/Uferwirkung ohne weltweite Fluidsimulation.
  Aufspaltung, Nachbarschaft und Reflexionen kosten dynamische Arbeit, daher
  Nahbedarf begrenzen und Fernwellen getrennt darstellen. Das linearisierte Modell
  löst weder Quellpegel noch Küstenabschluss, Strömung oder Wasser/Bett-Kontakte.

- **L-systems and Beyond**, Prusinkiewicz (Organizer), SIGGRAPH Course 2003.
  [Autorenquelle](https://algorithmicbotany.org/papers/sigcourse.2003.pdf) ·
  [PDF](vegetation/siggraph/2003-l-systems-and-beyond.pdf).
  **Relevanz: Mittel — Pflanzenrezepte / #1, #36.**
  Der SIGGRAPH-Kurs zeigt lageabhängige Profilkurven und Teilblätter entlang
  gemeinsamer Achsen. Damit eigene begrenzte Farn-/Grasrezepte und geteilte
  Prototypen aufbauen, ohne jeden Standort vollständig auszuwachsen. Kein
  vollständiger L-System-Interpreter erforderlich; botanische Richtigkeit,
  organische Kosten und Fernaggregation bleiben gesondert zu prüfen.
  Weitere Bezüge:
  [#36](https://github.com/Weichwolf/outshine/issues/36).

- **Developmental Models of Plant Architecture**, Prusinkiewicz / Lindenmayer / Hanan,
  SIGGRAPH 1988. [Autorenquelle](https://algorithmicbotany.org/papers/developmental.sig1988.pdf) ·
  [PDF](vegetation/siggraph/1988-developmental-models-of-plant-architecture.pdf).
  **Relevanz: Mittel — Wuchsform / #1, #105.**
  Entwicklungsregeln und Tropismus lenken Achsen und Verzweigungen. Konkreter
  Vergleich für vorbereitete räumliche Pflanzenpläne und lageabhängige Formen.
  Unsere Tangentenprojektion/ODE ist eine eigene begrenzte Konstruktion;
  heuristischer Tropismus ersetzt weder mechanischen Gewicht-/Steifheitssolver
  noch Lichtkonkurrenz. Wachstum nur für angeforderte gemeinsame Prototypen.
  Weitere Bezüge:
  [#105](https://github.com/Weichwolf/outshine/issues/105).

- **An Image Synthesizer**, Perlin, SIGGRAPH 1985.
  [Primärquelle](https://doi.org/10.1145/325334.325247) ·
  [PDF](vegetation/siggraph/1985-image-synthesizer.pdf), CMU-Kursarchiv.
  **Relevanz: Mittel — Prozedurale Materialien / #6, #35.**
  Räumliche Rauschfunktionen und ihre Komposition begründen kompakte Materialrezepte,
  etwa Rinde in physischen Astkoordinaten. Kleine Parameter ersetzen individuelle
  Fototexturen. Nichtlineare Kombinationen und zusätzliche Oktaven können neue
  hohe Frequenzen erzeugen; Pixelfilter und fertige Materialstufen bleiben nötig.
  Eigene Achsfelder/Blender-Rauschen sind kein Nachbau des Originalcodes.
  Weitere Bezüge:
  [#35](https://github.com/Weichwolf/outshine/issues/35).

- **European Atlas of Forest Tree Species**, San-Miguel-Ayanz et al. (Hg.), JRC 2016,
  DOI [10.2788/038466](https://doi.org/10.2788/038466).
  Artspezifische Kapitel für [Picea abies](vegetation/botany/2016-european-atlas-picea-abies.pdf),
  [Pinus sylvestris](vegetation/botany/2016-european-atlas-pinus-sylvestris.pdf),
  [Pinus cembra](vegetation/botany/2016-european-atlas-pinus-cembra.pdf) und
  [Larix decidua](vegetation/botany/2016-european-atlas-larix-decidua.pdf).
  [JRC-Primärquelle](https://forest.jrc.ec.europa.eu/en/european-atlas/atlas-data-and-metadata/).
  **Relevanz: Mittel — Botanisches Referenzwerk / #1, #131.**
  Vier vorhandene Artenkapitel liefern Wuchsform, Nadeln, Rinde und Habitat als
  Referenz für eigene geteilte Artenrezepte: Einzel-/Zweier-/Fünfernadeln und
  Lärchen-Kurztriebe. Fotos bleiben Vergleich, keine Produkttexturen. Europäische
  Kapitel belegen weder weltweite Abdeckung noch gemessene Allometrie;
  Bestandsverteilung, Formfehler und Fernprodukte brauchen eigene Nachweise.
  Ergänzende Schweizer Namen/IDs, am 14.10.2026 geprüft:
  [#131](https://github.com/Weichwolf/outshine/issues/131) ·
  [Fichte](https://www.infoflora.ch/de/flora/picea-abies.html) ·
  [Wald-Föhre](https://www.infoflora.ch/de/flora/pinus-sylvestris.html) ·
  [Arve](https://www.infoflora.ch/de/flora/pinus-cembra.html) ·
  [Europäische Lärche](https://www.infoflora.ch/de/flora/larix-decidua.html).

- **Leaf area index of boreal forests: Theory, techniques, and measurements**,
  Chen et al., JGR 1997, DOI [10.1029/97JD01107](https://doi.org/10.1029/97JD01107).
  [Autorenkopie](https://professorpaul.com/publications/chen_et_al_1997.pdf),
  [PDF](vegetation/botany/1997-leaf-area-index-of-boreal-forests.pdf).
  **Relevanz: Mittel — Bestandsdeckung / #1, #179.**
  Halbe grüne Gesamtfläche je Grundfläche sowie tatsächliche/wirksame LAI und
  Klumpung sauber trennen. Konkrete Referenz für Flächenziele und aggregierte
  Lichtantwort statt bloßer Blattzahlen. Nominale Prototypflächen liefern keine
  gemessene Bestands-LAI; Klumpung, Blickrichtung und Projektionsmodell verhindern,
  dass ein LAI-Wert allein eine opake Deckung oder sichere Verdeckung beweist.
  Weitere Bezüge:
  [NASA-Produktdefinition](https://access.earthdata.nasa.gov/collections/C2222147000-LPCLOUD).

- **Modeling the Mighty Maple**, Bloomenthal, SIGGRAPH 1985.
  [Autorenquelle](https://www.unchainedgeometry.com/JBloom/pdf/SIG85-ModelingMightyMaple.pdf) ·
  [PDF](vegetation/siggraph/1985-modeling-the-mighty-maple.pdf).
  **Relevanz: Mittel — Astflächen / #1, #46.**
  Glatte räumliche Achsen und variable Querschnitte als generalisierte Zylinder
  verbinden; nützliche Grundlage für geteilte Ast-/Nadelformen. Stabile Rahmen
  statt Frenet-Singularitäten bei geraden Teilen und Wendepunkten verwenden.
  Dichte Ausgabe begrenzen und passende Stufen vorbereiten; fotografische
  Rinden-/Blatttexturen der Veröffentlichung werden nicht übernommen.
  Weitere Bezüge:
  [#46](https://github.com/Weichwolf/outshine/issues/46).

- **Global and Local Deformations of Solid Primitives**, Barr, SIGGRAPH 1984.
  [Primärmetadata](https://authors.library.caltech.edu/records/99pd2-73028) ·
  [DOI](https://doi.org/10.1145/964965.808573) ·
  [PDF](vegetation/siggraph/1984-global-local-deformations.pdf), Universitätskopie.
  **Relevanz: Mittel — Deformation / #1, #81.**
  Jacobi-Matrix und inverse-transponierte beziehungsweise Kofaktor-Normalenabbildung
  begründen kohärente eigene Rinden-/Astdeformation. Ableitungen gegen Differenzen
  und Normalen gegen unabhängige lineare Lösung prüfen. Singularitäten und
  Invertierbarkeit beachten; lokale Deformation liefert weder Boolean-Weld noch
  allgemeinen Kollisionsnachweis. Vorbereitung gemeinsam statt je Instanz wiederholen.
  Weitere Bezüge:
  [#81](https://github.com/Weichwolf/outshine/issues/81).

- **Computation of Rotation Minimizing Frames**, Wang / Jüttler / Zheng / Liu, TOG 2008.
  [Autorenpreprint](https://www.ag.jku.at/pubs/2007wjzl.pdf) ·
  [PDF](vegetation/tog/2008-rotation-minimizing-frames-preprint.pdf), ungesetzte Vorabfassung.
  **Relevanz: Mittel — Räumliche Rahmen / #1, #27, #103.**
  Doppelreflexion liefert twistarme Rahmen entlang regulärer Kurven. Gegen eigene
  Tangentendrehung an identischen Ast-/Frondachsen vergleichen: Rahmenfehler,
  Vorbereitungskosten und sichtbare Querschnitte. Differentialnormalen müssen zur
  tatsächlichen Kurvenabbildung passen. Lokale Regularität ist kein Kollisions-
  oder Kraftnachweis; vorbereitete Rahmen zwischen Instanzen teilen.
  Weitere Bezüge:
  [#27](https://github.com/Weichwolf/outshine/issues/27) ·
  [#103](https://github.com/Weichwolf/outshine/issues/103).

- **Efficient Implementation of Marching Cubes' Cases with Topological Guarantees**,
  Lewiner / Lopes / Vieira / Tavares, JGT 2003.
  [Autorenquelle](https://thomas.lewiner.org/pdfs/marching_cubes_jgt.pdf) ·
  [PDF](vegetation/jgt/2003-marching-cubes-topological-guarantees.pdf), Autorenpreprint.
  **Relevanz: Mittel — Implizite Gabeln / #1, #28.**
  Mehrdeutige Zellfälle mit Flächen-/Innerentests auflösen, um gemeinsame
  Astgabeloberflächen als Vergleich zu erzeugen. Die Topologiegarantie betrifft
  die trilineare Gitterinterpolation, nicht die unbekannte kontinuierliche Fläche.
  Abtastung, Speicher und Kontinuitätsfehler extra begrenzen; kein Vollgitter je
  Pflanzeninstanz und kein notwendiger Zwischenschritt für Fernprodukte.
  Weitere Bezüge:
  [#28](https://github.com/Weichwolf/outshine/issues/28).

- **Interval Arithmetic and Recursive Subdivision for Implicit Functions and
  Constructive Solid Geometry**, Duff, SIGGRAPH 1992.
  [Primärquelle im MIT-Kursarchiv](https://fab.cba.mit.edu/classes/S62.12/docs/Duff_interval_CSG.pdf) ·
  [PDF](vegetation/siggraph/1992-interval-arithmetic-csg.pdf).
  **Relevanz: Mittel — Feldbedarf / #1, #28.**
  Intervallgrenzen und rekursive Gebietsunterteilung können unbetroffene Blöcke vor
  teurer Feldabtastung verwerfen. Konkreter Hebel für lokale implizite Gabeln.
  Alle Operationen einschließlich Rundung müssen einschließen; Punktproben allein
  reichen nicht. Rekursion, Unterteilungszahl und Scratch begrenzen. Keine kostenlose
  weltweite Verdeckung oder von Szenentiefe unabhängige Laufzeit daraus ableiten.
  Weitere Bezüge:
  [#28](https://github.com/Weichwolf/outshine/issues/28).

- **Interactive and Robust Mesh Booleans**, Cherchi / Pellacini / Attene / Livesu,
  SIGGRAPH Asia / TOG 2022. [Autorenquelle](https://arxiv.org/abs/2205.14151) ·
  [PDF](vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf), Autorenpreprint.
  **Relevanz: Mittel — Gemeinsame Oberflächen / #1, #29, #88.**
  Konforme Oberflächenzerlegung und Innen-/Außenklassifikation begründen robuste
  Vergleiche vollständiger Ast-/Baukörperflächen. Exakte beziehungsweise implizite
  Schnittkoordinaten kosten Aufbau und Speicher; begrenzte Eingaben vorbereiten.
  Manifold nutzt ein eigenes Verfahren. Dessen Bibliotheksgarantien ersetzen weder
  lokale Prüfung und organische Ausrundung noch gültige gerundete Ausgabe.
  Weitere Bezüge:
  [#29](https://github.com/Weichwolf/outshine/issues/29).

- **Embree: A Kernel Framework for Efficient CPU Ray Tracing**, Wald / Woop / Benthin /
  Johnson / Ernst, SIGGRAPH / TOG 2014.
  [Autorenquelle](https://www.sci.utah.edu/~wald/Publications/2014/embree/embree.pdf) ·
  [PDF](vegetation/siggraph/2014-embree-ray-tracing.pdf).
  **Relevanz: Mittel — CPU-Referenzabfragen / #1, #29.**
  BVH-/SIMD-Strahlabfragen sind ein Vergleich zu skalaren Elternflächenabfragen
  bei Astnormalen und Referenzbildern. Aufbau, Aktualisierung, Primitive-Kopien
  und Peak-Speicher mitbezahlen; statische Beschleunigung zwischen Abfragen teilen.
  Die Veröffentlichung behandelt damalige x86-Architekturen; publizierte Faktoren
  belegen weder A18-Leistung noch eine Pflichtbibliothek oder günstiges GPU-Streaming.
  Weitere Bezüge:
  [#29](https://github.com/Weichwolf/outshine/issues/29).

- **Improving Noise**, Perlin, SIGGRAPH2002.
  [Autorenquelle](https://mrl.cs.nyu.edu/~perlin/paper445.pdf) ·
  [PDF](clouds/siggraph/2002-improving-noise.pdf).
  **Relevanz: Mittel — Prozedurale Felder / #6, #7, #447.**
  Verbesserte Gittergradienten und glattere Interpolation reduzieren sichtbare
  Ableitungsartefakte. Nützlicher Baustein für kompakte Material-/Wolkendichtefelder.
  Relative Koordinaten, Advektion und große Weltkoordinaten gesondert numerisch
  prüfen. Glattheit ist keine Pixelfilterung; Oktaven, Samples und geteilte
  Texturbandbreite begrenzen. Keine native Präzisionsabnahme aus dem Paper.
  Weitere Bezüge:
  [#447](https://github.com/Weichwolf/outshine/issues/447).

- **The Real-time Volumetric Cloudscapes of Horizon Zero Dawn**, Schneider / Vos,
  SIGGRAPH-Kurs 2015.
  [Quelle](https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf) ·
  [PDF](clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf).
  **Relevanz: Mittel — Wolken / #7.**
  Der Kurs kombiniert geteilte 3D-Rauschtexturen, Höhen-/Formprofile und Raymarching
  in einer sphärischen Wolkenschicht. Kandidat für plausible Wolken aus deklariertem
  Wetter ohne globale Fluidsimulation. Dichte-/Lichtproben, Bandbreite, Schatten und
  zeitliche Stabilität gemeinsam messen. Historische PS4-Zahlen und verkleinerter
  Planetenradius sind keine Geräteabnahme oder physikalische Skalenvorgabe.

- **Animating Rotation with Quaternion Curves**, Shoemake, SIGGRAPH 1985.
  [Quelle](https://doi.org/10.1145/325165.325242) ·
  [PDF](animation/siggraph/1985-rotation-quaternion-curves.pdf).
  **Relevanz: Mittel — Kamera/Animation / #2, #811.**
  Quaternionprodukt und sphärische Interpolation begründen kohärente Rotationen.
  Konkreter Verbraucher ist die analytische Kamerarotation aus Framezeit statt
  gespeicherter Ansichten. Vorzeichen, kurze Bögen und Matrix-/Multiplikations-
  konventionen gegen unabhängige Achsrotationen prüfen. Quaternionkurven ersetzen
  keine Gelenk-/Kontaktsteuerung; unser Kameraweg implementiert keine Splines.

- **XPBD: Position-Based Simulation of Compliant Constrained Dynamics**, Macklin et al.,
  Motion in Games 2016. [Quelle](https://mmacklin.com/xpbd.pdf) ·
  [PDF](physics/mig/2016-xpbd.pdf).
  **Relevanz: Mittel — Nahe flexible Körper / #2, #41.**
  Zeitschrittbezogene Compliance und fortgeschriebene Multiplikatoren erlauben
  vergleichbarere Steifheit als gewöhnliches PBD. Kandidat für begrenzte Seil-/
  Stoff-/Pflanzenkontakte, kein allgemeiner Weltphysik-Ersatz. Iterationen, Substeps,
  Kontaktzahl und Fehler messen; vollständige Konvergenz ist nicht kostenlos.
  Render-LOD darf nötige Funktions-/Simulationsdaten nicht entfernen.

- **Synthesizing Sounds from Rigid-Body Simulations**, O'Brien / Shen / Gatchalian,
  ACM SIGGRAPH Symposium on Computer Animation 2002.
  [Quelle](https://jamesobrien.com/papers/Obrien-SSR-2002-07/Obrien-SSR-2002-07.pdf) ·
  [PDF](audio/sca/2002-rigid-body-sound-synthesis.pdf).
  **Relevanz: Mittel — Kontaktklang / #2.**
  Kontaktkräfte regen vorbereitete elastische Schwingungsmoden an. Wenige geteilte
  Moden und analytische zeitliche Antworten sind ein konkreter Vergleich für
  Materialklänge. Kleine lineare Schwingungen, Geometrie-/Materialparameter und
  Modalaufbau begrenzen die Aussage; keine Vollmesh-Eigenanalyse je Kontakt.
  Stimmenzahl, hörbarer Gewinn und Spatial-Audio-Kosten getrennt budgetieren.

## Relevanz: Gering

- **Tutorial 15: Publish web scenes**, Esri 2013, Produktdokumentation.
  [Quelle](https://resources.arcgis.com/en/help/pdf/cityengine/10.2/tutorial_15_publish_web_scenes.pdf) ·
  [PDF](buildings/esri/2013-interior-floors-web-scenes.pdf).
  **Relevanz: Gering — Produktdokumentation / #8, #30.**
  Geschosslayer und Schnittansichten sind ein visueller Vergleich für darstellbare
  Innenräume. Die Anleitung beschreibt Webexport und Bedienung eines vorhandenen
  Beispiels, keinen automatischen Raumplaner oder geometrischen Gültigkeitsnachweis.
  ArcGIS-/Webscene-Pipeline gehört nicht in unser Produkt; bei Raumverfahren
  sind die Layout-/Zugänglichkeitsquellen aussagekräftiger.
  Weitere Bezüge:
  [#30](https://github.com/Weichwolf/outshine/issues/30).

- **Leaf anatomy of Pinus thunbergii**, Ghimire et al., KJPT 2014.
  [Primärquelle](https://doi.org/10.11110/kjpt.2014.44.2.91) ·
  [PDF](vegetation/botany/2014-pinus-thunbergii-leaf-anatomy.pdf).
  **Relevanz: Gering — Botanische Nahform / #1, #46.**
  Mikroskopische Querschnitte begründen flache adaxiale und gekrümmte abaxiale
  Nadelseiten als Formreferenz für eigene Sektorflächen. Nur bei ausreichender
  Nahauflösung sichtbar; ferne Bestände brauchen keine Nadelquerschnitte.
  Artspezifische Anatomie ist keine universelle Koniferenform, Zellmodellierung
  und Texturübernahme sind nicht erforderlich.
  Weitere Bezüge:
  [#46](https://github.com/Weichwolf/outshine/issues/46).

- **Projection onto the Probability Simplex**, Wang / Carreira-Perpiñán, arXiv 2013.
  [Autorenquelle](https://arxiv.org/abs/1309.1541) ·
  [PDF](vegetation/math/2013-probability-simplex-projection.pdf).
  **Relevanz: Gering — Mathematischer Teilbaustein / #1, #28.**
  Endliche Sortierprojektion liefert gültige nichtnegative Gewichte fester Summe
  für die eigene symmetrische Feldmischung. Gegen unabhängige QP-/Gradienten-
  prüfung absichern. Die Optimalität gilt für dieses Projektionsproblem,
  nicht für Astgeometrie, Weltplanung oder Kosten. Kein eigenständiges Vegetations-
  oder SpeedTree-Verfahren; nur bei tatsächlich benötigter Mischung einsetzen.

- **Modeling and visualization of leaf venation patterns**, Runions et al., SIGGRAPH 2005.
  [Autoren-PDF](https://www.algorithmicbotany.org/papers/venation.sig2005.pdf) ·
  [PDF](vegetation/siggraph/2005-leaf-venation.pdf).
  **Relevanz: Gering — Blatt-Nahdetail / #1, #42.**
  Attraktoren und Entfernungsregeln erzeugen Adernetze, verbundene Radien steuern
  Struktur. Nützlich für wenige geteilte Blattrezepte/Materialfelder. Unser offenes
  Netz mit fester Kontur deckt Blattwachstum und geschlossene Maschen nicht ab.
  Wachstum/Nachbarsuche nicht je Blattinstanz wiederholen; unterpixelgroße Adern
  filtern, statt zusätzliche Geometrie oder Vollsolver zu laden.
  Weitere Bezüge:
  [#42](https://github.com/Weichwolf/outshine/issues/42).

- **DeepMimic: Example-Guided Deep Reinforcement Learning of Physics-Based Character Skills**,
  Peng et al., SIGGRAPH 2018.
  [Quelle](https://xbpeng.github.io/projects/DeepMimic/DeepMimic_2018.pdf) ·
  [PDF](animation/siggraph/2018-deepmimic.pdf).
  **Relevanz: Gering — NPC-Spezialsteuerung / #2.**
  Referenzbewegungen plus Training erzeugen physikbasierte Skills und lokale
  Gelenkmotor-Ziele. Forschungsoption für späteren Nahcharakter-Vergleich, kein
  fertiger allgemeiner NPC. Lizenzierte Referenzen, Training, Inferenz und
  Körper-/Kontaktsimulation sind eigene Kosten; zuerst begrenzte ausführbare
  Steuerung. Ein LLM-Ziel ist keine unmittelbar gültige Renderpose.

- **LDI Tree: A Hierarchical Representation for Image-Based Rendering**, Chang / Bishop / Lastra, SIGGRAPH 1999.
  [Publikation](https://doi.org/10.1145/311535.311571) ·
  [Universitätsarchiv](https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/chang99.pdf) ·
  [PDF](geometry/siggraph/1999-ldi-tree.pdf).
  **Relevanz: Gering — Räumliche Tiefenhierarchie / #899.**
  Hierarchische Abtastung ist mathematisch nützlich; unser orthographischer
  Gittervergleich [#902](https://github.com/Weichwolf/outshine/issues/902) verliert jedoch
  bei Coverage/Materialgrenzen und Speicher gegen die Originale. Dieses Profil nicht
  portieren oder durch weitere Layer-/Packingrunden retten. Andere Hierarchien brauchen
  einen neuen Bild-/Kostenbeleg; der negative Versuch widerlegt nicht jede LDI-Variante.

- **Taming the Shadow Terminator**, Chiang / Li / Burley, SIGGRAPH Talks 2019.
  [Autorenquelle](https://www.yiningkarlli.com/projects/shadowterminator.html) ·
  [PDF](vegetation/siggraph/2019-taming-shadow-terminator.pdf), korrigierte Autorenfassung.
  **Relevanz: Gering — Schattennormalen-Spezialfall / #29, #4.**
  Gezielter Schattenfaktor mildert den Übergang zwischen geometrischer und
  abweichender Schattierungsnormale. Als Diagnosevergleich nützlich, wenn genau
  dieser Fehler auftritt; behebt keine fehlende Geometrie oder grobe Schattenkarte.
  Die modellierte Mikroverschattung ist nicht echter Relieftransport. EEVEE-
  Positionsoffsets sind ein anderes Verfahren, kein Beleg einer eigenen Übernahme.

## Relevanz: Nicht relevant

- **Stereo Magnification: Learning view synthesis using multiplane images**,
  Zhou / Tucker / Flynn / Fyffe / Snavely, SIGGRAPH 2018.
  [Publikation](https://doi.org/10.1145/3197517.3201323) ·
  [Autoren-PDF](https://tinghuiz.github.io/papers/siggraph18_mpi_lowres.pdf) ·
  [PDF](geometry/siggraph/2018-stereo-magnification.pdf).
  **Relevanz: Nicht relevant — Gelernte Ansichtsrekonstruktion / #899.**
  Das veröffentlichte Gesamtverfahren braucht Stereo-Fotos, trainierte Rekonstruktion
  und RGB-Schichten; das passt nicht zu Outshines prozeduralen Assets und aktuellem
  PBR/Wetter. Geometrische Verankerung/Alpha-Komposition bleiben Vergleichsmathematik,
  begründen aber weder Modellübernahme noch Welt-/Residenzfreigabe.

- **Color Compatibility From Large Datasets**, O'Donovan / Agarwala / Hertzmann,
  SIGGRAPH / ACM TOG 2011.
  [Autoren](https://www.dgp.toronto.edu/~donovan/color/) ·
  [Quelle](https://www.dgp.toronto.edu/~donovan/color/colorcomp.pdf) ·
  [PDF](presentation/siggraph/2011-color-compatibility.pdf).
  **Relevanz: Nicht relevant — Gelernte Palettenpräferenz / #4, #6.**
  Bewertungen von Fünffarben-Paletten modellieren menschliche Präferenzen und
  Datensatz-/Benennungsbias, keine physikalischen Materialwerte oder Beleuchtung.
  Outshine besitzt keine vorgegebene Stilpalette; deshalb diese Auswahl nicht auf
  Weltmaterialien übertragen und keine Trainingsdaten/Modelle übernehmen.
  Separater Autorenartikel, kein Paper: [Oklab/OKLCH, Ottosson 2020](https://bottosson.github.io/posts/oklab/).
  Wahrnehmungsnahe Abstände können Diagnose/UI unterstützen; physikalische
  Beleuchtung und Materialmischung bleiben in linearem RGB.

## Bewertung ausstehend

Ein Eintrag ohne Relevanzurteil; fehlenden Volltext nicht durch Abstract oder
Implementierungswissen ersetzen.

- **Computing Vertex Normals from Polygonal Facets**, Thürmer / Wüthrich, JGT 1998.
  [Primärabstract](https://doi.org/10.1080/10867651.1998.10487487).

  **Volltext-/Abbildungsprüfung offen.** Der Primärabstract beschreibt Eckwinkel-
  gewichtung; der Verlags-PDF-Abruf liefert 403. Der Bezug zu eigener lokaler
  Differentialnormalenmittelung in [#40](https://github.com/Weichwolf/outshine/issues/40)
  ist keine Volltextabnahme. Auch der ergänzende Max-1999-
  [Autorenpreprint](https://escholarship.org/uc/item/7657d8h3) ist bisher nur textlich
  gelesen; der lokale PDF-Abruf blieb ohne Dokumentdaten, optische Prüfung offen.
  Die Unterscheidung von Oberflächenapproximation und Unterteilungserhaltung
  bleibt zu prüfen. Quellenanfrage und Rest besitzt [#919](https://github.com/Weichwolf/outshine/issues/919).

## Weitere Primärquellen und Fachnotizen

### Assets und Sichtbedarf — WI [#10](https://github.com/Weichwolf/outshine/issues/10) / [#12](https://github.com/Weichwolf/outshine/issues/12)

[Vorbereitete Assets, räumliche Pakete und Residency](engine/unreal/prepared-assets.md)
fasst Unreal DDC, World Partition/HLOD, Texture Streaming und Geometry Clipmaps zusammen.
Die Notiz verlinkt die Primärquellen, grenzt deren Übertragung ab und trennt SSD-Gültigkeit
von RAM-/GPU-Residency und budgetierten Laufzeitdetails. Epic-Referenzen sind Online-Dokumentation;
das Clipmaps-Paper liegt bereits als PDF unter Terrain/SIGGRAPH.

[Hierarchischer Bedarf vor Download und Geometrie](engine/visibility-driven-demand.md)
prüft konkrete MapLibre-/Cesium-Codepfade und grenzt die Übertragung auf Outshine ab.

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
| Weltweite Auswahl — [#12](https://github.com/Weichwolf/outshine/issues/12) | [Cesium Native: Auswahl](https://cesium.com/learn/cesium-native/ref-doc/selection-algorithm-details.html) | Elternabdeckung bis Kindpublikation, projizierter Fehler; Blick-Culling getrennt von Rundum-Residency |
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
