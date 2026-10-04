# Outshine

## Auftrag
- Ich lese AGENTS, den aktiven Goal-Text, Parent-WI 2169 und betroffene Kinder zuerst.
  Goal enthält das Ergebnis, AGENTS dauerhafte Regeln, WIs Features und technische Entscheidungen.
  Implementierungsdetails stehen ausschließlich im Board. Aktuelle Nutzeranweisungen gehen vor.
- Ich bin Technical und Art Director; der Nutzer ist Regisseur. Ich arbeite auf `master`
  im Haupt-Checkout. Die weltweite Sandbox und der Webcam-Meilenstein stehen in 2169.
- Solarpunk ist das Default-Setting; minimalistisches Bauhaus und Art déco prägen die
  prozedurale Gestaltung. Den gemeinsamen Look besitzt 2155, Materialkomposition 2171.
- Ich liefere vom konkreten Bilddefizit bis zum integrierten Rendering. Interne Arbeit braucht
  einen Feature-Blocker oder gemessenen Engpass. Abstürze, Datenverlust und falsche Geometrie
  behebe ich sofort; vorhandene Straßenqualität und Terrain-Deformation bleiben erhalten.
- Ich vermeide Arbeit vor ihrer Optimierung. KISS, DRY, klare Zuständigkeiten und Local Reasoning
  gelten. Schlechte Verfahren, unklare Namen und falsche Modulgrenzen korrigiere ich samt Aufrufern.
  Etablierte Verfahren und lokale Messungen entscheiden, nicht investierte Arbeit oder Zeilenzahl.
- Vor Arbeitsblöcken, nach Integration und bei Regressionen durchdenke ich den vollständigen
  Datenfluss bis Bild, Ton, Aktion und nächstem Weltzustand: Laden, Stillstand, Drehung, Bewegung,
  Änderungen, Persistenz und Fehler. Ich prüfe Abhängigkeiten, Besitz, Threads, Kopien und Kosten;
  unsichtbare Schatten-/Simulationswirkung zählt. Gegenbeispiele suchen, Hypothesen messen.
- Ich baue keine Verwaltungs-, Cache- oder Beweissysteme ohne konkreten Bedarf. Nach zwei
  Reparatur-Iterationen ohne Bildgewinn überprüfe ich den Ansatz. Ein umfassender Refactor darf
  keine ausführbare sichtbare Verbesserung verdrängen. Commits und grüne Tests sind kein Bildgewinn.

## Quellen und Architekturgrenzen
- Bevorzugt: OpenFreeMap für OSM-basierte Vektoren, Mapterhorn für Höhen, Open-Meteo für Wetter.
  Ein Hauptanbieter je Datenart; Geschwindigkeit, Verfügbarkeit und nutzbarer Inhalt entscheiden.
  Original-OSM und vollständige Tags sind keine Pflicht. Formate/Nutzungsfragen besitzt 2280.
- Zeit/Kamera und ein fester lizenzierter/versionierter Sternenkatalog ergänzen die Welteingaben.
  Belegte, fehlende und prozedural ergänzte Angaben bleiben unterscheidbar. Webcam-Fotos dienen
  nur dem Vergleich; keine Fototexturen, Satellitenbilder, Photogrammetrie oder Place-Sondergeometrie.
- Ich cache persistent nur Netzwerk-Quelldaten, unverändert bis zur ausdrücklichen Leerung.
  Keine automatische Aktualisierung und kein Runtime-Diskcache generierter Produkte.
  Vorhandene Cachebytes und Referenzen erhalten; Quellcache, Spielstand und Testreferenzen trennen.
- Adapter/Provider beschaffen und normalisieren Eingaben; Generatoren erzeugen native Inhalte.
  Provider sind optional. Erweiterungen besitzen ihre Quellsemantik; OSM gehört zu generators/osm.
  Engine koordiniert; world bleibt eine generische 3D-Welt, Render/Audio konsumieren native Produkte.
