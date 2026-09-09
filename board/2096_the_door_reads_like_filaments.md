Type: chore
State: active
Area: include, engine, scene
Tags: architecture, ownership, api
Parent: 2188
Depends: 2191, 2150

# Public API exposes native engine contracts with explicit ownership

## Befund

Die API wird nach Outshines Verträgen gestaltet. Filament-/Cesium-Namensgleichheit,
ein Header pro Typ und global eindeutige Kurzbezeichner sind keine Abnahmekriterien.
Keine pauschale Umbenennung von Geometry zu Model oder Loaded zu Asset.
Namespaces dürfen passende gleichnamige Begriffe enthalten; entscheidend sind
Verständlichkeit, Abhängigkeitsrichtung und die tatsächlich dargestellte Verantwortung.

- Die undefinierte SwapChain::logsTo-Deklaration ist entfernt; der Logger gehört
  nicht zur Swapchain. Process-globaler Logger und sichere Registrierung bleiben
  in 2208. Frameabschluss und GPU-Warten dokumentieren Zustand und Fehler.
- Engine::setGeometry besitzt eine native Kopie, aber ohne Entity-/Instanzzuordnung.
  Live::Carry verlangt Joined_ > 0 und lehnt rein generierte Geometrie ab.
  Draws überträgt alle Simulationskörper als Instanzen desselben SubjectProxy;
  Body::Asset bestimmt dort nicht die tatsächlich instanzierte Geometrie.
- Scene ist öffentlich mutierbar und austauschbar. Column leiht ihre Adresse;
  Index/Generation allein unterscheiden neu geöffnete Scenes nicht zuverlässig.
  Fremde oder neu entstandene Entities dürfen keine alten Komponenten übernehmen.
- Scenario.h bündelt Deklarationen vieler Systeme. Nach fachlichen Abhängigkeiten
  aufteilen, soweit dies isolierte Consumer und nachvollziehbare Verträge ermöglicht;
  keine Headerzahl als Ziel. Jeder öffentliche Header muss selbstständig verwendbar sein.
- Kamera und Audio lösen benannte Körper jetzt gegen die aktuelle Assembly auf;
  Trigger laufen mit Entity-Handles ohne Renderer. Instanzen ohne physikalischen
  Körper benötigen weiterhin dieselbe native Pose-/Geschwindigkeitsquelle.

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
