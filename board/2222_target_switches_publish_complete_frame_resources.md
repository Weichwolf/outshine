Type: defect
State: active
Parent: 2191
Depends:
Area: render, engine, test
Tags: ownership, state, gpu

# Target switches publish complete frame resources

## Problem

`SceneRenderer::DrawsInto` ersetzt Offscreen- oder Fenstertarget und Extent, aber
behält die bisherigen größen- und formatgebundenen Attachments, Readbacks und
Pipelinebindungen. Ein gültiger Targetwechsel kann damit einen gemischten Zustand
veröffentlichen. Der bestehende Test prüft Claim- und einzelne Texturfehler, nicht
das erste Pixel eines vollständig neu aufgebauten Frames.

## Entscheidung

Ein verschiebbarer interner `FrameResources` besitzt alle plan-/zielabhängigen
Texturen, Sampler, Pyramidenpuffer, temporalen Ziele und Stage-Pipelinebindungen.
Statische Geometrie, Materialassets und ihre Residency bleiben beim Renderer.
Formatgebundene Subject-Pipelines werden als Kandidat gebaut und erst mit den
neuen Attachments veröffentlicht; kein Stage darf auf die abgelösten Ziele zeigen.

`SubjectDraw` und `OverlayDraw` dürfen dauerhafte Inhalte (Residenz bzw. Atlas/Quads)
nicht mit Pipelines vermischen. Subject- und Glass-Pipelines einschließlich Transmission,
Velocityvertrag und Ground Lit-/Depth-Pipelines sind nun verschiebbare `FrameResources`.
Mesh-, Material-, Instanz- und Ground-Residenz bleiben bei ihren Zeichnern. GroundStorage
bleibt dauerhaft; alle Frame-Texturen, Sampler, Pyramide, temporalen Ziele und Stages wandern
gemeinsam.

`FrameResources` besitzt inzwischen Extent, Zieloberfläche, Attachments, Sampler, Pyramiden-
Readback, temporale Ziele, GPU-Handles, Subject-/Glass-Bindungen und alle formatgebundenen
Stage-Objekte; seine Beweglichkeit ist statisch gesichert. `Init` baut und konfiguriert den
vollständigen Plan gegen einen lokalen Kandidaten. Erst nach Erfolg bewegen sich Frame und Plan;
danach binden die langlebigen Zeichner auf die neue Binding-Adresse. Eine abgelehnte Pipeline
oder Textur erhält aktive Deklaration und lesbare Pixel. Der Test iteriert jede tatsächlich
ausgeführte Erzeugung von Textur, Sampler, Buffer und Grafikpipeline des Target-Kandidaten. Jeder
Fehler erhält Extent und Pixel; der unmittelbare Retry veröffentlicht den Wechsel. Der Idle-Fehler
nach vollständigem Kandidatenbau erhält dieselben Pixel (663 Checks). Fehlerhafte
Stage-Konfigurationen ohne SDL-Ressourcenerzeugung bleiben offen.

`ConfigureOverlay` erzeugt keinen Default-Atlas mehr im veröffentlichten `WorldContent`.
`OverlayDraw::Replace` erstellt ihn nur als lokalen Kandidaten zusammen mit einer tatsächlichen
Overlay-Aktualisierung; die Upload-Fehlersuite prüft Defaultbild, Fehlererhalt und Retry.

Offen bleiben Fehler nach vollständiger SDL-Ressourcenerzeugung, aber vor der ersten GPU-Arbeit:
jede `Configure`-Implementierung braucht eine gezielte Injektion. Die Prüfung erhält alte Pixel,
Target-Claim, Extent und Renderbarkeit und wiederholt den Wechsel erfolgreich. Ein normaler
Ressourcenfehler zerstört nur den Kandidaten; nur ein irreversibler Devicefehler wechselt in
einen expliziten Failed-Zustand.

## Abnahme

- [x] Offscreen 32x32 → 48x32 liefert nach dem Wechsel 48x32 Oberflächenpixel
      (56 Checks). Dies beweist noch keine dimensionierten internen Attachments.
- [x] Offscreen 32x32 → 48x32 baut sämtliche Attachments, Readbacks und Stages
      kandidatenseitig neu und rendert sie nach Veröffentlichung vollständig (56 Checks).
- [x] Fensterformat- und Extentwechsel ersetzen dieselben Ressourcen gemeinsam;
      das neue Fenster öffnet und präsentiert anschließend einen vollständigen Frame (59 Checks).
- [x] Jede tatsächlich gebaute Textur, jeder Sampler, Buffer und jede Grafikpipeline des
      Target-Kandidaten kann einzeln scheitern; Pixel, Extent und Retry bleiben gültig (663 Checks).
- [x] Der Kandidat entsteht vollständig vor dem Warten auf die letzte alte GPU-Nutzung; ein
      fehlgeschlagenes Warten erhält den alten Frame (663 Checks).
- [ ] Injektionen für Claim und jede Stage-Konfiguration ohne SDL-Ressourcenerzeugung erhalten
      alte Pixel, Claim und Renderbarkeit.
- [x] Erfolgreicher Wechsel gibt den alten Claim nach Veröffentlichung frei; wiederholte
      48×32↔32×32-Wechsel erzeugen und geben dieselbe Texturmenge frei (62 Checks).
- [ ] Negativkontrolle veröffentlicht Target vor Kandidatabschluss und verletzt
      mindestens Pixel-, Extent- und Fehlererhalt-Oracle.

Keine Warm-up-Frames, Deferred-Reset oder nachträgliches `Init` als Ersatz für die
atomare Veröffentlichung. Bildwirksamkeit anhand des Client-Pfads prüfen.
