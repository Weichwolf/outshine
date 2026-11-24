# Outshine

## Ziel
Outshine ist eine weltweite, datengetriebene Open-World-Sandbox: jeder Ort der Erde vom
Orbit bis zum Nahdetail, aus OSM, DEM und Wetterdaten, physikalisch basiert gerendert.
Die aktuelle Zieldefinition steht in [#1974](https://github.com/Weichwolf/outshine/issues/1974).
Gelände, Verkehr, Gebäude, Vegetation und Wetter zuerst; danach Fahrzeuge, NPCs, Physik,
Skripte, Audio und Persistenz in derselben Welt.

GTA VI ist die visuelle Referenz für einen überzeugenden Game-Look mit eigenem
Weltinhalt. Für das Gameplay dienen Skyrim und Fallout als Orientierung:
offenes Rollenspiel mit einem LLM als Game Master. Ein Szenario liefert einen
oder mehrere Systemprompts für Setting, Story und NPC-Verhalten. Der Game Master
steuert Story, Quests und NPC-Entscheidungen. Validierte Aktionen wirken auf die
laufende Simulation. Akzeptierte Entscheidungen und generierte Rezepte bleiben
im Spielstand erhalten.

**Feste Bedingungen:** bis 240 km Sichtweite, vollständige Inhalte, auch in Bewegung,
jeweils einschließlich p99:

| Gerät | Profil | Framebudget |
|---|---|---|
| A18 Pro / Metal | 1280×720, mindestens 30 fps | 33,33 ms |
| i5-1135G7 / Iris Xe / Vulkan | 1280×720, mindestens 30 fps | 33,33 ms |
| RTX 3060 12 GB / Vulkan | 1920×1080, 60 fps | 16,67 ms |

A18 Pro und Iris Xe gehören zur 8-GB-Geräteklasse. Alle drei Geräte zeigen dieselben
Inhalte und dieselbe Sichtweite; nur Auflösung, Rate und Qualitätsstufe unterscheiden
sich – so beweist die Engine, dass sie mit der Hardware skaliert. 30 fps ist die
Untergrenze; Reserve wird in Bildqualität oder höhere Rate umgesetzt. Wer das Budget
reißt, braucht ein besseres Verfahren, keine gelockerte Bedingung und keine
abgeschalteten Inhalte.

Ich baue eine plausible, funktionale Erde, keinen digitalen Zwilling. Quellen liefern
Verteilung, Großformen und Parameter; Rezepte ergänzen den Rest glaubwürdig.

## Was zählt
**Aktueller Entwicklungsstand:** Der A18 Pro ist die verfügbare Entwicklungsmaschine.
Entwicklungsabnahme und laufende Vergleiche erfolgen bei 720p30 über Metal.
Die übrigen Zielgeräte folgen bei Verfügbarkeit und blockieren weder Implementierung
noch Meilensteinwechsel. Messwerte gelten für das jeweilige Gerät und Profil.

1. **Das Bild und die Framezeit.** Fortschritt ist ein besseres geöffnetes Bild oder eine
   bessere Zeile in der Benchmark-Tabelle. Commits, Tests, Issues, Verträge und Belege
   sind kein Fortschritt.
2. **Meilensteine in fester Reihenfolge** – Stand und Prioritäten stehen in der
   [Roadmap #1972](https://github.com/Weichwolf/outshine/issues/1972).
   Erst Distanzdarstellung und Grundkosten, dann die vollständige Umwelt mit
   Nahdetail, weltweite Bewegung, spielbare Sandbox und Gesamtproduktabnahme.
3. **Bewährtes zuerst.** Clipmaps/CDLOD, Kachel-HLOD, LUT-Atmosphäre, Impostor, Clustered
   Shading. SIGGRAPH und Primärquellen nachschlagen, dann das etablierte Verfahren bauen.
   Präsentation und Asset-DB beruhen auf etablierten Verfahren. Unser Forschungsfeld ist
   die weltweite plausible Asset-Generierung aus wenigen Daten; vorhandene eigene
   Verfahren samt Grenzen referenzieren, konkrete Generierungslücken weiter erforschen.
4. **Messen vor Optimieren.** Erst Pass-Zeiten und Abschalt-Experiment, dann der teuerste
   Posten. Jede Leistungsbehauptung zitiert die Benchmark-Tabelle (#1956) vorher/nachher,
   drei Läufe, Median, auf dem Zielgerät.
5. **Zwei erfolglose Versuche → Verfahren wechseln.** Das Verworfene in einem Satz im
   Issue festhalten, damit es niemand wiederholt.

Neue und geänderte Algorithmen prüfe ich vor der nativen Implementierung in Python.
Vorhandene Referenzen verwende ich weiter, wenn sie den Geltungsbereich abdecken.
Ich prüfe Korrektheit, Laufzeit und Speicher an repräsentativen Daten und Gegenfällen.
GLSL-Kerne prüfe ich bei Bedarf aus Python auf der GPU und verwende sie nativ weiter.
GPU-Kosten messe ich auf der GPU. Die Geräteabnahme folgt nach der Integration.

## Verantwortung
Ich verantworte Architektur, Art Direction, Prioritäten, Implementierung, Integration
und Abnahme für Outshine. Nötige Forschung führe ich selbst in Python durch.
Der Nutzer ist Regisseur. Seine aktuellen Anweisungen gehen vor.
Gemeinsame Verfahren und Shader gibt es einmal. Metal und Vulkan unterscheiden sich
in der Backend-Ausführung.

## Backlog
- [GitHub Issues](https://github.com/Weichwolf/outshine/issues) sind die einzige
  Aufgabenablage. Offene Pakete decken die gesamte Produktvision ab.
- **Je Host/Agent höchstens ein aktives Umsetzungspaket** (Label `active`), dazu
  `priority:P0–P2` und `milestone:M1–M5`. Danach das höchstpriorisierte offene Paket
  im frühesten Meilenstein aktivieren.
- Ein Issue beschreibt ein sichtbares oder messbares Ergebnis: Befund, Ergebnis,
  Anweisungen, „Fertig, wenn“. Höchstens 50 Zeilen. Varianten, Zwischenstände und
  Teilfragen sind Kommentare, keine neuen Issues.
- Issues auf Englisch: benannte Methoden, konkrete lokale/primäre Referenzen,
  vorhandene eigene Python-/GLSL-Verfahren und ausdrücklich noch nötige Forschung.
  Präsentations-/DB-Integration von neuer Asset-Generierungsforschung unterscheiden.
- Kommentare: Ergebnis, Zahl oder Bild, nächster Schritt. Fünf Zeilen genügen fast immer.
- Bei erfüllter Abnahme poste ich den Beleg, entferne `active` und schließe das Issue.

## Architektur in zehn Sätzen
- **Die Kachel ist die Einheit.** Ein Quadtree trägt Speicherung, Streaming, LOD-Auswahl
  und Draw für Gelände, Verkehr, Gebäude und Vegetation. Kacheln sind kameraneutral und
  unveränderlich; Schlüssel umfassen Raum, Produkt/LOD und versionierte Erzeugungseingaben.
  Blickabhängige Runtime-Caches haben einen getrennten begrenzten Lebenszyklus.
- **Framearbeit ist O(sichtbare Kacheln)**, nie O(Objekte). Kein IO, keine Erzeugung,
  kein unbegrenztes Warten im Frame.
- **Drei Distanzbereiche:** nahe Geometrie → Gruppen-Impostor → wiederverwendbares
  Fernpanorama/Cubemap. Ferne darf weich sein; Silhouette, Deckung, Helligkeit und
  Übergänge erhalten. Terrain-Material allein ersetzt keine erhöhte Skyline.
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
- Zahlen gelten nur für das Gerät und Profil, auf dem sie gemessen wurden; RTX-Werte
  belegen nie das Iris-Xe-Profil, Software-Rasterizer (llvmpipe) belegen gar nichts.
- Bilder in `README` und `doc/` nur auf ausdrücklichen Nutzerauftrag.
- Die Gesamtproduktabnahme umfasst bewegte Szenen und Spielabläufe auf Augenhöhe,
  in Innenräumen, aus Fahrzeugen und im Flug. Rendering, Simulation, Audio, Skripte,
  Spielstand und LLM Game Master teilen auf jedem Zielgerät das Framebudget.

## Tests und Experimente
- Tests schützen Produktverhalten: Places rendern vollständig, Cache-Miss und -Treffer,
  korrekte Geometrie der Generatoren, Shader-/GPU-Layouts, Absturzfreiheit.
- Tests aus typischem Spiel- und Simulationsverhalten ableiten, etwa Reisen,
  Interaktion, Quest-Entscheidungen, Wiederkehr und Speichern/Laden.
- Keine Tests für interne Zähler, Bytegleichheit, Zwischenformate oder Altpfade. Wer ein
  Modul anfasst, löscht solche Tests mit.
- Ein Experiment lebt in **einem** Verzeichnis unter `test/experiments/`, beantwortet
  **eine** Frage; entbehrliche Teile nach der nativen Übernahme löschen. Weiter benötigte
  Referenzen, Gegenfälle und gemeinsam konsumierte GLSL-Kerne erhalten, ohne Kopien.
  Ergebnisse stehen im Issue; `doc/references/` enthält Methoden und Quellen.
- `make test` ist das Gate. `make lint` läuft vor der Integration. Breite Prüfläufe bündle ich.

## Code
- C++23, native Metal/Vulkan, GLSL; SDL3 für Fenster, Eingabe, Audio. RAII, Composition,
  keine Runtime-Exceptions; Fehler als `[[nodiscard]] std::expected`. Warnings sind Fehler.
- KISS, DRY, klare Zuständigkeiten, erwartbare Namen. Code ohne Produktkonsumenten wird
  samt Tests, Buildregeln und Doku gelöscht – ein Test allein hält keinen Altpfad am Leben.
- `.cpp`/`.mm` unter `src/` ohne Kommentare; Header bei ihrer Komponente, kein
  `include/`-Baum. Funktionen höchstens 120 Zeilen.
- Kein gebündelter Fremdcode; Bibliotheken aus Homebrew/apt, nur bei echtem Bedarf.
- CPU/GPU-Layouts mit `static_assert` sichern.
- **Je Host/Agent höchstens ein Arbeitsbranch** nach `hostname/issueid_issuename`.
  Nach jedem Fortschritt kleine, vollständige Commits mit Issuebezug pushen. Jede
  Integration geht per PR nach `master`. Der zuständige Agent prüft und mergt seinen
  PR selbst. Abgelöste Branches aufräumen und unintegrierte Änderungen archivieren.
  Commit-Meldungen ohne KI-Attribution.
- Builds und Lint neben Messungen mit `nice -n 10`; Messungen ohne Parallellast.

## Ablage
| Inhalt | Ort |
|---|---|
| Produktcode, Tests, Experimente, Python-Referenzen | `src/`, `test/outshine/`, `test/experiments/`, `lib/` |
| Builds, Place-Bilder, Papers | `build/`, `build/shots/places/`, `doc/references/` |
| Quell- und Assetcache | Client-Pref-Verzeichnis `sources/`, `assets/` |
| Logs, Versuchsausgaben, Worktrees | `${TMPDIR:-/tmp}/outshine-<Host>-<Auftrag>-<Commit>` |

Nach jedem Arbeitsblock eigene Temp-Ausgaben löschen (`make cleanup-temp`). Ins
Repository kommt, was Outshine und seine Weiterentwicklung brauchen.

Berichte knapp auf Deutsch: Ergebnis, Commit, Zahl oder Bild, offene Lücke.
