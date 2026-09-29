Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2188
Depends:
Area: client, render
Tags: pacing, responsiveness, performance

# Der Client lässt CPU-Zeit für andere Arbeit frei

## Ergebnis

Der Client erzeugt bei freier CPU keine ungebremste Frame-Schleife. Normale Darstellung
und Place-Aufnahmen takten mit höchstens 60 Frames pro Sekunde; wartende Threads schlafen.
Simulation, Qualitätsanforderung und verfügbare Weltinhalte bleiben unverändert.

## Ausgangsbefund und Entscheidung

PlaceCamera::MeasureFrames ruft advance/render unmittelbar hintereinander auf.
SceneRenderer::ClaimWindow bevorzugt Mailbox und Immediate vor VSync. Swapchain-Warten
allein begrenzt deshalb weder diesen Offscreen-Pfad noch zuverlässig die CPU-Arbeit.

## Umsetzung und Besitzer

Client besitzt FramePacer und dessen monotone Frist. SDL_DelayNS blockiert bis zur
nächsten Frist; kein Spin-Wait und kein pauschales Sleep nach jedem Frame. Überzogene
Fristen erzeugen keine Aufholschleife. Die nächste Arbeitsrunde beginnt frühestens
nach einer Frameperiode; langsame Arbeit erhält keine zusätzliche feste Pause.
PlaceCamera und zeitliche ScenarioCapture-Schleifen verwenden denselben Vertrag.
Warten liegt außerhalb gemessener advance/render-Zeiten; tatsächliche verstrichene Zeit
bleibt separat sichtbar. Explizite ungedrosselte Messung muss als solche erkennbar sein.
Renderer bevorzugt VSync für Fenster; SDL begrenzt eingereichte Frames. Die Engine
schläft nicht pauschal im advance-Hot-Path. Worker-Budgets bleiben getrennte Verträge.

## Offene Wirkung im Client

Die geöffneten Darmstadt-, Husum- und Koerbersee-Bilder zeigen keine sichtbare Verbesserung;
Feldkirch erreicht wieder das erhaltene frühere Bild, behält aber die falsche Geländewand.
Malcesine verändert einzelne Terrain-/Gebäudekanten ohne belegte Verbesserung. Deshalb
bleibt die Bildreproduzierbarkeit unter geänderter Frame-Taktung offen; keine neue Baseline.
Graz und Wien erreichen weiterhin keine vollständige Aufnahme innerhalb der bisherigen
Grenzen. Pacing allein schließt ihre Ladefehler nicht. Ein CPU-Vorher/Nachher-Vergleich und
Fenster-/Kamerafahrt-Abnahme fehlen; vorhandene Place-Ergebnisse sind keine Zielgeräte-Freigabe.

## Abnahme

Monotone Fristen, lange Frames und fehlende Aufholbursts mit kontrollierter Zeit prüfen.
Client-Aufnahme unter normalen Grenzen: Arbeitskosten getrennt von Wartezeit berichten;
CPU-Last vorher/nachher messen und gleiche vollständige Bildinhalte persönlich prüfen.
make format; fokussierte Client-/Renderer-Suites; make lint; alle Places rendern.
Kein höheres Frame-/Timeout-Limit und kein Herabsetzen von Refined auf Playable.
