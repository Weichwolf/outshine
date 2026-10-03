Type: feature
State: open
Architecture: planned
Priority: P3
Parent: 2169
Depends: 2188
Area: simulation, physics, gameplay, audio, script, engine
Tags: sandbox, agents, interaction, llm, persistence

# One physical world supports players, NPCs, scripts and spatial audio

## Ergebnis und Ist
Physikalisch konsistente interaktive Sandbox mit Spielern/LLM-NPCs/JS/HTML-CSS, Spatial
Audio und Save/Load/Replay. Fahrzeuge/Flugzeuge/Pflanzen sind Kraftmodell-Beispiele,
keine abschließende Featureliste. Rigid/Wrench/Prismatic, ActionHostAdapter, UI/Commands,
Audio/Animation bestehen; SimulationState integriert bisher nur Schwerkraft, Saves nur
numerische Traits. Allgemeine Kontakte/Gelenke, vollständige Persistenz und NPC-Steuerung fehlen.

## Besitzer und fehlender Vertrag
2188 liefert native Welt-/Entity-/Produktgrenzen. `physics` besitzt Körper/Kräfte/Solver;
actor/body dorthin migrieren. SimulationState besitzt festen Takt/Zustand/Commands;
Script/UI/LLM-Host Ziele/Events, Render/Audio immutable Posen. Vor Solver-Ausbau vorhandenen
Kern gegen etablierte lokal verfügbare Kerne prüfen; Kontakt/Gelenke/Kosten entscheiden,
daher `planned`. Visueller Meilenstein zuerst; keine zweite Sandbox-/Fahrzeugengine.

## Verfahren und Invarianten
- SI-Masse/Trägheit/Schwerpunkt/Pose; Kraft/Angriffspunkt → Beschleunigung/Drehmoment.
  Broadphase/Shape-Narrowphase und begrenzte Kontakt-/Gelenklösung für Terrain/Körper/Bauwerke;
  Reibung/Restitution, Gelenklimits/Motoren/Reaktionen und schnelle Bewegung konsistent lösen.
- Kollision unabhängig vom Render-LOD; fester Takt mit begrenztem Aufholen. Reifen,
  Aerodynamik/Auftrieb/Wind sind Modelle auf denselben Körpern. Schlaf-/Fernzustand spart
  Arbeit ohne verlorene kausale Zustandsänderungen; Pflanzen reduzieren Freiheitsgrade.
- Beobachtung/Navigation → lokale Steuerung oder JS/UI/LLM → validierter Command → Physik
  → Pose/Spielzustand/Events → Render/Audio. Entity/Tick/Version, Begrenzung/Abbruch verbindlich;
  keine direkte Renderpose-Manipulation außerhalb expliziter Setup-/Editormodi.
- LLM liefert Ziele/Dialog/Entscheidungen asynchron; lokale Steuerung bleibt ausführbar.
  Fristen/Queues, stale Antworten verwerfen; Modellkonfiguration im Host, kein Warten im Tick.
  JS/HTML/CSS behalten dokumentierte Teilmenge; NPC-Navigation nutzt native logische Netze.
- Save/Load transaktional/versioniert: Entity-/Asset-IDs, Quellen-/Producer-Versionen,
  Weltänderungen/Physik und notwendiger Script-/NPC-Zustand. Fehler verändern keinen gültigen
  Stand; Replay spielt aufgezeichnete Host/LLM-Events ab statt erneut das Modell zu fragen.
  Szenario, Netzwerkcache und Spielstand getrennt; Save ist kein Generatorcache.
- Kontakte/Schritte/Antrieb/Material erzeugen begrenzte Klangereignisse. Gefiltertes Noise/
  Resonanzen synthetisieren Wind/Regen/Wasser, Kräfte Motoren/Bewegung; stabile Seeds/
  samplegenaue Zeit, keine PCM-Arbeit für stumme Quellen oder Reset beim Sichtwechsel.
- AudioScene/AudioOcclusion teilt Listener-/Entity-/Kontaktzustand: Entfernung/Richtung,
  Doppler/Verdeckung, begrenzte Stimmen/Busse/Headroom/Limiter. Dialogblöcke asynchron vom
  Host; Audio wartet weder auf Netzwerk/LLM noch Geometrie. Animation nutzt dieselben Posen.

## Nächste Lieferung und Abnahme
Nach visueller Basis: allgemeiner Körper-/Weltkontakt und Gelenkantrieb, dann lokal/JS
und über aufgezeichnete LLM-Events steuerbarer NPC im Place; Audio und atomarer Save/Replay
anschließen. Kontakte/Massen/Kräfte stimmen mit Straßen/Brücken/Terrain überein. Ausgabe
25/30/60 fps ändert keine Simulation. Gemeinsames Bild-/Speicherbudget bleibt verbindlich;
Hockenheim ist spätere Integration, keine Voraussetzung.
