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
make suite SUITE=outshine/src/render/device prüft tatsächliche GPU-Commands, wiederholte Acquire-
und Submit-Fehler, genau einmaligen Verbrauch, kalten/warmen Retry, geänderte Atmosphäre,
endliche temporale Pixel, LUT-Werte und Live-Fehlerweitergabe. Die Suite aktiviert
GPU-Validierung. Ihre Device-Ownership ist ausdrücklich auf diese Beweistests beschränkt;
Die Validierung fand im Gelände-Vertexlayout instance_step_rate=1; SDL reserviert
dieses Feld und verlangt 0. Instancing bleibt über input_rate=INSTANCE aktiv; der
statische Layoutaufbau sichert die reservierten Felder per static_assert. Referenz:
https://wiki.libsdl.org/SDL3/SDL_GPUVertexBufferDescription
Clients und fachliche Tests erhalten keine SDL_GPU-Ownership. Die alte Claim-Ausnahme
für die nicht mehr vorhandene outshine/shader-Suite ist ersetzt.
- [x] Reale Offscreen-Acquire-/Submit-Abbrüche liefern Fehler und publizieren weder
      ungeschriebene LUTs noch weitergeschaltete History; der nächste Frame erholt sich.
- [x] Fusionierter Temporal-Pass liest seinen deklarierten Eingang statt seines Renderziels.
- [x] Schattenatlas nutzt denselben Submission-Vertrag: vorbereitete Schlüssel gelten
      erst nach erfolgreicher Einreichung; abgelehnte Frames bleiben wiederholbar.
      Readback verweigert unveröffentlichte Atlanten ohne Änderung des Zielvektors.
      Device-Suite: je 136 Checks ohne Fehler, normal und GPU-validiert; kalter Retry,
      Castermaskenwechsel, leeres Clear und geänderte Lichtrichtung geprüft.
      Herkunftsabhängige Casterauswahl, Instanzen und Terrain bleiben in 2128/2150 offen.
- [ ] Read-/Write-/Vorframe-Zugriffe im Plan ausdrücklich beschreiben und gegen konkrete
      Bindings prüfen; Shader-Katalog 2152 ersetzt keinen Ressourcen-Lebenszyklus.
- [ ] Fence-Wait-/Readback-Fehler prüfen und von bereits eingereichter Arbeit trennen;
      Screenshot darf die schreibgeschützte Swapchain nicht als Quelle lesen.
- [x] GroundStorage übernimmt Klassen/Palette gemeinsam als unveränderliche GPU-Version.
      Quellspans und uint32-Bytegrenzen werden getrennt von Mindestkapazität geprüft;
      leere/kurze Inputs initialisieren den Rest mit Null. Ein Upload-Submit veröffentlicht
      beide neuen Buffer; Fehler lassen alte Handles/Inhalte unverändert. Kein GPU-Wait
      im Uploadpfad; nachfolgende Commands sehen die eingereichte Version gemäß SDL.
      Map/Acquire/Submit-Fehler gelangen bis Live, auch initiale Fallback-Uploads sind Pflicht.
      Device-Test prüft GPU-Readback, gesperrte Seiten hinter Ein-Element-Quellen,
      wiederholte Fehler, größere Ersatzdaten und Retry; normal und GPU-validiert.
      Frische Ressourcen sichern Fehleratomarität beim Weltumbau. Upload-Ringe und
      budgetierte inkrementelle Streaming-Updates bleiben in 2149/2124.
- [ ] Weitere Upload-Helfer (u.a. GroundLattice) prüfen Acquire/Submit noch nicht.
      SubjectDraw::HandTables löscht TablesStale_ vor Retable-Erfolg; Retry erhalten.
      2149 besitzt persistente Upload-Ringe, dieses WI Fehler und Veröffentlichungszustand.
- [ ] Pass- und Ressourcen-Vorbedingungen nach SDL prüfen; Programmierfehler von echten
      Plattformfehlern trennen. Fehlgeschlagene Vorbereitung nie als neue Geometrie melden.
- [ ] Minimieren/Wiederherstellen, Resize, Fenster-Owner/CommandBuffer-Thread und Shutdown
      mit ausstehenden Uploads/Readbacks nachweisen; falscher Thread als Negativkontrolle.
- [ ] Resize-Screenshot prüft noch Deklarationsmaße; PNG/History und Frame-Pacing prüfen. IO-/Worker-
      Warten trennen, Laufzeit-/Speicherbudget nach 2092 messen.
