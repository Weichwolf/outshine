Type: bug
State: active
Area: render, host
Tags: architecture, gpu, lifecycle
Parent: 2188
Depends:

# GPU frames will obey SDL thread and submission contracts

## Vertrag und vorhandene Umsetzung

Stage bleibt logische Renderarbeit; der Compiler bildet daraus GPU-Pässe.
Aufzeichnung, erfolgreiche Submission und abgeschlossene GPU-Arbeit sind unterschiedliche
Zustände. SDL3 ist maßgeblich, keine vermutete Unreal-/RAGE-RHI:
https://wiki.libsdl.org/SDL3/SDL_AcquireGPUCommandBuffer
https://wiki.libsdl.org/SDL3/SDL_WaitAndAcquireGPUSwapchainTexture
https://wiki.libsdl.org/SDL3/SDL_SubmitGPUCommandBufferAndAcquireFence
https://wiki.libsdl.org/SDL3/SDL_CancelGPUCommandBuffer
CommandBuffer bleibt auf seinem Acquire-Thread; Swapchain auf dem Fenster-Owner.
Ein Submit verbraucht den Commandbuffer auch im Fehlerpfad. Nach Erwerb einer
Swapchain-Textur ist Cancel verboten; NULL-Target ohne SDL-Fehler heißt überspringen.
Device überlebt Ressourcen; SDL verzögert Release selbst. Keine zusätzliche
Retirement-Queue ohne tatsächlich außerhalb SDL liegende Lebensdauern.

PrepareFrame prüft Renderer/Kamera und Tabellen-/Placement-/DrawArgument-Vorbereitung
vor Acquire. RenderFrame liefert expected; Live::Draw reicht Fehler weiter.
Acquire-Fehler endet vor Encode; minimierter NULL-Swapchain-Frame wird verworfen.
Der Swapchain-Handle wird nach Submit nicht gespeichert. Frame-Transfer besitzt RAII.

Die vier atmosphärischen LUT-Stages benutzen Dirty/Recorded/Submitted. Ein begrenztes,
frame-lokales Journal registriert tatsächlich aufgezeichnete Updates. Nur erfolgreicher
Submit veröffentlicht sie; Abbruch macht sie wieder aufzeichnungsbedürftig. Bereits
gültige, unveränderte LUTs bleiben gültig. Jitter/History/LinearAt werden bei Submit-
Fehler zurückgesetzt; Vorframe-Matrizen und Geometrie erst nach Erfolg fortgeschrieben.

Fusioniertes TemporalResolve/Tonemap liest gemäß Stage-Deklaration SceneAerial;
ohne TemporalResolve bleibt der Eingang SceneLinear. Configure und Encode verwenden
dieselbe Auswahl. Vorher wurde SceneLinear gleichzeitig gelesen und beschrieben.
Der echte Device-Test lieferte dadurch 3072/4096 nicht-endliche Kanäle. Nach Korrektur
sind alle Kanäle endlich und drei erfolgreiche Framefolgen nach Abbrüchen pixelgleich
zur ununterbrochenen Ausführung; Irradiance ebenfalls gleich. Kein GLSL-Fix oder
geänderter Toleranzwert war dafür nötig. Sonnenpol-Normalisierung bleibt separat in 2167.

## Nachweis und verbleibende Abnahme

make suite SUITE=outshine/device prüft tatsächliche GPU-Commands, wiederholte Acquire-
und Submit-Fehler, genau einmaligen Verbrauch, kalten/warmen Retry, geänderte Atmosphäre,
endliche temporale Pixel, LUT-Werte und Live-Fehlerweitergabe. Die Suite aktiviert
GPU-Validierung. Ihre Device-Ownership ist ausdrücklich auf diese Beweistests beschränkt;
Die Validierung fand im Gelände-Vertexlayout instance_step_rate=1; SDL reserviert
dieses Feld und verlangt 0. Instancing bleibt über input_rate=INSTANCE aktiv; der
statische Layoutaufbau sichert die reservierten Felder per static_assert. Referenz:
https://wiki.libsdl.org/SDL3/SDL_GPUVertexBufferDescription
Clients und fachliche Tests erhalten keine SDL_GPU-Ownership. Die alte Claim-Ausnahme
für die nicht mehr vorhandene outshine/shader-Suite ist ersetzt.

Konventionssuite zuletzt 28/29: nur bekannter Mipmap-Wiederholungsfehler 2179,
423 Kanäle je Wiederholung, maximal 0,00268555, identische Tiefe. Kamera-PNGs geöffnet
und bytegleich zur gesicherten Referenz. Keine neue fotorealistische Place-Abnahme.

- [x] Reale Offscreen-Acquire-/Submit-Abbrüche liefern Fehler und publizieren weder
      ungeschriebene LUTs noch weitergeschaltete History; der nächste Frame erholt sich.
- [x] Fusionierter Temporal-Pass liest seinen deklarierten Eingang statt seines Renderziels.
- [ ] LightVisibilityStage veröffentlicht Held_ noch im Encode: Schattenatlas und
      vorbereitete CastAt/CastFrom-Metadaten in denselben Submission-Vertrag aufnehmen.
- [ ] Read-/Write-/Vorframe-Zugriffe im Plan ausdrücklich beschreiben und gegen konkrete
      Bindings prüfen; Shader-Katalog 2152 ersetzt keinen Ressourcen-Lebenszyklus.
- [ ] Fence-Wait-/Readback-Fehler prüfen und von bereits eingereichter Arbeit trennen;
      Screenshot darf die schreibgeschützte Swapchain nicht als Quelle lesen.
- [ ] Upload-Helfer prüft Map/Acquire/Submit noch nicht. SetGroundClasses vermischt
      Mindestkapazität mit Quelllänge: bounded Quellspan und Größenüberlauf prüfen.
      SubjectDraw::HandTables löscht TablesStale_ vor Retable-Erfolg; Retry erhalten.
      2149 besitzt persistente Upload-Ringe, dieses WI Fehler und Veröffentlichungszustand.
- [ ] Pass- und Ressourcen-Vorbedingungen nach SDL prüfen; Programmierfehler von echten
      Plattformfehlern trennen. Fehlgeschlagene Vorbereitung nie als neue Geometrie melden.
- [ ] Minimieren/Wiederherstellen, Resize, Fenster-Owner/CommandBuffer-Thread und Shutdown
      mit ausstehenden Uploads/Readbacks nachweisen; falscher Thread als Negativkontrolle.
- [ ] PNG/History nach Wiederherstellung prüfen; Frame-Pacing und unzulässiges IO-/Worker-
      Warten trennen, Laufzeit-/Speicherbudget nach 2092 messen.
