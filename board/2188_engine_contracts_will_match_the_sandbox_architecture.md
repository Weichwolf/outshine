Type: feature
State: active
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
Dichte GPU-Farbbereiche und ein gemeinsamer Placement-Vertrag für Darstellung/Culling bestehen.
Der deklarative Generatorpfad und spezialisierte Weltqueues laufen noch getrennt; Engine kennt
OSM-Felder/Akquisition, world enthält konkrete Höhen-/Wetterprovider. Das erschwert Änderungen.

## Umfang und Besitzer
Dieser WI migriert jeweils den für 2280/2336 oder ein Bildfeature benötigten Pfad. Keine
vollständige SDK-Neufassung als Vorbedingung und kein zusätzlicher Universal-Scheduler.
API-Adapter besitzt Adresse/Auth/Antworthülle, Provider Formatdecode/Normalisierung,
Generator Fachplanung/Produkte; Engine Bedarf/Jobs/Publikation, world native Inhalte.
Gemeinsame IO-/Assetcache-/Jobdienste enthalten keine Quellsemantik. 2280 besitzt den Cache:
Engine fordert native Assets an; Misses erzeugen sie über Generatoren. Derselbe öffentliche Vertrag lädt sie
nach räumlicher/LOD-Auswahl. Indexabfragen: Frustum oder Radius R um Weltposition x,y,z.

## Konkrete Integration
1. Die laufende Gebäude-/Terrainlieferung über denselben öffentlichen Vertrag anbieten:
   räumlicher Bedarf, Zeit, Seed, Qualitätsauftrag, vollständige Eingaben → native Produkte
   samt Bounds, Kosten und repräsentationsgerechter Geometrie-/Bildgültigkeit. Fehler und gültig leer
   unterscheiden. Provider optional; lokale Zufalls-/Spielzustandsinputs sind zulässig.
   Detailauftrag/Auswahl gelten für Terrain, Gebäude, Vegetation und importierte Assets;
   Fachpläne bleiben privat. Asset-ID, konservative Bounds, LOD-/Abhängigkeiten und native
   serialisierbare Produkte anbinden; keine zweite Builtin-Persistenz. Mesh-, Oberflächen- und
   Volumenprodukte brauchen passende Formate. Cachehits umgehen Provider/Rohling-Neubau.
   Budgetierte Runtime-Verfeinerung erhält fertige Rohlinge, keine unvollständigen Quelldaten.
   Identische Misses teilen einen Job; Hits/Misses publizieren dieselben nativen Produkte.
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
Renderer besitzt Vertexformat, Adressraum und Lebensdauer (`SubjectDraw`, `SubjectResidency`,
`SceneResources`). Optionale Streams reservieren nur eigene Produkte, keine Löcher anderer
Geometrie. Für 2173 zuerst dichte Float4-Farbspeicherung mit eigenem Bereichsallocator:
Vertexshader liest über Vertexindex plus expliziten Farb-Offset des Placement-Datensatzes.
Current/Previous-Pose und Farb-Offset besitzen einen gemeinsamen CPU/GPU-Vertrag; Größe,
Alignment und Offsets statisch sichern. Flat/Lit, Culling, Instanzen und Borrow-Pfade migrieren.
Batching bleibt bestehen; unveränderte Frames packen keine Welt neu. Farbe/Gate-Bilder bleiben
identisch bei weniger Gerätebytes. Konstante Erscheinung später in Material-/Batchdaten;
Quantisierung braucht Fehlerschranken, die öffentliche Float-API keinen Wertebereichsverlust.

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
