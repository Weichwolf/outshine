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
XML-/JSON-Dateien, vollständiges Inline-JSON und partielle JSON-Overrides teilen den
öffentlichen Szenariolader und Validator; Overrides gelten auch für Places.
Render::ContentSelection wählt native Mesh-Teile, Terrain-Lattice und Instanzen unabhängig
von Renderstufen. JSON/XML und Overrides teilen den Vertrag; Standard ist die vollständige
Darstellung. Quellbedarf, gespeicherte Assets, logische Welt und Höhenkontakte bleiben bestehen.
Straßenfachplanung/native Netzassets und die Gebäude-Quellaufbereitung liegen in generators/osm;
Engine führt Worker/Publikation. POI-Verknüpfung und Gebäuderezepte bleiben in dieser Erweiterung.
Cachetreffer umgehen Layout/Verknüpfung/Profilierung; dieselben AssetCache/ResolveAsset-Dienste laden.
Der deklarative Generatorpfad und spezialisierte Weltqueues laufen noch getrennt; Engine kennt
OSM-Felder/Akquisition, world enthält konkrete Höhen-/Wetterprovider. Das erschwert Änderungen.

## Umfang und Besitzer
Dieser WI migriert jeweils den für 2280/2336 oder ein Bildfeature benötigten Pfad. Keine
vollständige SDK-Neufassung als Vorbedingung und kein zusätzlicher Universal-Scheduler.
API-Adapter besitzt Adresse/Auth/Antworthülle, Provider Formatdecode/Normalisierung,
Generator Fachplanung/Produkte; Engine Bedarf/Jobs/Publikation, world native Inhalte.
Gemeinsame IO-/Assetcache-/Jobdienste enthalten keine Quellsemantik. Öffentliche AssetCache/
ResolveAsset nutzen dieselben Bounds, Pakete und Abfragen für Builtins und externe Factories.
ResolveAssetRequest bindet einen vor Quellenarbeit bekannten Auftrag an ein fertiges Produkt;
externer Generator mit unbekannter Inhalts-ID integriert. Builtin-Akquisition noch zu migrieren.
Byteformate bleiben beim Produktbesitzer; der Dienst dekodiert keine OSM-/Terrainsemantik.
2280 besitzt den Cache: Engine fordert native Assets an; Misses erzeugen sie über Generatoren.
Derselbe öffentliche Vertrag lädt sie nach räumlicher/LOD-Auswahl: Frustum oder Radius R um x,y,z.

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
   Gemeinsamer Bedarf aus 2336 wählt native Eltern/Kinder vor Provideranforderungen, einschließlich
   Höhen-/Audio-/Kontaktnebenpfaden. Erweiterungen liefern Bounds/Unsicherheit/Produktbezug;
   world kennt keine DEM-/OSM-Tags. Räumlicher Assetindex bleibt generatorunabhängig.
2. Szenario-Datei oder vollständiges Inline-JSON plus optionale JSON-Overrides über denselben
   Lader/Validator führen. JSON bildet die vorhandenen Sektionen ab; Objekte ergänzen,
   Arrays ersetzen, null entfernt. Overrides bestimmen Layerwahl und anschließend das
   vollständige Ergebnis. Client-Inhaltsschalter entfallen; Kamera-/Messbefehle bleiben.
   Dateipfade relativ zur Quelldatei, Inline-Layer relativ zum Shipped-Verzeichnis auflösen.
   Neue Inhaltsparameter gehören zum jeweiligen Fach-WI und werden hier generisch übernommen.
   Render-Auswahl: `terrain`, `instances` und `meshPart` mit exakten nativen Namen;
   leere Namensliste zeigt alle Teile. Auswahl betrifft Farbe/Tiefe/Schatten, nicht Fachplanung.
3. Fachliche Vorbereitung aus Engine/SurfacePreparation zur verantwortlichen Erweiterung
   führen. OSM/XML/MVT zu generators/osm, Höhen-/Wetterdecoder zu ihren Erweiterungen.
   Private Sonderaufrufe beim Anschluss entfernen, statt einen Wrapper darüberzulegen.
4. Gemeinsame native Assets/Instanzen/Netze behalten; Szenario und glTF münden in dieselben
   Produkte. Weltbezug ist allgemein; Geodäsie ein Adapter. Double-Welt → kamera-relative
   Floats, rechtshändig Y-up/CCW; Render/Audio/Kontakte teilen denselben Ursprung.
5. Ein begrenzter Compute-Worker, paralleles IO, Render-/Audiothreads mit klarer Ownership.
   Vorbereitung → fertiges Produkt → atomare Veröffentlichung/Freigabe. Abbruch und
   explizite Input-/Parameterwechsel schützen laufende Jobs; Cache-Vertrag ausschließlich 2280.
   Große unveränderte Snapshots teilen, keine Weltkopie für ein lokales Produkt.
6. Öffentlicher WeatherSnapshot für 2172 enthält Ort/Höhe/UTC, Einheiten, bekannte/fehlende
   Felder und Herkunft. Spätere Commands/Physik/Persistenz gehören zu 2136, nicht in diesen Umbau.

## Kompakte native Renderprodukte
Renderer besitzt Vertexformat, Adressraum und Lebensdauer (`SubjectDraw`, `SubjectResidency`,
`SceneResources`). Optionale Streams reservieren nur eigene Produkte, keine Löcher anderer
Geometrie. Für 2173 zuerst dichte Float4-Farbspeicherung mit eigenem Bereichsallocator:
Vertexshader liest über Vertexindex plus expliziten Farb-Offset des Placement-Datensatzes.
Current/Previous-Pose und Farb-Offset teilen Größe/Alignment/Offsets als CPU/GPU-Vertrag.
Flat/Lit, Culling, Instanzen und Borrow-Pfade migrieren.
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
