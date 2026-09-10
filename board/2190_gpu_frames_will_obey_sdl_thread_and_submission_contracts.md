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

Konventionssuite zuletzt 28/29: nur bekannter Mipmap-Wiederholungsfehler 2179,
423 Kanäle je Wiederholung, maximal 0,00268555, identische Tiefe. Kamera-PNGs geöffnet
und bytegleich zur gesicherten Referenz. Keine neue fotorealistische Place-Abnahme.

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
- [ ] PNG/History nach Wiederherstellung prüfen; Frame-Pacing und unzulässiges IO-/Worker-
      Warten trennen, Laufzeit-/Speicherbudget nach 2092 messen.
