Type: chore
State: active
Area: include, engine, world, import
Tags: architecture, ownership, api
Parent: 2188
Depends: 2191, 2150

# Public API exposes native engine contracts with explicit ownership

## Befund

Die API wird nach Outshines Verträgen gestaltet. Filament-/Cesium-Namensgleichheit,
ein Header pro Typ und global eindeutige Kurzbezeichner sind keine Abnahmekriterien.
Der API-Bericht inventarisiert alle öffentlichen Header statt Vorbildnamen zu zählen.
Namespaces dürfen passende gleichnamige Begriffe enthalten; entscheidend sind
Verständlichkeit, Abhängigkeitsrichtung und die tatsächlich dargestellte Verantwortung.

- Die undefinierte SwapChain::logsTo-Deklaration ist entfernt; der Logger gehört
  nicht zur Swapchain. Process-globaler Logger und sichere Registrierung bleiben
  in 2208. Frameabschluss und GPU-Warten dokumentieren Zustand und Fehler.
- Engine::setGeometry besitzt eine native Kopie, aber ohne Entity-/Instanzzuordnung.
  Live::Carry verlangt Joined_ > 0 und lehnt rein generierte Geometrie ab.
  Draws überträgt alle Simulationskörper als Instanzen desselben SubjectProxy;
  Body::Asset bestimmt dort nicht die tatsächlich instanzierte Geometrie.
- Die Registry besitzt eine stabile Adresse und ist nicht verschiebbar. Handles
  tragen Registry-Epoche, Index und Generation; Columns speichern vollständige
  Identitäten. Fremde Handles und Reopen dürfen keine Komponenten übernehmen.
  Engine-Zugriff bleibt öffentlich mutierbar; Systembindung nach Reopen klar begrenzen.
- Registry-Abfragen dürfen ungültige Handles nicht als Body oder belegten Sitz
  ausgeben. roleOf liefert optional; seatOf prüft beide Handle-Lebensdauern.
  Entfernung, Slot-Wiederverwendung und fremde Registry mit API-Tests abdecken.
- Geometry besitzt direkte setPlacement/setMaterial/setLight-Methoden statt geliehener
  Managerfassaden. Ungültige Indizes dürfen keine Daten ändern; clear löscht aktive Zugriffe.
  Mesh-/Material-Ressourcen und Instanzen gemäß 2150 trennen, ohne zweite Geometrie.
- Scenario.h bündelt Deklarationen vieler Systeme. Nach fachlichen Abhängigkeiten
  aufteilen, soweit dies isolierte Consumer und nachvollziehbare Verträge ermöglicht;
  keine Headerzahl als Ziel. Jeder öffentliche Header muss selbstständig verwendbar sein.
- Kamera und Audio lösen benannte Körper jetzt gegen die aktuelle Assembly auf;
  Trigger laufen mit Entity-Handles ohne Renderer. Instanzen ohne physikalischen
  Körper benötigen weiterhin dieselbe native Pose-/Geschwindigkeitsquelle.

Callback-Grenze: Script-Werte explizit typisieren; Nothing/Ref nicht als Zahl null
weiterreichen. Argumente auf kMaxArgs begrenzen, ohne Heapkopie pro Aufruf; Host
bleibt geliehen. Adapter isoliert prüfen, Fehler ohne Client-Aufruf ablehnen.

## Entscheidung und Zuständigkeit

2150 implementiert native Mesh-/Material-Assets, Instanzzuordnung und einen gemeinsamen
Renderpfad für Importer und Generatoren. 2191 besitzt transaktionale Zustandsübergänge
und die Gültigkeit zusammengehöriger Deklaration, Assembly und Bindungen.
Dieses WI gestaltet und prüft den öffentlichen Consumer-Vertrag dieser Fähigkeiten.
Die Abhängigkeiten blockieren die Gesamtabnahme, nicht unabhängige API-Korrekturen.

Assets besitzen unveränderliche Daten; Entities/Instanzen referenzieren sie über
validierbare Handles. Engine/Simulation besitzen Weltzustand und Lebensdauer.
Ein Körper ohne Renderkomponente bleibt simulierbar; Mesh-Instanzen benötigen keine
Physik. Eine Mesh-Ressource kann mehrfach instanziert werden, ohne ihre Daten zu kopieren.
Materialien, Sichtbarkeit, Animation und Schatten hängen nicht vom Importformat ab.

Geliehene Referenzen/Spans nennen Lebensdauer, Invalidierung, Mutabilität und Threadbindung.
Langlebige Identitäten erkennen fremde Owner und Generationen. Fehler liefern owned
expected-Ergebnisse mit klarer Erhaltungsgarantie; kein globaler Fehler als einziges Ergebnis.
View-/Target-Auswahl und Facaden-Ownership nach Nutzung gestalten, nicht als Vorbildkopie.
Dokumentation nennt Räume, Einheiten, Vorbedingungen, Kosten und Fehlerverträge.

