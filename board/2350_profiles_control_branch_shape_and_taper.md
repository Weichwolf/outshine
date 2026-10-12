Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, profiles, branches, oracle

# Profiles control branch shape and taper

## Ergebnis und Besitzer
Eigene Bézierprofile, schnelle wiederverwendbare Lookup-Produkte und profilierte Astflächen.
2348 besitzt SpeedTree-Funktionsnachbau/Referenzanalyse, 2346 Organ-/Pflanzenanschlüsse.
Verlauf/Elternbezug vor Auswertung deklarieren; Kamera/LOD verändert den Formplan nicht.
Sichtbares Defizit: reine lineare Rohrverjüngung, fehlende kontrollierte Radiusverläufe/Wurzelanläufe.

## Referenz und Verfahren
Ghidra: EyeBezierSpline::EvaluateRawPoint 0x212d030, Evaluate 0x212ced0;
CSplineProperty::EvaluateProfile 0x1fbaf30, Tiled 0x1fbac70 und ParentValue 0x1fba5c0.
Rohauswertung verwendet kubische Segmente; schnelles Profil interpoliert 1.000 vorbereitete Punkte.
Float-Signaturen in Ghidra korrigieren; Parameterbindung von unbelegten Feldnamen trennen.
[SpeedTree Curves](https://docs.unity3d.com/speedtree-modeler/manual/curves.html):
Elternlage und Verlauf, relative/absolute Elternkoordinaten und mehrere Elternlevel unterscheiden.
[Weber/Penn, SIGGRAPH 1995](../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf)
ergänzt die hierarchischen Form-/Verjüngungsparameter; kein vollständiger SpeedTree-Solver.
Eigene Bernsteinprofile mit [SciPy BPoly](https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.BPoly.html);
[brentq](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.brentq.html) als Einzelvergleich,
[elementwise.find_root](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.elementwise.find_root.html)
als gemessene Batch-Auswertung; kompakte Tabellen vorhalten.
Native Originalroutinen nur als private numerische Mikro-Oracles in Downloads ausführen;
keinen fremden Code ins Repository übernehmen. Keine Modeler-Szenen-/Kostenfreigabe daraus ableiten.

## Umsetzung und Abnahme
Finite monotone x-Profile, explizite Grenzen; Tabellen unveränderlich/versionierbar.
Unabhängige Polynom-/Differentialprüfung, echte Originalausgaben und Endpoint-/Clamp-Fälle.
Aufbau und gleiche Abfragen getrennt messen; Fehler gegen genaue Kurve an unabhängigen Positionen.
`profile_curve.py` und `profile_experiment.py` besitzen Kurven/Tabelle sowie native Gegenprüfung und Kosten.
`profiled_axis_mesh` ergänzt `axis_surface.py`; Radius und Ableitung gehören zusammen; gültige geschlossene Geometrie,
Normalen, Anschlüsse und Maße prüfen. Bild unter gleicher Kamera/Licht mit Referenzbezug öffnen.
Generische Kalibrierungsformen für Funktionen; vollständige Pflanzenrezepte folgen nach 2348.

## Aktueller Beleg
Vier Bildprofile/2.048 Abfragen je Verfahren, fünf CPU-Läufe: Batch statt Einzelinversion,
vollständige 1.000er Tabelle einmal erzeugen, wiederholte Abfragen mit NumPy-Interpolation.
28 native Konfigurationen gegen fünf Originalroutinen: rohe Punkte, Lookup, Presets,
Zusatzkurven, Ausgangs-/Eingangsbegrenzung, gekachelte Drift und Basisskalierung.
Kontrollpuffer/fertige Tabellen geliefert; Binär-, Harness- und Eingabehash im Bericht.
Original-Tabellenaufbau/Modeler-Szene nicht ausgeführt. Maximal 4,96e−7 absolute Abweichung.
Monotonie über das quadratische Ableitungsminimum prüfen; gekreuzte gültige Handles erhalten.
Table-Batches: Aufbau 1,6–4,1 ms, 8.000 Bytes; Lookup rund 0,25 ms gegen 1,9–6,1 ms Batch-Inversion.
Lookup-Kurvenabweichung bis 1,48e−4 am geprüften Datensatz; keine beliebige Kurvenfehlergarantie.
Vier geschlossene Radiusflächen samt endlichen Dreiecken, Außenorientierung und Differentialnormalen.
Drei bisherige Rohrfälle bleiben bytegleich; beliebige Kurven-/Pflanzenqualität noch nicht freigegeben.

## Offene Lücke
Vollständige Property-/Varianz-/Mehr-Elternbindung, räumliche Spine-Kurven, Kräfte, organische
Gabeln, Rinde, Wind und native Runtime-/Gerätekosten bleiben offen.
