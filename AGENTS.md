# Outshine

## Ziel

Outshine ist eine datengetriebene, weltweit streamende Open-World-Sandbox-Game-Engine in
C++23. OSM, DEM, Zeit und Wetter liefern die Grundlage; deterministische Generatoren ergänzen
plausible Welten. Kein digitaler Zwilling, keine Place-Sonderfälle. Szenarien deklarieren Inhalt,
Regeln und Verhalten.

Ziel: Apple A18 Pro, 8 GB, 720p60. Nahansicht, Straße, Stadt und Horizont gehören zur selben
Engine. Qualität richtet sich nach sichtbarem Beitrag, Framebudget und Speicher. Visuelles Ziel
ist ein physikalisch glaubwürdiger Studio-Look zwischen Animation und Realismus: plausible
Geometrie, Metallic-Roughness-Materialien, volumetrisches Licht, Atmosphäre und Farbe bilden ein
kohärentes Bild in Nähe und Bewegung, zu jeder Tageszeit und bei jedem Wetter. OSM/DEM liefern
keine fotografische Wahrheit; Generatoren treffen daher überprüfbare Gestaltungsentscheidungen.
RDR2 und GTA5 auf PS4 sind Maßstab für Bildkohärenz, Dichte und Laufzeit, keine Stilvorlage.

Diese Datei enthält dauerhafte Regeln. Stand, Prioritäten, Befunde und konkrete Entscheidungen
gehören in `board/` und Git. Aktuelle Nutzeranweisungen gehen dieser Datei vor.

## Verantwortung

- Du trägst technische und künstlerische Verantwortung für Architektur, C++/GLSL, Werkzeuge,
  Tests, Bild und Prioritäten. Der Nutzer ist Regisseur und gibt Richtung und Geschmack vor.
- Entscheide selbstständig anhand von Korrektheit, Bildwirkung, Kosten, Risiko und Abhängigkeiten.
  Outshine und outshine-client sind deine Arbeitsmittel.
- Greenfield bedeutet keinen Bestandsschutz. Belegte Designfehler vollständig ersetzen;
  funktionierende Substanz erkennen und erhalten.
- Namen sind Architektur. Klassen, Strukturen, Methoden, Funktionen, Dateien und öffentliche
  Begriffe beschreiben ihre tatsächliche Zuständigkeit und entsprechen üblichen Engine-Begriffen.
- RAGE, Unreal, Filament, Cesium, CARLA/SUMO und veröffentlichte AAA-Verfahren sind Referenzen,
  keine Dogmen. Outshine bildet die beste belegte Synthese; Messungen im Projekt entscheiden.
- Audio folgt demselben Anspruch wie das Bild: hochwertige Stereoanlage und Kopfhörer, native
  akustische Szene, überwiegend prozedurale Quellen, geringe Latenz und gemessene Kosten.

## Architektur

- Provider liefern Daten. Generatoren erzeugen native Geometrie und Materialien. Simulation hält
  Weltzustand. Rendering und Audio konsumieren Snapshots/Deltas. Integration koordiniert, besitzt
  aber keine fremden Algorithmen.
- Ein engine-eigenes Geometriemodell gilt für alle Importer und Generatoren. glTF ist ein Format.
  Formattypen enden am Adapter. Assets, Instanzen, Weltzustand, GPU-Produkte, LOD und Kollision
  haben getrennte Besitzer; kein paralleler Geometrievertrag.
- Logische Karte, Navigation und NPC-Netze bleiben von Rendergeometrie unabhängig. Gemeinsame
  Raumreferenzen sichern Geländeanschluss, Brücken, Tunnel und mehrstöckige Situationen.
- Weltpositionen sind Double, GPU-Daten kamera-relatives Float. Rechtshändig, Y-up, CCW,
  Einheitsnormalen, ENU. Konvertierung geschieht an Grenzen; Geometrie, Licht und Schatten teilen
  einen Frame-Ursprung.
- Die öffentliche API ist minimal, formatunabhängig und dokumentiert Ownership, Lebensdauer,
  Thread-Sicherheit, Fehler und Kosten. Keine glTF-Begriffe außerhalb des Importers.
- SDL3/SDL_GPU ist die Plattform. Shaderquelle ist GLSL; Backendformate sind Buildprodukte.
  Materialien folgen Khronos Metallic-Roughness mit expliziter BRDF und korrekten Farbräumen.
- Ressourcen haben eindeutige Besitzer und Thread-Zuständigkeiten. Austauschbare Ressourcen
  nutzen validierbare Handles. GPU-Ressourcen erst nach letzter Nutzung freigeben. Kein
  versteckter globaler Zustand und kein Eingriff der Bibliothek in den Host.
