Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, branches, welding, geometry

# Organic branch forks share one surface

## Ergebnis und Besitzer
Gemeinsame organische Gabeloberflächen ersetzen sichtbare Nähte überlappender Einzelrohre.
2348 besitzt SpeedTree-Funktionsanalyse; 2350 Radiusprofile, 2351 räumliche Astachsen,
2346 spätere Pflanzen-/Organanschlüsse. Der Funktionsnachbau bleibt vor Artenrezepten.

## Referenz und Verfahren
Ghidra: ComputeWeldRing, WeldStart, CacheWeldRing, GetWeldParent und FixBifurcation.
WeldHit delegiert abhängig vom Raycaster an Elternaufrufe oder Cast/CastForWeld; statischer Befund.
RadiusProfile kombiniert Profil-/Override-Auswertung und eine bedingte gewichtete Profilmodulation;
unbekannte Konstanten/Feldbindungen bleiben offen, kein numerisch ausgeführter Originalbeleg.
[SpeedTree Welding](https://docs.unity3d.com/speedtree-modeler/manual/branch-generator-properties.html#welding):
Elternanschluss, Raycast-Alternative, Offset und Verbreiterung des Anschlussbereichs.
[Bloomenthal, SIGGRAPH 1985](../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf)
liefert räumliche Gabel-/Sattelreferenzen; Abbildungen zur Verzweigung optisch prüfen.
Eigene lokal begrenzte Verschmelzungsform aus Astsegmenten; direkte radiale Flächenabtastung
mit volumetrischem Lewiner-Marching-Cubes auf denselben Eingaben vergleichen.
Bibliotheken nutzen; Feld, Gradienten und Fehler unabhängig prüfen. Kein Fremdcode im Repository.
[Lewiner et al., JGT 2003](../doc/references/vegetation/jgt/2003-marching-cubes-topological-guarantees.pdf)
mit [scikit-image 0.26.0](https://scikit-image.org/docs/stable/api/skimage.measure.html#skimage.measure.marching_cubes):
Topologiegarantie betrifft das Gitterinterpolant, keine beliebige kontinuierliche Eingabe.
[Simplex-Projektion, Wang/Carreira-Perpiñán 2013](../doc/references/vegetation/math/2013-probability-simplex-projection.pdf)
für symmetrische kompakte Feldmischung; PDFs einschließlich Algorithmen/Abbildungen optisch geprüft.

## Eigene Form und Datenfluss
Segmentfelder si=ri²−dist² zum Segment ab gemeinsamem Ursprung, positive Radien.
σ(s)=max_{w≥0,Σw=1}(w·s−τ||w||²/2)+τ/2; Gewichte durch Simplex-Projektion bestimmen.
max(s)≤σ≤max(s)+τ(1−1/N)/2; allein dominante Segmente außerhalb der Mischzone unverändert.
Bei σ=0 sind alle Segmentabstände mindestens ri; ∂σ/∂t≤−2rmin²/t entlang Ursprungsstrahlen.
Eigener Beleg für eindeutige radiale Nullstelle und nichtverschwindende Oberflächengradienten.
Feldwerte haben m², keine vorzeichenbehaftete Distanz; Normalen aus dem Gradienten ableiten.
`fork_field.py` besitzt Plan/Feld; `fork_surface.py` Gitter-/radiale Flächen, `fork_adaptive.py` Vergleichspfad.
`cube_surface.py` ist gemeinsame Cube-Parametrisierung; Kronenaufrufer migrieren, Ausgaben erhalten.
Gemessener Engpass: dichte Volumenfelder brauchen beim 128er Gitter rund 0,33–0,47 s je Gabel.
Konservative Blockgrenzen vor Punktabfragen testen; alle acht Werte jeder oberflächenschneidenden
Zelle erhalten. Feldarbeit und Intervallarbeit getrennt zählen; Mesh/Bild bei gleichen Eingaben erhalten.
[Duff, SIGGRAPH 1992](../doc/references/vegetation/siggraph/1992-interval-arithmetic-csg.pdf):
Gebietsgrenzen und Unterteilung ergänzen die eigenen Segment-/Feldgrenzen; nach außen runden.
Blockdistanz folgt 1-Lipschitz-Segmentdistanz und umschließender Boxkugel; Feldgrenzen aus σ-Schranke.
Ein numerischer Sicherheitsaufschlag ergänzt die Rundung; kein universeller IEEE-Feldbeweis behauptet.
34.560 Eck-/Innenproben in 864 Boxen, 1/3/16 Segmente, Maßstäbe 0,001–1000 und Ursprung 10⁶.
24 kleine dichte/Blockgitter liefern identische Vorzeichen, Schnittzellwerte und sämtliche Mesharrays.
Volumenspeicher bleibt dicht; zusätzliche Auswahlmasken zählen separat, nur Feldarbeit wird ausgelassen.

## Verjüngte Anschlussform
Gleiche Astachsen mit unabhängigen positiven Basis-/Endradien.
`tapered_fork.py` besitzt Hüllen, `fork_refinement.py` begrenzte Projektion/Unterteilung,
`tapered_experiment.py` unabhängige Verfahrensprüfung und vollständige Bildprodukte.
Je Segment h(x)=max_{0≤t≤1}[(r0+tΔr)²−|x−tv|²]; das ist die Vereinigung linear variierter Kugeln.
Für |v|²>Δr² liegt das Maximum bei clamp((x·v+r0Δr)/(|v|²−Δr²),0,1).
Sonst genügt das größere Endpunktmaximum; eingeschlossene Kugeln und Nullsegmente explizit prüfen.
Gradient per Hüllsatz; gleiche symmetrische Mischung wie konstante Gabeln. Keine nächsten-Punkt-Verjüngung.
Auf der Nullfläche gilt x·(x−tv)≥r(t)r0>0: radiale Nullstellen bleiben eindeutig und regulär.
Dünne Enden verfehlen bei 192 Zellen den 5-mm-Probestrahlvertrag; zusätzliche Vertexprojektion allein reicht nicht.
Gemeinsame Kanten einmal halbieren, alle vier Kinddreiecke orientiert erhalten und neue Punkte projizieren.
Projektion: maximal sechs Newton-Schritte, Bewegungsgrenze, Residuum und Million-Dreiecke-Budget explizit.
Feldabweichung der Vertices, tatsächlicher Strahl-/Dreiecksfehler und Kosten getrennt prüfen.
Gemessener Geometrieengpass: ausgewählte verjüngte Gabeln benötigen 68.380–118.112 Dreiecke.
Adaptive radiale Patches erreichen bei Tiefe sieben an dünnen Enden ihr Limit; Tiefe neun
bei unverändert maximal 16.384 Patches prüfen. CPU/Produktgröße als getrennte Kosten vergleichen.
Min-/Max-Radien kapseln das Segmentfeld für die bestehende konservative Blockauswahl ein.
Unabhängige skalare Optimierung, Kugel-/Konstantfälle, Differentialnormalen und identische Gittermeshes prüfen.
581 skalare Optimierungen: maximale Feldabweichung 8,89e−16 m²; 111 analytische Kegeltangentialproben.
Je Form 113 Differential-/Transformationspunkte und 1.356 Boxproben; zehn ungültige Eingaben/Budgets abgewiesen.
Drei Formen/sieben Varianten: unveränderter 5-mm-Probevertrag und geschlossene außenorientierte Flächen.
Adaptive Tiefe neun bei 4 mm lokaler Probentoleranz: 8.080–8.336 Dreiecke, 2,75–3,42 mm Strahlprobenfehler.
Optimierte Punktbatches/Kantenauflösung: drei Meshes samt Arbeitszählern exakt gegen vorherige Kerne geprüft.
Lokale Proben rund 422–447 ms; lokale CPU-Zeit und Dreieckszahl geringer als bisherige ausgewählte Gitterverfahren.
2-mm-Varianten verfehlen teils ihr lokales Tiefen-/Flächenziel und bleiben trotz kleiner Strahlproben rot.
Keine globale Fehler-, Frame- oder Gerätefreigabe; Kerne aus festem Git-Commit nur als eigener Vergleichscode.
Verjüngte Funktionsgabeln mit gleicher Kamera optisch geprüft; vollständige Elternbindung bleibt offen.

## Umsetzung und Abnahme
Stabile Quelldaten bestimmen Gabel und Anschlüsse; angeforderte Auflösung vor Erzeugung.
Eine geschlossene außenorientierte Oberfläche, gültige Dreiecke, gemeinsame Normalen;
getrennte interne Rohrflächen dürfen den neuen Anschluss nicht darstellen.
Gleiche Form-/Fehleranforderung, Erzeugung und Produkt-/Arbeitsspeicher getrennt messen.
Symmetrische, ungleiche und räumliche Astgabeln unter kontrollierter Kamera/Licht rendern;
mit dem bisherigen Überlappungsbild und passender Referenz tatsächlich optisch vergleichen.
Beobachtete Stichprobenfehler nicht als allgemeine Hausdorff-Schranke ausgeben.

## Gekrümmte Astpläne
`curved_fork.py` verbindet getrennte CubicSpine-/Profile-Pläne über ihre eigenen Segmenthüllen.
Je Ast gilt max über seine Segmente; erst die Astfelder werden symmetrisch gemischt.
Zusätzliche Stationen dürfen keinen künstlichen Mischwulst erzeugen. Geradenunterteilung invariant prüfen.
C1-Kurven besitzen Sehnenfehler ≤max|c″|/(8n²); Profilabweichung an allen Tabellenknicken exakt begrenzen.
Gemeinsame Summe aus Achs-/Radiusfehler bestimmt Stationen vor der Feldabtastung, begrenzt auf 256 Segmente.
Räumlich gekrümmte Äste müssen nicht sternförmig sein; keine radiale Ein-Nullstellen-Annahme übernehmen.
Dichte/blockweise Gitter direkt vergleichen; konservative Segmentgrenzen astweise per Maximum kombinieren.
Polynomwurzel-Referenz für kubische Achsen/lineare Radien, Differentialnormalen und geschlossene Netze prüfen.
Ghidra: ComputeShaping interpoliert Stationswerte/Richtungen; FreeHandBendSpine liest eine Spine-Abtastung,
verschiebt Stationspositionen und normalisiert abgeleitete Richtungen. Statischer Befund, Propertybindung offen.
Neun Geraden-Unterteilungen bleiben feld-/gradientengleich; ein gebogener Ast liefert drei Strahlschnitte.
126 feste kontinuierliche Referenzpunkte → Mesh und 128 Meshproben → Referenzprojektion getrennt prüfen.
Beide beobachteten Abstände ≤5 mm; keine globale Hausdorff-Schranke oder garantierte nächste Projektion.
Drei Formen/vier Verfahren: Block96/128/96 gewählt, 17.012–25.392 Dreiecke, 2,63–3,35 mm Probenfehler.
Lokale Mediane 0,90/2,35/1,37 s: erheblicher Feldkostenbedarf bleibt; keine Runtime-Freigabe.
Dichte/Block96-Mesharrays identisch; alle gewählten Flächen geschlossen, außenorientiert, ohne Normalenumkehr.
1280×720-Eevee-Bild optisch geprüft: gebogene Silhouetten und zwei Seitenanschlüsse sichtbar,
keine getrennten Rohrnähte; glatte Funktionskörper ohne Rinde sind noch keine botanischen Modelle.

## Offene Lücke
Vollständige SpeedTree-Weld-Property-/Elternflächenbindung, beliebige gekrümmte Gabelregionen,
Rinde, Wind, ganze Pflanzen und native Runtime-/Geräteabnahme bleiben offen.