## Öffentliche Frame-Vorbedingungen
BeginFrame verweigert Verschachtelung über dieselbe oder kopierte Facade vor Setup;
ein Ende verbraucht den Scope. Extent-Prüfung erfolgt vor Szene-/GPU-Vorbereitung.
Owner-/Target-Tests prüfen Scope-Erhalt und Größenfehler vor verzögertem Asset-Fehler.
## Frame-Abschluss und Fenster-Readback
SDL_gpu.h (lokal /opt/homebrew/include/SDL3): Swapchain-Acquire führt bei Submit
bereits zur Präsentation; die Swapchain-Textur ist ausschließlich beschreibbar.
Live::Present zeichnete nochmals, RenderFrame las für Screenshots aus der Swapchain.
Beide Pfade ersetzt; FrameTex enthält bereits Tonemapping und Overlay.
Implementiert: Scope Closed/Open/DrawSucceeded statt FrameOpen-Bool. Erfolgreiche
Draw-Anfragen (auch ein minimiert übersprungener Frame) erfüllen den Scope; endFrame
schließt ihn ohne weiteren Draw. Ohne erfolgreichen Draw bleibt der bisherige
Fenster-Draw beim Ende erhalten. Direkte Render-/Readback-Anfragen bleiben explizite
Draws. Fehlgeschlagener Readback nach erfolgreichem Draw löst keinen Ersatz-Draw aus.
Readback nur aus deklarierten Farbzielen: FrameTex/Offscreen, nie Swapchain; SDL-Ursache weiterreichen.
WantsPixels/Taken-Zwischenzustand entfällt. Acquire-/Map-/Wait-Fehler verweigern den
Readback ohne Überschreiben der Ausgabe; vorbereitete Kopien sauber abbrechen.
Öffentliche API plus abgefangene echte SDL-Calls prüfen Submit/Swapchain-Anzahl,
Fenster/Offscreen, leeren Scope, Kopien, minimierten Skip, Draw-/Readback-Fehler und
Retry und Planwechsel prüfen; Outputs sind laut API zusätzliche Ressourcen. Fenster-/Offscreen-
Pixel sind identisch, PNGs visuell geprüft: dasselbe farbige Dreieck vor Schwarz.
Minimierte Readbacks/Frische des letzten Bildes und explizite GPU-Outcomes bleiben
separat zu präzisieren; ein erfolgreicher CPU-Aufruf beweist keine GPU-Fertigstellung.
Framing berechnet Frustumdiagnostik auch bei Audits=false: Arbeit/Timing separat korrigieren.
## Wiederholbare Tabellen-Uploads
HandTables hält den Stale-Marker bis Erfolg und verwirft bei Fehler nutzbare Jobs/Args.
Upload- und Grow-Kopien prüfen Acquire/CopyPass/Submit; Grow erhält alten Buffer bei Fehler.
FailedTablesRemainRetryable belegt Map/Acquire/Pass/Submit, wiederholte Fehler, Retry
und GPU-Inhalte normal/SDL-validiert; Grow zusätzlich Handle und Kapazität nach Fehler.
Retable trennt Subject-/Piece-Aufbau, Materialzuordnung und Upload; Instancing-Test grün.
Cross prüft Stream-IDs, Zielenden und 16-Byte-Summen vor Mutation in uint64; Grow ohne
uint32-Überlauf. GPU-Test: Release vor ungültigem Eintrag/Summenüberlauf bleibt aus,
keine Allokation; normal/validiert grün. Referenz: SDL Uint32-Größen.
Verzögerte Uploads werden erst nach erfolgreichem Submit quittiert; Aufnahmeabbruch und
Retry mit GPU-Inhalt belegt; Frame-/Fensterregressionen normal/SDL-validiert grün.
Pending-Batch wird vor sofortigem Cross oder Buffer-Ersatz/Grow geordnet eingereicht;
Fehler erhalten Batch/Buffer. GPU-Test belegt Mixed-Reihenfolge, Grow mit Pending und Retry.
Room entfällt; Residency trennt Preserve/Discard. Frame-/Instancing-Regressionen grün.
CopyPass vor Swapchain-Acquire geprüft; Retry/Frame/Fenster in sechs Profilen grün.
Nächster Schritt: Buffer-Ersatz lokal anlegen; Fehler erhält Handle/Kapazität/GPU-Daten.
Allokationsfehler injizieren. Mehrbuffer-Atomarität und Upload-Budgets bleiben offen.
