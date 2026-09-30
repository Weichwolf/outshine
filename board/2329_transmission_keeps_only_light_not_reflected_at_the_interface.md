Type: bug
State: active
Architecture: ready
Priority: P0
Parent: 2145
Area: render
Tags: water, materials
Depends:

# Transmissive surfaces retain the energy left after interface reflection

## Ergebnis und vorhandene Fähigkeit
Bei flachem Blick spiegeln Wasserflächen stärker und zeigen weniger Untergrund.
Der bestehende GGX-Pfad berechnet Fresnel bereits für reflektiertes Licht. Der
Transmission-Zweig in `render/shaders/litFragment.glsl` addiert jedoch den ganzen
Hintergrund unabhängig von dieser Reflexion. Körbersee zeigt deshalb zu viel Bett.

## Implementierung und Besitz
- Derselbe normalisierte Blickvektor, Shading-Normal und Material-Fresnel gelten
  für Reflexion und Transmission. Der Hintergrund erhält den verbleibenden Anteil
  `1 - F`; Metallanteil überträgt kein Hintergrundlicht. Vorhandene Tönung und
  deklarierte Volumenabsorption bleiben Faktoren desselben Beitrags.
- Kein Alpha-Fade, festes Tiefenimitat oder Foto-Tint. Der vorhandene Transmission-
  Pass und native Materialien bleiben bestehen. Wassergeometrie wird nicht neu gebaut.
- Referenz ist Filaments Fresnel-/Dielektrikumsmodell im lokalen Checkout
  `ef1a133d`; keine Behauptung über proprietäre RAGE-Implementierung.
- Echte Tiefenabsorption, Brechung, Wellen, Küsten und Szenenreflexion bleiben
  eigenständige Lieferungen von 2145/2129. Dieser Fix liefert keine Gewässerpegel.

## Abnahme
- Einfarbiger Hintergrund hinter einer dielektrischen Fläche: Der gemessene
  zusätzliche RGB-Beitrag entspricht analytisch `Hintergrund * (1 - F)` für
  senkrechten und flachen Blick. Gleiche Beleuchtung über schwarzem Hintergrund
  isoliert den Reflexionsbeitrag; ein ungedämpfter Hintergrund muss scheitern.
- Körbersee/Husum öffnen und mit dem vorherigen Commit vergleichen. Bestehende
  Straßen-/Terrain-Pixel erhalten; CPU/GPU-Kosten im Place-Budget. Fokussierte
  GPU-Abnahme, Shader-Artefakte und vollständiger Lint.
