Type: bug
State: open
Area: include, engine
Tags: architecture, lifecycle
Parent: 2188
Depends:

# Public handles will preserve owner identity and lifetime

## Befund und Entscheidung

Outshine.h: Renderer und SwapChain enthalten Engine*; Engine ist beweglich.
Engine.cpp: default Move verschiebt S_, bereits ausgegebene Fassaden zeigen weiterhin
auf das alte Engine-Objekt. Das ist ein belegtes Invalidierungsrisiko, noch kein
ausgeführter Crashnachweis. beginFrame prüft nur die Ausdehnung der übergebenen
SwapChain und beginnt dann den Frame der eigenen Engine: Besitzer werden nicht geprüft.
Host*, Generator-Referenzen und scene()-Referenzen brauchen denselben Vertragsaudit.

**Benchmark**: Filament dokumentiert Erzeugung/Zerstörung durch die Engine.
https://github.com/google/filament/blob/main/filament/include/filament/Engine.h
Unreal/RAGE-Namenskonventionen lösen keine Lebensdauerfrage; RAGE-Details unbelegt.

Entscheidung: Engine als stabilen, nicht beweglichen Owner führen; erzeugte Fassaden
als ausdrücklich geliehene, an diesen Owner gebundene Handles. Keine Shared-Ownership
nur zur Kaschierung falscher Zuständigkeit. Clients mit Engine-Moves auf eindeutige
Owner-Indirektion umstellen. Falls ein realer Client Move benötigt, vor Implementierung
stattdessen stabile State-Identität einschließlich Shutdown-Invalidierung begründen.
Fremde SwapChain vor jedem Frame-Zustandswechsel zurückweisen.
Lebensdauer, Threadbindung und Invalidierung aller geliehenen Public-Werte dokumentieren.

## Abnahme

- [ ] Compile-Prüfung des gewählten Move-Vertrags; keine still dangling Fassaden.
- [ ] Zwei Engines: fremde SwapChain wird mit konkretem Fehler zurückgewiesen,
      beide bleiben danach mit ihren eigenen Targets nutzbar.
- [ ] Renderer/SwapChain/Host/Generator/scene-Lebensdauer im öffentlichen Vertrag.
- [ ] Client baut ohne interne Includes; gültige Fenster-/Offscreen-Pfade unverändert.
- [ ] Negativkontrolle entfernt Owner-Prüfung und lässt Zwei-Engine-Oracle scheitern.