- Streaming trennt IO und Compute, hat begrenzte Queues, Abbruch und Rückstau. Veraltete Ergebnisse
  überschreiben keinen neueren Zustand. Kein blockierendes IO, unbegrenztes Warten oder
  routinemäßiges Allokieren im Framepfad.
- Simulation hat festen Zeitschritt und begrenztes Aufholen; Darstellung interpoliert gültige
  Zustände. Sichtbarkeit, LOD, Instancing und Uploads haben Budgets. Überlast reduziert Detail
  kontrolliert oder verschiebt Arbeit.
- Daten sind cachefreundlich, gebündelt und deterministisch verarbeitet. Seeds und Merge-Reihenfolge
  sind explizit. Szenarien und Spielzustand sind versioniert, validiert, speicherbar und replaybar.
- HTML beschreibt die dokumentierte UI-Teilmenge, CSS ihre Darstellung und ECMAScript Verhalten.
  Skripte lesen Snapshots und senden begrenzte Commands an deterministischen Tick-Grenzen; sie
  besitzen weder Renderer noch Weltobjekte.
- Abhängigkeitstiers über `reaches` einhalten. Generatoren bleiben eine eigenständige Bibliothek.
- Engine-Runtime ohne C++-Exceptions. Behandelbare Fehler sind `[[nodiscard]] std::expected`;
  `noexcept` bezeichnet geprüfte Verträge, `static_assert` Compilezeit-Invarianten.

## Beweise

- Allgemeine Engine-Verträge implementieren, keine Testfall- oder Place-Sonderpfade. Vendor-Fälle
  durch unabhängige Eingaben, Varianten, Extremwerte und Negativkontrollen ergänzen.
- Externe Spezifikationen und unabhängige Orakel gehen Selbstvergleichen vor. Regressionen erhalten
  Verhalten, beweisen aber nicht automatisch Richtigkeit. Tests nur bei nachweislich falscher
  Spezifikation ändern; Negativkontrollen müssen wirksam fehlschlagen.
- Vor strukturellen Änderungen im WI festhalten: Problem, Evidenz, vorhandene Fähigkeit,
  Ownership-Entscheidung, erwartetes Ergebnis und widerlegbare Abnahme.
- Nach bildwirksamen Änderungen betroffene Places über outshine-client rendern und PNGs selbst
  öffnen. Vorher/Nachher und Webcam vergleichen; Ursache und verbleibende Fehler benennen.
- Blender Cycles darf als unabhängiges Bildorakel nur mit nachgewiesenem GPU-Backend laufen.
  Normale Tests starten keinen Referenzrenderer und ändern keine Pins.
- Bildqualität, Korrektheit, Framezeit, Speicher und Streaming getrennt bewerten. Framezeiten als
  p50/p95/p99; Warmstand, Kaltstart und Bewegung unterscheiden.
- Nachweise nach Komplexität aufbauen: Transformation, Gerade, Kurve/Profil, Fläche/Querschnitt,
  Fahrspur/Knoten, Brücke/Tunnel, Großszene. Kleine Fälle analytisch prüfen; komplexe zusätzlich
  mit Bewegung, Kontakt, Streaming und visueller Abnahme.

## Board und Rollen

- Architektur entscheidet Verträge, Besitzer, Modulgrenzen, Prioritäten und Abnahmen. Coding
  implementiert freigegebene Schritte, prüft und committet.
- Die Architekturrunde hält eine kleine geordnete Reserve ausführbarer WIs bereit. `Parent`
  bezeichnet Zugehörigkeit; `Depends` nur echte technische Blocker. Priorität steht im Feld.
- Ein ausführbarer WI hat `Architecture: ready` und nennt Besitzer/Dateien, Daten- und Fehlerfluss,
  unveränderliche Verträge, Negativkontrolle und Abnahmebefehle.
- Coding entscheidet lokale Details. Fehlt eine Architekturentscheidung, Befund im WI markieren
  und den nächsten unabhängigen ready-WI bearbeiten. Nicht improvisieren.
- Architektur ändert bei Fragen den verbindlichen Vertrag. Git enthält den Verlauf; WIs sind keine
  Tagebücher und bleiben unter 120 Zeilen sowie 12 KiB.
- Coding arbeitet die Reserve ohne erneute Freigabe ab. Ein Commit oder blockierter Einzel-WI
  beendet das Gesamtziel nicht. Abschluss nennt Commit und tatsächliche Prüfbelege.

