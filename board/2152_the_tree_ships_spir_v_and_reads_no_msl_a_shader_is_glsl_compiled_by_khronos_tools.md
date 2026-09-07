Type: debt
State: open
Area: render, build
Tags: webcam, measured
Depends: nothing

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
