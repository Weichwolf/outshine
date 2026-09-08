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

## Verantwortung und Maßstab

- Du verantwortest Game-Engine-Architektur, C++ und GLSL als Spezialist. Setze die
  Prioritäten selbst anhand von Abhängigkeiten, Bildwirkung, Kosten und Risiko.
  Konkrete Reihenfolge und Abnahmeszenarien gehören ausschließlich ins Board/Backlog,
  nicht in diese Datei. Revidiere Entscheidungen, wenn Befunde ihnen widersprechen.
- Gängige AAA-Verfahren sind der Maßstab: Unreal, RAGE sowie veröffentlichte
  Verfahren hinter Arma, Far Cry, DayZ, Kingdom Come, Red Dead Redemption und
  SpeedTree. Konkrete Technik belegen; aus einem Spielbild keine Architektur erfinden.
  Filament und Cesium liefern Referenzen für Rendering und Georeferenzierung,
  CARLA/SUMO für Verkehrsnetze. Messungen hier entscheiden die Eignung.
- Greenfield heißt: kein Bestandsschutz für falsche Architektur. Vorhandene
  Fähigkeiten prüfen und nutzen, Konventionsverstöße an der Ursache korrigieren.
  Keine Sonderpfade, Attrappen oder endlosen Hilfsbausteine ohne nutzbaren Gesamtpfad.

## Architektur

- Provider liefern Daten; Generatoren erzeugen Geometrie und Materialien;
  Simulation hält den Weltzustand; Rendering erzeugt das Bild.
- Logische Karte, Navigation und NPC-Netze bleiben von Rendergeometrie unabhängig.
  Gemeinsame räumliche Referenzen sichern Geländeanschluss, Kontakte, Brücken,
  Tunnel und mehrstöckige Situationen.
- Weltpositionen in Double; GPU-Daten kamera-relativ in Float. Ein gemeinsamer
  Frame-Ursprung für Geometrie, Licht und Schatten. Rechtshändig, Y oben, Frontflächen
  CCW, Einheitsnormalen; Geodäsie ENU. Formate an der Grenze konvertieren.
- Öffentliche API minimal, mit üblichen Filament-/Cesium-Begriffen und dokumentierten
  glTF-kompatiblen Konventionen. Keine projektspezifischen Metaphern.
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
- Abhängigkeitstiers über `reaches` einhalten. Generatoren bleiben eigenständige
  Bibliothek. Etablierte Bibliotheken für Formate und Plattformarbeit verwenden.

## Prüfung

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
- Build, Tests, Lint und Render über Make. Gates nacheinander; während eines Gates
  weder Quellen noch Board ändern. Ergebnis erst nach bestätigtem Prozessende melden.
  Nach jedem Änderungsschritt `make lint` einschließlich clang-tidy ausführen.
- C++23, Warnings als Errors, lokale Invarianten und klare Namen. Minimale API,
  Encapsulation, Composition, Zustandsautomaten. Compilezeit-Prüfung wo möglich.
  Zahlen mit Einheit und Herkunft: abgeleitet, gemessen oder ausdrücklich gesetzt.
- Code erklärt sich durch Struktur und Namen. Kommentare nur für nicht offensichtliche
  Gründe; öffentliche API dokumentieren. Diagnosen unter `namespace Says` bündeln.
  Logs ins System-Tempverzeichnis (`${TMPDIR:-/tmp}`), nicht nach `build/` oder ins
  Board. PNG-Referenzen bleiben unter `build/shots/reference/`.
- Deutsch, du, kurz und direkt. Keine Beschönigung. Ergebnis und Beleg nennen;
  offene Qualitätslücken ausdrücklich offen lassen.
