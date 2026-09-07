Type: feature
State: open
Area: world, render
Tags: webcam, measured
Depends: 2145, 2167

# Water and rough surfaces reflect the generated world

## IST / Umsetzung

Husum, Wien, Koerbersee und Malcesine zeigen fast strukturlose dunkle Wasserflächen;
die Fotos spiegeln Himmel/Ufer/Berge. Die zu spiegelnde Welt ist die generierte Sandbox.

1. WaterBody-Geometrie aus 2145 erhält dielektrisches Fresnel, Absorption/Transmission,
   Tiefe und wind-/Fetch-abhängige mehrskalige Normalen. Ufer-/Fließschaum nur wo begründet.
2. Prefiltered Sky/local probes als vollständiger Fallback; günstiges SSR mit Depth-Pyramid,
   Thickness-/Validity-Test und zeitlicher Reprojektion ergänzen. Off-screen-Lücken dürfen
   nicht schwarz werden; Wasserbewegung in der Reprojektion berücksichtigen.
3. Für dominante ebene Wasserfläche begrenzten Planar-View mit Oblique-Clipping,
   gespiegelter Kamera/Winding, reduziertem LOD und Updatebudget erproben. Eine Fläche
   darf nicht die gesamte Welt pro Frame nochmals in voller Qualität zeichnen.
4. Roughnessabhängiger Übergang; transparente/gläserne und nasse Materialien nutzen
   denselben IBL-Vertrag. Keine Rekursion und keine doppelte Reflexionsenergie.

- [ ] Spiegelobjekt außerhalb des Hauptbildes bleibt über Fallback/Planar plausibel;
      falsche Planenlage/Clip-Ebene scheitert an geometrischem Spiegeloracle.
- [ ] Husum vertikale Kaimauer/Fronten; Malcesine Berge/Halbinsel; Koerbersee dunklerer
      kleiner See; Windreihe und Kamerafahrt auf Ghosting/Flicker prüfen. Keine exakten Wellen.
- [ ] Jede Stufe einzeln zeitlich und visuell vergleichen; 2092 misst den Gesamtrender.

Wahl: [Filament IBL](https://google.github.io/filament/dup/iblprefilter.html) plus gemessene
SSR/Planar-Stufen wie in öffentlich dokumentierten Echtzeitrenderern. RAGE nur Bildbenchmark.
Die frühere Metal-RT-Forderung entfällt: Hardwarefähigkeit ist kein SDL_GPU-API-Vertrag.
