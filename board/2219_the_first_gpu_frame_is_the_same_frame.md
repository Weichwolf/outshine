Type: bug
State: active
Parent: 2188
Depends: 2179, 2190
Area: render, SDL3, test
Tags: determinism, backend, measured

# The first GPU frame is the same frame

## Befund

`MipmappedChessRepeatsLinearPixels` misst 467 geänderte lineare Kanäle (maximal
0.00268555) zwischen erstem und allen folgenden Frames; Tiefe und alle späteren
Frames sind bitgenau. Ein verworfener Vorlauf-Frame macht den Test grün, ist aber
keine Reparatur. Mip-Copy-Reihenfolge, `SubjectDraw::CarryFrame` und GLSL `precise`
ändern die Ursache nicht.

`ScoreHowFarTheAirReaches` zeigt denselben Effekt nach zwei identischen
Deklarationen: 262 von 230400 RGBA8-Kanälen weichen ab. Seine Bandwerte wirken nur
gerundet gleich; die exakte Wiederholungsprüfung ist zu Recht rot. Streaming ist nach
`preload` abgeschlossen. Damit ist der Befund backendübergreifend am ersten GPU-Draw
oder an dessen vorheriger Ressourcen-/Pipelinestellung zu behandeln, nicht als
Atmosphären- oder Mip-Ausnahme.

Die Culling-Inspektion hatte zuerst 4 394 688 gegenüber 3 989 952 Indizes bei je
33 Batches gezeigt. Eine verworfene Untersuchung mit Tangenten-Kugelrechteck und
zwei zusätzlichen Pyramidentexeln hielt danach in allen Frames exakt 4 394 688
Indizes, die linearen Frames unterschieden sich dennoch um 404 Kanäle. Das
Culling-Ergebnis ist somit nicht die Ursache. ABeautifulGame trägt keine Alpha-Modi
und `Shape` clustert ohnehin nur opaque/masked Geometrie. Der verbleibende Übergang
ist der `occludes`-Steuerpfad beziehungsweise der Zustand der Tiefenpyramide selbst.

Lokaler Referenzstand: `../SDL` fa2c02b (3.4.16) kompiliert MSL über
`newLibraryWithSource(..., options:nil)`; `../SDL_shadercross` 1ff05be bietet für
SPIR-V→MSL nur die Ziel-MSL-Version, keine Präzisions- oder Compileoption. Die
erzeugte GLSL-Variante ist `texture2d<float>.sample`.

`SDL_GPU_DRIVER=vulkan` verweigert dieser Host mit `unsupported`; ein zweites
SDL_GPU-Backend ist hier nicht verfügbar und bleibt als externe Abnahme offen.
`xcrun metal` und `metallib` fehlen ebenfalls; ein lokaler Metallib-Versuch ist
ohne vollständiges Xcode nicht reproduzierbar.

## Lösung und Abnahme

1. Die Tiefenpyramide vor ihrem ersten Culling-Zugriff als vollständige All-Far-Historie
   erzeugen und den gleichen `occludes`-Pfad ab Frame eins ausführen. Die Initialisierung
   gehört in den Setup-/Ressourcenübergang, darf weder einen Vorlauf-Frame noch Warten
   im Echtzeitpfad erzeugen und muss bei Zielwechsel erneut gelten. Erst dann
   Ressourcen-, Tabellen- und Pipelineübergänge einzeln gegen unveränderte Bilddaten
   prüfen. Uploadbatches und Diagnoseausgaben nicht behalten.
2. Falls Metal die Ursache ist, GLSL als Quelle behalten und einen reproduzierbaren
   backend-spezifischen Buildproduktpfad wählen, der die benötigte Compilersemantik
   ausdrückt. Keine handgeschriebene Vendor-Shaderquelle und keine Testfall-Ausnahme.
3. Dieselbe Wiederholbarkeitsabnahme auf mindestens einem weiteren SDL_GPU-Backend
   ausführen; Backend/Driver/Shaderprodukt protokollieren.

- [ ] Erster, zweiter und erneut deklarierter statischer Frame sind bytegenau;
      Mip-, Atmosphären- und Negativkontrollen bleiben fachlich unverändert.
- [ ] Nachweis trennt native Compiler-/Samplerabweichung von Enginezustand.
- [ ] Startupkosten, GPU-Synchronisation und Shaderprodukt sind gemessen; der
      Echtzeitpfad erhält keinen blockierenden oder versteckten Zusatzframe.
