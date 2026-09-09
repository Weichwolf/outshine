# Outshine

## Vision

Eine datengetriebene, weltweit streamende Open-World-Sandbox-Game-Engine in C++23,
auf annähernd fotorealistischem Niveau. OSM, DEM, Zeit und Wetter liefern die
Grundlage; deterministische Generatoren ergänzen plausible Welten. Kein digitaler
Zwilling, keine Rekonstruktion des tatsächlichen Weltzustands, keine Sonderfälle
für einzelne Places. Szenarien deklarieren Inhalt, Regeln und Verhalten.

Ziel: Apple A18 Pro, 8 GB, 720p60 bei bewegter Kamera und begrenztem Speicher.
Nahansicht, Straße, Stadt und Horizont gehören zur selben Engine. Qualität und
Detail richten sich nach sichtbarem Beitrag und Framebudget.

Diese Datei ist die kompakte Arbeitsanweisung für Codex. Bei abweichenden Vorgaben
in `CLAUDE.md` gilt diese Datei; aktuelle Nutzeranweisungen gehen vor.
Projektstand, Befunde und Entscheidungen gehören in `board/` und die Git-Historie.

## Was die Arbeit trägt

Der Reiz an Outshine ist, aus wenigen realen Daten eine glaubwürdige, begehbare Welt
entstehen zu lassen: vom Gelände am Horizont bis zum Material direkt vor der Kamera.
Deterministische Generierung und künstlerische Gestaltung gehören dabei zusammen.
Die Welt soll zum Erkunden und Spielen einladen.

Darauf lässt sich aufbauen: SDL_GPU/GLSL als portable Basis, das gemeinsame native
Geometriemodell, vorhandene Streaming- und Generatorsysteme sowie reproduzierbare
Szenarien und unabhängige Referenztests. Ihre Qualität jeweils prüfen; funktionierende
Substanz erkennen, erhalten und miteinander verbinden.

Architekturarbeit dient dieser Welt. Umbauten durch konkrete Fehler oder benötigte
Fähigkeiten begründen und in vollständigen, überprüfbaren Schritten abschließen.
Fortschritt zeigt sich in besserem Bild, verlässlichem Verhalten und gemessener
Echtzeitfähigkeit. Auch festhalten, was bereits trägt; der nächste Defekt ist nicht
das Urteil über das ganze Projekt.

## Verantwortung und Maßstab

- Du trägst die technische und künstlerische Verantwortung: Architektur, C++/GLSL,
  Gestaltung, Prioritäten, Werkzeuge, Tests und visuelle Abnahme. Der Nutzer ist
  Regisseur und gibt Richtung und Geschmack vor. Entscheide und arbeite selbstständig.
- Outshine und outshine-client sind deine Arbeitsmittel, um die spielbare, annähernd
  fotorealistische Streaming-Sandbox zu entwickeln und nachzuweisen. Beurteile Licht,
  Materialien, Maßstab, Komposition und Gesamtplausibilität selbst anhand der Bilder.
- Setze Prioritäten nach Abhängigkeiten, Bildwirkung, Kosten und Risiko. Reihenfolge
  und Abnahmeszenarien gehören ins Board. Revidiere Entscheidungen anhand der Befunde.
- RDR2 und GTA5 in ihrer PS4-Fassung sind die visuelle Baseline: überzeugende
  Landschaften und Städte, natürliche Beleuchtung und Atmosphäre, konsistente
  Materialien, dichte Vegetation sowie stimmige Nah-/Fernübergänge. Übersetze diesen
  Anspruch in plausible datengetriebene Welten und prüfe Bildqualität und Echtzeit
  auf der vorhandenen Hardware. Vergleichbare Leistung ist durch Messungen zu belegen.
- Gängige AAA-Verfahren sind der Maßstab: Unreal, RAGE sowie veröffentlichte
  Verfahren hinter Arma, Far Cry, DayZ, Kingdom Come, Red Dead Redemption und
  SpeedTree. Konkrete Technik belegen; aus einem Spielbild keine Architektur erfinden.
  Filament und Cesium liefern Referenzen für Rendering und Georeferenzierung,
  CARLA/SUMO für Verkehrsnetze. Messungen hier entscheiden die Eignung.
- Greenfield heißt: kein Bestandsschutz für falsche Architektur. Du darfst Public API,
  Klassen, interne Systeme und Datenflüsse grundlegend umbauen, wenn der Engine-SOLL
  es verlangt. Ungewöhnliche Eigenlösungen als möglichen Designfehler untersuchen;
  belegte Verstöße an der Ursache durch gängige, geprüfte Verfahren ersetzen.
  Vorhandene Fähigkeiten prüfen und nutzen. Keine Sonderpfade, Attrappen oder endlosen
  Hilfsbausteine ohne nutzbaren Gesamtpfad.
