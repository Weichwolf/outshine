Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, plants, surfaces

# Complete plant surfaces preserve branch plans

## Ergebnis und Besitzer
Die bestehenden Ast-/Organpläne liefern eine vollständige Testpflanze mit zusammenhängender Holzoberfläche.
2346 besitzt Pflanzenhierarchie/Organe; 2352 lokal organische Gabeln; 2348 SpeedTree-Funktionsumfang.
Funktionstest vor weltweiten Artenrezepten, keine botanische Abnahme oder native Performancefreigabe.

## Verfahren und Entscheidung
Ein globales Gitter passt nicht zu meterlangen Stämmen mit submillimeterdünnen Zweigenden.
Oberflächenvereinigung auf denselben bestehenden Rohrmeshes gegen sequentielle Vereinigung vergleichen.
`manifold3d==3.5.4` als externe Bibliothek in `.venv`; eigene Eingaben/Prüfungen, kein gebündelter Fremdcode.
Python steuert den nativen Bibliothekskern; Ausgabe-Doubles und getrennte Ecknormalen erhalten.
Analytische Rohrnormalen als Vertexattribute durch Schnittinterpolation übernehmen und normalisieren.
Neu berechnete Meshnormalen erzeugten lange Schattierungsstreifen am Stamm und wurden verworfen.
Vor Expansion höchstens 4.096 Achsen/eine Million Quelldreiecke; keine heimliche Inhaltsauslassung.
Schnitt-Splitter mit 10⁻⁸ m Bibliothekstoleranz vereinfachen; geometrische Prüfgrenzen bleiben erhalten.
Geschlossene Vereinigung und organische Ausrundung sind getrennte Anforderungen; keine Gleichsetzung.
Ghidras ComputeWeldRing/WeldHit (2352) verwendet Profile und Elternabfragen; kein Beleg für Mesh-Booleans.
[Interactive and Robust Mesh Booleans, SIGGRAPH Asia 2022](https://arxiv.org/abs/2205.14151)
begründet den Verfahrensvergleich; PDF-Seite 1 tatsächlich optisch geprüft, Ablage im Quellenregister.
[Manifold](https://manifoldcad.org/docs/html/) implementiert einen eigenen Algorithmus; keine Gleichsetzung.

## Geprüfter Zwischenstand
Drei Seeds, jeweils 64 Achsen/571 Blätter, gleiche 16-seitige/24-längsgeteilte Rohrmeshes.
Gemeinsame Vereinigung lokal 271–287 ms; Achsaufbau separat, drei Wiederholungen je Methode/Seed.
Je Ergebnis 54.798–54.818 Dreiecke, eine geschlossene außenorientierte Komponente, Eulerzahl zwei.
192 unabhängige Raumwinkelproben je Seed/Methode, davon 128 innerhalb aller dünnen Achsen.
Sechs analytische Würfelvereinigungen mit Überdeckung/Gleichheit/Flächenkontakt stimmen im Volumen.
Die Methoden stimmen zusätzlich im Kernel-Differenzvolumen überein; das ist kein unabhängiges Oracle.

Gesamtpflanze und Anschluss-Nahbild optisch geöffnet: unveränderte dünne Zweige und Blattgruppen.
Die korrekte Vereinigung allein verbessert die organische Bildwirkung nicht; kein Qualitätsgewinn behauptet.
Sie ersetzt innere Rohrflächen durch einen gemeinsamen Körper. Nächster Bildbedarf ist lokale Ausrundung,
nicht weitere Optimierung der unveränderten scharfen Anschlussform.

## Abnahme und offene Lücke
Alle 64 Holzachsen/571 Blätter des bestehenden Jungbaums, gleiche stabile IDs, Anker und Formen.
Geschlossene orientierte Oberfläche, sichtbare dünne Enden und keine inneren Rohrkappen;
analytische Boolesche Prüffälle, gemeinsame Eingaben und getrennte Erzeugungs-/Produktkosten.
Belaubte Gesamtansicht plus Anschluss-Nahbild optisch prüfen; keine reine Verwaltungsverbesserung.
Vollständige organische Weldbindung, Rinde/UVs, Wind und vollständige LODs bleiben offen.

## Lokaler Anschlusskragen
Hermite-Ringe auf Raycast-Treffern verworfen: Seed 17 verlor 1,09×10⁻⁵ m³ Holz und zeigte 945 Normalenwidersprüche.
Stattdessen lokale Eltern-/Kinderhüllen nach 2352 hinzufügen und gemeinsam mit allen Originalrohren vereinigen.
Elternsegmente innerhalb eines Astes per Maximum, erst Eltern-/Kinderfelder symmetrisch mischen.
Kleine vollständige Volumenprodukte je Anschluss; keine globale Feingitter-Erzeugung für dünne Zweige.
Stabile Astachsen/Organanker bleiben erhalten; Parameter und räumliche Unterstützung explizit speichern.
Dichte/Blockabtastung desselben Feldes vergleichen; tatsächliche Nah-/Gesamtbilder und Kosten prüfen.

Normalen: unveränderte Dreiecke exakt übernehmen; neue Ecken per kurzem Strahl auf die unvereinfachte
Referenz projizieren. Referenz vor Raycast als ein Original markieren und Normalen der neuen Face-Reihenfolge
zuordnen; RayHit-Indizes gehören zur internen Triangulierung, nicht zu alten Export-Run-Gruppen.
Normalen der Kragen in Oberflächennähe über die tatsächlichen Eltern-/Kinderrohre angleichen.
Dies ist Normalengestaltung für den lokalen Übergang, kein Beleg für eine global G1-stetige Geometrie.
Gebündelte Eingaben ersetzen dieselben skalaren Strahlabfragen; alle 63 Ergebnisse bis 3,3×10⁻¹⁶ gleich.
Intervallbasierte Normalauswahl verworfen: zusätzliche Klassifikation langsamer, sichtbare Normalen verändert.
Kleine Probe: 242.482 Vertexabfragen von 9,53 auf 5,36 s; kein Geräte-/Runtime-Nachweis.

Boolesche Splitter: Seed 23 enthält trotz Kernelvereinfachung ein Dreieck mit 5,6–8,1 nm Kanten.
Lokale Kantenkollaps-Bereinigung nur für vollständig unter 0,1 µm liegende Splitter;
Linkbedingung, erhaltene Orientierung und unveränderte Flächenprüfung bleiben verbindlich.
Höchstens 32 getrennte Kollapskanten; keine akkumulierte Vertexverschiebung, sonst expliziter Fehler.
Geometrie vor Normalenübertragung bereinigen; Verschiebung und Anzahl im Produktbericht ausweisen.
Drei Seeds geprüft: 160.618–164.720 Dreiecke, geschlossen, Eulerzahl zwei, keine Normalenumkehr.
Je 192 unabhängige Raumwinkelproben, davon 128 innerhalb aller 64 dünnen Achsen.
Beobachteter Verlust gegenüber Originalholz 7,7–9,9×10⁻¹² m³, unter dem 10⁻¹⁰-m³-Probenbudget.
Blockabtastung 4,10–4,75 s gegen dicht 6,02–7,34 s, drei Wiederholungen; alle Mesharrays identisch.
Normalen mit gebündelten Strahlen 2,45–2,95 s, Vereinigung 4,53–5,63 s, je einmal; erhebliche Kostenlücke.
Gesamter Prüfprozess einschließlich beider Sampler/Referenzprüfungen: Peak-RSS 917,6 MiB.
Neue 1280×720-Nah-/Gesamtbilder optisch geprüft: sichtbare Anschlussaufweitung, dünne Zweige erhalten.
Stammschattierung ohne langen Normalenstreifen; natürliche Krone und Rindenmaterial bleiben offen.

## Strahlkosten
Profiler: skalare Elternflächenabfragen dominieren die Normalenanpassung.
Einfache AABB-Vorauswahl verworfen: 484.964 → 399.922 Abfragen, exakt gleiche Normalen, kein Zeitgewinn.
[Embree, SIGGRAPH 2014](../doc/references/vegetation/siggraph/2014-embree-ray-tracing.pdf), Seite 1 optisch geprüft,
begründet gebündelte CPU-Abfragen. Externes `embreex==4.4.0` in `.venv`, kein gebündelter Fremdcode.
Referenz lokal zentrieren/skalieren; float32-BVH liefert Dreieckskandidaten, Double-Ebene präzisiert Treffer.
Beide Strahlhälften suchen jeweils den nächsten passend orientierten Treffer, maximal 16 Schnittschritte.
Fortschritt 2×10⁻⁶ der Referenzausdehnung; Näherung ausdrücklich nur gegen geprüfte Kalibrierfälle abnehmen.
Fehlende Treffer mit möglichem Boxschnitt, randnahe baryzentrische Treffer und streifende Abfragen
auf den bestehenden Double-Raycaster zurückführen; reine float32-Abfragen verfehlten einzelne Treffer.
Getrennte Referenzaufbaukosten, drei abwechselnde Wiederholungen; Geometrie, Trefferabdeckung und Normalen prüfen.
Vollständige Oberfläche und aktuelles Nahbild bleiben das Übernahmekriterium.
Vollständige drei Pflanzen weiterhin geschlossen und ohne Normalenumkehr; gleiches Verlustvolumen.
Gespeicherte Gesamtpflanze: Vertices/Dreiecke exakt gleich, maximale Normalenkomponente 5,2×10⁻¹³ verändert.
777 Mehrkörper-/Maßstabs-/Verschiebungsproben: Trefferabdeckung gleich, relativer Positionsfehler ≤1,14×10⁻¹⁰.
Drei Pflanzen/je drei abwechselnde Läufe: Normalenmediane 2,32–2,46 statt 5,58–5,74 s (2,33–2,40×).
98.965–104.825 Rückfallabfragen je Pflanze; Referenzaufbau zusätzlich 195–208 statt 112–121 ms.
Alle 189 Anschlussprodukte behalten Geometrie/Trefferabdeckung; Normalenkomponenten maximal 5,91×10⁻¹³ abweichend.
Neue Nah-/Gesamtansicht optisch geöffnet: beide 1280×720-Bilder pixelgleich zum vorherigen skalaren Verfahren.
Nutzerauftrag: aktuelle Nah-/Gesamtbilder neu in 2560×1440; Szenario-Vorgaben für weitere Renderings angehoben.
Die 1280×720-Pixelgleichheit bleibt ein getrennter Vergleich, keine Behauptung über neue Bildabmessungen.
Im 2560×1440-Nahbild feine Schattierungslinien an einem dünnen Ast sichtbar.
Kontrollbild ohne Schatten: Linien verschwinden; Selbstverschattung ist die Ursache.
EEVEE mit `bpy==5.2.2` übernommen; Geometrie/Normalen und Schatten erhalten.
[Chiang/Li/Burley, SIGGRAPH 2019](../doc/references/vegetation/siggraph/2019-taming-shadow-terminator.pdf):
beide Seiten mit Gleichungen/Diagrammen optisch geprüft; EEVEE nutzt Positionsoffsets statt ihres BRDF-Faktors.
Gemeinsame Preview-Konfiguration erhält Standardwerte: Normaloffset 0, Geometrieoffset 0,1.
EEVEE 5.2 mit Null-Normaloffset beseitigt bereits die Linien; zusätzliche 0,1 nur ≤1/255 Pixelabweichung.
Alle Python-Renderer auf das gepinnte `.venv`-bpy migriert; tatsächliche Einstellungen im Bildbericht gespeichert.
Alle 16 Showcase-Ansichten und drei Plot-Vergleichsaufnahmen neu in 2560×1440 optisch geprüft.
Vergleichsplots prüfen gemeinsame aktuelle Renderer-Hashes; der alte Dirty-Plangenerierungsmarker ersetzt dies nicht.
Relative Bildausschnitte an die tatsächliche Auflösung binden und die Originalpixel unverändert vergleichen.
