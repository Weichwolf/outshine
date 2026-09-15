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
Indizes, die linearen Frames unterschieden sich dennoch um 404 Kanäle. Eine
vollständige All-Far-Pyramide ohne spätere Pyramiden-Updates ließ 400 Kanäle rot;
festes Jitter, keine History-Fortschreibung und unveränderter Linear-Targetindex
ließen 470 rot. Weder Culling-Ergebnis noch Pyramideninhalt oder Temporal-Fortschritt
sind die Ursache. ABeautifulGame trägt keine Alpha-Modi und `Shape` clustert ohnehin
nur opaque/masked Geometrie.

Lokaler Referenzstand: `../SDL` fa2c02b (3.4.16) kompiliert MSL über
`newLibraryWithSource(..., options:nil)`; `../SDL_shadercross` 1ff05be bietet für
SPIR-V→MSL nur die Ziel-MSL-Version, keine Präzisions- oder Compileoption. Die
erzeugte GLSL-Variante ist `texture2d<float>.sample`.

`SDL_GPU_DRIVER=vulkan` verweigert dieser Host mit `unsupported`; ein zweites
SDL_GPU-Backend ist hier nicht verfügbar und bleibt als externe Abnahme offen.
`xcrun metal` und `metallib` fehlen ebenfalls; ein lokaler Metallib-Versuch ist
ohne vollständiges Xcode nicht reproduzierbar.

## Lösung und Abnahme

1. `SceneHdr`, `SceneAerial`, `SceneLinear` und die beteiligten LUT-/Irradiance-Outputs
   nacheinander bytegenau lesen. Erst- und Folgeframe müssen pro Stufe dieselben
   Eingänge, Clear/Load/Store-Zustände und Abhängigkeiten haben. Den ersten abweichenden
   Producer mit einem vollständigen Ressourcenvertrag reparieren; keine Vorlauf-Frames,
   Uploadbatches oder Diagnoseausgaben behalten.
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