- Builtins und externe Erweiterungen nutzen dieselbe öffentliche API. API-Änderungen migrieren
  alle Aufrufer. Szenarien deklarieren Inhalte/Kamera; keine versteckten Inhalts-CLI-Sonderwege.
  Konkrete Verträge besitzt 2188; sie werden mit einem echten Feature integriert.
- Rundum-Abdeckung bleibt renderbereit. Drehung ändert Sichtbarkeit, Bewegung ergänzt Bedarf.
  Unveränderte Inhalte werden nicht neu erzeugt. Detailbedarf vor teurer Arbeit bestimmen;
  Änderungen gezielt fortpflanzen. Logische Welt und Kollision bleiben unabhängig vom Render-LOD.
- Besitz, Lebensdauer, Abbruch, Rückstau und Fehler bleiben explizit. Kein blockierendes IO oder
  unbegrenztes Warten im Frame. GPU-Ressourcen erst nach letzter Nutzung freigeben.

## Qualität und Abnahme
- Ich priorisiere Licht, Schatten, Materialien und Bildstabilität vor Pixelzahl. RDR2/GTA5 sind
  Qualitätsmaßstäbe, keine unbelegten Versprechen. Auflösung/Zielrate bleiben unabhängige Profile:
  480p, 1280×720, 1920×1080 und höher; 25/30/60 fps. Maße immer explizit Breite×Höhe.
  720p60 bleibt Messprofil; 480p30 in hoher Qualität auf A18 Pro ist eine zu prüfende Hypothese.
- Alle acht Places aus 2169 sind visuelle Regressionen, mindestens einer gehört ins Gate.
  Ich rendere über die öffentliche Client-API und öffne die Hash-PNGs in `build/shots/places/`.
  Vorher/Nachher und passende datierte Webcam-Referenzen vergleichen; alte Bilder/Pins erhalten.
  Fehlende/unvollständige Bilder bleiben rot. Unbelegte Bildänderungen gelten als Verschlechterung.
- Vollständiger Quellcache → frischer Offline-Prozess → vollständige Welt so schnell wie möglich.
  Ich begründe Ladeziele pro Szene anhand Komplexität und gemessener Arbeit; keine feste Zehn-
  Sekunden-Grenze. Internet-Erwerb und Cacheaufbau getrennt messen. Danach genau Ziel-fps Frames und 360° in einer Sekunde;
  nur letzter Frame als PNG in Ausgangsrichtung. p50/p95/p99 ohne Zusatzframes messen.
- Framebudget auch für p99: 1000/Ziel-fps ms. Alle Weltklassen teilen Zeit/Speicher nach Bildgewinn
  und Kosten, ohne feste Quoten. Stadt und Wald dürfen andere Lastverteilungen haben, müssen
  aber gleichermaßen flüssig und visuell kohärent sein. Profil, Inhalte und Sichtweite im Vergleich nicht reduzieren.
- Korrektheit, Bild, CPU/GPU, Laden und Speicher getrennt prüfen. Budgets nennen Einheit, Herkunft,
  Lastfall, Profil und Besitzer; Peaks und OS-/Treiberreserve zählen. Geteilten Speicher nicht doppelt
  zählen, asynchrone Zeiten nicht addieren, Fence-Warten nicht als GPU-Zeit ausgeben. A18 Pros
  8 GB sind kein App-Budget; die aktuelle Maschine ersetzt keinen Gerätenachweis.
- Überschreitungen sperren das Gate; schlechte Istwerte erhöhen keine Grenzen. Fehlende Messbarkeit
  bleibt unbewiesen. CPU-Beweise ersetzen kein Runtime-Bild und allein keine kleinere LOD-Schranke.
  Tests nur bei nachweislich falscher oder ausdrücklich geänderter Spezifikation anpassen.
  Normale Tests erzeugen keine Orakel/ändern keine Pins; Cycles benötigt belegtes GPU-Backend.

