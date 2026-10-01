Type: feature
State: open
Architecture: planned
Priority: P3
Parent: 2169
Depends:
Area: gameplay, simulation, physics, audio, script, engine
Tags: sandbox, agents, interaction

# The visual world becomes a playable and populated sandbox

## Ergebnis und vorhandene Fähigkeit
Spieler erkunden dieselbe prozedurale Welt zu Fuß, im Fahrzeug und im Flug; Verkehr,
Figuren, Interaktion, räumliches Audio und persistenter Spielzustand machen sie zur Sandbox.
Deklarative Szenarien, begrenzte Commands, starre Körper, importierte Animationen,
UI-/Script- und Audiopfade bestehen; vollständige Kontakte, Bevölkerung und Spielablauf fehlen.
Diese Phase folgt dem visuellen Meilenstein, vorhandene Fähigkeiten bleiben erhalten.

## Architektur und nächste Lieferung
SimulationState verarbeitet deterministische feste Schritte und publiziert eigene
Snapshots an Render/Audio. Programme/Host-Provider liefern begrenzte Commands/Events.
Zuerst Spielerbewegung und verlässlichen Kontakt in einer vorhandenen Welt liefern;
danach Fahrzeug-/Verkehrsfluss und Figuren. Verträge für Kontakte, Agent-Pose und
Spielzustand vor Ausbau entscheiden; keine unbewiesene Architektur als `ready` erklären.

## Umsetzung und Invarianten
- Logische OSM-Netze bleiben unabhängig von Render-LOD; Kontaktprodukte kommen aus
  denselben nativen Bauwerken/Terrain. Körper treffen Boden, Wände, Brücken und Tunnel.
- Spielersteuerung, Flug und Fahrzeuge verwenden die gemeinsame Raumreferenz.
  Hockenheim-Runden werden spätere Integrationstests, kein Ersatz für Sandbox-Funktionen.
- Bevölkerung/Verkehr deterministisch aus Standort/Nutzung/Zeit ableiten. Agenten als
  kompakte Zustands-/Posezeilen, räumliche Aktivierung und begrenzte Update-/Animationsraten.
  Entfernte Akteure erhalten logische Zustände statt vollständiger Simulation pro Objekt.
- Verhalten als Programme/Zustandsautomaten; optionale Host-Antworten asynchron als
  replaybare Events. Kein Frame wartet auf Netzwerk-/Modellantwort oder Audioarbeit.
- Räumliches Audio, Occlusion und optionaler HRTF-Ausgang nutzen die tatsächliche Welt.
  HTML/CSS/ECMAScript-Teilmenge und UI senden begrenzte Commands, besitzen keine Weltobjekte.
- Spielzustand, Szenarien, Saves und Replay versionieren; begrenztes IO/atomarer Ersatz
  erhält letzten vollständigen Stand. Bibliotheksnutzer steuern Inhalte über die öffentliche API.

## Abnahme
Ein kleiner deklarativer Spielablauf beweist Erkunden, Kontakt, Verkehr/Figur, Interaktion,
Audio und Save/Replay in derselben Welt. Langlauf und Bewegung erhalten deterministischen
Zustand, begrenzten Speicher und Renderbudget; keine zweite Demo-/Szenario-Engine.
