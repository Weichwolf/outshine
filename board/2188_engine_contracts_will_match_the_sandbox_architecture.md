Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: public-api, engine, generators, world
Tags: extension, ownership, integration

# Builtin and external generators use the same working world pipeline

## Ergebnis und Ist
Ein öffentlicher Erweiterungsvertrag führt Provider/Generatoren bis in die native Welt.
ProviderRegistry, Generator::make, Geometry/Material und kamera-relative Darstellung bestehen.
Der deklarative Generatorpfad und spezialisierte Weltqueues laufen noch getrennt; Engine kennt
OSM-Felder/Akquisition, world enthält konkrete Höhen-/Wetterprovider. Das erschwert Änderungen.

## Umfang und Besitzer
Dieser WI migriert jeweils den für 2280/2336 oder ein Bildfeature benötigten Pfad. Keine
vollständige SDK-Neufassung als Vorbedingung und kein zusätzlicher Universal-Scheduler.
API-Adapter besitzt Adresse/Auth/Antworthülle, Provider Formatdecode/Normalisierung,
Generator Fachplanung/Produkte; Engine Bedarf/Jobs/Publikation, world native Inhalte.
Gemeinsame IO-/Cache-/Jobdienste enthalten keine Geografie- oder Quellsemantik.

## Konkrete Integration
1. Die laufende Gebäude-/Terrainlieferung über denselben öffentlichen Vertrag anbieten:
   räumlicher Bedarf, Zeit, Seed, Qualitätsauftrag, vollständige Eingaben → native Produkte
   samt Bounds, Kosten und konservativer/ungeklärter Fehlerschranke. Fehler und gültig leer
   unterscheiden. Provider optional; lokale Zufalls-/Spielzustandsinputs sind zulässig.
2. Fachliche Vorbereitung aus Engine/SurfacePreparation zur verantwortlichen Erweiterung
   führen. OSM/XML/MVT zu generators/osm, Höhen-/Wetterdecoder zu ihren Erweiterungen.
   Private Sonderaufrufe beim Anschluss entfernen, statt einen Wrapper darüberzulegen.
3. Gemeinsame native Assets/Instanzen/Netze behalten; Szenario und glTF münden in dieselben
   Produkte. Weltbezug ist allgemein; Geodäsie ein Adapter. Double-Welt → kamera-relative
   Floats, rechtshändig Y-up/CCW; Render/Audio/Kontakte teilen denselben Ursprung.
4. Ein begrenzter Compute-Worker, paralleles IO, Render-/Audiothreads mit klarer Ownership.
   Vorbereitung → fertiges Produkt → atomare Veröffentlichung/Freigabe. Abbruch und
   explizite Input-/Parameterwechsel schützen laufende Jobs; Cache-Vertrag ausschließlich 2280.
   Große unveränderte Snapshots teilen, keine Weltkopie für ein lokales Produkt.
5. Öffentlicher WeatherSnapshot für 2172 enthält Ort/Höhe/UTC, Einheiten, bekannte/fehlende
   Felder und Herkunft. Spätere Commands/Physik/Persistenz gehören zu 2136, nicht in diesen Umbau.

## Kompakte native Renderprodukte
Renderer besitzt Vertexformat, Adressraum und Lebensdauer je Layoutgruppe
(`SubjectDraw`, `SubjectResidency`, `SceneResources`). Optionale Streams reservieren nur
die dazugehörigen Produkte, nicht Löcher für anders formatierte Geometrie. Layoutgruppen
bündeln Draws; unveränderte Frames packen keine Welt neu. Konstante Erscheinungsparameter
gehören in Material-/Batchdaten. Fehlerschranken begründen jede Attributquantisierung;
die öffentliche Float-API erhält keinen stillen Wertebereichsverlust. 2173 liefert den Farbfall.

## Bewährte Formatdecoder
`base/format/Json` nutzt künftig simdjson, `base/format/Xml` pugixml; ihre konkreten Typen
bleiben privat. Erst mit dem betroffenen Importpfad integrieren, keinen Parserumbau vor Gebäuden.
Eingabelimits, UTF-8/Numbers, Fehlerpositionen, Lebensdauer der Views und Roundtrip-Verträge
erhalten; keine Exception-basierten Aufrufe und kein Parsing im Frame. Ein gemeinsamer Decoder
je Format ersetzt Eigenparser, keine Parallelpfade. Installation: [Abhängigkeiten](../doc/dependencies.md).
[simdjson](https://github.com/simdjson/simdjson), [pugixml](https://pugixml.org/).

## Forschungsgrundlage
[Standards und Backendverträge](../doc/references/README.md): glTF/SDL_GPU statt einer
SIGGRAPH-Begründung für API-Lebensdauer. Import und Szenario teilen native Assets;
Quellformat endet am Adapter. GPU-Driven-Verfahren nur innerhalb verfügbarer SDL-Verträge.

## Abnahme
Ein externer Provider/Generator ersetzt den entsprechenden Builtin im echten Place ohne
private Includes oder zweite Pipeline. Modulabhängigkeiten sichern world ohne Quelltypen.
Bild/Funktion bleiben erhalten, zusätzlicher Zustand entfällt. Erst integrierte Nutzung
belegt den Vertrag; neue Interfaces und bestandene Include-Prüfungen allein reichen nicht.