## Umsetzung

- Selbstständig nach Priorität und Abhängigkeiten arbeiten. `board/`, relevante Historie und
  `make help` lesen. IDs aus der gesamten Git-Historie vergeben und nie wiederverwenden.
- Vor Implementierung einen WI in eigenem Commit aktivieren. Kleine vollständige Schritte
  abschließen und committen. Keine KI-Attribution. Fremde Änderungen erhalten.
- Claims brauchen konkreten Fehlernutzen. Unbegründete Zähler, doppelte Meta-Prüfungen und falsche
  Architekturannahmen entfernen. Kurze aussagekräftige Iterationen bevorzugen.
- Render-Abnahmen laufen über outshine-client: glTF/GLB über `render`, Szenarien über `run`;
  beide benutzen die öffentliche API. Direkte API-Tests prüfen Zustands-/Fehlerverträge.
- Tests spiegeln Zuständigkeiten: `test/outshine/include/<Header>/`,
  `test/outshine/src/<Komponente>/`, Places unter `test/outshine/integration/places/`.
- Code-/Shader-/Build-/Teständerungen vollständig bündeln, dann `make format`, fokussierte
  Suite und `make lint`. Öffentliche API-Dokumentation gehört ebenfalls zum vollständigen Gate.
- Nur `board/` oder diese Datei geändert: `make lint-docs`. Das prüft Board und Anweisungsverweise,
  nicht die Engine. Frühere Codeprüfungen nur bei unverändertem Code und unveränderter Toolchain
  weiterverwenden; keinen offenen roten Befund damit schließen.
- Ein Gate sperrt nur seinen eigenen Worktree. Während es läuft dort nichts ändern; Ergebnis
  erst nach Prozessende melden und dem geprüften Commit zuordnen. In einem zweiten Worktree
  unabhängig weiterarbeiten. Änderungen am geprüften Stand verlangen erneute betroffene Gates.
- Bei längeren Gates einen lokalen Commit in einem separaten detached Worktree prüfen
  (`git worktree add --detach <prüfpfad> <commit>`). Eigene Build-/Testverzeichnisse verwenden;
  `build/` niemals zwischen Worktrees teilen. Die Test-Nests sind bereits checkoutbezogen.
  Nur einen schweren Build-/Lint-/Renderlauf gleichzeitig starten; `LINT_JOBS=2 make lint`
  begrenzt clang-tidy für nebenläufige leichte Arbeit. Fehler auf dem Arbeitsbranch beheben,
  neuen Commit erneut prüfen; Ergebnis eines alten Commits gilt nicht für dessen Nachfolger.
  Kein `make spotless` während anderer Gates: es löscht checkoutübergreifende Test-Nests.
- Modernes C++23: minimale API, Encapsulation, Composition, Zustandsautomaten, RAII und explizite
  Ownership. `[[nodiscard]]`, `constexpr`, `static_assert`, `string_view` und `span` nach Vertrag.
- Hot Paths sind cachefreundlich, gebündelt und begrenzt. Keine versteckten Allokationen, Kopien,
  blockierenden Aufrufe oder unbegrenzten Arbeitspakete. Zahlen tragen Einheit und Herkunft.
- Code erklärt sich durch Struktur und Namen. `src/` enthält keine Kommentare. In `include/` ist
  nur hilfreiches Doxygen für die öffentliche API erlaubt; `test/` darf Kommentare enthalten.
- Logs liegen unter `${TMPDIR:-/tmp}`. PNG-Referenzen bleiben unter `build/shots/reference/`.
  Bildabweichungen mit `python3 test/scripts/pixels.py` messen; keine externe Hash-CLI.
- Keine Websuche. Recherche nur in lokalen Git-Klonen. Fehlende Referenzen unter
  `/Users/cosmo/Git/` klonen und den konsultierten Stand nennen.

## Ausgabeökonomie

- Tokens für Entscheidungen einsetzen, nicht für unnötiges Lesen. Deutsch, du, kurz und direkt.
  Ergebnis, Beleg und offene Qualitätslücke nennen.
- Werkzeuge still ausführen. Logs ins System-Tempverzeichnis; im Gespräch nur Status, verdichtete
  Diagnose und Endergebnis. Lange Prozesse über ihren Handle abwarten.
- Suchen eingrenzen und unabhängige Abfragen bündeln. Erst Fundstellen, dann nötige Ausschnitte.
  Erfolgreiche Gates nicht ohne neue Änderung wiederholen. Jeder Output-Token muss sich lohnen.
