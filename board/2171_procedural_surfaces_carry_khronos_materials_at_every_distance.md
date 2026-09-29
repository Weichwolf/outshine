Type: feature
State: active
Architecture: ready
Parent: 2169
Area: world, render, generators
Tags: materials, visual
Depends:
Priority: P1

# Baustoffe wirken in Nähe, Bewegung und Licht glaubwürdig

## Ergebnis

Eine Straße zeigt körnigen Asphalt, mineralischen Beton, verputzte Fassaden, glaubwürdiges
Glas und gegliederte Dachoberflächen. Sie unterscheiden sich durch Maßstab, Rauheit, Relief
und Lichtantwort. Keine uniforme Farbfläche und keine aufgemalte Fensterreihe ersetzt Geometrie.

## Vorhanden und offen

Native MR-Materialien, UVs und Materialbindung existieren. Die geöffneten Places wirken dennoch
flach und repetitiv. GroundMaterials trägt bereits GrainSizeM, HeightAmplitudeM und Rauheit.
Diese vorhandene Materialbeschreibung mit stabiler Oberflächenauswertung verbinden.

## Umsetzung

1. Eine bestehende Straßen-/Gebäudeszene unter gleicher Kamera und Beleuchtung verbessern:
   Asphalt, Beton, Putz, Glas und Dach als kleine erste Materialfamilie. Rezepte und Maßstäbe
   im bestehenden Materialdatenpfad halten; keine zusätzliche Registry vor sichtbarem Nutzen.
2. GroundMaterials besitzt Terrain-/Bodenparameter; SubjectMaterials die native Bindung.
   Generatoren liefern Gebäudekoordinaten in Metern; groundLit/groundClass und subjectLighting
   konsumieren dieselben physikalischen Parameter. Stabile Welt-/Objektkoordinaten verwenden.
3. Farbe, Roughness und gefiltertes Normal-/Bump-Detail gemeinsam auswerten. Putz bleibt matt,
   Glas zeigt Reflexion und Tiefe, Metall verwendet korrekte Metalness. Lineare Datenkanäle,
   sRGB-Farbe und vorhandenen Metallic-Roughness-Pfad erhalten. Keine Helligkeitskosmetik.
4. Materialwechsel folgt Konstruktion und Nutzung: Sockel, Wand, Fensterrahmen, Glas, Dach,
   Asphalt und Bordstein besitzen sinnvolle Grenzen. Feuchte/Alterung ergänzt das Material;
   sie ist kein zufälliger Schmutzfilter und benötigt keinen vorgezogenen Wettersolver.
5. Frequenzen in der Entfernung filtern; Muster dürfen nicht schwimmen oder flimmern.
   Echtes Displacement verändert Kontakt/Geometrie und benötigt deren Fehlergrenzen;
   der erste Schritt verwendet Oberflächennormalen und überdeckt keine Terrainfehler.

## Grenzen und Abhängigkeiten

Kein Startblocker durch Wolken, NPCs, Vegetation oder den vollständigen Import-Orakelkatalog.
Vorhandene rote Materialbefunde bleiben offen; betroffene Verträge beim Ändern reparieren.
2138 liefert Fensterlaibungen, Rahmen und Eingänge. 2167/2128 liefern Licht und Schatten;
Materialien werden zunächst im bestehenden Licht beurteilt, nicht bis dahin zurückgestellt.
Unbekannte oder ungültige Materialdaten ergeben einen expliziten Fehler/definierten Fallback.

## Fertig, wenn

Darmstadt, Husum und Wien zeigen in Straßenhöhe bei 5/30/200 m unterscheidbare plausible
Baustoffe; Bewegung, Streiflicht und Entfernung erhalten die Wirkung ohne Texturschwimmen.
Alle Places profitieren vom selben Materialpfad; keine Ortskorrektur. Kosten bleiben begrenzt.
Vertauschte Daten-/Farbkanäle oder falscher Weltmaßstab müssen diese Abnahme sichtbar verletzen.
make format; betroffene Material-/Shader-Suites; make lint; alle Places rendern und PNGs öffnen.

## Aktiver Schritt: Fassaden behalten ihren Materialanteil in Entfernung

`facadePattern.glsl` blendet Fenster und Rahmen bei großen Pixelfußabdrücken vollständig
aus. Entfernte Fassaden werden dadurch zu hellen Wandflächen; die Materialverteilung
verschwindet statt gefiltert zu werden. `periodicBand.glsl` integriert periodische Bänder
über den Pixelfußabdruck. Fassade nutzt getrennte horizontale/vertikale Ableitungen;
Fenster, Rahmen und Fugen behalten ihren Flächenanteil auch unter einem Pixel.
Keine neue Geometrie, Farbpalette oder LOD-Fehlerschranke. Analytische Flächenmittel und
unabhängige numerische Integration prüfen den Filter; Darmstadt/Husum und alle Places
prüfen Bildwirkung, Flimmern und Kosten. Räumliche Fassadendetails bleiben WI 2138.

## Archivvergleich

Rosenheim/Flensburg unterscheiden warmen Ziegel, hellen Putz, Backstein, dunkle Dächer
und Glas schon aus der Webcam-Perspektive. Erst diese Flächenanteile und Maßstäbe lösen,
dann Mikrostruktur. 2325 steuert Nässe/Schnee materialgebunden. Kamera-Sensorflecken,
Kompressionsrauschen und fehlende Vegetation sind keine Materialvorlagen.
