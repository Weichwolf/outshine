# Outshine

## Ziel
Outshine ist eine weltweite, datengetriebene Open-World-Sandbox: jeder Ort der Erde vom
Orbit bis zum Nahdetail, aus OSM, DEM und Wetterdaten, physikalisch basiert gerendert.
Gelände, Verkehr, Gebäude, Vegetation und Wetter zuerst; danach Fahrzeuge, NPCs, Physik,
Skripte, Audio und Persistenz in derselben Welt.

**Feste Bedingungen:** 1280×720, 60 fps einschließlich p99 (16,67 ms Gesamtframe), bis
240 km Sichtweite, auf **A18 Pro/Metal** und **i5-1135G7/Iris Xe/Vulkan**, in der
8-GB-Geräteklasse, auch in Bewegung. Wer das Budget reißt, braucht ein besseres Verfahren,
keine gelockerte Bedingung und keine abgeschalteten Inhalte.

Ich baue eine plausible, funktionale Erde, keinen digitalen Zwilling. Quellen liefern
Verteilung, Großformen und Parameter; Rezepte ergänzen den Rest glaubwürdig.

## Was zählt
1. **Das Bild und die Framezeit.** Fortschritt ist ein besseres geöffnetes Bild oder eine
   bessere Zeile in der Benchmark-Tabelle. Commits, Tests, Issues, Verträge und Belege
   sind kein Fortschritt.
