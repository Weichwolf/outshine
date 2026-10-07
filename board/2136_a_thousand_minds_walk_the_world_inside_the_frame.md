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
und Ton/Persistenz. Bullet liefert Broadphase, Kontakte und Körper-/Gelenksolver; Eigen dient
linearen Berechnungen außerhalb der CPU/GPU-ABI. Integration bleibt `planned` und folgt dem
visuellen Meilenstein. Keine separate Fahrzeug-/Sandboxengine und kein pauschaler Vec/Mat-Umbau.

## Verfahren
- SI-Masse/Trägheit/Schwerpunkt/Pose; Kraft/Angriffspunkt → Beschleunigung/Drehmoment.
  Broadphase/Shape-Narrowphase, begrenzte Kontakt-/Gelenklösung für Terrain/Körper/Bauwerke,
  Reibung/Restitution, Limits/Motoren und schnelle Bewegung. Kollision unabhängig vom Render-LOD.
- Fester Takt mit begrenztem Aufholen. Reifen/Aerodynamik/Auftrieb/Wind auf denselben Körpern;
  Schlaf-/Fernzustand spart Arbeit ohne verlorene Weltwirkung. Vegetation reduziert Freiheitsgrade.
  Physics kapselt Bullet; Engine besitzt Tick, IDs, Commands, Speichergrenzen und Posenexport.
  Keine Library-Typen in Welt-/Saveformaten. Determinismus innerhalb eines festen Builds prüfen;
  plattformübergreifend bitidentisches Replay ist durch die Bibliothek allein nicht belegt.
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
- Grobe Umgebung am Hörer teilen; begrenzte wichtige Stimmen, Richtungs-/Hallbusse und
  virtualisierte Ferngeräusche statt Quelle-Quelle-Paaren. Native Kontaktabfragen aus 2336 nutzen.
  Cubemap-Transfer als Experiment mit Frequenzdämpfung, Laufzeit und Ankunftsrichtung; Distanz
  und Mehrwege erhalten. Ein Richtungswert allein beschreibt weder Wandnähe noch mehrere Echos.
  Feste Strahl-/Traversierungs-/Stimmenbudgets; GPU nur asynchron nach Kostenvergleich.
  Quellenbewertung höchstens O(N), feste Auswahl-Buckets statt Vollsortierung O(N log N).
  Ausbreitung/Mixer bleiben durch feste Stimmen-, Strahl- und Buslimits begrenzt;
  keine Quelle-Quelle-Paare und keine Umgebungsabfrage pro virtualisierter Quelle.
  [Geprüfter GitHub-Code und Grenzen](../doc/references/audio/bounded-spatial-audio.md).

## Forschungsgrundlage
[XPBD, MIG 2016](../doc/references/physics/mig/2016-xpbd.pdf),
[DeepMimic, SIGGRAPH 2018](../doc/references/animation/siggraph/2018-deepmimic.pdf),
[Rigid-Body Sound, SCA 2002](../doc/references/audio/sca/2002-rigid-body-sound-synthesis.pdf)
([Primärquellen/Einordnung](../doc/references/README.md)): Bullet für Körper/Kontakte;
Compliance für reduzierte deformierbare Modelle prüfen. Lokale Ziele/Navigation steuern
Gelenkmotoren, nicht Renderposen. Zuerst klassische Steuerung; Training/Referenzbewegungen
sind kein visueller Blocker. Kontaktkräfte treiben wenige Materialresonanzen; Spatial Audio
verarbeitet Richtung/Verdeckung separat. Save/Replay erhält Zustand und externe Events.
Recast/Detour als lokale Boden-Navigation prüfen; Straßengraph und Flug-/Wassermodelle
bleiben eigene Bewegungsverträge. Keine Library-Integration ohne den ausführbaren NPC-Pfad.

## Abnahme
Allgemeiner Körper-/Weltkontakt und Gelenkantrieb stimmen auf echten Straßen/Brücken/Terrain.
Ein NPC ist lokal, über JS und aufgezeichnete LLM-Events steuerbar; Ton und atomarer Save/Replay
erhalten den Zustand. 25/30/60 fps ändern keine Simulation. Hockenheim ist spätere Integration.

Installation: [Abhängigkeiten](../doc/dependencies.md).
[Bullet](https://github.com/bulletphysics/bullet3), [Eigen](https://libeigen.gitlab.io/).
