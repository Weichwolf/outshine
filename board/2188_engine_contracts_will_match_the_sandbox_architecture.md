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
RawTile hält geschlossene OSM-Inputs mit schwachem Archivbezug; BuildingGeometry hält
native Polygone/Quellbelege. Vollständige Archive verbleiben im Quellenladepfad. SimulationState integriert bisher
Schwerkraft; Rigid/Wrench/Prismatic liefern Grundlagen, keinen vollständigen Weltkontakt.
Script/ActionHostAdapter und Ui::Markup/Style/Layout bestehen und bleiben verwendbar.
SurfacePreparation und Straßenaufträge liegen noch unter engine/streaming. StreetGraphPreparation
nutzt den gemeinsamen Compute-Worker mit Abbruch und geteiltem Auftragsbesitz.
ClassificationPreparation verbindet Ingestion und Publikation noch in engine/streaming;
ClassificationBuild nutzt den gemeinsamen Compute-Worker. Der pure ClassificationRasterizer
liegt unter generators/terrain; world hält nur den immutable Klassifikationssnapshot.
Fine/Coarse behalten ihren Raumbezug und die jeweils konsumierte Quellrevision.
GroundClassBuffer besitzt GPU-Packing und Digest unter render; ClassStructure bleibt ein natives CPU-Produkt.
Engine publiziert beide zusammen; render hält nur seinen Uploadpuffer. Native Netze halten keine
Quellarchive. OSM-Provider, Erwerb, Zellverfeinerung und Netz-/Routenaufbau liegen unter generators/osm und nutzen geliehene Engine-Queues. Generische Quellenkonfiguration erhält den Registry-Auftrag explizit; Builtin-Komposition liegt unter generators. Decoder/Validierung in world/data und konkrete Engine-Kopplung bleiben offen.

## Zuständigkeiten und gerichteter Datenfluss
| Besitzer | Eingabe → Ausgabe | Grenze |
|---|---|---|
| IO/Cache/Jobs | begrenzte Aufträge → Bytes/Arbeitsresultate | Quellunabhängig; Zellabdeckung folgt Adresse, Cachepolicy und Payload-Digest; keine Weltplanung |
| world | native Produkte → generischer Weltzustand | Geometrie, Identitäten, Topologie und Herkunft; keine Quell-/Generatorinputs |
| engine | Szenario + Kamera/Zeit/Qualität → versionierter Weltbedarf | Registrieren, koordinieren, native Produkte geschlossen publizieren |
| generators/<domain> | Bedarf + 0:N Provider/Inputs → native Produkte | Eigene Beschaffung/Adapter/Erzeugung; gemeinsame Dienste, kein Weltbesitz |
| physics + SimulationState | Commands + Kontakte → Simulationssnapshot | Fester Takt, Massen/Kräfte/Gelenke; eigene Lebensdauer |
| render/audio | Welt-/Simulationssnapshot → Bild/Ton | Sichtbarkeit/Ausgabe; keine Quellabfragen oder Weltgenerierung |
| Script/UI/LLM-Host | Eingabe/Events → validierte Commands | Kein direkter Objektbesitz; keine blockierende Modellantwort |

Engine koordiniert diese Besitzer. Renderer konsumiert immutable Weltprodukte und
aktuelle Posen; Render-LOD verändert weder logische Netze noch Physik oder Spielzustand.
Bibliotheksnutzer ersetzen Provider/Generator/Host über dieselbe öffentliche Registrierung.
Physik liegt fachlich unter physics; actor/body und unspezifische Subject-Bezeichner
beim betroffenen Ausbau nach Bedeutung migrieren, keine Alias-Schichten.

## Korrektur der Modulgrenzen
| Heute vermischt | Zielbesitzer und gerichtete Grenze |
|---|---|
| world/data: IO, Cache, OSM, Copernicus, MVT | Allgemeine Dienste getrennt; konkrete Quellen/Adapter gehören ihrer Generator-Erweiterung |
| world/ground: OSM/MVT-Ingestion und Produkte | generators/osm übersetzt Original-OSM; world hält ausschließlich native Produkte |
| world/data + engine: OSM-Quellen und Pipeline-Typen | generators/osm besitzt Beschaffung, Decode und Jobs; Engine konsumiert den allgemeinen Generatorvertrag |
| Gebäude-Publikation (bereinigt) | BuildingGeometry trägt nur native Polygone und generische Provenienz/IDs |
| actor/body: Rigid/Prismatic | physics besitzt Simulation; actor konsumiert sie |
| import (bereinigt) | Native CPU-Assets ohne Rendererfreigabe; keine transitive Render-Abhängigkeit |
| Klassifikations-Upload (bereinigt) | world hält native Grids; render besitzt gepackte Uploadprodukte und deren Lebensdauer |
| private Builtin-Bakes neben Generator-API | Ein öffentlicher Input-/Productvertrag für Builtins und Erweiterungen |
| Client-Flags/Place-Sonderablauf | Szenario deklariert Generatoren/Inhalte und Kamerafahrt; Client führt aus und misst |
`world` konsumiert weder Quellenformate noch Generatorinputs. Engine kennt nur öffentliche Erweiterungsverträge.
Quellformate enden im Adapter; Generatorinputs gehören dem jeweiligen Generatorvertrag.
Jede Migration entfernt den alten Pfad und bekommt eine prüfbare Abhängigkeitsgrenze.

