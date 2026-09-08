Type: debt
State: active
Parent: 2169
Area: render, build
Tags: webcam, measured
Depends:

# GLSL is the shader source and each SDL backend proves its pipeline

## Repariert

12ceb790 migriert die Runtime-Shader auf GLSL/SPIR-V über glslang und SDL_shadercross;
alle neun Places rendern aktuell über SDL_GPU/Metal. Die ursprünglichen MSL-Pfade und
„vier Compute-Kernels erst migriert“-Zwischenstände sind überholt.
SDL_GPU akzeptiert nicht auf jedem Backend SPIR-V unmittelbar: SDL_shadercross erstellt
die passende Backendform. GLSL als Quelle ist nicht automatisch Khronos-Materialkonformität.

## Offen / Abnahme

- [ ] Buildpaket enthält alle Graphics-/Compute-Stages und passende Reflection; keine
      handgeschriebene MSL-/Metal-Implementierung im Renderer, reproduzierbare Compilerpins.
- [ ] Backend-Smoketests für tatsächlich unterstützte SDL-Backends; nicht vorhandene Geräte
      als ungeprüft ausweisen. Vulkan-/DXIL-Erfolg nicht aus Metal-Renders ableiten.
- [ ] Khronos-Corpus für Maps/Normalen/Mirroring/Specular unverändert prüfen; bekannte rote
      Materialfälle schließen nicht durch gelockerte Schwellen. 2171 besitzt die Umsetzung.
- [ ] Binding-/Storage-Synchronisation mit negativer Kontrolle; 2149 hält Konventionen,
      2093/2153 Lint-/Harness-Schulden. Quellmigration allein schließt sie nicht.

Wahl: Khronos-Tools und SDL_shadercross als portable Grenze; Filament als Materialreferenz.
Keine experimentellen Metal-RT-/Imageblock-Abzweige im verbleibenden Boardauftrag.

## Paket- und Binding-Prüfung

Benchmark: SDL-GPU-SPIR-V-Bindingkonventionen und echte SPIRV-Cross-Reflection:
https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader
https://wiki.libsdl.org/SDL3/SDL_CreateGPUComputePipeline
Make veröffentlicht dieselbe Artefaktliste, die sein shaders-Target baut. Der alte
MSL-Scanner mit grünem 0/0 ist ersetzt. Lint und make test-shader-artifacts prüfen
jedes Artefakt: vorhanden, reflektierbar, main in der richtigen Stage, lückenlose
Descriptor-Sets in SDL-Reihenfolge, zulässiges Ressourcenprofil und feste Workgroups.
Leere/partielle Listen, zusätzliche/unvollständige Artefakte und Toolfehler sind rot.
Pro Artefakt Hash, Ergebnis, Reflection und Laufzeit sowie Tool-Fingerprint im System-Temp.

Nachweis: 455/455 Artefakte (39 Vertex, 408 Fragment, acht Compute); Paketprüfung
im ersten Lauf 0,73 s. Acht Testgruppen mit realem glslang/SPIRV-Cross: alle Resource-
Klassen, entfernte Graphics-/Compute-Dateien, falsche Sets/Bindings/Stages, leere und
partielle Inventare, Descriptor-Arrays/Push-Constants/Spezialisierung außerhalb des
unterstützten Profils sowie fehlendes/fehlerhaftes/ausbleibendes Reflection-Tool.
Keine Shader- oder Renderänderung und keine behauptete neue Bild-/Backend-Abnahme.

## Verbleibende Selektor- und Laufzeitabnahme

Graphics-Selektoren sind noch nicht vollständig an die Paketprüfung angebunden.
Der Compute-Katalog unten deckt seine Artefakte und Shape-Zähler ab, aber keine
konkreten gebundenen Ressourcenidentitäten oder Host-Layouts. Gemeinsame typisierte
Shader-Deskriptoren/Selektoren für Runtime und Inventarexport verwenden; deren vollständige
Variantenmenge gegen Buildartefakte, Reflection und CPU-Verträge prüfen. Neue Consumer
müssen automatisch teilnehmen; keine zweite handgepflegte Liste oder C++-Textheuristik.
- [ ] Fehlende Renderer-Variante, falscher Shape und falsch gebundene Ressource werden rot.
- [ ] Host-/Shader-Layouts und Stage-Interfaces samt Negativkontrollen stimmen überein.
- [ ] Synchronisation und tatsächlich unterstützte Backends wie oben abgenommen.
WI 2207 besitzt Paketauflösung und checkout-unabhängigen Start.

## Compute-Katalog und nachgewiesener Stand

Alle acht Builtin-Compute-Programme liegen in einem constexpr-Katalog: typisierte ID,
Artefaktpfad und ComputeShape. Stage-Pipeline-Aufbau und Workgroup-Daten verwenden
dieselbe ID; Cull/Scan/Compact haben keine getrennten String-/Shape-Argumente mehr.
CreateComputePipeline liefert nodiscard expected mit eindeutigem RAII-Besitz; ungültige
ID/GPU publiziert keinen leeren Erfolgswert. CompileComputePipeline bleibt die untere
SPIR-V-Ressourcengrenze mit unveränderten negativen Shape-Prüfbedingungen.
Ein kompilierter Export liest den Katalog als span. Make/Lint vergleichen die vollständige
Compute-Artefaktmenge und alle neun Shape-Felder gegen Reflection, ohne Python-Kopie
der Runtime-Zähler. Fehlende/zusätzliche/ungültige Katalogeinträge sind rot.

Nachweis: 455/455 Artefakte, 8/8 Compute-Verträge; zehn Testgruppen einschließlich
Mutationen jedes Shape-Felds und fehlender Einträge grün. 32 echte GPU-Checks auf Metal
prüfen sämtliche Builtin-Pipelines, ungültige IDs/GPU und bisherige Compiler-Fehlerpfade.
Alle 455 Shaderbytes und beide visuell geöffneten Kamera-PNGs sind gegenüber der
vorher gesicherten Basis unverändert. Konventionssuite 28/29: bekannter Mipmap-
Wiederholungsfehler (2179), drei Vergleiche mit je 449 abweichenden Kanälen, maximale
Abweichung 0,00268555, Tiefenwerte identisch. Kein Retry bis grün, keine Schwellenlockerung.
Tidy weiterhin 212. Stage-Zustand/Submission aus 2190 und obige Graphics-/Host-/Slot-
Abnahmen bleiben offen; dieser Schritt ist keine vollständige Stage-Modell-Abnahme.