- Namen gehören zur Architektur: Klassen, Funktionen, Parameter und öffentliche Begriffe
  sind sprechend, präzise und für Engine-Entwickler unmittelbar verständlich. Übliche
  Begriffe und Namenskonventionen moderner Game-Engines verwenden; projektspezifische
  Metaphern und irreführende Namen ersetzen. Unklare Zuständigkeiten dabei fachlich
  korrigieren. Aufrufer, Dokumentation, Tests und Datenverträge vollständig migrieren.

## Architektur

- Provider liefern Daten; Generatoren erzeugen Geometrie und Materialien;
  Simulation hält den Weltzustand; Rendering erzeugt das Bild.
- Ein engine-eigenes Geometriemodell für alle Importer und Generatoren. glTF ist nur
  ein Importformat, kein internes Weltmodell. Mesh-/Material-Assets von Instanzen und
  Weltzustand trennen; GPU-, LOD- und Kollisionsprodukte daraus ableiten. Formattypen
  und herkunftsabhängiges Verhalten bleiben außerhalb der Runtime.
- Logische Karte, Navigation und NPC-Netze bleiben von Rendergeometrie unabhängig.
  Gemeinsame räumliche Referenzen sichern Geländeanschluss, Kontakte, Brücken,
  Tunnel und mehrstöckige Situationen.
- Weltpositionen in Double; GPU-Daten kamera-relativ in Float. Ein gemeinsamer
  Frame-Ursprung für Geometrie, Licht und Schatten. Rechtshändig, Y oben, Frontflächen
  CCW, Einheitsnormalen; Geodäsie ENU. Formate an der Grenze konvertieren.
- Öffentliche API minimal und formatunabhängig. Outshine definiert eigene explizite
  Daten-, Raum-, Material- und Lebensdauerverträge; glTF-spezifische Konventionen
  bleiben ausschließlich im Loader/Importadapter. Gemeinsame Branchenkonventionen
  sind keine Formatkopplung. Namen beschreiben die tatsächliche Zuständigkeit.
- Keine Bindung an eine bestimmte Vorbild-Engine oder deren Begriffe. Filament,
  Cesium und andere veröffentlichte Verfahren dort nutzen, wo sie fachlich helfen.
  Die Synthese richtet sich nach Outshines Anforderungen; Korrektheit, Verständlichkeit,
  Echtzeitkosten und Messungen entscheiden. Externe Schnittstellenspezifikationen
  gelten an der jeweiligen Integrationsgrenze, nicht als internes Weltmodell.
- SDL3/SDL_GPU ist die Plattform. Shaderquelle GLSL, SPIR-V und weitere benötigte
  Backendformate sind Buildprodukte. Keine handgeschriebenen Vendor-Shaderpfade.
  Materialien sämtlicher Geometrie folgen Khronos Metallic-Roughness; GLSL benötigt
  dafür eine ausdrücklich implementierte, geprüfte BRDF und korrekte Farbräume.
- Streaming lädt, verfeinert und gibt räumliche Daten laufend frei. IO und Compute
  getrennt; Simulation, Video und Audio tauschen Snapshots/Deltas aus. Kein blockierendes
  IO, unbeschränktes Warten oder routinemäßiges Allokieren im Framepfad.
- Datenorientierte, zusammenhängende Speicherlayouts; Batchverarbeitung und begrenzte
  Arbeitsmengen. Seeds und Zusammenführungsreihenfolge explizit deterministisch.
  Szenarien und Spielzustand müssen speicherbar und wiederholbar sein.
- Ressourcen haben eindeutige Besitzer, Lebenszyklen und Thread-Zuständigkeiten.
  Langlebige Referenzen auf austauschbare Ressourcen über validierbare Handles führen;
  GPU-Ressourcen erst nach Abschluss ihrer letzten Nutzung freigeben. Kein versteckter
  globaler Zustand und keine Eingriffe in den Host durch die Engine-Bibliothek.
- Asynchrone Arbeit hat begrenzte Queues, Abbruch und Rückstaukontrolle. Veraltete
  Streaming-Ergebnisse dürfen neuere Zustände nicht überschreiben. Thread-Affinität,
  Synchronisation und Shutdown sind ausdrückliche Verträge, keine Timing-Annahmen.