## Verbindliche gemeinsame Verträge
- WorldDemand enthält vollständige räumliche Abdeckung, Kamera-/Höhenbezug, Projektion
  und Qualitätsauftrag sowie UTC, Revision und Frist. Blickrichtung verändert keine Rundum-Residency.
  Quell-, Produkt- und Sichtbarkeitspläne getrennt halten; unterschiedliche Raster erlauben.
- GenerationRequest enthält Raumreferenz, Abdeckung, stabile Identität/Seed, gepinnte
  native Inputs und erlaubten Bildschirmfehler. Product enthält native Geometrie oder
  kompakte Instanzen/Parameter, Bounds, Abhängigkeiten und bekannte/ungeklärte Fehlerschranke.
  Generatoren übernehmen Demand und melden Fortschritt, Fehler oder vollständige Produkte;
  Quellenerwerb ist asynchron, Compute nutzt gepinnte Inputs ohne blockierendes IO.
- SourceReceipt identifiziert Originalquelle, Adresse, Revision/Digest und Gültigkeit.
  Produktpins halten konsumierte Inputs, nicht automatisch vollständige OSM-Zellarchive.
  Alle Originaltags bleiben im Quellcache; konsumierte Semantik/Herkunft bleibt am Produkt.
  Gebäudeinputs übernehmen ausschließlich ihre typisierten Wurzeln samt transitiven Referenzen,
  Tags und Quellbelegen. Ein schwacher Archivbezug dient der Wiederverwendungsprüfung, nicht
  dem Produktbesitz. Zell-Snapshots freigeben, soweit kein tatsächlicher Nutzer sie braucht.
- Geometrie, Kontakte, Licht und Wasser teilen expliziten Frame-Ursprung/Höhendatum.
  Double-Welt → kamera-relative Floats; rechtshändig Y-up/CCW. Erde ist ein Raumadapter,
  keine versteckte Voraussetzung jedes externen Generators.
- ProductKey umfasst Source-/Producer-Version, Seed/Parameter und Abhängigkeiten.
  Jobs tragen Revision und Abbruch; stale Ergebnisse ersetzen keinen neuen Stand.
  Begrenzte IO-Zellschritte geben die gemeinsame Queue frei; ein Handle endet erst nach dem letzten Schritt.
  Kandidaten wechseln atomar. GPU-Ressourcen leben bis nach ihrer letzten Submission.
- Unveränderte Produkte einschließlich gültiger Leerprodukte werden nicht erneut aufgebaut.
  Bedarf/Qualität ersetzt betroffene Produkte; Eltern halten Abdeckung bis Kinder bereit sind.
  Speicher-/Arbeitsgrenzen gehören zum jeweiligen Besitzer; fehlende Daten sind kein Leerprodukt.
- SimulationCommand adressiert stabile Entities und validiert Einheiten/Zustand. JS,
  UI und LLM-NPCs teilen Kräfte, Impulse, Gelenkantriebe und Interaktionen. Physik berechnet
  Folgen. Editor-/Setup-Poseänderungen sind explizite Modi, kein verdeckter Laufzeitpfad.
- LLM-Antworten sind asynchrone, begrenzte Events mit Tick/Entity-/Versionsbezug.
  Deterministische lokale Steuerung funktioniert während ausstehender Antworten weiter;
  Replay nutzt aufgezeichnete Events statt erneut Modellantworten anzufordern.
- Wetter liefert einen öffentlichen Orts-/UTC-/Höhen-Snapshot mit Einheiten, Gültigkeit
  und Herkunft. Physikalische Wind-/Wasser-/Materialzustände konsumieren denselben Snapshot.
- Szenario/glTF-Loader publizieren dieselben nativen Assets; Formattypen enden im Adapter.
  Spatial Audio und Save/Load teilen Entity-/Pose-/Versionsverträge aus 2136.
  Places deklarieren stationäre 360°-Fahrt in einer Sekunde; Capture speichert nur den letzten Frame.
  Generatorauswahl und Vegetation gehören ins Szenario; CLI wählt Szenario/Ausgabe/Messung.
  Öffentliche API ist Greenfield; sämtliche Builtins und Aufrufer zusammen migrieren.

## Ausführbare Lieferung
1. Öffentlichen zustandsbehafteten Demand-/Produktvertrag anschließen; bisheriges synchrones
   make(Request) ersetzt keine Weltpipeline. OSM-Typen/Provider/Decoder/Jobs unter generators/osm
   zusammenführen und sämtliche Engine-Aufrufer migrieren. Include-/Typgrenzen erzwingen.
2. Szenario besitzt Generatorauswahl und Kameraprogramm; CLI-Inhaltsüberschreibungen entfernen.
3. 2280s Pipeline mit nativer Ingestion, Produkt-/Pinbesitz und gemeinsamen Jobdiensten verbinden.
4. 2336s Bedarf vor Geometrie- und Terrainanforderung platzieren; Snapshot-Pins/Produktbesitz entkoppeln.
5. Wettervertrag für 2172 sowie Command-/Snapshot-Grenze für 2136 vervollständigen.
Andere Features konsumieren jeweils den fehlenden Teilvertrag, keine pauschale Gesamtabnahme.

## Abnahme
Ein externer Provider/Generator ersetzt Builtins bis zum Bild ohne private Includes.
Vollständiger warmer Place bleibt im Budget; Entfernen unnötiger Pins verliert keine Semantik.
JS und ein aufgezeichnetes LLM-Event wirken über denselben physikalischen Commandpfad.
Deklarierte Verträge, tatsächliche Aufrufer und Runtime beschreiben dasselbe System.
