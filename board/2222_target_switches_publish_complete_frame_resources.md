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

`SubjectDraw` und `OverlayDraw` vermischen heute dauerhafte Inhalte (Residenz bzw.
Atlas/Quads) mit Pipelines. Vor `FrameResources` beide in dauerhaften Inhalt und
verschiebbare, formatgebundene Bindungen schneiden. GroundStorage bleibt dauerhaft;
alle Frame-Texturen, Sampler, Pyramide, temporalen Ziele und Stages wandern gemeinsam.

`SubjectDraw` baut seine Subject-Pipelines bereits als lokalen Kandidaten; Mesh-, Material-
und Instanzresidenz bleiben stehen. `GroundLattice` trennt seine Lit-/Depth-Bindungen nun von
Seiten, Grid und Instanzen. Beide Bindungssätze werden später gemeinsam mit den übrigen
formatgebundenen Frame-Ressourcen getauscht.

`DrawsInto` validiert Extent und baut/claimt den Kandidaten mit aktuellem Device,
Plan und Zielformat vollständig. Erst danach wartet es die letzte alte Nutzung ab,
tauscht Ressourcen, Target und Dimension gemeinsam und gibt alte Fensterclaims
frei. Fehler geben ihre SDL-Ursache zurück, zerstören nur Kandidaten und erhalten
den alten Frame les- und renderbar. Ein irreversibler Devicefehler wechselt
ausdrücklich in Failed; ein normales Ressourcenproblem nicht.

## Abnahme

- [ ] Offscreen 32x32 → 48x32 rendert nach dem Wechsel vollständige 48x32-Pixel
      mit frischem Renderer-Referenzbild und ungültiger alter Readback-Generation.
- [ ] Fensterformat- und Extentwechsel ersetzen dieselben Ressourcen gemeinsam.
- [ ] Injektionen für Claim, jede Attachment-/Pyramid-/Pipeline-Erzeugung und
      Stage-Konfiguration erhalten alte Pixel, Claim und Renderbarkeit.
- [ ] Erfolgreicher Wechsel gibt den alten Claim nach Veröffentlichung frei; kein
      Ressourcenwachstum bei Wiederholung.
- [ ] Negativkontrolle veröffentlicht Target vor Kandidatabschluss und verletzt
      mindestens Pixel-, Extent- und Fehlererhalt-Oracle.

Keine Warm-up-Frames, Deferred-Reset oder nachträgliches `Init` als Ersatz für die
atomare Veröffentlichung. Bildwirksamkeit anhand des Client-Pfads prüfen.