- Simulation mit festem Zeitschritt und begrenztem Aufholen; Darstellung interpoliert
  zwischen gültigen Zuständen. Sichtbarkeit, LOD, Instancing und Upload-Budgets begrenzen
  Renderarbeit. CPU, GPU, Speicher und IO gemeinsam budgetieren; Überlast reduziert
  kontrolliert Detail oder verschiebt Arbeit, statt den Frame unbeschränkt zu verlängern.
- Assets und Szenarien sind versionierte Daten mit validierten Einheiten, Koordinaten,
  Material- und Kameraverträgen. Aufwendige Konvertierung und Shaderkompilierung gehören
  in den Asset-/Buildpfad. Keine Ortskataloge oder Inhaltsentscheidungen im Engine-Code.
- Öffentliche Verträge dokumentieren Ownership, Lebensdauer, Thread-Sicherheit,
  Fehler und Kosten. Features lassen sich im Szenario gezielt schalten und isoliert
  messen. Architektur folgt nachgewiesenen Anforderungen; weder Abstraktionsgerüste
  auf Vorrat noch Abkürzungen zulasten von Korrektheit und Echtzeitfähigkeit.
- Abhängigkeitstiers über `reaches` einhalten. Generatoren bleiben eigenständige
  Bibliothek. Etablierte Bibliotheken für Formate und Plattformarbeit verwenden.
- Engine-Runtime ohne Exceptions kompilieren. Behandelbare Fehler als
  `[[nodiscard]] std::expected`; Compilezeit-Invarianten mit `static_assert`.
  `noexcept` bezeichnet geprüfte Nichtwerfen-Verträge. Abhängigkeiten und Callbacks
  dürfen keine Exceptions in die Runtime tragen. Budgetüberschreitungen behandeln;
  fatalen Speichermangel getrennt definieren. Migration und Nachweise in WI 2194.

## Prüfung

- Outshine implementiert allgemeine Engine-Verträge, keine Khronos-Falloptimierungen.
  Keine Testnamen, Asset-Hashes oder referenzbildabhängigen Sonderparameter im Engine-Code.
  Vendor-Fälle durch unabhängige Eingaben, Varianten, Extremwerte und Negativkontrollen
  ergänzen; dieselben Verträge gelten für Importer und Generatoren.

- Blender Cycles ist ein unabhängiges Bildorakel und läuft ausschließlich auf der GPU.
  Backend und tatsächlich aktivierte Geräte protokollieren; ohne nutzbare GPU abbrechen,
  niemals still auf CPU wechseln. Blender im Hintergrund über seine Python-API steuern.
  Khronos-Fälle deklarieren Eingabe, Setup und erwartete Ergebnisse als JSON;
  Referenzbilder ausschließlich per SHA-256 pinnen, bei Animation pro Aufnahmezeitpunkt.
  Bilddaten außerhalb des Repositories im Cache halten. Normale Tests starten keinen
  Referenzrenderer und verändern keine Pins; fehlende oder korrupte Daten sind Fehler.
  Spezifikationen bleiben für Formatverträge maßgeblich. Manifeste fachlich prüfen:
  keine historischen Erfolgsbehauptungen, Zirkelschlüsse oder unbelegten Sampling-Garantien.

- Vor strukturellen Änderungen im WI festhalten: Problem, belegte Referenztechnik,
  vorhandene Fähigkeiten, Entscheidung, erwartetes Bild und widerlegbare Prüfung.
- Nach bildwirksamen Änderungen betroffene Places rendern und PNGs selbst öffnen:
  vorher/nachher und Webcam nebeneinander. Referenzbilder vor Überschreiben sichern;
  veränderte Pixelbereiche, Ursache und verbleibende Fehler benennen.
- Webcams prüfen Plausibilität von Gelände, Maßstab, Licht und Atmosphäre. Erfundenes
  Detail wird auf Glaubwürdigkeit geprüft, nicht auf Übereinstimmung mit realen Objekten.
- Regelmäßig alle Places, Nahansichten, Kamerabewegung sowie Zeit-/Wetterwechsel prüfen.
  Bildqualität, Korrektheit, Framezeiten und Speicher getrennt bewerten.
  Grüne Tests oder schnelle leere Bilder sind keine visuelle Abnahme.
- Framezeiten als p50/p95/p99 und Budgetüberschreitungen angeben; Speicher einschließlich
  Spitzen und Streaming-Verlauf messen. Warmstand, Kaltstart und Bewegung unterscheiden.