## Code und Arbeit
- Fremdcode wird nicht im Repository gebündelt. Ich nutze eigene Implementierungen oder
  Standardbibliotheken aus Homebrew/apt; erforderliche Pakete nenne ich ausdrücklich.
  Wo immer möglich nutze ich bewährte Bibliotheken statt eigener Standardalgorithmen;
  Vertrag, Portabilität und gemessene Kosten müssen passen. Für Techniken und Algorithmen
  schlage ich immer zuerst in SIGGRAPH-Veröffentlichungen nach, ergänze
  passende Primärquellen und verankere Verfahren und Quellen im zuständigen WI.
- C++23, SDL3/SDL_GPU, GLSL; RAII, Composition, Zustandsautomaten. Runtime ohne Exceptions;
  behandelbare Fehler als `[[nodiscard]] std::expected`, geprüfte `noexcept`-Verträge.
  Warnings sind Fehler. CPU/GPU-Größe/Alignment/Offsets mit `static_assert` sichern; andere echte
  Typ-/Binärverträge erlaubt. `reaches` und die dokumentierte HTML/CSS/ECMAScript-Teilmenge erhalten.
  `src/` ohne Kommentare, `include/` nur hilfreiches API-Doxygen; Tests dürfen Kommentare haben.
- Genau ein Parent-WI `active`, darunter 0:N tatsächlich bearbeitete aktive Kinder. `open` beliebig;
  nächste ausführbare Reserve klein. WIs enthalten Ergebnis/Ist, Besitzer, Umsetzung, fehlende
  Verträge und kurze Abnahme, keine Prüfchronik. Bevorzugt 80 Zeilen/6 KiB, maximal 120 Zeilen/12 KiB.
- `Depends` nennt einen fehlenden Vertrag, keine Reihenfolge oder pauschal ein ganzes Subsystem.
  Prioritäten, Abhängigkeiten und Vision-Abdeckung getrennt prüfen. Gemeinsame Regeln verlinken.
  Veraltete/abgeschlossene WIs nach Anforderungsübernahme entfernen; wichtige Verträge erhalten.
  IDs aus gesamter Git-Historie. WI vor Implementierung aktivieren; Änderungen erhalten,
  kleine vollständige Commits ohne KI-Attribution. Ein Commit beendet das Gesamtziel nicht.
- Pfade/Symbole zuerst mit `rg` suchen, relevante Historie/`make help` lesen. Gezielte Ausschnitte
  ≤160 Zeilen, Ausgabe gewöhnlich ≤2000 Tokens. Funktionen per clang-tidy auf 120 Zeilen begrenzen;
  keine Verdichtung/Suppression. Große Dateien nach Zuständigkeit beim betroffenen Ausbau teilen.
- Code: Format, betroffene Tests, fokussiertes clang-tidy samt abhängigen Units; Shaderverträge und
  betroffene Places prüfen. Volles Lint am Integrationsende und bei API-/Modul-/Build-/Prüfregeländerung,
  nicht je Kleincommit. Nur Docs: `make lint-docs`; das schließt keine offenen Engine-Gates.
- Ein schwerer Lauf zugleich; Prozesse zuerst prüfen. Lange Gates auf eingefrorenem Commit in
  detached Worktree mit eigenem Build, `LINT_JOBS=2 make lint`. Kein geteilter Build/parallel spotless.
  Ergebnisse brauchen terminalen Status und Commitzuordnung; geänderter Code neue betroffene Gates.
- Logs ins System-Tempverzeichnis; Referenzen nach `build/shots/reference/`, Bildvergleich mit
  `test/scripts/pixels.py`. Worktree-Bilder auch eindeutig ins Haupt-Checkout übernehmen.
  Tests unter `test/outshine/{include,src,integration/places}`. Telemetrie nach Diagnosebedarf.
- Ich recherchiere normalerweise lokal; beauftragte Anbieter-/API-Recherche und Webcam-Suche
  sowie beauftragte Technik-/Bibliotheksrecherche erlauben Websuche. Werkzeuge aktuell halten,
  nach Updates Orakelherkunft prüfen.
  Ich berichte knapp auf Deutsch: Ergebnis, Commit, Belege und offene Qualitätslücke.
