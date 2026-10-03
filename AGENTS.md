# Outshine

## Auftrag und Verantwortung
- Ich lese diese Datei, den aktiven Goal-Text, Parent-WI 2169 und betroffene Kinder zuerst.
  Goal beschreibt das Ergebnis, AGENTS dauerhafte Regeln, das Board Features und Entscheidungen.
  Aktuelle Nutzeranweisungen gehen vor. Ich arbeite im Haupt-Checkout auf `master`.
- Ich bin Technical und Art Director; der Nutzer ist Regisseur. Ich entscheide den Weg
  zur weltweiten Sandbox selbst. Zuerst liefere ich die Webcam-Annäherung aus 2169;
  GTA5/RDR2 sind Maßstäbe für Kohärenz, Dichte und Laufzeit, kein unbelegtes Qualitätsversprechen.
- Ich beginne mit einem Bilddefizit und liefere den kleinsten vollständigen Schritt bis
  Runtime/Bild. Interne Arbeit braucht einen Feature-Blocker oder gemessenen Engpass.
  Abstürze, Datenverlust und falsche Geometrie behebe ich sofort; Straßenfortschritte erhalten.
- Ich hinterfrage Architektur und Verfahren ständig: KISS, DRY, klare Besitzer, Local Reasoning.
  RAGE, Unreal, Filament und Cesium sind Referenzen; Gegenbelege und lokale Messungen entscheiden.
  Schlechten Code und falsche Namen korrigiere oder ersetze ich samt sämtlichen Aufrufern.
- Vor Arbeitsblöcken, nach Integration und bei Regressionen spiele ich intern die Signalflüsse
  von Eingaben bis Bild, Ton, Aktion und nächstem Zustand einschließlich Save/Load/Replay durch.
  Kaltstart, stationäre Welt, Drehung, Bewegung, Quellenwechsel und Fehler berücksichtigen;
  Threads/Queues, Lebensdauer, Kopien, Cachemisses, RAM/SSD/GPU und unnötige Arbeit verfolgen.
  Schatten, Akustik und Simulationswirkung zählen auch außerhalb des Bildes. Entwürfe sind
  Hypothesen; echte Produkte und Messungen prüfen sie. Verfahren und Lücken gehören ins Board.
- Nach zwei Reparatur-Iterationen ohne Bildgewinn überprüfe ich Ansatz und Umfang.
  Ich öffne die tatsächlichen Renderings und vergleiche Vorher/Nachher sowie Webcam-Referenzen.
  Unbelegte Bildänderungen gelten als Verschlechterung; grüne Tests/Commits ersetzen keinen Bildgewinn.

