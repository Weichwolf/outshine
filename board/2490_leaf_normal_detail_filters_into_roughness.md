Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2488
Depends:
Area: generators, render
Tags: vegetation, python, normal, material, lod

# Leaf normal detail filters into roughness

## Ergebnis und Besitzer
2490 ergänzt 2488s Materialprodukte um prozedurale Normalen und gefilterte Normalenstatistik.
Bilddefizit: schwache Feinaderstruktur; Bump-Höhe allein liefert keinen vollständigen Material-LOD-Vertrag.
2489 besitzt Pigment-R/T, 2490 Normalen-/Rauheitsdetail aus denselben gespeicherten Oberflächen.

## Referenz und Verfahren
Vorhandene Ghidra-Analyse von UnifyNormals/ComputeGlobalLighting: Normalenrichtung und Tangentenrahmen
werden gestaltet, keine gebackene Lichtfarbe. Referenzshader fs.fx 500–526 verwendet Normal-/Glanzkarte
und Tangentenrahmen; das belegt keine bestimmte Mipfilterung in SpeedTree.
[SIGGRAPH 2007 Han et al.](../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf),
Seite 1 optisch geprüft: normale Mittelung verliert die Verteilung und verändert Shading.
[LEAN Mapping, Olano/Baker 2010](../doc/references/vegetation/i3d/2010-lean-mapping.pdf),
Seiten 3–4 optisch geprüft: erste/zweite Neigungsmomente linear filtern.
Eigene Normalenerzeugung aus physischer Reliefhöhe und Blattmetrik; keine fremden Texturen übernehmen.
Alle Karten und Mipstufen vollständig cachen. Makroform, Aderpläne und Pigmentkarten erhalten.

## Abnahme und Grenzen
Geometrische Ableitung gegen unabhängige Fläche prüfen, Momenthierarchie gegen direkte Blockstatistik.
Korrektheit, Bild, Aufbau-/Ladekosten und Speicher separat. Neue Bilder 2560×1440 optisch prüfen.
Grundfunktion vor Pflanzenrezepten; vollständige botanische Kalibrierung und native GPU-Filterung offen.

## Umsetzung und lokale Belege
`Blade.surface_jet` liefert analytische du/dv neben bisheriger Position/Normalen/Fläche.
Die drei ursprünglichen 64×4-Meshprodukte sind nach Extraktion exakt identisch in allen Arrays.
`leaf_normal_moments.py`: T=du/|du|, N=norm(du×dv), B=N×T;
p=−h_u/|du|, q=(−h_v−p·T·dv)/(B·dv). Tangentennormale norm(p,q,1).
Physische Höhe aus dem unveränderten gecachten 60-µm-Relief, zentrale UV-Differenzen,
einseitige Ableitung am Texelrand. Erstordnung eines Normalenoffsets; h·dN-Term bleibt vernachlässigt.
384 unabhängige versetzte Tangentenflächen: maximal 3,33e−16 Normalenfehler; physische Ebene geprüft.
Momente (p,q,p²,pq,q²,r⁴), 2×2-Boxhierarchie von 512² bis 1².
Direkte Auswertung aller Blöcke gleich bis 4,34e−14; 5461 Kovarianzblöcke unabhängig geprüft.
Gleiche zehn Mipstufen, je drei wechselnde Läufe: 22,4/23,5/24,3 statt 74,9/75,4/77,1 ms.
Rauheitsnäherung r_eff=(E[r⁴]+trace(Cov))^(1/4), bei 1 begrenzt.
Dies ist keine exakte GGX-Faltung: GGX hat keine endliche volle Neigungsvarianz.
Volle Neigungskovarianz erhalten, Vorschau nutzt nur isotrope Spur; multimodale Verteilungen bleiben offen.
Statistik im lokalen UV-Tangentenraum; starke Basiswechsel innerhalb grober Texel sind eine weitere Näherung.

Materialversion 4: zehn gecachte 16-Bit-RGB-TIFF-Normalen und 8-Bit-Rauheitskarten.
`tifffile 2026.9.20` bereits in .venv vorhanden, nun explizite Anforderung; kein Fremdcode gebündelt.
Normale/Rauheit unkomprimiert zusammen 2 446 675 Bytes je Form über alle Stufen;
separate Float32-Moment-NPZ 8 388 600 Bytes unkomprimiert für Statistik/weiteren Filterpfad.
Gegenversuchs-Rauheitskarten zusätzlich, nicht Teil des ausgewiesenen Normal-/Rauheitsprodukts.
Erzeugung der Neigungen 0,21–0,25 s, Kartenexport 0,15–0,17 s; Diagnoseexport separat etwa 0,17 s.
Maximale normale Kartenabweichung beim frischen Reload 2,23e−5 rad, Rauheit ≤0,5/255+1e−6.
Alle zehn Stufen mit Hashes/Größen geprüft, vollständiges Laden/Prüfen lokal etwa 0,11–0,14 s.
Blender-Decodefehler <3e−8; drei tatsächliche Normal-/Rauheits-/Transmissionsbindungen geprüft.
52 gemeinsame Nah-/Grobvertices je Form erhalten, Adergraph und Pigmentkarten unverändert.
Reproduktion: `leaf_normal_experiment.py`, `leaf_normal_reload.py`, `leaf_normal_preview_checks.py`;
Material-/Graph-Replay aus 2488 mit `--folder .../normal-leaves`.

## Bild und offene Grenze
2560×1440-Vergleich optisch geprüft: volle Normale links, absichtlich grobe 32²-Stufe ohne/mit
Streuung Mitte/rechts. Kleine Bildänderung; kein großer Feinader-Bildgewinn aus dieser Filterung.
Das künstliche Zweigruppen-Gegenbeispiel im Plot zeigt die verbleibende Mehrgipfel-Lücke.
Automatische native trilineare/anisotrope Momentfilterung und deren GPU-Kosten bleiben offen.
Feinaderdichte, Relief/Glanz, Blattseiten und botanische Kalibrierung sind weiter Bilddefizite.
Nahbild Normalkarte gegen bisherigen Bump: 2,52 % Pixel ändern sich, 2023 Pixel um mehr als 1/255,
Maximum 3/255; mittlerer RGB-Kanalfehler 0,01235/255. Bildwirkung weitgehend erhalten.
Ober-/Unterseite und Normal-Mipvergleich tatsächlich optisch geprüft; kein großer Qualitätsgewinn behauptet.
Speicherangaben oben sind Assetdaten, keine GPU-Residency; native Kosten bleiben unbewiesen.
Lokaler Prozesspeak 149.27 MiB inklusive direktem Vergleich und Diagnose.
Fünf bisherige Material-v2-Kontrollen pixelgenau unverändert; 43 öffentliche Renderprodukte aktuell geprüft.
