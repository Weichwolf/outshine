# Unsichtbare Werfer vor der Licht-Tiefengrenze (#424)

`OffscreenCastersPreserveDirectionalShadows.cpp` prüft die öffentliche Engine-API
mit einem festen diffusen Empfänger und einem geschlossenen Opaque-/Masked-Werfer.
Die Kamera bleibt bei +100 m und blickt zum Empfänger bei 0. Ihr Far ist 2.000 m;
alle Werfer sind unsichtbar. Die Sonne steht in positiver Z-Richtung.

Ein expliziter Lichtradius von 16.384 m ergibt hier ein Licht-Tiefenintervall von
±32.768 m. Der Werfer liegt innerhalb bei +16 km, sonnenwärts außerhalb bei +40 km,
überquert diese Grenze oder liegt hinter dem Empfänger bei −40 km. Ein Werfer
zwischen Sonne und Empfänger verdeckt ihn auch vor der künstlichen Light-Near-
Grenze. Depth-Clamp erhält diese Verdeckung; Clipping entfernt sie.

Der Test vergleicht vollständige lineare Bilder mit unveränderter Kamera und
Geometrie. Innen-/Außen-/Grenzfälle müssen denselben Schatten erzeugen; der hintere
Werfer muss das unverschattete Bild erhalten. Der direkte Lambert-Lichtentzug ist
auf 8×8×3 zentralen Werten mit explizitem Reflexionsgrad 0,5 unabhängig geprüft;
1 % berücksichtigt die vorhandene HDR-Präzision. Leere oder durchgehend schwarze
Bilder erfüllen den Lichtentzug nicht.

```sh
sh test/run.sh outshine/include/Outshine/OffscreenCastersPreserveDirectionalShadows
```

Normale Tests schreiben keine Bilder. Ein optionales einzelnes CLI-Ausgabepräfix
am erzeugten Testprogramm speichert neun RGBA32-Floatdiagnosen bei 32×32.

## Tatsächlicher Gegenlauf

Testquelle: d5a322c4b. Linux/RTX 3060, identisches SPV-Paket, getrennte passende
Header-/Archivpaare. Der reguläre bisherige Host c177f25de hat denselben nativen
Produktquellstand wie die PR-Basis c8ba7b998. Der native Gegenstand ist die vollständig
privat kopierte #410-Linkclosure mit Renderer 187a5b380 und Backend 6156a8010;
kein Objekt und kein Header dieser Produzenten wurde verändert.

| Öffentliche Regression | Bisheriger Host | Unveränderter nativer Clip-Pfad |
|---|---|---|
| Normal, keine Bildausgabe | 132 Checks / 0 Fehler | 132 / 4 |
| Mit neun Captures | 141 / 0 | 141 / 4 |
| Innen-/Gegenrichtung | Bytegleich | Bytegleich |
| Sonnenwärts außerhalb | Schatten bleibt | 1.024 Pixel verlieren Verdeckung |
| Grenzübergang | Schatten bleibt | 96 Pixel verändern sich |

Die vier Fehler betreffen ausschließlich Außen-/Grenzfälle beider Materialarten.
Die Materialien enthalten keine Bilder; #425s verlorene Mipkette ist kein Einfluss
in diesem Gegenfall. #425 erklärt unabhängig den tatsächlichen Husum-Befund.

Die Quelle kompiliert mit ausschließlich öffentlichen Repository-Includes plus
Check.h, Format und fokussiertes clang-tidy ohne eigene Befunde. Die unveränderten
Check.h-Befunde bleiben ausgewiesen; kein vollständiges Integrations-Lint oder
Backendfix wird durch diese Testlieferung behauptet.

Der zweiseitige PDF-Vergleich ist optisch geprüft und liegt lokal im Haupt-Checkout:
`build/shots/reference/offscreen-shadow-d5a322c4b/comparison.pdf`.
Originale, verlustfreie D32-Diagnosen, komplette Linkinput-/Shaderhashes und terminale
Befehle: `/tmp/outshine-light-shadow-parity-18a5ddc8a/`, insbesondere
`producer.json`, `public-regression/public-only.json` und `public-regression/*.rgba32`.

#424 besitzt die bestätigte ClampDepth-/DepthClamp-/Unsupported-Auswahl und die
Backend-/Verbraucherumsetzung. Dieser PR liefert den öffentlichen Lichtvertrag.
Keine GPU-/Frame-/p99-, Speicherpeak-, Metal-/A18- oder vollständige Place-Abnahme;
keine zusätzliche Runtime-Arbeit, keine neuen Pins oder Orakel.

Primärquellen: [SDL-Rasterzustand](https://wiki.libsdl.org/SDL3/SDL_GPURasterizerState),
[Vulkan-Tiefe](https://docs.vulkan.org/guide/latest/depth.html).
