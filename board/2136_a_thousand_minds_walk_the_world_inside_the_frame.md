Type: feature
State: open
Architecture: planned
Priority: P3
Parent: 2169
Depends:
Area: simulation, physics, gameplay, audio, script
Tags: sandbox, interaction, persistence

# One physical world supports players, NPCs, scripts, audio and persistence

## Ergebnis und Ist
Physikalische Open-World-Sandbox mit LLM-NPCs, JS/HTML-CSS, Spatial Audio und Save/Load/Replay.
Rigid/Wrench/Prismatic, ActionHostAdapter, UI/Commands, Audio/Animation bestehen. Aktuell
nur einfache Schwerkraftintegration und numerische Traits-Saves; Kontakte/Gelenke, vollständige
Persistenz und NPC-Steuerung fehlen. Fahrzeuge/Flugzeuge/Pflanzen sind Beispiele, keine Grenze.

## Besitzer und Reihenfolge
Physics besitzt Körper/Kräfte/Solver, SimulationState festen Takt/Zustand/Commands;
Host/Script/UI/LLM Ziele/Events, Render/Audio immutable Posen. Native Entities/Assets bestehen;
benötigte API-Erweiterungen mit 2188 integrieren, kein pauschaler Architektur-Blocker.
Nach visueller Basis zuerst allgemeine Weltkontakte/Gelenkantrieb, dann steuerbarer NPC
und Ton/Persistenz. Vor Solver-Ausbau vorhandenen Kern gegen etablierte lokal verfügbare
Kerne prüfen; dieser Entwurf bleibt `planned`. Keine separate Fahrzeug-/Sandboxengine.

## Verfahren
- SI-Masse/Trägheit/Schwerpunkt/Pose; Kraft/Angriffspunkt → Beschleunigung/Drehmoment.
  Broadphase/Shape-Narrowphase, begrenzte Kontakt-/Gelenklösung für Terrain/Körper/Bauwerke,
  Reibung/Restitution, Limits/Motoren und schnelle Bewegung. Kollision unabhängig vom Render-LOD.
- Fester Takt mit begrenztem Aufholen. Reifen/Aerodynamik/Auftrieb/Wind auf denselben Körpern;
  Schlaf-/Fernzustand spart Arbeit ohne verlorene Weltwirkung. Vegetation reduziert Freiheitsgrade.
- Beobachtung/Navigation → lokale Steuerung oder JS/UI/LLM → validierter Command → Physik
  → Zustand/Pose/Events → Render/Audio. Entity/Tick/Version und begrenzte Queues, kein direkter
  Renderpose-Ersatz außer explizitem Setup/Editieren. Dokumentierte JS/HTML/CSS-Teilmenge halten.
- LLM liefert Ziele/Dialog asynchron; lokale Steuerung bleibt ausführbar. Fristen/Abbruch und
  stale Antworten behandeln, Modellkonfiguration im Host; kein Warten auf Netzwerk im Tick.
- Save/Load transaktional/versioniert: IDs, logischer Zustand, Weltänderungen/Physik und nötiger
  Script-/NPC-Zustand. Quellen-/Producer-Versionen erhalten; kein Generatorcache im Spielstand.
  Replay zeichnet Host-/LLM-Events auf, statt Antworten erneut zu erzeugen.
- Kontakte/Schritte/Antrieb/Material erzeugen Klangereignisse; Noise/Resonanzen synthetisieren
  Wind/Regen/Wasser/Motoren. Stabile Seeds/samplegenaue Zeit, keine Arbeit für stumme Quellen.
  AudioScene/AudioOcclusion teilt Posen/Kontakte: Richtung/Entfernung, Doppler/Verdeckung,
  begrenzte Stimmen/Busse/Headroom/Limiter. Dialog asynchron, Audio wartet auf keine Geometrie.

## Abnahme
Allgemeiner Körper-/Weltkontakt und Gelenkantrieb stimmen auf echten Straßen/Brücken/Terrain.
Ein NPC ist lokal, über JS und aufgezeichnete LLM-Events steuerbar; Ton und atomarer Save/Replay
erhalten den Zustand. 25/30/60 fps ändern keine Simulation. Hockenheim ist spätere Integration.