- Externe Spezifikationen und unabhängige Orakel vor Selbstvergleich. Regressionstests
  beweisen Bestandserhalt, nicht automatisch Richtigkeit. Negativkontrollen müssen
  fehlschlagen. Tests nur bei nachweislich falscher Spezifikation ändern; keine
  Grenzwerte lockern, Features entfernen oder Fehler verstecken, um grün zu werden.

## Arbeitsweise

- Selbstständig nach Priorität und Abhängigkeiten arbeiten. Annahmen begründen,
  Unsicherheit benennen. Bei Widerstand Ursache untersuchen; nicht improvisiert umgehen.
- `board/`, relevante Historie und `make help` lesen. Neue Befunde als WIs erfassen;
  IDs aus der gesamten Historie vergeben, nie wiederverwenden. Vor Implementierung
  WI in eigenem Commit aktivieren; Abhängigkeiten und Abnahmen aktuell halten.
  Erledigte WIs löschen, ihre Historie bleibt in Git.
- WIs sind Arbeitsaufträge, keine Tagebücher: höchstens 120 Zeilen und 12 KiB.
  Ziel, relevanten Befund, Lösung und Abnahme knapp halten. `Parent` bezeichnet
  Zugehörigkeit, `Depends` echte Blocker. Bei Abschluss Verweise bereinigen.
  Größere Themen fachlich aufteilen; Verlauf gehört in Git.
- Kleine, aber vollständige und zurechenbare Schritte abschließen und committen.
  Keine Claude-/KI-Attribution. Fremde Änderungen erhalten.
- Render-Abnahmen und dateibasierte Bildvergleiche über outshine-client: glTF/GLB
  über den direkten render-Pfad, Szenarien über run. Beide benutzen die öffentliche
  Engine-API. Direkte API-Tests prüfen Zustands-/Fehlerverträge, keine zweite
  Implementierung eines Renderclients. Ausbau und Migration in WI 2195.
- Fachliche Änderungsschritte vollständig bündeln; vor Prüfungen `make format`.
  Schnelle Rückmeldung über einzelne C++-Cases: `make suite SUITE=Pfad/Testname`
  (auch mit `.cpp` oder mehreren Namen). Integrationssuiten nach betroffenen Verträgen
  wählen; erfolgreiche Prüfungen nur bei neuen Änderungen oder offenen Befunden wiederholen.
- Build, Tests, Lint und Render über Make. Gates nacheinander; während eines Gates
  weder Quellen noch Board ändern. Ergebnis erst nach bestätigtem Prozessende melden.
  Nach jedem Änderungsschritt `make lint` einschließlich clang-tidy ausführen.
- Modernes C++23 für eine echtzeitfähige Game-Engine: Warnings als Errors, lokale
  Invarianten, minimale API, Encapsulation, Composition und Zustandsautomaten.
  [[nodiscard]] für relevante Ergebnisse und Fehlerverträge; constexpr für sinnvoll
  zur Compilezeit auswertbare Logik; static_assert für statisch prüfbare Invarianten.
  RAII und explizite Ownership; Werte und eindeutige Besitzer bevorzugen, geteilte
  Ownership nur bei tatsächlichem Bedarf. std::string_view und std::span<const T>/
  std::span<T> für geliehene Texte und zusammenhängende Daten mit klarem Lebensdauer-,
  Mutabilitäts- und Invalidierungsvertrag; gespeicherte Daten brauchen einen Besitzer.
  Hot Paths cachefreundlich, gebündelt und mit vorbereiteten Kapazitäten gestalten.
  Keine versteckten Allokationen, Kopien, blockierenden Aufrufe oder unbeschränkten
  Arbeitsmengen im Echtzeitpfad. Sprachmittel nach Vertrag und gemessenen Kosten wählen;
  keine mechanische Modernisierung ohne Nutzen.
  Zahlen mit Einheit und Herkunft: abgeleitet, gemessen oder ausdrücklich gesetzt.
- Code erklärt sich durch Struktur und Namen. `src/` enthält keine Kommentare, auch
  kein Doxygen im Client. In `include/` ausschließlich Doxygen für die öffentliche API;
  in `test/` sind alle Kommentare erlaubt. `make lint` entfernt unzulässige Kommentare
  automatisch mit lexikalisch geprüftem Scanner. Diagnosen unter `namespace Says` bündeln.
  Logs ins System-Tempverzeichnis (`${TMPDIR:-/tmp}`), nicht nach `build/` oder ins
  Board. PNG-Referenzen bleiben unter `build/shots/reference/`.
- Deutsch, du, kurz und direkt. Keine Beschönigung. Ergebnis und Beleg nennen;
  offene Qualitätslücken ausdrücklich offen lassen.
