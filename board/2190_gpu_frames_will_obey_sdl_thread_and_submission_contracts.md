Type: bug
State: open
Area: render, host
Tags: architecture, gpu, lifecycle
Parent: 2188
Depends:

# GPU frames will obey SDL thread and submission contracts

## Befund und Entscheidung

SceneRenderer.cpp Draw: SDL_AcquireGPUCommandBuffer ohne unmittelbare NULL-Prüfung;
Swapchain-Acquire fasst Fehler und erfolgreichen NULL-Output im gleichen Fehlerlog
zusammen und läuft in die Pass-Encodierung weiter. SDL erlaubt NULL etwa minimiert.
SceneRenderer hält Fence-Ring; GpuOwned.h besitzt bereits RAII-Wrapper.
Dies beweist noch keinen GPU-Use-after-free; Submit-/Abbruchpfade vollständig auditieren.

**Benchmark**: SDL3 ist hier maßgeblich, nicht eine vermutete Unreal-/RAGE-RHI.
https://wiki.libsdl.org/SDL3/SDL_AcquireGPUCommandBuffer
https://wiki.libsdl.org/SDL3/SDL_WaitAndAcquireGPUSwapchainTexture
https://wiki.libsdl.org/SDL3/SDL_ReleaseGPUBuffer
CommandBuffer bleibt auf seinem Acquire-Thread; Swapchain-Acquire auf dem Thread,
der das Fenster erzeugte. NULL-Target ohne SDL-Fehler heißt Frame überspringen.
Nach erfolgreichem Swapchain-Acquire ist Cancel verboten; jeder Pfad besitzt genau
seine dokumentierte Submit-/Cancel-Verantwortung. Kein altes HostSurface weiterverwenden.

Framezustand explizit modellieren: Vorbereitung, Aufnahme, Präsentation übersprungen,
Abschluss/Fehler. Histories und Framezähler nur gemäß tatsächlich abgeschlossener Arbeit
fortschreiben. Frame-Pacing von unzulässigem IO-/Worker-Warten getrennt messen.
Device überlebt Ressourcen; SDL verzögert Release selbst bis sicher. Zusätzliche
Retirement-Queues nur für tatsächlich außerhalb SDL liegende CPU-/Produktlebensdauern.
2149 besitzt persistente Upload-Ringe; dieses WI besitzt Fehlerpfade und Threadvertrag.

## Abnahme

- [ ] Fenster minimieren/wiederherstellen, Resize und Offscreen rendern korrekt.
- [ ] Injizierter Acquire-/Submit-Fehler: kein NULL-Encode, keine alte Swapchain,
      kein doppeltes Submit/Cancel und nachvollziehbarer Fehler bis zur Public API.
- [ ] Thread-Oracle prüft Fenster- und CommandBuffer-Owner auch nach 2130.
- [ ] Shutdown mit ausstehenden Uploads/Readbacks ist leak- und racefrei.
- [ ] Negativkontrollen falscher Thread und NULL-Encode verletzen die jeweiligen Orakel.
- [ ] PNG/History nach Wiederherstellung visuell prüfen; Budget nach 2092 getrennt messen.

## Weitere belegte Fehlerpfade

SceneRenderer::Draw protokolliert Fehler aus HandTables/HandPlacements/HandDrawArguments,
encodiert aber weiter. Der Rückgabewert von SubmitGPUCommandBufferAndAcquireFence wird
ungeprüft als Fence benutzt; Jitter/History/LinearAt wurden bereits vor Acquire umgestellt.
Auch der Upload-Helfer in SceneRenderer.cpp dereferenziert MapGPUTransferBuffer direkt
in memcpy und prüft Acquire/Submit nicht. Diese Pfade in denselben Frame-/Uploadvertrag
aufnehmen: fehlgeschlagene Vorbereitung nicht als erfolgreiche neue Geometrie publizieren.
- [ ] Upload-/Map-/Tabellen-/Fence-Fehler injizieren; keine NULL-Nutzung, keine
      weitergeschaltete History und kein angeblich erfolgreicher Frame mit alten Daten.
