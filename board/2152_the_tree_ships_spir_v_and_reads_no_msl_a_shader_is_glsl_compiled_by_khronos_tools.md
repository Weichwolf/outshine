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

## Belegter blinder Shader-Check

Quellaudit 2026-09-08: test/scripts/entries_vs_shaders.py sucht nur *.msl und fs*/vs*-
Stringnamen. Nach GLSL-Migration findet es weder Definitionen noch Aufrufe und liefert
bei 0/0 trotzdem Erfolg. VertexArms-static_asserts beweisen nicht alle Compute-/Graphics-
Artefakte oder Bindings. ShaderFile prüft einzelne geladene Artefakte, nicht Paketvollständigkeit.
Buildmanifest, Varianten, SPIR-V-Reflection und tatsächlich verwendete Stages vergleichen;
leere oder fehlende Eingabemengen ausdrücklich ablehnen. WI 2207 besitzt Paketauflösung.
- [ ] Absichtlich entfernte Graphics-/Compute-Variante und falsches Binding werden rot;
      vollständiger GLSL-Build grün. Kein MSL-Textscanner als Shader-Abnahmenachweis.

## Aktiver Schritt: Paket- und Binding-Prüfung

Benchmark: SDL-GPU-SPIR-V-Bindingkonventionen und echte SPIRV-Cross-Reflection:
https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader
https://wiki.libsdl.org/SDL3/SDL_CreateGPUComputePipeline
Make veröffentlicht dieselbe Artefaktliste, die sein shaders-Target baut. Lint prüft
jedes deklarierte Artefakt: vorhanden, reflektierbar, main in der richtigen Stage,
lückenlose Descriptor-Sets in SDL-Reihenfolge, zulässige Ressourcentypen und feste
Compute-Workgroups. Leere Listen, zusätzliche/unvollständige Artefakte und Toolfehler
werden rot. Pro Artefakt Ergebnis und Laufzeit im System-Temp protokollieren.
Reale glslang-/SPIRV-Cross-Fixtures prüfen fehlende Graphics-/Compute-Dateien,
falsche Sets/Bindings/Stages und Prozessfehler. Keine Änderung gültiger Renderbilder.
Dies ersetzt den blinden MSL-Scanner, schließt aber weder den Abgleich mit allen
Renderer-Selektoren und ihren Shape-Verträgen noch Backend-/Synchronisationsabnahme.
Diese verbleibenden Nachweise bleiben ausdrücklich Teil dieses WI.
