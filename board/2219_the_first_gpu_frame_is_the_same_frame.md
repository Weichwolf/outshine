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

Die Culling-Inspektion trennt die Schach-Frames erstmals ursächlich: ohne vorige
Tiefenpyramide hält Frame eins 4 394 688 Indizes, spätere Frames 3 989 952 bei je
33 Batches. Tiefe bleibt exakt gleich. Die Occlusion-Historie darf ihre sichtbaren
Ergebnisse nicht ändern; gleiche Culling-Zähler sind dagegen kein allgemeiner Vertrag.
Konservative Projektion/Pyramidenabfrage und transparente Geometrie prüfen.

Lokaler Referenzstand: `../SDL` fa2c02b (3.4.16) kompiliert MSL über
`newLibraryWithSource(..., options:nil)`; `../SDL_shadercross` 1ff05be bietet für
SPIR-V→MSL nur die Ziel-MSL-Version, keine Präzisions- oder Compileoption. Die
erzeugte GLSL-Variante ist `texture2d<float>.sample`.

`SDL_GPU_DRIVER=vulkan` verweigert dieser Host mit `unsupported`; ein zweites
SDL_GPU-Backend ist hier nicht verfügbar und bleibt als externe Abnahme offen.
`xcrun metal` und `metallib` fehlen ebenfalls; ein lokaler Metallib-Versuch ist
ohne vollständiges Xcode nicht reproduzierbar.

## Lösung und Abnahme

1. Erstframe-Übergänge von Ressourcen, Tabellen, Compute-Culling und Pipelinebindung
   einzeln gegen unveränderte Bilddaten prüfen. Einen Plattformpfad nur mit belegter
   Ursache ändern; Uploadbatches, Diagnoseausgaben und Warm-up-Frames nicht behalten.
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
