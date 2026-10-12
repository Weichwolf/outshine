Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, spine, frames, curves

# Spatial branch spines have stable surface frames

## Ergebnis und Besitzer
Räumliche Astverläufe mit stabilen Querschnittsrahmen und profilierten geschlossenen Flächen.
Sichtbares Defizit: Kreisbögen beschränken Astformen auf eine Ebene; Wendepunkte und
räumliche Krümmung benötigen einen konsistenten Rahmen für Geometrie und Kindanschlüsse.
2348 besitzt Referenzanalyse/Funktionsumfang, 2350 Radiusprofile, 2346 Pflanzenanschlüsse.

## Referenz und Entscheidung
Ghidra prüft ComputeSpine/ComputeFrame, BranchRings/Normals, OrthoNormalize und SpineVertex::Lerp.
Belegte Operationen getrennt von offenen Property-/Kraftbindungen beschreiben.
[Bloomenthal, SIGGRAPH 1985](../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf):
Räumliche Astachsen und Querschnittsflächen; Frenet ist bei geraden Teilen/Wendepunkten singulär.
[Wang et al., TOG 2008, Autorenpreprint](../doc/references/vegetation/tog/2008-rotation-minimizing-frames-preprint.pdf):
Doppelreflexion als Vergleich zur Tangentendrehung; gleiche Positionen/Tangenten und Startorientierung.
Standardbibliotheken für Kurven, unabhängige ODE-/analytische Referenzen für Rahmen verwenden.
Referenz-PDFs einschließlich Algorithmus und Formabbildungen optisch prüfen.
ComputeFrame delegiert an Spine/Weld/Droop/Shaping; vollständige Feldbindung bleibt offen.
SpineVertex::Lerp mischt neun Rahmenfloats linear und orthogonalisiert anschließend;
OrthoNormalize verwendet Gram-Schmidt, auch für linksorientierte Eingangsrahmen.

## Umsetzung und Abnahme
Kurvenplan und Radiusprofil bleiben kameraunabhängig; nur angeforderte Stationen/Seiten erzeugen.
Ein Startquerschnitt und expliziter Roll-Verlauf bestimmen Orientierung und Organanschlüsse.
Gerade Strecken, Wendepunkte, nichtplanare Kurven, Einheiten und Transformationen prüfen;
degenerierte Eingaben begrenzt ablehnen. Keine verdeckte Umkehr/NaN-Geometrie akzeptieren.
Verfahrenskosten, Rahmenfehler und Produktbytes auf denselben Eingaben getrennt messen.
Geschlossene Flächen, Außenorientierung, Normalen und sichtbare räumliche Form rendern/öffnen.
Native Originalkern-Ausgaben als private Mikro-Oracles, keine fremde Implementierung im Repository.

## Eigener Stand und Beleg
`spine_curve.py`: SciPy-Hermite-/Bernsteinkurven, endliche C1-Segmente mit Regularitätszertifikat.
Ableitungskontrollvektoren projizieren auf den Sehnenguide: vmin>0; κ≤amax/vmin².
`spine_frames.py`: Doppelreflexion mit Quaternionen-Präfix und SciPy-Rotation; Referenzschleife/Tangentendrehung.
`spine_surface.py`/`tube_topology.py`: Ellipsen, Radiusprofile, Roll-Ableitung, harte Endkappen und Ecknormalen.
32 Fälle gegen originales OrthoNormalize, Abweichung ≤1,11e−7; Binär-/Harness-/Inputhash im Bericht.
Analytische Helix, unabhängige DOP853-Rahmen, Differentialnormalen, Transformations-/Umkehrprüfung.
Differentialnormalen innerhalb glatter Segmente prüfen; C1-Knoten besitzen einseitige Krümmungen.
Vier Kurven: geschlossene außenorientierte Flächen bei drei direkt erzeugten Auflösungen.
Erstes Produkt aus frischem Kurvenplan/Profil einschließlich BPoly-Aufbau getrennt von Wiederholungen messen.
Gleiche Fehleranforderung ≤1e−5: Doppelreflexion mit 32 statt 1.024 Segmenten bei Tangentendrehung.
Lokale siebenfach gemessene CPU-Kosten stehen in `spines/spines.json`; keine Modeler-/Gerätezeit.
Kontrollierte Funktionsrenderings vor Artenrezepten; Quellen sind unveränderte Kurven/Profile.

## Offene Lücke
Vollständige SpeedTree-Spine-/Force-/Noise-Propertybindung, organische Gabeln, Rinde,
Wind, kameraunabhängige kontinuierliche Kindanker, bewegte LOD-Übergänge und native Geräteabnahme bleiben offen.
Das Segmentzertifikat ist hinreichend, nicht notwendig; lokale Regularität beweist keine globale Kollisionsfreiheit.