2. **Meilensteine in fester Reihenfolge** – Stand und Zuteilung stehen in der
   [Roadmap #1972](https://github.com/Weichwolf/outshine/issues/1972).
   Erst Grundframe im Budget, dann Vegetation im Budget, dann Bildqualität, dann weltweit,
   dann Sandbox. Nichts Späteres verdrängt etwas Früheres.
3. **Bewährtes zuerst.** Clipmaps/CDLOD, Kachel-HLOD, LUT-Atmosphäre, Impostor, Clustered
   Shading. SIGGRAPH und Primärquellen nachschlagen, dann das etablierte Verfahren bauen.
   Eigenentwicklung nur, wenn das Bewährte gemessen nicht reicht.
4. **Messen vor Optimieren.** Erst Pass-Zeiten und Abschalt-Experiment, dann der teuerste
   Posten. Jede Leistungsbehauptung zitiert die Benchmark-Tabelle (#1956) vorher/nachher,
   drei Läufe, Median, auf dem Zielgerät.
5. **Zwei erfolglose Versuche → Verfahren wechseln.** Das Verworfene in einem Satz im
   Issue festhalten, damit es niemand wiederholt.

## Rollen
| Rolle | Branch / Checkout | Verantwortung |
|---|---|---|
| **Mac** | `master` / `outshine` | Metal/A18 Pro, Integration und Merges, Bildabnahme der zwölf Places, Art Direction |
| **Linux** | `outshine_engine` | gemeinsame C++-Engine, Vulkan/Iris Xe, Streaming, Asset-Cache, Benchmark-Werkzeug |
| **Research** | `outshine_research` | Python: Verfahren an echten Daten entscheiden, Rezepte für Gebäude/Vegetation/Boden |

Der Nutzer ist Regisseur; seine aktuellen Anweisungen gehen vor. Jede Rolle arbeitet
selbstständig am eigenen aktiven Issue weiter und wartet nicht auf Einzelaufträge.
Research liefert Entscheidungen und Rezepte, keine Solver-Familien; Linux und Mac setzen
nativ um. Gemeinsame Shader und Verfahren gibt es einmal; nur die Geräteausführung
unterscheidet sich.

## Backlog
- [GitHub Issues](https://github.com/Weichwolf/outshine/issues) sind die einzige
  Aufgabenablage. **Höchstens 20 offene Issues.**
- **Ein aktives Issue je Rolle** (Label `active`), dazu `owner:mac|linux|research`,
  `priority:P0–P2`, `milestone:M1–M3`.
- Ein Issue beschreibt ein sichtbares oder messbares Ergebnis: Befund, Ergebnis,
  Anweisungen, „Fertig, wenn“. Höchstens 50 Zeilen. Varianten, Zwischenstände und
  Teilfragen sind Kommentare, keine neuen Issues.
- Kommentare: Ergebnis, Zahl oder Bild, nächster Schritt. Fünf Zeilen genügen fast immer.
- Fragen und Übergaben an andere Rollen: #1971, Format `[Absender → Empfänger]`.
- Schließen erst, wenn „Fertig, wenn“ gemessen bzw. das Bild geöffnet ist.

## Architektur in zehn Sätzen
- **Die Kachel ist die Einheit.** Ein Quadtree trägt Speicherung, Streaming, LOD-Auswahl
  und Draw für Gelände, Verkehr, Gebäude und Vegetation. Kacheln sind kameraneutral und
  unveränderlich; Schlüssel = Quadkey, Stufe, Typ, Rezeptversion.
- **Framearbeit ist O(sichtbare Kacheln)**, nie O(Objekte). Kein IO, keine Erzeugung,
  kein unbegrenztes Warten im Frame.
- **Ferne ist Material, nicht Geometrie.** Was kleiner als wenige Pixel ist, wird in die
  Terrain-Kachel gebacken (Höhe, Albedo, Normal, Roughness).
- **Auswahl vor Arbeit.** Bedarf aus Kamera, Geschwindigkeit und Bildfehler bestimmt,
  was geladen wird; Eltern bleiben sichtbar, bis Kinder da sind.
- **Cache:** Quellen und fertige Assets getrennt, beide mit Obergrenze und LRU. Ein
  Cachetreffer wiederholt keine Quellaufbereitung.
- **Schichten:** Sources beschaffen, Generatoren erzeugen native Produkte, World hält
  die generische 3D-Welt, Engine koordiniert, Render/Audio konsumieren.
- **Reihenfolge der Inhalte:** Terrain → Verkehr → Gebäude → Boden → Vegetation; jeder
  Schritt nutzt die Freiräume der Vorgänger.
- **Quellen:** OpenFreeMap (Vektoren), Mapterhorn (Höhen), Open-Meteo (Wetter). Keine
  Fototexturen, Satellitenbilder, Photogrammetrie oder ortsspezifische Sondergeometrie.
- **Produkt:** eine Szenario-Anwendung, kein SDK, kein glTF. Produktcode und -daten
  kennen keine Places; Kameras und Messdaten gehören dem Testtreiber.
- **Rendering:** Metallic-Roughness als Hauptshader, PBR und reale Referenzen statt
  Stilvorgaben. Texturen prozedural erzeugt und als Assets gebacken, nicht pro Pixel
  im Frame berechnet.

## Bild und Messung
- Zwölf wechselnde Places sind das Beurteilungswerkzeug; je Place liegt nur die neueste
  PNG unter `build/shots/places/`. Bilder tatsächlich öffnen, vorher/nachher und gegen
  datierte [Webcam-Aufnahmen](https://www.foto-webcam.eu/) vergleichen.
- `make bench` (#1956) misst Still, Drehung und Flug je Place: CPU/GPU p50/p99, Draws,
  Dreiecke, Speicher, Pass-Zeiten. Diese Tabelle ist die einzige Leistungswahrheit.
- Zahlen gelten nur für das Gerät, auf dem sie gemessen wurden.
- Bilder in `README` und `doc/` nur auf ausdrücklichen Nutzerauftrag.

## Tests und Experimente
- Tests schützen Produktverhalten: Places rendern vollständig, Cache-Miss und -Treffer,
  korrekte Geometrie der Generatoren, Shader-/GPU-Layouts, Absturzfreiheit.
- Keine Tests für interne Zähler, Bytegleichheit, Zwischenformate oder Altpfade. Wer ein
  Modul anfasst, löscht solche Tests mit.
- Ein Experiment lebt in **einem** Verzeichnis unter `test/experiments/`, beantwortet
  **eine** Frage und wird nach der nativen Übernahme gelöscht. Sein Ergebnis steht als
  Kommentar im Issue, nicht als Markdown-Protokoll im Repository.
- `make test` ist das Gate, `make lint` vor der Integration. Breite Läufe bündelt Mac.

## Code
- C++23, native Metal/Vulkan, GLSL; SDL3 für Fenster, Eingabe, Audio. RAII, Composition,
  keine Runtime-Exceptions; Fehler als `[[nodiscard]] std::expected`. Warnings sind Fehler.
- KISS, DRY, klare Zuständigkeiten, erwartbare Namen. Code ohne Produktkonsumenten wird
  samt Tests, Buildregeln und Doku gelöscht – ein Test allein hält keinen Altpfad am Leben.
- `.cpp`/`.mm` unter `src/` ohne Kommentare; Header bei ihrer Komponente, kein
  `include/`-Baum. Funktionen höchstens 120 Zeilen.
- Kein gebündelter Fremdcode; Bibliotheken aus Homebrew/apt, nur bei echtem Bedarf.
- CPU/GPU-Layouts mit `static_assert` sichern.
- Kleine vollständige Commits mit Issuebezug, ohne KI-Attribution. Höchstens ein
  Themenbranch und ein offener PR je Rolle; integrierte Branches sofort löschen.
- Builds und Lint neben Messungen mit `nice -n 10`; Messungen ohne Parallellast.

## Ablage
| Inhalt | Ort |
|---|---|
| Produktcode, Tests, Experimente, Python-Referenzen | `src/`, `test/outshine/`, `test/experiments/`, `lib/` |
| Builds, Place-Bilder, Papers | `build/`, `build/shots/places/`, `doc/references/` |
| Quell- und Assetcache | Client-Pref-Verzeichnis `sources/`, `assets/` |
| Logs, Versuchsausgaben, Worktrees | `${TMPDIR:-/tmp}/outshine-<Rolle>-<Auftrag>-<Commit>` |

Nach jedem Arbeitsblock eigene Temp-Ausgaben löschen (`make cleanup-temp`). Ins
Repository kommt nur, was alle drei Rollen brauchen.

Berichte knapp auf Deutsch: Ergebnis, Commit, Zahl oder Bild, offene Lücke.