## Öffentliche Modulgrenzen

- `core/`: Fehler, Logging und Basistypen; `math/`: gemeinsame Mathematik.
- `world/`: Entity.h enthält Handles; EntityRegistry.h die stabile Entity-Verwaltung,
  erreichbar über Engine::entities(). Implementierung unter src/world/entity/.
  World als übergreifenden Laufzeitbesitzer aus bestehendem Besitz zusammenführen;
  keine zweite parallele Weltverwaltung.
- `assets/`: native Geometrie, Materialien, Texturen und Animationsdaten.
  Material.h beschreibt Wertebereiche, Farbräume, Einheiten, Kanalbelegung und
  owner-lokale Referenzen; keine Formatkonvention als impliziter Runtime-Vertrag.
  Fresnel-/Index-Wertoperationen constexpr/noexcept nach geprüften Vorbedingungen.
- `import/`: GltfImporter ersetzt Loaded; Formatobjekte bleiben im Adapter.
- `generation/Generate.h`: native Generatorverträge; geliehene Provider und Setupkosten.
  Registry besitzt eindeutige, nichtleere Registrierungsnamen; Generatoren bleiben
  explizit geliehen. Ungültige Shipped-Werte dürfen keinen Arrayzugriff auslösen.
- `export/`: exportGlb übernimmt writeGlb mit owned expected-Ergebnis; kein
  Exportvertrag im Generatorheader. src/import hält die gemeinsamen Formatadapter.
  Bestehender Writer verliert native Texturbindungen und zusätzliche Materialfaktoren;
  nicht darstellbare Inhalte vor Ausgabe ablehnen, danach Exportabdeckung ausbauen.
  GLB-Größen vor Padding auf 32-Bit-Containergrenze prüfen; size_t-Überlauf
  darf keine übergroße Nutzlast akzeptieren. Grenzwerte ohne Allokation testen.
  GLB-Header/Chunklängen nach Khronos 2.0 §4.4 prüfen; unterstützte Faktoren und
  Namen direkt im JSON prüfen, Fehler und unveränderte Eingaben separat abnehmen.
  Referenz: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#glb-file-format-specification
- `render/`: native Kamera mit lokaler Pose, Projektion und Belichtung; Renderer
  und Ausgabeziele. Szenario-Views besitzen den Modus FollowEntity/Local/Geodetic
  und geodätische Eingaben. Importheader benötigen keine Szenariodefinitionen.
- `simulation/` und `audio/`: tatsächlich öffentliche Systemverträge.
- `scenario/`: ScenarioDefinition beschreibt den initialen Aufbau und die
  Konfiguration; Reader/Writer gehören hierher, allgemeine Kameratypen nicht.
- SurfaceState/SurfaceKind/Winding sind interne Renderableitungen von Material;
  aus include/scene nach src/render verschieben, alle Consumer gemeinsam migrieren.
  Native Lichtparameter dokumentieren Lux/Candela, lineare Farbe, lokale Richtung,
  Reichweite und Spot-Halbwinkel; keine unbewiesene Validierung behaupten.
- Engine.h enthält die Fassade; Outshine.h bleibt optionaler Sammelheader.

Implementierung, Consumer, Installationspfade und Dokumentation gemeinsam migrieren.
Importer und Generatoren dürfen weder Szenariotypen als native Assettypen benötigen
noch den Laufzeitbesitz bestimmen. Szenariodaten konfigurieren die native World.

## Abnahme

- [ ] Kein deklarierter, undefinierter oder bedeutungsloser öffentlicher Einstieg.
- [ ] Externer Client baut und linkt ausschließlich mit installierten öffentlichen
      Headern/Library; kein src/-Include und kein Importdokument für native Szenen.
- [ ] Zwei verschiedene native Meshes an zwei Entities, eine zusätzliche Instanz,
      ein unsichtbarer Physikkörper und eine Instanz ohne Physik funktionieren gemeinsam.
- [ ] Reorder, Bewegung, Entfernung/Recreation und Austausch von Geometrie erhalten
      korrekte Identitäten; alte/fremde Handles werden kontrolliert abgelehnt.
- [ ] Derselbe Consumer-Vertrag gilt für generierte und importierte Geometrie;
      unabhängige Pixel-/Transform-Orakel und visuell geprüfte PNGs belegen dies.
- [ ] Alle öffentlichen Header sind selbstständig verwendbar; API vollständig
      dokumentiert, make lint inklusive clang-tidy und relevante Consumer-Tests grün.
- [ ] Negativkontrollen für falsche Entity-Zuordnung, verlorene Materialreferenzen
      und vorzeitige Ressourcenfreigabe verletzen jeweils ihr Oracle.
