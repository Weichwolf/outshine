Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: public-api, engine, world, generators, render, simulation
Tags: extension, ownership, native-model, foundation

# One engine contract connects sources, world products and simulation

## Ergebnis und belegter Iststand
Outshine besitzt eine gemeinsame Architektur für die weltweite visuelle Welt und die
spätere physikalische, programmierbare Sandbox. Eingebaute und externe Erweiterungen
verwenden dieselben öffentlichen Verträge. Vorhandene Systeme migrieren; keine zweite Engine.
ProviderRegistry/SourceSet, native Geometry/Material, Double-Welt und kamera-relative
GPU-Daten bestehen. Generate::Request trägt Ort/Seed/Ground/Coarseness, aber keinen
Projektions-/Fehlervertrag. Native StructureBake-Pfade umgehen diesen Generatorvertrag.
OriginalStructurePreparation fordert Terrain nach einem gemeinsamen heightZoom an;
RawTile hält vollständige SourceSnapshots gepinnt. SimulationState integriert bisher
Schwerkraft; Rigid/Wrench/Prismatic liefern Grundlagen, keinen vollständigen Weltkontakt.
Script/ActionHostAdapter und Ui::Markup/Style/Layout bestehen und bleiben verwendbar.

## Zuständigkeiten und gerichteter Datenfluss
| Besitzer | Eingabe → Ausgabe | Grenze |
|---|---|---|
| world/data | öffentliche Provider → Originalbytes/Quellbelege | Formate enden am Adapter; Netzwerkcache gehört hierher |
| world | Originalobjekte → native Semantik/Topologie | IDs, Tags, Herkunft, Raum-/Höhenbezug; keine GPU-Objekte |
| engine/streaming | Position/Höhe/Projektion → Bedarf/residente Produkte | Plan, begrenzte Jobs, Invalidierung, geschlossene Publikation |
| generators | native Inputs + Detailauftrag → native Produkte | Pure Seeds/Versionen; kein Renderer, Netzwerk oder versteckter Weltbesitz |
| physics + SimulationState | Commands + Kontakte → Simulationssnapshot | Fester Takt, Massen/Kräfte/Gelenke; eigene Lebensdauer |
| render/audio | Welt-/Simulationssnapshot → Bild/Ton | Sichtbarkeit/Ausgabe; keine Quellabfragen oder Weltgenerierung |
| Script/UI/LLM-Host | Eingabe/Events → validierte Commands | Kein direkter Objektbesitz; keine blockierende Modellantwort |

Engine koordiniert diese Besitzer. Renderer konsumiert immutable Weltprodukte und
aktuelle Posen; Render-LOD verändert weder logische Netze noch Physik oder Spielzustand.
Bibliotheksnutzer ersetzen Provider/Generator/Host über öffentliche Registrierung.
Physik liegt fachlich unter physics; actor/body und unspezifische Subject-Bezeichner
beim betroffenen Ausbau nach Bedeutung migrieren, keine Alias-Schichten.

## Verbindliche gemeinsame Verträge
- WorldDemand enthält vollständige räumliche Abdeckung, Kamera-/Höhenbezug, Projektion
  und Qualitätsauftrag. Blickrichtung beeinflusst Sichtbarkeit, nicht Rundum-Residency.
  Quell-, Produkt- und Sichtbarkeitspläne getrennt halten; unterschiedliche Raster erlauben.
- GenerationRequest enthält Raumreferenz, Abdeckung, stabile Identität/Seed, gepinnte
  native Inputs und erlaubten Bildschirmfehler. Product enthält native Geometrie oder
  kompakte Instanzen/Parameter, Bounds, Abhängigkeiten und bekannte/ungeklärte Fehlerschranke.
  Geburt eines Produkts ist Vorbereitung, kein synchroner Rendereraufruf.
- SourceReceipt identifiziert Originalquelle, Adresse, Revision/Digest und Gültigkeit.
  Produktpins halten konsumierte Inputs, nicht automatisch vollständige OSM-Zellarchive.
  Alle Originaltags bleiben im Quellcache; konsumierte Semantik/Herkunft bleibt am Produkt.
  Zell-Snapshots nach abgeschlossener Ingestion freigeben, soweit kein tatsächlicher Nutzer sie braucht.
- Geometrie, Kontakte, Licht und Wasser teilen expliziten Frame-Ursprung/Höhendatum.
  Double-Welt → kamera-relative Floats; rechtshändig Y-up/CCW. Erde ist ein Raumadapter,
  keine versteckte Voraussetzung jedes externen Generators.
- ProductKey umfasst Source-/Producer-Version, Seed/Parameter und Abhängigkeiten.
  Jobs tragen Revision und Abbruch; stale Ergebnisse ersetzen keinen neuen Stand.
  Kandidaten wechseln atomar. GPU-Ressourcen leben bis nach ihrer letzten Submission.
- Residenter Weltstand wird bei unveränderten Eingaben nicht erneut aufgebaut. Bedarf
  und Qualitätsänderung ersetzen betroffene Produkte; Eltern halten Abdeckung bis Kinder bereit sind.
  Speicher-/Arbeitsgrenzen gehören zum jeweiligen Besitzer; fehlende Daten sind kein Leerprodukt.
- SimulationCommand adressiert stabile Entities und validiert Einheiten/Zustand. JS,
  UI und LLM-NPCs teilen Kräfte, Impulse, Gelenkantriebe und Interaktionen. Physik berechnet
  Folgen. Editor-/Setup-Poseänderungen sind explizite Modi, kein verdeckter Laufzeitpfad.
- LLM-Antworten sind asynchrone, begrenzte Events mit Tick/Entity-/Versionsbezug.
  Deterministische lokale Steuerung funktioniert während ausstehender Antworten weiter;
  Replay nutzt aufgezeichnete Events statt erneut Modellantworten anzufordern.
- Wetter liefert einen öffentlichen Orts-/UTC-/Höhen-Snapshot mit Einheiten, Gültigkeit
  und Herkunft. Physikalische Wind-/Wasser-/Materialzustände konsumieren denselben Snapshot.
- Groundless/glTF, Audio, Szenario-Roundtrip und deklarative Spielabläufe bleiben nutzbar.
  Öffentliche API ist Greenfield; sämtliche Builtins und Aufrufer zusammen migrieren.

## Ausführbare Lieferung
1. 2280s unabhängige Erwerbspipeline an die Runtime anschließen; Rohbytes/Decode getrennt begrenzen.
2. GenerationRequest/Product und gemeinsame Raum-/Fehlerwerte öffentlich machen; Builtins migrieren.
3. 2336s Bedarf vor Geometrie- und Terrainanforderung platzieren; Snapshot-Pins/Produktbesitz entkoppeln.
4. Wettervertrag für 2172 sowie Command-/Snapshot-Grenze für 2136 vervollständigen.
Andere Features konsumieren jeweils den fehlenden Teilvertrag, keine pauschale Gesamtabnahme.

## Abnahme
Ein externer Provider/Generator ersetzt Builtins bis zum Bild ohne private Includes.
Vollständiger warmer Place bleibt im Budget; Entfernen unnötiger Pins verliert keine Semantik.
JS und ein aufgezeichnetes LLM-Event wirken über denselben physikalischen Commandpfad.
Deklarierte Verträge, tatsächliche Aufrufer und Runtime beschreiben dasselbe System.
