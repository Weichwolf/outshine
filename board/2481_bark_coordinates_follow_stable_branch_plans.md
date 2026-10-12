Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, bark, materials

# Bark coordinates follow stable branch plans

## Ergebnis und Besitzer
Die vollständige Testpflanze erhält prozedurale Rindenstruktur entlang ihrer bestehenden Astachsen.
2348 besitzt SpeedTree-Funktionsanalyse, 2353 die vereinigte Holzoberfläche, 2351 räumliche Rahmen.
Material-/Koordinaten-Funktionstest vor weltweiten Artenrezepten; keine botanische oder native Freigabe.

## Sichtbarer Bedarf und Verfahren
Die aktuellen Stämme sind glatte einfarbige Funktionskörper ohne sichtbare Rindenstruktur.
Ghidra: ComputeBranchTexCoords (0x1cbadd0), ComputeBranchVCoords (0x1c51190),
ComputeBlendedUVs (0x1c739f0), FillBaseTexCoordValues (0x1cad8e0); vier Dekompilierungen terminal erfolgreich.
BlendedUVs hebt U-Sprünge über einer halben Periode durch Periodenoffsets auf;
fünf zyklische Glättungsdurchläufe mit Faktor 0,25. ELF-Konstanten 0,5/0,25 gegengeprüft.
V-Koordinaten verwenden Elternaufrufe, Materialseitenverhältnis und Profile; Feldbindung offen.
Eigene räumliche Materialkoordinaten aus demselben kameraunabhängigen Astplan, keine SpeedTree-UV-Kopie.
Kreisachsen analytisch auf Bogenlänge und zwei lokale Querkoordinaten projizieren.
Elternphase am Ansatz vererben, Kinderchart innerhalb begrenzter radialer Unterstützung weich einmischen.
Gemeinsame Meshvertices erhalten ein Feld; kein Winkel-U und damit keine umlaufende UV-Naht.
Das ist keine verzerrungsfreie globale Parametrisierung oder vollständige SpeedTree-Propertybindung.
Keine Fototexturen oder fremden SpeedTree-Materialien übernehmen.
[Perlin/SIGGRAPH 1985](../doc/references/README.md) begründet räumliche Textur/Rauschkomposition.
Eigenes anisotropes Blender-Rauschen steuert Farbe/Normalen in Metern; dünne Zweige bleiben glatter.
Vollständiges versioniertes Materialrezept mit physischer Skala/Farben/Relief liegt beim Meshprodukt;
der Bildadapter lädt es direkt, Mesh-/Organprodukte bleiben unverändert wiederverwendbar.
Blender-Nodes sind der Bildadapter; native Materialauswertung, grobe LOD-Filterung und Rindengeometrie offen.

## Funktions- und Kostenbeleg
765 analytische Punkte: Gerade, positive/negative Kreisachsen bis π, Rundumnaht, Transformation/Skalierung.
Unabhängige skalare Nächstpunktminimierung: größter beobachteter Parameterfehler 5,36×10⁻⁹.
64 Achsen / 80.311 Holzvertices: dichte und räumlich begrenzte Felder exakt identisch.
cKDTree-Suchkugel enthält die Unterstützung über halbe Bogenlänge plus doppelten Basisradius.
Arbeit vor Feldberechnung begrenzen; leere/ungültige/zu große Anfragen expandieren weder Achsen noch Blätter.
Zusatzattribute 2.569.952 Bytes; Geometrie, Ecknormalen und alle Organprodukte exakt erhalten.
Lokale Erzeugung 0,45 s gegen dicht 4,85 s Median aus drei abwechselnden frischen Prozessen;
Peak-RSS bis 114,9 gegen 135,3 MiB einschließlich Imports/Eingabe; Einlesen separat.
Kleine Shell startet Messworker nach exec: geerbte Experiment-RSS-Spitze gehört nicht zur Messung.
Keine Runtime-/Gerätefreigabe; 382.646 statt 5.139.904 Punkt-/Achsen-Auswertungen.
`bark_experiment.py` erzeugt eigenständige vollständige Produkte, `bark_preview.py` prüft ihre Bildwirkung.

## Abnahme und offene Lücke
Gleiche Holzgeometrie und Organanker; Koordinaten und Materialdetails ändern nur die Oberfläche.
2560×1440-Nahbild und Gesamtpflanze optisch geprüft: längs gerichtete Struktur folgt dem Astansatz,
dünne Zweige glatter; keine schwarzen Pixel. Bildgewinn am Nahdetail, Krone weiterhin Funktionsprobe.
Periodische Nähte, gebogene Äste, Ansatz und dünne Zweigenden prüfen; Details vor teurer Arbeit wählen.
Vollständige SpeedTree-Propertybindung, Rindengeometrie, Wachstum, Wind und native LODs bleiben offen.
