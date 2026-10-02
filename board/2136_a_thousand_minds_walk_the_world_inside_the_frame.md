Type: feature
State: open
Architecture: planned
Priority: P3
Parent: 2169
Depends: 2188
Area: gameplay, simulation, physics, audio, script, engine
Tags: sandbox, agents, interaction, llm, programmable-physics

# One physical world supports players, LLM NPCs and scripts

## Ergebnis und belegter Iststand
Spieler, LLM-NPCs, JavaScript und HTML/CSS-Oberflächen interagieren mit derselben
weltweiten Sandbox. Konsistente Massen, Kräfte, Beschleunigungen und Gelenke erlauben
unterschiedliche bewegliche Systeme; Fahrzeuge, Flugzeuge und Vegetation sind Beispiele,
keine abgeschlossene Featureliste. Die visuelle Welt bleibt die erste Lieferung.
Rigid/Wrench integrieren lineare und rotatorische Bewegung; Prismatic liefert Reaktionen.
SimulationState::Integrate verwendet bisher nur Schwerkraft, keinen allgemeinen Weltkontakt.
Script/ActionHostAdapter, Ui::Markup/Style/Layout, Commands, Audio und importierte
Animationen bestehen. Ein vollständiger Kontakt-/Gelenkpfad und LLM-NPC-Entscheidungen fehlen.

## Besitzer und Architektur
`physics` besitzt Körper, Kräfte, Kontakt-/Gelenklösung; actor/body fachlich dorthin migrieren.
SimulationState besitzt festen Takt, Entities und Commands. 2188 liefert die gemeinsame
Raum-/Command-/Snapshot-Grenze. Render/Audio konsumieren immutable Posen/Zustände.
Script/UI/Host liefern Commands und Events, keine Objektzeiger oder eigene Physik.
Vor Solver-Ausbau vorhandenen Kern und etablierte lokal verfügbare Kerne abgleichen;
Kontakt-/Gelenkkern sowie Kosten entscheiden. Daher bleibt die Solverarchitektur `planned`.

## Physik und bewegliche Systeme
- SI-Einheiten, Masse/Trägheitsschwerpunkt und Orientierung explizit; Kraft/Angriffspunkt
  erzeugen Beschleunigung/Drehmoment. Gemeinsamer Kontaktpfad für Terrain, Körper und Bauwerke.
- Statische Kontaktgeometrie bleibt unabhängig von Render-LOD. Räumliche Broadphase,
  Shape-Narrowphase und begrenzte Kontakt-/Gelenklösung erhalten stabile lokale Kontakte.
  Gelenke tragen Grenzen, Motoren und Reaktionen; kein getrenntes Fahrzeug-Sonderphysiksystem.
- Fester Takt mit begrenztem Aufholen; Impulse, Reibung, Restitution und schnelle Bewegung
  konsistent lösen. Energie-/Impulserhaltung und dissipative Effekte unterscheiden.
- Fahrzeugreifen, Aerodynamik, Auftrieb und Windlast sind Kraftmodelle auf denselben Körpern.
  Vegetationsbiegung konsumiert Wind/Steifigkeit/Masse; Distanz begrenzt aktive Freiheitsgrade,
  ohne sichtbare Form-/Bewegungssprünge. Keine vollständige Fernsimulation jedes Blattes.

## LLM NPCs, JS und UI
- LLMs erzeugen begrenzte Ziele, Dialog und Entscheidungen asynchron. Deterministische
  lokale Steuerung führt sie über dieselben validierten Commands aus wie JS und UI.
- Entity-/Tick-/Versionsbezug, begrenzte Queues und Antwortfristen; stale Antworten verwerfen.
  Simulation wartet nicht. Modellkonfiguration gehört dem Host; keine Modellabhängigkeit im Physikkern.
- Antworten als replaybare Events protokollieren. Spielzustand/Saves besitzen atomare,
  versionierte Publikation. Replay fragt kein Modell erneut für aufgezeichnete Entscheidungen.
- JS/HTML/CSS erhalten die dokumentierte Teilmenge. Kräfte, Impulse, Gelenkantriebe und
  Interaktionen ändern die Welt über physikalische Regeln, nicht direkte Renderpose-Manipulation.
- Navigation konsumiert logische OSM-Netze. Räumliche Aktivierung, begrenzte Entscheidungs-
  und Animationsraten; entfernte Akteure bleiben kompakter Spielzustand.
- Audio/Occlusion und Animation verwenden dieselben Weltkontakte/Posen. Hockenheim-Runden
  sind spätere Integrationen, keine Voraussetzung der allgemeinen Sandbox.

## Persistenz und Spatial Audio
- Save/Load besitzt stabile Entity-/Asset-IDs, Quell-/Producer-Versionen, Weltänderungen,
  Physikzustand sowie nötigen Script-/NPC-/Replay-Zustand. Transaktional laden/publizieren;
  fehlerhafte/incompatible Saves verändern keinen gültigen Weltstand.
- Szenario-Autorendaten, Netzquellcache und Spielstand getrennt besitzen. Ein Spielstand
  ist kein Generatorcache; native Produkte aus referenzierten Inputs/Versionen rekonstruieren.
- Spatial Audio verwendet AudioScene/AudioOcclusion und denselben Listener-/Entity-Snapshot:
  Entfernung, Richtung, Doppler und geometrische Verdeckung mit begrenzter Stimmenzahl.
  Audio wartet weder auf Netzwerk noch LLM oder Geometrieaufbau.
- Simulation bleibt fester Takt unabhängig von 25/30/60-fps-Ausgabe. Schlafende Körper
  aktivieren bei relevanten Kräften/Kontakten/Commands; entfernte NPCs reduzieren Aufwand,
  ohne kausale Spielzustandsänderungen oder sichtbare Interaktionen zu verlieren.

## Nächste Lieferung und Abnahme
Nach dem visuellen Meilenstein: allgemeiner Körper-/Weltkontakt und ein Gelenkantrieb,
dann ein durch JS und aufgezeichnete LLM-Events steuerbarer NPC im bestehenden Place.
Kontakt und Bewegung folgen Massen/Kräften; Straßen/Brücken und Terrain stimmen überein.
Spieler, Script, NPC, Audio und Save/Replay laufen in derselben Welt ohne zweite Demo-Engine.
Das gemeinsame Render-/Speicherbudget bleibt verbindlich; keine Detailklasse erhält einen Freibrief.
