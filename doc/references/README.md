# Forschungsgrundlage für Outshine

Stand: 2026-10-07. Primärquellen: Autoren, Forschungsgruppen, Kursveranstalter und Normgeber.
30 Publikationen sind lokal als PDF vorhanden. SIGGRAPH-Papers, Kurse und verwandte
Veröffentlichungen sind getrennt bezeichnet. Das ist eine kuratierte Grundlage, keine
vollständige Literaturübersicht oder Bestätigung der Machbarkeit auf A18 Pro.

Die folgenden Empfehlungen sind Architekturentscheidungen für die Feature-WIs;
eine Literaturquelle ersetzt weder Integration noch Bild- und Laufzeitbelege.
Reihenfolge aus WI 2169: Gebäude → Terrain → Infrastruktur → Vegetation → Wolken;
Simulation und Gameplay folgen dem visuellen Meilenstein.

## PDFs und Nutzung

Alle PDFs liegen nach Bereich/Konferenz direkt unter `doc/references/` und sind versioniert.
Die kanonischen Downloadlinks stehen bei jeder Quelle.
Kein Fremdcode, Texturpaket oder Trainingsdatensatz wird übernommen.

## Asset-Laden und Residency — WI 2280

[Vorbereitete Assets, räumliche Pakete und Residency](engine/unreal/prepared-assets.md)
fasst Unreal DDC, World Partition/HLOD, Texture Streaming und Geometry Clipmaps zusammen.
Die Notiz verlinkt die Primärquellen, grenzt deren Übertragung ab und trennt SSD-Gültigkeit
von RAM-/GPU-Residency und budgetierten Laufzeitdetails. Epic-Referenzen sind Online-Dokumentation;
das Clipmaps-Paper liegt bereits als PDF unter Terrain/SIGGRAPH.

## Gebäude und kompakte Fassaden — WI 2173 / 2171

