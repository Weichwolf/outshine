Type: feature
State: open
Architecture: planned
Priority: P2
Parent: 2169
Area: generators, render, engine
Tags: vegetation, budget, streaming
Depends:

# Ground cover adds measured image value with bounded overdraw

## Scope und Priorität

Nach 2111s nativer Waldgrundlage; kein vollständiges globales Streaming als Startblocker.
Krautige Vegetation nutzt native Mesh-/Material-/Instanzprodukte. Verholzte Sträucher
teilen vorhandenes Wachstum 2176. Wolken liegen ausschließlich in 2140/2172.
Partikel/Feuer/Rauch sind ein späterer eigenständiger Slice der Gesamtreserve 2169,
keine zweite Runtime im selben Bodenvegetations-WI. Alte P5/P6-Wartefolge ist aufgehoben.

## Architektur und konkreter erster Entwurf

Vorhandene Scatter-/Instanzpfade prüfen. Ein deklarierter ebener Teststreifen mit EINER
Büschelform und einem geteilten Prototyp, Nah/Fern/Nah-Kamerafahrt; kein Artenframework.
Ground besitzt Bodenzustand/Eignung und semantische Gebäude-/Straßen-/Wasserfreiräume;
Generator besitzt Form/Placement, render residente Auswahl und Alpha-/Overdrawkosten.
Seeds und Grenzzuordnung folgen stabiler Weltidentität (2098), nie Kamera oder Arrival.
WeatherSnapshot liefert denselben Wind/Zeitvertrag wie Bäume und Wolken.

Unter Pixelmaßstab geht Mikrodetail in gefilterte Materialwirkung über. Geometrie
rechtfertigt sich durch Silhouette, Gegenlicht oder flachen Blickwinkel, nicht Entfernung
allein. Boden-only ist unabhängiger Bild-/Kostenvergleich, keine vollständige Weltvorbedingung.
Materialkerne aus 2171 verwenden; dessen gesamte Generatorabnahme blockiert den Prototyp nicht.
Budget 2314 umfasst Vegetation, Infrastruktur und Himmel; Alpha-Overdraw und Schatten
zählen vollständig, gemeinsame Prototypen werden einmal resident gehalten (2228).

Vor ready: Owner-Dateien, native Inputs/Outputs, begrenzte Placement-/Upload-/Residency-
Caps und Fehler-/Abbruchfluss festlegen. Öffentliche Client-Placement-Regeln 2126 sind
nur für einen solchen API-Ausbau nötig; keine Pflicht für die interne bestehende Pipeline.

## Abnahme

- [ ] Boden-only/Büschel bei Bewegung und Jahreszeiten: opened PNGs, Mehrwert und
      Übergänge; Overdraw/CPU/GPU/Bytes getrennt. Fehlende Büschel sind kein Timinggewinn.
- [ ] Nachbartiles, Wiederkehr und Seeds; Gebäude/Fahrbahnen/Wasser bleiben frei.
- [ ] Gegenlicht/Nah/Fern: stabile Coverage, begrenzte Arbeit, kein Windzeit-Sprung.
- [ ] Falsche Matrix, verlorene Ausschlussmaske, unbeschränkte Population, deaktiviertes
      Zeichnen und fehlende Freigabe verursachen unabhängige FAIL-Kontrollen.
- [ ] make format; fokussierte Owner-Suites; full make lint; öffentliche Client-Fahrt.
