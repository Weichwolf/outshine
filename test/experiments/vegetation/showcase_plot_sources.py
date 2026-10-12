"""Declared plot inputs and captions for the vegetation showcase."""

PLOT_SOURCES = (
    (
        "forks/forks.json",
        "fork_plot",
        "Gemeinsame Astgabeln · Formfehler, Erzeugung und Speicher",
        "Drei Verfahren auf derselben kompakten Verschmelzungsform: gleichmäßige und adaptive radiale Abtastung sowie Lewiner-Marching-Cubes. Je Gabel 355 unabhängige Zufalls-/Ast-/Endstrahlen und dieselbe beobachtete Fehleranforderung von 5 mm. Eine geschlossene, außenorientierte Komponente mit gemeinsamen analytischen Normalen. Drei lokale CPU-Wiederholungen; erste Erzeugung und Feld-/Produktbytes getrennt. Die Stichproben sind keine globale Hausdorff-Schranke oder Modeler-/Runtime-Kostenfreigabe.",
    ),
    (
        "spines/spines.json",
        "spine_plot",
        "Räumliche Astachsen · stabile Querschnitte",
        "Tangentendrehung, direkte Doppelreflexion und Quaternionen-Auswertung an derselben analytischen Helix. Gleicher Startquerschnitt und dieselbe Fehleranforderung; sieben lokale CPU-Läufe. Vier räumliche Astflächen sind zusätzlich gegen unabhängige ODE-Rahmen und Differentialnormalen geprüft. Das Original-Oracle prüft ausschließlich die Orthogonalisierung, keinen vollständigen SpeedTree-Spine-Solver oder dessen Kosten.",
    ),
    (
        "profiles/profiles.json",
        "profile_plot",
        "Radiusprofile und Lookup-Kosten",
        "Vier identische Kurven und 2.048 gemeinsame Abfragen; fünf lokale CPU-Läufe, Tabellenaufbau ausgeklammert. Brent-Einzelinversion, Chandrupatla-Batches und vorgehaltene 1.000er Tabelle. Originale SpeedTree-Routinen prüfen rohe Punkte und skalare Lookup-/Preset-/Zusatz-/Tile-/Skalierungspfade mit kontrollierten Puffern. Keine Modeler-, BRDF-, Wind- oder native Frame-Freigabe.",
    ),
    (
        "render/views-shadow-pass.json",
        "views_render_plot",
        "Eigene Ansichten für Kamera und Schatten",
        "Unveränderte Originalpixel aus festen Kronen- und Schattenausschnitten. Referenz und getrennte Lichtauswahl haben dieselbe Rendererfassung, Holzgeometrie, Kamera, Licht und 32 EEVEE-Samples bei 1280 × 720. Die mittlere Spalte hält den vorherigen gemeinsamen Kamerakarten-Schattenpfad zur Diagnose fest. Ein Bild belegt keine Wind-, Bewegungs-, Material- oder native LOD-Freigabe.",
    ),
    (
        "selection/selection.json",
        "selection_coverage_plot",
        "Blatt-Auswahl oder Rundum-Ansichten",
        "Vier identische 60-Blatt-Gruppen × 24 unabhängige Richtungen auf gemeinsamen 8 × 8 Rastern. Flächenausgleich durch größere verbleibende Blätter erhält die räumliche Deckung nicht. Rundum-Ansichten wählen eine Karte pro Gruppe; Schatten benötigen die jeweilige Lichtrichtung. Alpha und Normalen bilinear, RGBA8 und achtfaches Supersampling. Keine vollständige BRDF, Wind-/Übergangs- oder Runtime-Freigabe.",
    ),
    (
        "selection/selection.json",
        "selection_storage_plot",
        "Ansichten: weniger Draw-Geometrie, mehr Texturen",
        "Alle 32 beziehungsweise 64 unbeleuchteten Ansichten vollständig vorgehalten. Zwei RGBA8-Basiskarten je Ansicht, vor Atlasrändern und Mips. Lokale CPU-Erzeugung samt benötigter Sichtflächenbewertung; ein Aufbau je Gruppe, Median und Spanne über vier Gruppen. Quelle und angeforderte Geometrie sind identisch; Speicher-/GPU-/Overdraw-Kosten besitzen eigene offene Grenzen.",
    ),
    (
        "clouds/clouds.json",
        "cloud_group_plot",
        "Blattgruppen: Deckung und Kartenbedarf",
        "Vier Gruppen mit je 60 Blättern × 24 feste Richtungen, einschließlich flacher und unterer Ansichten. Gemeinsamer Bildausschnitt aller Verfahren auf einem 8 × 8 Raster; identische Referenzraster geprüft. RGBA8-Attribute, bilineare Abtastung und achtfaches Supersampling. Mittel und Maximum gegen dasselbe räumliche Quellmesh. Achsenebenen sind verworfene Vergleichsverfahren; auch die Organebenen belegen kein allgemeines LOD-Gate.",
    ),
    (
        "clouds/clouds.json",
        "cloud_raster_plot",
        "Rasteraufwand bei gleichen Eingaben",
        "Identische 3.960 Dreiecke und Attribute auf einem 64 × 64 Raster. Fünf lokale CPU-Läufe je Verfahren, Aufbau ausgeklammert. Median und Minimum/Maximum; logarithmische Achse. Referenzdeckung und Attribute in 16 Prüfungen verglichen. Keine GPU- oder Gerätemessung.",
    ),
    (
        "clouds/clouds.json",
        "cloud_render_plot",
        "Krone und Schatten im direkten Bildvergleich",
        "Unveränderte Originalpixel aus demselben Ausschnitt der beiden 1280 × 720 Renderings. 37.800 identische Quellblätter, gleiche Holzgeometrie, Kamera, Licht und 32 EEVEE-Samples. Farbskala: größter absoluter RGB-Kanalunterschied. Eine statische Ansicht belegt weder Wind noch Bewegungsstabilität oder allgemeine LOD-Qualität.",
    ),
    (
        "sheets/sheets.json",
        "leaf_lod_plot",
        "Blatt-LOD: Deckung gegen Geometriekosten",
        "24 orthographische Ansichten bei 8 Pixeln Blattgröße, vierfach unterteilt rasterisiert. Gleiche Blattflächen und Richtungen; räumliche Vereinfachung gegen projizierte Karten. Einzelblattdiagnose ohne Baumverdeckung, vollständige BRDF oder native Laufzeit. Kleine Positionsfehler garantieren keine Deckung.",
    ),
    (
        "growth-unique/growth.json",
        "growth_plot",
        "Nachbarsuche im Vergleich",
        "18 Fälle · 3 Arten × 2 Mengen × 3 Seeds. Balken: Median über die drei Seeds; Spannen: Minimum bis Maximum. Je Seed ein lokaler Lauf, identische Astpläne; keine p95- oder Gerätemessung.",
    ),
    (
        "crown/crown.json",
        "crown_plot",
        "Grobes Produkt direkt erzeugen",
        "Eiche, Seed 2. Gleiche geometrische Toleranz für beide Verfahren. Die Schranke gilt für die kontinuierliche Hüllengeometrie; Laubdeckung, Licht und Schatten sind nicht darin belegt. Die Galerie verwendet eine andere Kamera.",
    ),
    (
        "demand/demand.json",
        "demand_plot",
        "Nahdetail erst bei Bedarf",
        "Vollständige und bedarfsgesteuerte Suche liefern dieselben angeforderten Standorte. Zusätzlich erzeugte Nahstandorte stammen aus vollständigen Zellprodukten. Cachehit in dieser Probe: Wiederholung im selben Prozess; persistenter nativer Cache und fertiges Wald-Bild fehlen.",
    ),
    (
        "organs/organs.json",
        "organ_storage_plot",
        "Blattgeometrie teilen",
        "Gleiche Geometrie, Ecknormalen und UVs: ein Mesh mit Platzierungen gegen vollständige Expansion. 32 Längssegmente, eine Querteilung je Blatthälfte. Balken: Median aus drei Zusammenstellungen; Spannen: Minimum bis Maximum. Gemeinsamer Mesh-Aufbau und Platzierungsplan sind ausgeklammert. Keine GPU-, Draw- oder Laufzeitmessung; die Nahbilder verwenden feinere Blätter.",
    ),
    (
        "plants/plants.json",
        "plant_request_plot",
        "Zweigdetails gezielt erzeugen",
        "Dieselbe angeforderte Blattgruppe mit exakt gleichen IDs und Positionen: nur Zweig und zwei Vorfahren erzeugen oder vorher die ganze Pflanze expandieren. Fünf erste Anfragen, Median und Minimum/Maximum. Rezeptaufbau, Meshing und Rendering sind ausgeklammert; Speicher umfasst Platzierungsarrays ohne Python-Objektkosten. Wiederholung nutzt denselben Prozess, keinen persistenten Runtime-Cache.",
    ),
)