- **Instant Architecture**, Wonka et al., SIGGRAPH 2003.
  [Quelle](https://peterwonka.net/Publications/pdfs/2003.SG.Wonka.InstantArchitecture.high.pdf) ·
  [PDF](buildings/siggraph/2003-instant-architecture.pdf).
  Begrenzte Split-Regeln strukturieren Fassaden. Regeln erst aus geliefertem Grundriss,
  Höhe und Nutzung ableiten; sie rekonstruieren keine unbekannte reale Fassade.
- **Procedural Modeling of Buildings**, Müller et al., SIGGRAPH 2006.
  [Quelle](https://peterwonka.net/Publications/pdfs/2006.SG.Mueller.ProceduralModelingOfBuildings.final.pdf) ·
  [PDF](buildings/siggraph/2006-procedural-modeling-of-buildings.pdf).
  Ein gemeinsamer Gebäudeplan steuert Körper, Dach und Nahmodule. Kein allgemeiner
  CGA-Interpreter; bekannte Sonderklassen dürfen keine Wohnhausregeln erhalten.
- **Grammar-based Encoding of Facades**, Haegler et al., EGSR 2010.
  [Quelle](https://peterwonka.net/Publications/pdfs/2010.EGSR.Haegler.GrammarBasedEncoding.pdf) ·
  [PDF](buildings/egsr/2010-grammar-based-encoding-of-facades.pdf).
  Wiederholungen kompakt parametrisieren und im Shader auswerten. Nahsilhouette,
  Laibungen und betretbare Räume brauchen Geometrie; Fernmuster benötigen Filterung.

## Geometrie, Sichtbarkeit und Terrain — WI 2336 / 2337 / 2338

[Hierarchischer Bedarf vor Download und Geometrie](engine/visibility-driven-demand.md)
prüft konkrete MapLibre-/Cesium-Codepfade und grenzt die Übertragung auf Outshine ab.
- **GigaVoxels: Ray-Guided Streaming for Efficient and Detailed Voxel Rendering**,
  Crassin et al., I3D 2009.
  [Quelle](https://www-sop.inria.fr/reves/Basilic/2009/CNLE09/CNLE09.pdf) ·
  [PDF](geometry/i3d/2009-gigavoxels-ray-guided-streaming.pdf).
  Sicht-/LOD-Traversierung meldet nur benötigte fehlende Hierarchieknoten. Anfragebündelung
  und gröbere Ersatzdaten übernehmen; keine HTTP-Anfrage je Pixel und keine Pflicht zur Voxelwelt.
- **Simplification Envelopes**, Cohen et al., SIGGRAPH 1996.
  [Quelle](https://www.cs.umd.edu/gvil/papers/simp_env.pdf) ·
  [PDF](geometry/siggraph/1996-simplification-envelopes.pdf).
  Beidseitige Oberflächenabstände über begrenzende Hüllen sichern. Das Prinzip ergänzt
  unsere Formfehlerverträge; Hausdorff-Nähe folgt nicht aus wenigen Samples oder einem
  meshoptimizer-Fehlerwert. Offene/nichtmannigfaltige Baukörper und Kontakte eigens behandeln.
- **GPU-Driven Rendering Pipelines**, Haar / Aaltonen, SIGGRAPH-Kurs 2015.
  [Quelle](https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf) ·
  [PDF](geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf).
  Material-Batches, kompakte Instanzen und Cluster mit Bounds statt Einzelobjekt-Draws.
  Bedarf vor Mesh-Erzeugung bleibt CPU-/Generatorarbeit; GPU-Culling spart keine bereits
  erzeugten Bytes. Indirect-/Compute-Pfade nur bei passendem SDL_GPU-Backend und Messgewinn.
- **Geometry Clipmaps: Terrain Rendering Using Nested Regular Grids**, Losasso / Hoppe,
  SIGGRAPH 2004. [Quelle](https://hhoppe.com/geomclipmap.pdf) ·
  [PDF](terrain/siggraph/2004-geometry-clipmaps.pdf).
  Regelmäßige verschachtelte Gitter und inkrementelle Randaktualisierung vermeiden Vollneubau.
  Mit GroundLattice vergleichen, keine zweite Terrainpipeline. Clipmaps allein liefern weder
  weltweite Quellabdeckung noch Ellipsoidübergang oder konservative Höhenfehlerschranken.
- **Procedural Noise using Sparse Gabor Convolution**, Lagae et al., SIGGRAPH 2009.
  [Quelle](https://graphics.cs.kuleuven.be/publications/LLDD09PNSGC/LLDD09PNSGC_paper.pdf) ·
  [PDF](materials/siggraph/2009-sparse-gabor-noise.pdf).
  Richtung und Frequenzspektrum explizit steuern, anisotrop aus dem Pixel-Footprint filtern.
  Gerichtete Felsstruktur prototypisieren; Kernelkosten gegen einfachere gefilterte Noise
  messen. Noise liefert keine belegte Geologie und keinen Ersatz für DEM-Silhouette.

## Materialien, Licht und Bildstabilität — WI 2171 / 2155

- **Physically Based Shading at Disney**, Burley, SIGGRAPH-Kurs 2012.
  [Quelle](https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf) ·
  [PDF](materials/siggraph/2012-disney-physically-based-shading.pdf).
  Kleine verständliche Parameterfamilie aus gemessenen Reflexionen; Metallic/Roughness
  als gemeinsame Gestaltungssprache. Unser Khronos-BRDF bleibt verbindlich; Disney-Modell
  und zusätzliche Lobes nicht ungeprüft übernehmen.
- **Real Shading in Unreal Engine 4**, Karis, SIGGRAPH-Kurs 2013.
  [Quelle](https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf) ·
  [PDF](materials/siggraph/2013-unreal-engine-4-shading.pdf).
  BaseColor/Metallic/Roughness und gemeinsame Materialbibliothek vereinheitlichen den Look.
  Masken vor Licht auswerten; begrenzte Parameterblend spart BRDF-Arbeit, ist aber keine
  exakte Mischung beliebiger Reflexionsmodelle. Lack/Metall und extreme Roughness prüfen.
- **Crafting a Next-Gen Material Pipeline for The Order: 1886**, Neubelt / Pettineo,
  SIGGRAPH-Kurs 2013.
  [Quelle](https://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf) ·
  [PDF](materials/siggraph/2013-the-order-material-pipeline.pdf).
  Gemeinsame Templates, Masken und Materialkomposition für Schmutz, Mörtel und Nässe.
  Die Pipeline ist keine Metallic-Roughness-Spezifikation. Offline-Texturbakes/Fotoerwerb
  nicht übernehmen; unsere Masken/Parameter bleiben prozedural und begrenzt.
- **Surface Gradient-Based Bump Mapping Framework**, Mikkelsen, JCGT 9(3), 2020.
  [Quelle](https://jcgt.org/published/0009/03/04/paper.pdf) ·
  [PDF](materials/jcgt/2020-surface-gradient-bump-mapping.pdf).
  Höhen/Normalen aus verschiedenen UVs, Triplanar- und Decal-Projektionen im gemeinsamen
  Oberflächengradientenraum komponieren. Finale Normale einmal rekonstruieren, gemeinsame
  Filterung/Roughness prüfen. Bump ersetzt weder Silhouettengeometrie noch Materialschichtung.
- **Color Compatibility From Large Datasets**, O'Donovan / Agarwala / Hertzmann,
  SIGGRAPH / ACM TOG 2011.
  [Autoren](https://www.dgp.toronto.edu/~donovan/color/) ·
  [Quelle](https://www.dgp.toronto.edu/~donovan/color/colorcomp.pdf) ·
  [PDF](presentation/siggraph/2011-color-compatibility.pdf).
  Gelernte Bewertungen von Fünffarben-Paletten sind Gestaltungshilfen, kein universelles
  Schönheitsgesetz. Grau/Beige, Erde, Grün und Blau gemeinsam statt je Generator wählen;
  Flächenanteile, Licht und Szene zusätzlich beurteilen. Keine Trainingsdaten/Modelle übernehmen.
  [Oklab/OKLCH, Ottosson 2020](https://bottosson.github.io/posts/oklab/) ergänzt wahrnehmungsnahe
  Helligkeits-/Chroma-/Farbtonabstände bei Vorbereitung; es ist ein Autorenartikel, kein Paper.
  Beleuchtung und physikalische Materialmischung erfolgen weiterhin in linearem RGB.
- **Moving Frostbite to Physically Based Rendering**, Lagarde / de Rousiers,
  SIGGRAPH-Kurs 2014, Kursnotizen Revision 3 (2015).
  [Quelle](https://seblagarde.wordpress.com/wp-content/uploads/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf) ·
  [PDF](lighting/siggraph/2014-frostbite-pbr-course-notes.pdf).
  BRDF, Roughness, Lichtgrößen, Reflexion, Farbraum und Kameraantwort gemeinsam kalibrieren.
  Vorhandenen Khronos-Vertrag erhalten; Parameter/IBL vor neuen Passketten verbessern.
  Referenzverfahren separat prüfen, ihre Kosten sind kein Runtime-Budget.
- **A Survey of Temporal Antialiasing Techniques**, Yang / Liu / Salvi, CGF / Eurographics 2020.
  [Autoren](https://research.nvidia.com/labs/rtr/publication/yang2020survey/) ·
  [Verlags-PDF](https://diglib.eg.org/server/api/core/bitstreams/53732e70-b64d-46f4-bbae-865eb7673a35/content) ·
  [PDF](presentation/cgf/2020-temporal-antialiasing-survey.pdf).
  Bewegungsvektoren, History-Gültigkeit und Rekonstruktion gemeinsam behandeln.
  Neue Produkte, Ursprungssprünge, Disocclusion, Wasser und Wind sind eigene Lastfälle;
  längere History darf Detailverlust und Geisterbilder nicht als Stabilität kaschieren.
- **Moment Shadow Mapping**, Peters / Klein, ACM SIGGRAPH I3D 2015.
  [Autoren/Errata](https://momentsingraphics.de/I3D2015.html) ·
  [korrigierte Autorenfassung](https://momentsingraphics.de/Media/I3D2015/MomentShadowMapping.pdf) ·
  [PDF](lighting/i3d/2015-moment-shadow-mapping.pdf).
  Vier filterbare Tiefenmomente gegen PCF vergleichen; zunächst bestehende Tiefenschatten
  stabilisieren und filtern. Vier 16-Bit-Kanäle brauchen bei 2048² bereits
  2048² × 8 / 2²⁰ = 32 MiB ohne Mips, Blur-Ziele und Tiefe. Ein einzelner Lookup bedeutet
  keinen kostenlosen Pass. Quantisierungs-/Moment-Bias und Light Bleeding eigens prüfen.
- **Clustered Deferred and Forward Shading**, Olsson / Billeter / Assarsson, HPG 2012.
  [Quelle](https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf) ·
  [PDF](lighting/hpg/2012-clustered-shading.pdf).
  Lokale Lichter anhand räumlicher Cluster zuordnen statt alle Lichter je Fragment zu prüfen.
  Sonne/Himmelslicht separat behandeln; transparente Flächen brauchen gültige Clusterlisten.
  Listenaufbau, Speicher und Überlauf gehören zum Vertrag. Für wenige Lichter bleibt der
  einfache Pfad; Cluster erst mit einem gemessenen Nacht-/Innenraumlastfall integrieren.
- **Efficient GPU Screen-Space Ray Tracing**, McGuire / Mara, JCGT 2014.
  [Quelle](https://jcgt.org/published/0003/04/04/) ·
  [PDF-Quelle](https://jcgt.org/published/0003/04/04/paper.pdf) ·
  [PDF](lighting/jcgt/2014-efficient-screen-space-rays.pdf).
  Perspektivkorrekte DDA vermeidet redundante Pixelabfragen beim Strahlmarsch.
  Begrenzte Schritte und konsistente Tiefe/Dickenannahmen; fehlende verdeckte oder außerhalb
  des Bildes liegende Flächen bleiben eine grundlegende Grenze. Gefiltertes IBL liefert
  die Grundreflexion, SSR ergänzt gültige Treffer. Gemeinsamer Pfad für Wasser und Fassaden.

## Straßen, Wasser und technische Objekte — WI 2281 / 2145 / 2338

- **Interactive Procedural Street Modeling**, Chen et al., SIGGRAPH 2008.
  [Quelle](https://peterwonka.net/Publications/pdfs/2008.SG.Chen.InteractiveProceduralStreetModeling.pdf) ·
  [PDF](infrastructure/siggraph/2008-interactive-procedural-street-modeling.pdf).
  Graphbearbeitung von Geometrie trennen. OSM-Verbindungen und Ebenen bleiben maßgeblich;
  die Tensorfeld-Netzerzeugung ersetzt unser reales Straßennetz nicht. Brückentragwerke
  benötigen zusätzlich klare Anschluss-/Kontaktregeln und kompakte wiederholte Bauteile.
- **Wave Particles**, Yuksel / House / Keyser, SIGGRAPH 2007.
  [Quelle](https://www.cemyuksel.com/research/waveparticles/waveparticles.pdf) ·
  [PDF](water/siggraph/2007-wave-particles.pdf).
  Interaktive Wellen für nahe Körper-/Uferwirkung prüfen; ferne Wellen analytisch darstellen.
  Das Verfahren löst weder Quellpegel noch Küstenabschluss. Erst Wasser/Bett/Ufer korrigieren,
  danach Animation auf demselben Körper; kein globales Grundwassermesh.

## Vegetation, Atmosphäre und Wetter — WI 2111 / 2172

- **Physically Based Real-Time Translucency for Leaves**, Habel / Kusternig / Wimmer,
  EGSR 2007.
  [Quelle](https://www.cg.tuwien.ac.at/research/publications/2007/Habel_2007_RTT/Habel_2007_RTT-Preprint.pdf) ·
  [PDF](vegetation/egsr/2007-real-time-leaf-translucency.pdf).
  Reflexion und Blatttransmission mit kompakter gerichteter Basis auswerten. Eigene
  prozedurale Blattparameter statt fotografischer Mess-/Texturdaten; Instanzen, Schatten
  und Wind teilen die Lichtwelt. Ein Blattmodell allein behebt keinen fehlenden Stammkontakt.
- **Stochastic Transparency**, Enderton / Sintorn / Shirley / Luebke, TVCG 2011,
  erweiterte Fassung der I3D-Arbeit 2010.
  [Quelle](https://research.nvidia.com/sites/default/files/pubs/2011-08_Stochastic-Transparency/stochtransp-tvcg.pdf) ·
  [PDF](vegetation/tvcg/2011-stochastic-transparency.pdf).
  Stochastische Subpixel-Abdeckung liefert korrekte Alpha-Mischung im Mittel bei begrenztem
  Speicher, erzeugt aber Rauschen. Gegen einfaches Alpha-Test/Dither samt Schatten messen;
  MSAA und Akkumulation sind echte Kosten. Keine automatische Wahl für den Zielchip.
  [Computing Alpha Mipmaps, Castaño](https://www.ludicon.com/castano/blog/articles/computing-alpha-mipmaps/)
  beschreibt die näherungsweise Belegungserhaltung bei festem Cutoff durch Skalierung/Bisektion;
  diskrete kleine Mips erlauben nicht immer exakte Deckung. Autorenartikel, kein SIGGRAPH-Paper.
- **Realistic Modeling and Rendering of Plant Ecosystems**, Deussen et al., SIGGRAPH 1998.
  [Quelle](https://algorithmicbotany.org/papers/ecosys.sig98.pdf) ·
  [PDF](vegetation/siggraph/1998-plant-ecosystems.pdf).
  Bestandsplan, Pflanzenform und Darstellung trennen; Prototypen, Gruppen und Organe teilen.
  Fernwald braucht Verbände statt Einzelbaum-/Blattarbeit. Das Offline-Verfahren belegt
  keine Echtzeitkosten; Overdraw, Wind, Schatten und Artenmischung separat integrieren.
- **Billboard Clouds for Extreme Model Simplification**, Décoret et al., SIGGRAPH 2003.
  [Autoren](https://graphics.cs.yale.edu/publications/billboard-clouds-extreme-model-simplification) ·
  [PDF-Quelle](https://graphics.cs.yale.edu/sites/default/files/bc03_0.pdf) ·
  [PDF](vegetation/siggraph/2003-billboard-clouds.pdf).
  Mehrere räumliche Ebenen statt einer flachen Kronenkarte als Fernprototyp vergleichen.
  Alpha-Belegung, Mips, Normalen und Silhouette erhalten; leere Texel und Overdraw können
  den Geometriegewinn aufheben. Aus eigenen prozeduralen Modellen erzeugen, kein Fotoatlas. Gemeinsame Basisprodukte
  als Asset-Rohlinge speichern (2280); Nahdetails und aktuelles Licht/Wind folgen zur Laufzeit.
- **Real-time Realistic Rendering and Lighting of Forests**, Bruneton / Neyret, Eurographics 2012.
  [Publikation](https://doi.org/10.1111/j.1467-8659.2012.03016.x) ·
  [Autoren-PDF](https://maverick.inria.fr/Publications/2011/BN11a/article.pdf) ·
  [PDF](vegetation/eg/2012-real-time-forests.pdf).
  Sicht-/Lichtkorrelation und mittlere Bestandsreflexion über Detailwechsel erhalten.
  Z-Felder und Shader-Maps sind Vergleichsmodelle, keine automatische Wahl: ihre vorbereiteten
  Texturmengen passen nicht ungeprüft zum A18-Pro-Ziel. Horizontbestand aggregiert Deckung,
  Kronenhöhe und Lichtantwort; einzelne Bäume erst bei sichtbarer Formwirkung.
- **A Scalable and Production Ready Sky and Atmosphere Rendering Technique**, Hillaire,
  EGSR 2020. [Quelle](https://sebh.github.io/publications/egsr2020.pdf) ·
  [PDF](atmosphere/egsr/2020-production-ready-atmosphere.pdf).
  Kompakte LUTs verbinden Boden-, Flug- und Orbitansicht mit Luftperspektive.
  Bestehende SkyStage vergleichen/ergänzen; gemeinsame planetare Maße und Lichtgrößen.
  Wetterparametrisierung ist eine plausible Ergänzung, keine gemessene Aerosolverteilung.
- **The Real-time Volumetric Cloudscapes of Horizon Zero Dawn**, Schneider / Vos,
  SIGGRAPH-Kurs 2015.
  [Quelle](https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf) ·
  [PDF](clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf).
  Prozedurale Dichte, adaptive Abtastung und temporale Rekonstruktion budgetieren;
  Wind, Licht und Wolkenschatten nutzen denselben Zustand. Publizierte PS4-Zeiten sind
  kein A18-Pro-Beleg. Keine Wolkenarbeit vor dem vereinbarten Welt-/Vegetationsausbau.

## Physik, Animation, NPCs und Klang — WI 2136

- **XPBD: Position-Based Simulation of Compliant Constrained Dynamics**, Macklin et al.,
  Motion in Games 2016. [Quelle](https://mmacklin.com/xpbd.pdf) ·
  [PDF](physics/mig/2016-xpbd.pdf).
  Zeitschrittbezogene Compliance für Seile, Stoff und Pflanzen prüfen. Bullet bleibt
  Körper-/Kontaktbaseline; begrenzte Iterationen ersetzen keine Genauigkeitsprüfung.
- **DeepMimic: Example-Guided Deep Reinforcement Learning of Physics-Based Character Skills**,
  Peng et al., SIGGRAPH 2018.
  [Quelle](https://xbpeng.github.io/projects/DeepMimic/DeepMimic_2018.pdf) ·
  [PDF](animation/siggraph/2018-deepmimic.pdf).
  Ziele → lokale Steuerung → Gelenkmotoren → Posen; LLM setzt keine Renderpose direkt.
  Das Verfahren benötigt Referenzbewegungen und Training, erzeugt keinen fertigen NPC.
  Zuerst begrenzte klassische Steuerung; Lernverfahren sind keine Meilenstein-Voraussetzung.
- **Synthesizing Sounds from Rigid-Body Simulations**, O'Brien / Shen / Gatchalian,
  ACM SIGGRAPH Symposium on Computer Animation 2002.
  [Quelle](https://jamesobrien.com/papers/Obrien-SSR-2002-07/Obrien-SSR-2002-07.pdf) ·
  [PDF](audio/sca/2002-rigid-body-sound-synthesis.pdf).
  Kontakte/Kräfte treiben Materialresonanzen. Wenige analytische oder im RAM vorbereitete
  Modi statt Vollmesh-Eigenanalyse je Klangereignis; Spatial Audio bleibt eigener Ausgabepfad.
  Stimmen-/CPU-Budget und hörbarer Gewinn bestimmen die Modellkomplexität.
- [Begrenztes Spatial Audio: geprüfter Steam-Audio-/Resonance-Audio-Code](audio/bounded-spatial-audio.md).
  Native Abfragen wiederverwenden; grobe gemeinsame Umgebung, begrenzte Stimmen/Hallbusse.
  Cubemap/GPU sind zu messende Experimente, keine von der Quellenzahl unabhängige Simulation.

## Bereiche mit geeigneteren Primärquellen

| Bereich / WI | Grundlage | Entscheidung für Outshine |
|---|---|---|
| Quellen/HTTP/Cache — 2280 | [HTTP-Semantik, RFC 9110](https://www.rfc-editor.org/rfc/rfc9110) | libcurl; paralleles begrenztes IO, Status-/Retry-/Abbruchverträge; Quellbytes separat, Assetbedarf → Treffer laden / Miss generieren; räumlicher Assetcache mit LOD/Residency |
| Weltweite Auswahl — 2336 | [Cesium Native: Auswahl](https://cesium.com/learn/cesium-native/ref-doc/selection-algorithm-details.html) | Elternabdeckung bis Kindpublikation, projizierter Fehler; Blick-Culling getrennt von Rundum-Residency |
| Import/Material/Animation — 2188 / 2171 / 2136 | [Khronos glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) | Szenario und glTF in dieselben nativen Produkte, keine parallele Assetwelt |
| Backend/Submission — 2188 / 2155 | [SDL3 GPU](https://wiki.libsdl.org/SDL3/CategoryGPU) | Nur verfügbare Backendverträge; Ownership, Synchronisation, tatsächliche GPU-Bytes messen |
| JS/UI/LLM — 2136 | [ECMAScript](https://tc39.es/ecma262/) und vorhandene HTML/CSS-Teilmenge | Begrenzte Commands/Events, lokale ausführbare Steuerung, keine Netzwerkabhängigkeit im Tick |
| Navigation — 2136 | [Recast/Detour](https://recastnav.com/md_Docs_2__1__Introduction.html) | Begehbare Flächen und lokale Pfadkorridore; Straßengraph für Verkehr, eigene Flug-/Wassermodelle statt einer universellen 2D-Navigation |
| Astronomie — 2172 | [IAU SOFA](https://www.iausofa.org/cookbooks) | Zeit-/Koordinatenkonventionen und Beobachterkorrekturen prüfen; Astronomie aus UTC/Ort, nicht als Live-Datenabhängigkeit |
| Parser/Mathematik/Geometrie/Physik | [Systembibliotheken](../dependencies.md) | Bewährte Implementierungen kapseln; keine Library-Typen in Welt, ABI oder Saveformat |
| Save/Replay, Telemetrie, Budgets — 2136 / 2188 / 2169 | Eigene Zustands-/Lebensdauerverträge im WI | Versionierte atomare Zustandsübergänge, aufgezeichnete externe Events; Literatur begründet keine Vollständigkeit |

Offene Implementierungsfragen bleiben im zuständigen WI: Quellsemantik/Sonderbauten,
globale Abdeckung, konservative Formfehler, Schattenqualität, Reflexionssichtbarkeit,
bewegliche Kontakte und lokale NPC-Navigation. Eine Quellenliste schließt diese Features nicht.