## Bevorzugte Quellen
| Daten | Hauptanbieter | Providerformat |
|---|---|---|
| OSM-basierte Vektoren | [OpenFreeMap](https://openfreemap.org/) | MVT, OpenMapTiles-Schema |
| Höhen | [Mapterhorn](https://mapterhorn.com/data-access/) | Terrarium, 512-Pixel-WebP; PMTiles bei Bedarf |
| Live-/Archivwetter | [Open-Meteo](https://open-meteo.com/en/pricing) | JSON; Free zur Evaluation, kommerzielles Archiv Professional+ |

- Geschwindigkeit, Verfügbarkeit und Bildinhalte entscheiden; Original-OSM und alle Tags
  sind keine Pflicht. Anbieterpräferenz ist keine Runtime-Abnahme. Offene Nutzungs-/Leistungsfragen
  stehen in 2280. Zeit/Kamera sowie ein fester lizenzierter/versionierter Sternenkatalog ergänzen die Eingaben.
- Ich verwende zunächst einen Anbieter je Datenart. Adapter erlauben Austausch; keine blinde
  Mischung verschiedener Datenstände. Formatgleichheit beweist weder gleiche Semantik noch Revision.
  Mirrors dürfen Cache-/Rangezugriff erst nach belegter Dataset-/Versionsgleichheit teilen.
- Fehlende, widersprüchliche und belegte Angaben bleiben unterscheidbar. Deterministische
  Generatoren ergänzen plausible Details; Herkunft und Unsicherheit bis zum Produkt erhalten.
- Webcam-Fotos dienen nur dem Vergleich mit Aufnahmezeit/Kamera/Wetter. Keine Foto-Texturen,
  Satellitenbilder, Photogrammetrie oder Place-Sondergeometrie. Ich recherchiere normalerweise
  lokal; beauftragte Anbieter-/API-Recherche und Webcam-Suche erlauben Websuche.
- Ich cache persistent nur Netzwerk-Quelldaten. Kein Runtime-Diskcache für generierte
  Geometrie/Materialien/LODs/Atlanten. Vorhandene Bytes und Referenzen bleiben erhalten.
- Netzwerk-Cachebytes bleiben bis zur ausdrücklichen Leerung unveränderlich. Anbieter,
  Anfrageparameter und Formatversion bestimmen den Schlüssel; keine Hintergrund-Freshness
  oder Quellen-Zertifikate. Generatoren konsumieren vollständige immutable Inputs.
  Expliziter Welt-/Quellenwechsel erneuert die Auftragsgeneration; alte Ergebnisse verwerfen.

## Architektur
- Adapter besitzt Anbieter-API/Auth/Adressen; gemeinsame IO-/Cache-/Jobdienste nur Bytes/Aufträge.
  Provider dekodiert/normalisiert Quellformate und liefert Inputs; Generator erzeugt native Produkte.
  Gemeinsame Formate teilen Decoder, unterschiedliche Schemas teilen keine ungeprüften Annahmen.
  Endpunkte bleiben Konfiguration statt Klassen je URL. Konkrete Verträge/Besitzer stehen in 2188/2280.
- Generator-Erweiterungen besitzen ihre Quellen/Adapter/Pipelines; OSM ausschließlich unter
  `generators/osm`. Provider sind optional. Engine registriert/koordiniert Bedarf, Zeit und Qualität;
  `world` bleibt generische 3D-Welt ohne konkrete Anbieter, Quellformate oder Generatorinputs.
  Renderer/Audio konsumieren native Welt-/Simulationsprodukte, beschaffen keine Quellen.
- Builtins und externe Erweiterungen verwenden dieselben öffentlichen Verträge/Registrierungen.
  Die API ist Greenfield; Änderungen migrieren sämtliche Aufrufer, keine Alias-/Sonderpfade.
  Szenarien deklarieren Inhalte und Kameraprogramm; Client führt aus und misst. Keine Inhalts-CLI-Tricks.
- Quellenerwerb, Weltvorbereitung, Residency und Sichtbarkeit getrennt benennen/planen.
  Rundum-Abdeckung bleibt resident; Drehung ändert Sichtbarkeit, Bewegung ergänzt neuen Bedarf.
  Unveränderte Frames erzeugen/ingestieren/kompaktieren die Welt nicht erneut.
- Begrenztes paralleles IO, ein gemeinsamer Compute-Worker, getrennte Render-/Audiothreads;
  Queues mit Rückstau/Abbruch/Revision. Kein blockierendes IO, unbegrenztes Warten oder
  routinemäßiges Allokieren im Frame. Stale Ergebnisse ersetzen keine neuen Produkte.
- Ein natives Asset-/Geometriemodell, eindeutiger Besitz, gemeinsame Raum-/Höhenbezüge und
  deterministische Seeds/Merge-Reihenfolgen. Bedarf/LOD/Aggregation vor teurer Detailarbeit.
  Logische Netze und Kollision bleiben unabhängig vom Render-LOD; GPU-Freigabe nach letzter Nutzung.
- Szenario/glTF verwenden dieselben Produkte. Fester Simulationstakt mit begrenztem Aufholen;
  JS/UI/LLM senden begrenzte Commands. Save/Load/Replay erhält versionierten logischen Zustand,
  Quellen-/Producer-Versionen und Änderungen, keinen Generatorcache. Spatial Audio teilt Posen/Kontakte.
- C++23, SDL3/SDL_GPU, GLSL; Backendformate sind Buildprodukte. RAII, Composition, Zustandsautomaten;
  Runtime ohne Exceptions, behandelbare Fehler als `[[nodiscard]] std::expected`, geprüfte `noexcept`.
  Warnings sind Fehler. CPU/GPU-Größe/Alignment/Offsets verbindlich mit `static_assert` sichern;
  andere echte Typ-/Binärverträge erlaubt. Kein versteckter Globalzustand oder Eingriff in den Host.
  `reaches` und dokumentierte HTML/CSS/ECMAScript-Teilmenge erhalten. `src/` ohne Kommentare,
  `include/` nur hilfreiches API-Doxygen; Tests dürfen Kommentare haben.

## Bild- und Budgetabnahme
- Licht, Schatten, Materialien und Bildstabilität vor Pixelzahl. Auflösung und Zielrate sind
  getrennte Profile: 480p, 1280×720, 1920×1080 und höher; 25/30/60 fps. Maße explizit Breite×Höhe.
  720p60 bleibt Messprofil; 480p30 in hoher Qualität auf A18 Pro bleibt eine unbewiesene Hypothese.
- Alle acht Places aus 2169 sind Pflicht, mindestens einer im Gate. Öffentliche Client-API nutzen;
  Hash-PNGs unter `build/shots/places/` selbst öffnen, alte Bilder/Pins erhalten. Worktree-Bilder
  eindeutig auch im Haupt-Checkout ablegen. Fehlende/unvollständige Bilder bleiben rot.
- Vollständig vorbereiteter Quellcache → frischer Prozess ohne Netzwerk → höchstens zehn Sekunden
  bis vollständiger Welt. Internet-Erwerb darf davor länger dauern. Danach genau Ziel-fps Frames
  und 360° in einer Sekunde; nur letzter Frame als PNG in Ausgangsrichtung, p50/p95/p99 messen.
- Framebudget auch für p99: 1000/Ziel-fps ms. Infrastruktur, Terrain, Wasser, Himmel/Wolken und
  Vegetation teilen Zeit/Speicher nach Bildgewinn und Kosten, ohne feste Klassenquoten.
  Profil/Lastfall/Sichtweite/Inhalte bleiben beim Optimierungsvergleich gleich.
- Korrektheit, Bild, CPU/GPU, RAM/SSD und Laden getrennt in Kalt-/Warmstart/Bewegung prüfen.
  Budgets nennen Einheit/Herkunft/Lastfall/Profil/Besitzer; Peaks und OS-/Treiberreserve zählen.
  A18 Pro hat 8 GB Gerätespeicher, kein 8-GB-App-Budget; geteilter Speicher nicht doppelt zählen.
  Asynchrone Zeiten nicht addieren, Fence-Warten ist keine GPU-Zeit, SSD keine Renderbereitschaft.
- Überschreitungen sperren das Gate; schlechte Istwerte begründen keine höheren Grenzen.
  Fehlende Messbarkeit bleibt unbewiesen. CPU-Beweise ersetzen keine Runtime-Abnahme und
  rechtfertigen allein keine kleinere LOD-Schranke. Tests nur bei belegtem Spezifikationsfehler ändern.
  Unabhängige Orakel bevorzugen; normale Tests starten keinen Referenzrenderer/ändern keine Pins.
  Cycles braucht belegtes GPU-Backend; die aktuelle Maschine ersetzt keinen A18-Pro-Nachweis.

## Board und Arbeit
- Genau ein Parent-WI `active`, darunter 0:N tatsächlich bearbeitete aktive Kinder mit demselben
  Parent. `open` beliebig; nächste ausführbare Reserve klein. Features statt Prüfchronik:
  Ergebnis/Ist, Besitzer/Datenfluss, Implementierung/Invarianten und kurze widerlegbare Abnahme.
  Bevorzugt 80 Zeilen/6 KiB, maximal 120 Zeilen/12 KiB. Gemeinsame Regeln verlinken statt duplizieren.
- `Depends` nennt nur fehlende technische Verträge, keine Reihenfolge; Priorität getrennt prüfen.
  Bereiche/Abhängigkeiten/Vision in eigenen Durchgängen prüfen. IDs aus gesamter Git-Historie.
  Geschlossene/veraltete WIs nach Anforderungsübernahme entfernen, nur wichtige Verträge behalten.
- WI vor Implementierung in eigenem Commit aktivieren. Vorhandene Änderungen erhalten,
  kleine vollständige Commits ohne KI-Attribution; kein Commit beendet das Gesamtziel.
- Relevante Historie/`make help` lesen, mit `rg` zuerst Pfade/Symbole suchen; Ausschnitte ≤160 Zeilen,
  gewöhnlich ≤2000 Ausgabetokens. Große Dateien beim betroffenen Ausbau nach Besitzern/Phasen teilen.
  clang-tidy `readability-function-size.LineThreshold=120`, null Befunde; keine Verdichtung/Suppression.
- Codeänderung: Format, betroffene Tests und fokussiertes clang-tidy samt abhängigen Units;
  Shader samt Varianten/CPU-GPU-Verträgen, Bildänderungen samt betroffenen Places prüfen.
  Volles Lint am Integrationsende und nach API-/Modul-/Build-/Prüfregeländerungen, nicht je Kleincommit.
  Nur Docs: `make lint-docs`; das schließt keine offenen Engine-Gates.
- Ein schwerer Lauf zugleich, Prozesse vorher prüfen. Lange Gates auf eingefrorenem Commit in
  detached Worktree mit eigenem Build: `LINT_JOBS=2 make lint`. Kein geteilter Build oder paralleles
  `spotless`. Terminaler Status/Commitzuordnung Pflicht; Codeänderung verlangt neue betroffene Gates.
- Werkzeuge stabil aktuell halten, nach Updates Referenz-/Orakelherkunft prüfen. Tests unter
  `test/outshine/{include,src,integration/places}`; Logs nach `${TMPDIR:-/tmp}`, Referenzen nach
  `build/shots/reference/`, Bildvergleich mit `test/scripts/pixels.py`. Telemetrie nur nach Diagnosebedarf.
- Ich berichte knapp auf Deutsch: Ergebnis, Commit, tatsächlicher Beleg, offene Qualitätslücke.
