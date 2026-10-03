Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: public-api, engine, world, generators, render
Tags: extension, ownership, native-model

# Public contracts connect adapters, providers, generators and native world products

## Ergebnis und Ist
Eingebaute und externe Erweiterungen laufen über dieselbe öffentliche API bis ins Bild.
ProviderRegistry/SourceSet, Generate::Request/Generator, native Geometry/Material,
Double-Welt und kamera-relative GPU-Daten bestehen. ProjectedErrorBudget ist vorhanden;
Abstands-/Produktschranken und zustandsbehaftete Generatorintegration fehlen.
Source.DecodeElevation liefert native HeightRaster; Provider besitzt WebP-Dekodierung,
Terrain übernimmt den Samplespeicher. Legacy-PNG/COG-Decode bleibt zu migrieren.
Konkrete MVT/Copernicus-Typen liegen noch in world/data, WeatherProvider in world/weather;
SourceAcquisition/OSM-Import liegen bereits unter generators/osm. Engine koppelt weiter
konkrete Pipelines, native StructureBake-Pfade umgehen den allgemeinen Lebenszyklus.

## Besitzer und Datenfluss
| Besitzer | Vertrag / Grenze |
|---|---|
| API-Adapter in generators/<domain> | Anbieteradressen/Auth/TileJSON/Antworthülle; nutzt gemeinsame HTTP-Dienste |
| Provider in generators/<domain> | Gemeinsamer Formatdecode, Schema-/Einheiten-/Datum-Normalisierung → gepinnte Inputs |
| Generator in generators/<domain> | Bedarf + 0:N Provider/Inputs → native Geometrie/Instanzen/Parameter/Netze |
| IO/Cache/Jobs | Begrenzte Bytes/Receipts/Aufträge; keine Quellsemantik oder Weltplanung |
| engine | Registriert, plant versionierten Bedarf, publiziert vollständige Produktstände |
| world | Generische native Assets/Entities/Topologie/Provenienz; keine Quellformate/Generatorinputs |
| render/audio | Welt-/Simulationssnapshot → Bild/Ton; kein Quellenerwerb oder Weltaufbau |
| physics/Script/UI/Host | Gemeinsamer Zustand/Commands aus 2136; kein Besitz an Renderprodukten |

API-Adapter → Provider → Generator → native Welt. Formatdecode steht vor Schemaadapter;
URL/Auth ist Konfiguration statt Klasse je Endpoint. Provider dürfen lokale Zustands-/
Zufallsinputs liefern; reine Generatoren brauchen keinen Provider. Bibliotheksnutzer
registrieren eigene Implementierungen; keine privaten Builtin- oder Host-Sonderwege.

## Gemeinsame Verträge und Migration
- WorldDemand: Raum/Abdeckung, Kamera-/Höhenbezug, Projektion/Qualität, UTC, Revision/Frist.
  Quellbedarf, Produktbedarf und Sichtbarkeit getrennt; Blickrichtung kürzt keine Residency.
- GenerationRequest: stabile Identität/Seed, gepinnte Inputs, Entfernung und erlaubter
  Bildschirmfehler. Product: native Inhalte, Bounds, Abhängigkeiten, bekannte/ungeklärte
  konservative Fehlerschranke. Erwerb asynchron, Compute ohne blockierendes IO.
- SourceReceipt: Anbieter/Dataset/Adresse/Revision/Digest/Gültigkeit. Formattypen enden im
  Provider; ProductKey hält konsumierte Quellen-/Producer-Versionen und Parameter.
  Produkte pinnen nötige Inputs, nicht vollständige XML/MVT-Archive. Gültig leer ≠ fehlend.
- Gemeinsamer Raum-/Höhenbezug: Double-Welt → kamera-relative Floats, rechtshändig Y-up/CCW.
  Erde ist ein Raumadapter, keine Pflicht jedes externen Generators. Kontakte/Render/Audio teilen Posen.
- Lebenszyklus: begrenzte Vorbereitung → atomare native Publikation → gezielte Erneuerung/
  Freigabe; Revision/Abbruch und Elternabdeckung erhalten. Keine Neuaufbereitung unveränderter Frames.
- Konkrete Quellen/Decoder aus world/data zu ihrer Erweiterung verschieben; gemeinsame
  Formatdecoder wiederverwenden. Surface-/ClassificationPreparation aus engine/streaming nach
  Erzeugungsbesitz migrieren; Engine behält nur allgemeine Koordination. Abhängigkeiten absichern.
- Native Gebäudeinputs/BuildingGeometry, Klassifikationsgrids und logische Netze erhalten;
  GroundClassBuffer/Upload bleiben render-eigen. Szenario/glTF publizieren dieselben Assets;
  Szenarien besitzen Inhalte/Kamera. WeatherSnapshot erhält Ort/UTC/Einheiten/Gültigkeit/Herkunft.
- Cacheöffnung erhält vorhandene Bytes; Trim nur explizite IO-Wartung unbenutzter Caches.
  Gepinnte Inputs, Publikation und GPU-Freigabe besitzen eindeutige Lebensdauer.

## Nächste Lieferung und Abnahme
1. Öffentlichen Abstands-/Produktschrankenvertrag bis zur nativen Gebäudequeue integrieren;
   Bedarf vor Terrain/Detailarbeit. Gröberes DEM braucht eine eigene Höhenfehlerschranke.
2. Provider-/Adaptergrenzen und Generatorlebenszyklus im 2280-Ladepfad durchsetzen;
   vorhandene Registrierungen nutzen, konkrete Engine-Aufrufe entfernen.
3. Wetter-Snapshot für 2172 anschließen; spätere Commands/Simulation besitzt 2136.
Ein externer Provider/Generator ersetzt einen Builtin bis zum echten Place-Bild ohne private
Includes. Weltmodule kennen keine MVT-/OSM-/Copernicus-Typen; Runtime und API beschreiben
denselben Lebenszyklus. Keine Schranken-/Budgetbehauptung allein aus Deklarationen.
