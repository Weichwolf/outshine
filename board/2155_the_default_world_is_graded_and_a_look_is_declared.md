Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: render, engine, client
Tags: lighting, shadows, hdr, presentation

# Light and camera response make existing world geometry convincing

## Ergebnis und Ist
Kohärentes Tages-/Nachtlicht, räumliche Schatten/Reflexionen und stabile Kameraantwort.
Native Sky/Atmosphären-LUTs, LightVisibility, HDR/TAA/Tonemap bestehen; Places wirken flach.
GGX-gefilterter Himmel/Split-Sum und Reverse-Z-PCF bestehen; lokale Weltreflexionen und
kalibrierte Schatten-/Belichtungswirkung fehlen. Glas bleibt oft zu dunkel.

## Besitzer und nächste Lieferung
Renderer besitzt Licht/Pässe/History, Client Kamera/Pacing, Place-Katalog Pose/FOV/UTC.
Zuerst vorhandene Stadt-/Bergformen mit Himmelsfüllung, Sonnenschatten und Belichtung verbessern.
2155 besitzt den gemeinsamen Look, 2171 Materialkomposition, 2172 Wetter/Wolken/Astronomie.
Asset-Rohlinge sind unbeleuchtet; aktuelle Beleuchtung bleibt Runtime und wird gezielt erneuert.

## Schatten und viele Lichter
- Vorhandene Rundum-Sonnenbereiche: 256/1024/4096 m Halbausdehnung plus Weltfit; texelstabil.
  4096² D32F mit vier 2048²-Bereichen kostet 64 MiB. Blickdrehung baut diese nicht neu auf.
  Tiefenintervalle aus Terrain/Instanzbounds schneiden; relevante Außen-Caster erhalten.
  Reverse-Z-Präzision, Empfängerebene/2×2-PCF, Bias und Übergänge ohne Kontaktverlust prüfen.
- Schatten nach Bildwirkung/Pass staffeln (2340); fremde Nah-/Fern-Verdeckung nicht übernehmen.
  Entfernte Caster können unsichtbar und trotzdem wirksam sein. Aktuelle Posen/Wind berücksichtigen.
- Tausende lokale Lichter: räumliche Tiefencluster/Clustered Forward statt Vollschleife je Pixel.
  Kleine Lichtmengen behalten günstigen Pfad. Überlauf explizit; keine still verlorenen Lichter.
  Emission, direkte Wirkung und Schatten getrennt budgetieren; keine Schattenkarte je Fenster.

## Reflexion und Kamera
- Medium → gemeinsame EnvironmentSpecular-Antwort → Gebäude/Terrain/Wasser/Glas; Welt-Up,
  Sonnenrichtung, Höhe/Horizont und Texelzentren explizit. Sonnendirektlicht nicht doppelt zählen.
- GGX/Smith und Roughness teilen direkte/indirekte Energie. Bestehender Atlas: sieben 64²-Stufen
  mit Rand, RGBA16F, 238,2 KiB. Octaeder-Ränder/Interpolation erhalten; Referenz Filament `ef1a133d`.
  Gemittelter Boden ersetzt keine lokalen Spiegelungen. Begrenzte SSR/Probes ergänzen gültige
  Treffer; fehlende/verdecke Treffer fallen auf denselben Himmel zurück, keine Sonderfensterfarben.
- HDR/linearer Farbraum/Belichtung teilen einen Ausgabepfad. Tageszeit/Exposition ändern keine
  Materialbasis. Kamera gegen Landmarken/Relief kalibrieren, Formfehler nicht durch Pose verstecken.
- TAA: gemeinsame Tiefe/Bewegung, NDC/UV/Y/Jitter; dilatierte Konturbewegung, Hintergrundreprojektion,
  gültige History am festen Raster. Disocclusion/Scene-/Ursprungswechsel aktualisieren betroffene Pixel.
  Farbclip begrenzt Ghosting/Ringing; Stillstand akkumuliert. Schärfe, Drehung, Wasser/Laub prüfen.
- Invariante Lichttabellen/Schatten vor erstem Messframe auf demselben Device vorbereiten und
  Submission abschließen; kein Zusatzframe. Änderungen erneuern nur abhängige Produkte.

## Gemeinsamer Look
Minimalistischer Solarpunk: klare Bauhausvolumen/-raster, gezielte Art-déco-Staffelung/Reliefs.
Wenige Materialfamilien und konsistente Proportionen; belegte Gebäudeformen/-farben erhalten.
Grau/Beige, warme Erdtöne, abgestufte Grüntöne und ruhige Blautöne teilen eine Palette (2171).
OKLCH bei Vorbereitung für Helligkeit/Chroma/Farbton und Hell-Dunkel-Hierarchie nutzen;
lineares RGB für Licht/Komposition. Abstand/Harmoniemodell ist kein Schönheitsbeweis.
Keine Paletteoptimierung je Fragment oder nachträgliche Objekt-Sonderfärbung. Details folgen
Bildwirkung. Vegetation/Boden/Schatten bilden gemeinsame Kontakte, keinen aufgesetzten Look.

## Bewährte Verfahren
[Frostbite-PBR](../doc/references/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf),
[Clustered Shading](../doc/references/lighting/hpg/2012-clustered-shading.pdf),
[Screen-Space-DDA](../doc/references/lighting/jcgt/2014-efficient-screen-space-rays.pdf):
gemeinsame Lichtantwort, begrenzte Lichtlisten/SSR. [TAA](../doc/references/presentation/cgf/2020-temporal-antialiasing-survey.pdf)
und [Moment Shadows](../doc/references/lighting/i3d/2015-moment-shadow-mapping.pdf):
Tiefe/Bias zuerst; Momente nur bei Gesamtgewinn samt Blur/Bytes/Errata.
[Color Compatibility](../doc/references/presentation/siggraph/2011-color-compatibility.pdf),
[Oklab](https://bottosson.github.io/posts/oklab/), [Recherche](../doc/references/README.md).

## Abnahme
Datierte klare/bedeckte Stadt-/Bergbilder gewinnen Tiefe/Materiallesbarkeit. Morgen/Abend/Nacht
plausibel; Bewegung ohne Ghosting, Kontaktverlust oder Belichtungssprung. Beton, Boden,
Vegetation und Himmel wirken zusammen. Bild/Kosten im gleichen Profil getrennt prüfen.
