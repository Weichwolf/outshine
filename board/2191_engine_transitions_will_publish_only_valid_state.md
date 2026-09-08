Type: bug
State: active
Area: engine, include, scenario
Tags: architecture, state, errors
Parent: 2188
Depends:

# Engine transitions will publish only valid state

## Befund und Entscheidung

Engine.cpp setzt Picture.Targeted vor erfolgreichem Device.DrawsInto.
Declaring.cpp handleEvent gibt für irrelevante Events unexpected(S_->Error) zurück:
nicht behandelt und Fehler sind vermischt, Error kann leer oder veraltet sein.
EngineHeld.h verteilt Phasen über Taken, Targeted, FrameOpen, Carrying, Mixing usw.
Unabhängige Eigenschaften bleiben erlaubt; Phasen mit verbotenen Kombinationen
benötigen dagegen explizite Zustandsautomaten. Keine pauschale Boolean-Ersetzung.

**Benchmark**: Filament trennt Engine-Ressourcen und Frame-Aufrufverträge.
https://github.com/google/filament/blob/main/filament/include/filament/Renderer.h
Unreal/RAGE sind kein Beleg für atomare outshine-Declare-Semantik; diese folgt aus
unserem Szenario-/Sandbox-Vertrag und wird hier ausdrücklich festgelegt.

Konfiguration validieren, Kandidaten aufbauen, erst dann veröffentlichen. Fehlgeschlagene
änderbare Konfiguration erhält den letzten gültigen Zustand; irreversibler Devicefehler
wechselt ausdrücklich in Failed. Keine Erfolgsvortäuschung durch alte Framebilder.
Fehler als strukturierter Code mit Kontext; SDL-Text am Fehlerort übernehmen.
Eventergebnis unterscheidet behandelt, ignoriert und fehlgeschlagen.
Alle Engine-Mutatoren inventarisieren, einschließlich offers/setRoots/setSurfaces,
declare/assemble und save/restore. Unsupported-Deklarationen nach 2131 zurückweisen.
2185 besitzt Feature-Ressourcen, 2189 Handle-Identität, 2151 Persistenzschema.

## Aktiver Schritt: Target-Konfiguration

SceneRenderer::DrawsInto gibt bislang das alte Fenster/Offscreen-Target vor Claim,
Swapchain-Konfiguration und Texturerzeugung frei. Kandidaten zuerst vorbereiten;
Fehler kopieren, Kandidaten freigeben, alte Ressourcen/Dimensionen erhalten.
Offscreen-Textur über vorhandenes OwnedTexture übernehmen. Window-Claim bei späterem
Fehler zurückgeben. SDL_GetWindowSizeInPixels prüfen; Targeted/Frame erst nach Erfolg
publizieren. Wechsel während offenem Frame verweigern. Gültige Fenster bleiben geliehen.
https://wiki.libsdl.org/SDL3/SDL_ClaimWindowForGPUDevice
https://wiki.libsdl.org/SDL3/SDL_SetGPUSwapchainParameters
https://wiki.libsdl.org/SDL3/SDL_CreateGPUTexture

Tests: Erstkonfiguration verweigert, ungültige Extents, Fenster bereits von anderer
Engine beansprucht, Wechsel bei offenem Frame, alter Owner weiter nutzbar und Claim
nach Freigabe wieder möglich. Fehler an der SDL-Grenze gezielt injizieren; Kontrolle
mit vorzeitiger Veröffentlichung muss scheitern. Keine erwartete Änderung gültiger
Pixel; Fenster-/Offscreen-Wechsel und verbleibende Resize-/Pipeline-Verträge getrennt
prüfen. Dieser Schritt schließt die übrigen Engine-Mutatoren nicht ab.

## Abnahme

- [ ] Öffentliche Übergangstabelle nennt erlaubte Reihenfolge und Fehlergarantien.
- [ ] Fehler an jeder Build-/Validate-/Publish-Grenze injizieren; gültiges altes
      Szenario bleibt nutzbar oder ausdrücklich Failed, nie halb veröffentlicht.
- [ ] Irrelevantes Event ist Ignored; echter Fehler trägt passenden Code/Kontext.
- [ ] Wiederholtes Declare, Targetwechsel und Featurewechsel ohne Ressourcenwachstum.
- [ ] Negativkontrolle publiziert vor Validierung; Zustandserhalt-Oracle wird rot.
