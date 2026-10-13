Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, needles, organs

# Needles and fascicles share stable organ plans

## Ergebnis und Besitzer
2491 ergänzt fehlende räumliche Nadeln und Nadelbündel in 2346s Organen und 2483s Generatorgraphen.
Einzelorgane und Kurztriebe teilen stabile IDs, Anker und angeforderte Nah-/Grobprodukte.
Funktionsproben vor vollständigen botanischen Pflanzenrezepten; 2349 bleibt zweite Etappe.

## Referenz und eigener Ansatz
SpeedTree VFX/Sample_Pine/Pine.spm: vier BatchedLeaf-Generatoren für Nadeln/Cluster,
geteiltes Cutout Needles_1 (10 Vertices/10 Dreiecke), Modellformat 9.5.2 im 10.2-Paket.
Ghidra: ComputeBatch, ComputeBatchWorker, SetupIndicesForSkin; alle drei terminal erfolgreich.
Referenzpipeline verwendet Meshassets; kein prozeduraler botanischer Nadelgenerator daraus belegt.
Eigene begrenzte Kreisführung, kontrollierter Twist, elliptischer oder rhombischer Querschnitt,
spitzer Abschluss und harte Basis-/Querschnittskanten; analytische Normalen.
SIGGRAPH 1995 Weber/Penn und 2009 Self-organizing Trees aus 2346 begründen Organ-/Elternhierarchie.
Botanische Morphologie steuert Funktionsproben, keine freigegebenen weltweiten Rezepte.

## Abnahme
Numerische Ableitungen, geschlossene positive Meshes, harte Kanten und Unterteilung prüfen.
Gemeinsame Nah-/Grobpunkte sowie IDs/Anker/Platzierungen erhalten; Budget vor Meshing prüfen.
Gleiche Inputs und Qualitätsanforderungen für gebündelte/skalare Geometrie; Kosten/Speicher separat.
Prozedurale Materialien cachen, frischer Cache-Reload und 2560×1440-Bilder optisch prüfen.
Native GPU-Kosten, vollständige Artenformen, LOD-Deckung/Schatten und dynamische Nadeln bleiben offen.

## Umsetzung und lokale Belege
`needle_surface.py`: Kreisführung/Bogenlänge, kontrollierter Twist und positive Querschnittsform.
Radius wächst per Smoothstep von 0,55R auf R, ab t=a fällt er als R(1−((t−a)/(1−a))²) zur Spitze.
L>0, R≤L/8, |Biegung|≤π, |Twist|≤4π, a∈[0,25;0,875], Aspekt∈[0,125;1].
Damit bleibt die axiale Jacobi-Komponente ≥L(1−π/8)>0; Spitzennormale als richtungsabhängiger Grenzwert.
Querschnitts-/Längsableitungen aus derselben Fläche; Rhombus-Kanten besitzen getrennte Ecknormalen.
32×16 / 16×8 Unterteilung für Ellipsen, 32×4 / 16×4 für Rhomben, geschlossener Basisdeckel und Spitzenfächer.
356 numerische Differentialpunkte: maximaler Normalenfehler 5,23e−9; Flächendichte separat geprüft.
Acht geschlossene, positiv orientierte, nicht degenerierte Meshes; Euler-Zahl 2, jede Kante zweimal gegensinnig.
130/66 gemeinsame Ellipsen-/Rhombusvertices exakt erhalten; gebündelt/skalar alle Arrays bitgleich.
Volumenreferenz L·A·R²·∫Profil² für symmetrische Querschnitte analytisch integriert.
Beobachtete Defizite Nah 0,72–3,21 %, Grob 1,75–12,45 %; keine pauschale Deckungs-/Schattenfreigabe.
Gleiche Meshes, drei wechselnde Läufe: Ecknormalen gebündelt 3.00/2.83/1.57 statt 708.26/721.10/173.68 ms.
Vertices/Topologie in beiden Verfahren gebündelt; Kostenvergleich betrifft die Ecknormalenstrategie.
Prozesspeak 111.88 MiB inklusive Imports/Prüfungen, lokale CPU-Messung ohne native GPU-Aussage.

Generatorart `needle` in Rezept, Bounds, Quellen und Elternrahmen integriert; Geometriequelle Version 2.
`needle_recipes.py`: 14 Kurztriebe ×2/×5 sowie 40 einzelne rhombische Nadeln; kontrollierte Varianz.
Budgetablehnung vor Meshing/Posen, Teilanfrage und Plan-Roundtrip geprüft; Nah/Grob IDs/Frames exakt gleich.
Gerade konstante Zylinder benötigen nur zwei Längssegmente: 72 statt 1176 Dreiecke bei zwölf Seiten.
Beide repräsentieren denselben geraden polygonalen Mantel; gleiche Orientierung/Volumen unabhängig geprüft.
Vollständige drei Nahprodukte 119 016, Grobprodukte 33 124 instanzierte Dreiecke, gleiche 209 Instanzen.
Prozedurale lineare Farb-/Rauheitskarten 64×256; aggregierte Längsbänder, keine erzeugten mikroskopischen Zellen.
8 % Durchlicht als feste Materialnäherung; keine gemessene Nadeloptik. Karten/UV-Naht werden aus Cache geladen.
Frischer Reload ohne Mesh-/Posen-/Materialaufbau; bisherige Graphprüfungen in separatem Output bestanden.

## Morphologie und offene Qualität
[Flora of New Zealand](https://www.nzflora.info/factsheet/taxon/Pinus-sylvestris.html):
Pinus sylvestris besitzt Zweierbündel, 2,5–7 cm lange und 1–1,5 mm breite, teilweise gedrehte Nadeln.
[EU-JRC Picea](https://forest.jrc.ec.europa.eu/en/european-atlas/qr-trees/norway-spruce/):
Picea abies mit 1–2,5 cm langen, vierkantigen Nadeln und hellen punktierten Linien.
Das begründet Funktionsmaße, keine Artenfreigabe. Pinus braucht je Bündel passende Halbkreis-/Sektorformen;
elliptische Querschnitte sind ausdrücklich Proxies. Fünferbündel ebenfalls nur Gattungsprobe.
SpeedTree-Nadeltextur optisch geprüft, ausschließlich Referenz; eigene Materialien/Geometrie.
Weber/Penn 1995 Seite 5 optisch erneut geprüft: Organformen/Skalierung und hierarchische Elternbindung.
Reproduktion: `needle_experiment.py`, `needle_reload.py`, `needle_preview.py`; drei Bilder 2560×1440.
Wind/Kollision, natürliche Spitzen/Schichten, Material/LOD-Deckung, Sektorformen und ganze Pflanzen offen.

Alle drei 2560×1440-Ansichten optisch geprüft: räumliche Nadeln/Bündel sichtbar, noch glatt/regelmäßig.
46 öffentliche Renderprodukte mit aktuellen Eingabe-/Rendererhashes geprüft; kein SpeedTree-Gleichwertigkeitsbeleg.
