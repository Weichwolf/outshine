# Outshine

> **Eine Game Engine, die auf PS4- und A18-Pro-Hardware in 720p60 läuft, mit der Technologie von Days
> Gone und Horizon Forbidden West, und in der sich Spiele wie Witcher 3, Fallout 4 und GTA 5 optisch,
> inhaltlich und funktionell rein über deklarative `mods/` abbilden lassen. Grundlage der prozeduralen
> Welt sind OSM-, Höhen-, Wetter- und Sternendaten vom Kachelserver. Durch LLM-Integration werden alle
> Entitäten intelligent und die Spielwelt dynamisch.**

Eine Engine, **auf eine Maschine zugeschnitten** — nicht auf Entwickler und Artists. Szenario rein,
spielbares Spiel raus. Vier Bauentscheidungen: die Welt wird **aus OSM geladen** statt modelliert ·
**ein** Physiksystem trägt Laufen, Fahren, Fliegen, Schwimmen · ein **Epochen- und Verfallsregler**
kleidet dieselbe Geometrie ein · die Akteure **denken**. [`doc/vision.md`](doc/vision.md)

**OSM ist die Welt, keine Datenquelle unter mehreren.** Gelände, Landbedeckung, Bauwerke, Infrastruktur
und Vegetationsverteilung kommen aus denselben Vektoren — deshalb sind das gezeichnete und das
klassifizierte Ding **per Konstruktion dieselbe Linie**.

## Haltung

**Die Messlatte ist eine World Sandbox auf Unreal-Niveau, allein aus dem, was der Kachelserver liefert**
— sie gilt für alles Sichtbare. Das laufende Ziel darunter steht in [`doc/goal.md`](doc/goal.md), bindend
bis widerrufen; **Kommentare des Eigners schlagen es.** Und **der Weg ist das Ziel**: Monate, kein
Abnahmetermin — eine Runde, die etwas gelernt hat, ist auch ohne Lieferung eine gute Runde, aber nur,
wenn das Gelernte **mit seiner Messung** in `## Gaps` stehenbleibt.

**Erfinderisch sein, auf Bewährtes aufbauen.** Wo etwas mehrfach gelöst ist — AAA-Titel, SpeedTree,
OSM-Referenzimplementierungen, Microsoft Flight Simulator —, ist der etablierte Weg der Ausgangspunkt;
**die Abweichung braucht einen Grund**, und der steht bei ihr.

**Nichts im Baum ist Besitzstand** — kein Format, kein Verzeichnis, kein Algorithmus, kein Dokument.
Bringt ein Ansatz das Bild nicht näher an die Fotografie, fliegt er. **Kein Freibrief:** revidierbar ist
jede getroffene *Entscheidung*, nicht die Messpflicht, nicht die Herkunft jeder Zahl, nicht das Löschen
des Ersetzten in derselben Runde — das sind die Werkzeuge, mit denen revidiert wird.

## Der Dreiklang

> **`doc/` = was wir wollen · `src/` = was wir können · `test/` = was wir beweisen.**
Jede Aussage hat **genau einen Ort**; `doc/` spiegelt **Verzeichnisse** von `sim/src/`. **Beschreibt ein
Dokument etwas Gelöschtes, fliegt es** — für die Historie ist Git zuständig. **Kommentare fallen fast
vollständig weg**; es bleibt EINE Aufgabe: **das lokale Warum am Entscheidungspunkt.**

**Spec zuerst.** Jede Runde ändert ZUERST das `## Spec` ihrer Themendatei, baut, bis `## State` es
erfüllt, führt `## State`/`## Gaps` nach und schreibt eine Zeile ins `journal.md`. Verworfenes bleibt mit
seiner **Messung** in `## Gaps`. Dokumente enthalten Gegenwart und Zukunft, nie Vergangenheit. Diese
Datei bleibt **unter 100 Zeilen**; Einstieg ist [`doc/INDEX.md`](doc/INDEX.md).

## Prinzipien (nicht verhandelbar)

1. **Rein deklarativ, und die Sprache ist JSON.** Ein Titel bringt **keine `.cpp` und keine Welt**. JSON
   ist schema-prüfbar, diffbar und **erzeugbar**; ein Eigenformat wäre ein Parser, den niemand bestellt
   hat. Shader für eigenes Aussehen sind erlaubt — Aussehen ist kein Wissen. [`doc/mods.md`](doc/mods.md)
2. **Die Engine ist texturfrei.** Zulässig sind nur der **Cache einer berechenbaren Funktion** (Sky-,
   Transmissions-LUT) und **Messdaten, die naturgemäß ein Raster sind** (DEM, Luftbild, Sterne) — **nie
   autoriertes Aussehen**; es gibt keine Artists. Nebengewinn: Mip-Abhängigkeit, Zoomsprünge,
   Abtastgitter und Filterartefakte **können in einer Funktion nicht auftreten**.
3. **Die Physik ist unsere eigene und deklarativ.** Fünf Teile — Segmente, Gelenke, Kontakte,
   Kraftquellen, Medium — plus Modell, Materialien, Gehirn; dasselbe Format trägt Möbel, Mensch, Wolf,
   Panzer, Flugzeug — [`doc/body-format.md`](doc/body-format.md). **Sie muss für die Darstellung
   reichen, nicht mehr.**
4. **Outshine weiß alles, ein Mod kennt nur, was er kennt.** Prüfbar: *braucht das Wissen, das kein
   Teilnehmer haben könnte?* Ja → Engine, sonst Mod. **Mit LLM-Akteuren ist es die tragende Regel**: ein
   Gehirn sieht nur über Sensoren, wirkt nur über simulierte Systeme; ein Kontakt trägt keine Identität.
5. **Alles läuft IM Client.** Physik, Welt und Bild sind ein Prozess, ein Adressraum, WASM wie nativ.
6. **Server-seitig nur zwei Container:** `fb-tiles` (`tiles/`, :8081) und `fb-sim` (`sim/`, :8080).
   Der Kachelserver liefert DEM, OSM, Luftbild, Wetter und Sternenkarte — sonst nichts.
7. **Die Mathematik ist deterministisch.** Gibt das Tempo das Ergebnis, ist die Kopplung ein Bug.

## Architektur & Build

`fb-tiles` liefert per HTTP an den Client aus Prinzip 5. `sim/src/` hat **fünf** Verzeichnisse:
`clients`, `core`, `render`, `units`, `world`, und **zwei Clients auf derselben Quellenliste**:
**`gpu_walk`** (nativ, das Frame-Orakel) und **wasm**. Beide lesen nur `mods/demo/scene.json`.

Gebaut wird nur über Make-Targets. `sim/`: `walk` | `wasm` | `worker` | `image` | `up`. Tore:
`verify-layers` | `verify-trees` | `verify-types`. **Der wasm-Client baut in JEDER Runde mit.**
Warnings = Errors (`-Wall -Wextra -Wpedantic`) · Frame-Beweis oder Messung · vendor read-only.
**Leistung ist eine Verteilung über eine bewegte Kamera**, nie Mittelwert, nie Minimum:
`tools/walkbench.py` (vier Geschwindigkeiten, p50/p95/p99), `tools/determinism.py` — **jede Messung
pinnt ihr Binary.**

## Harte Regeln im Code

- **Keine verstreuten Ausgaben.** `Log` für Ereignisse, `TelemetryBus` für Zustand. Core ist I/O-frei.
- **Jede Zahl trägt ihre Herkunft** — hergeleitet, gemessen oder `[SET]`.
- **Was ersetzt wird, wird in derselben Runde gelöscht** — ein toter Pfad, der noch feuern kann, ist
  schlimmer als eine Zeile zu viel. Rückfalltüren sind tote Pfade, Diagnosen nicht.
- **Es gibt eine Fassung.** Keine Qualitätsstufen während der Grundentwicklung.
- **Entwicklung läuft strikt seriell** — ein Agent im Baum. Dateitrennung schützt vor Überschreiben,
  nicht vor Störung: Baum und Compiler sind gemeinsam.
- **Nach JEDEM abgenommenen Schritt wird committed** — „Git holt es zurück" gilt nur, wenn es drin ist.
- `core/` zeigt nie nach oben. Peers rufen sich nie gegenseitig.
- C++17, **kein Präfix**, PascalCase, **`namespace outshine`**, Klasse pro Datei. Ausnahmen:
  `world/terrain/` (C-ABI-Bibliothek, `tiles/` ruft denselben DEM-Dekoder) und `FBWX` (Formatname).

## Referenzen

Der Stand der Technik ist geschrieben. Wer hier etwas neu erfindet, schuldet den Grund — und der steht
neben der Abweichung. Begründung je Titel in [`doc/references.md`](doc/references.md).

| Feld | Kanon |
|---|---|
| **Engine** | Gregory, *Game Engine Architecture* 3e · Lengyel, *Foundations of Game Engine Development* I–III |
| **Rendering** | Akenine-Möller u.a., *Real-Time Rendering* 4e · Pharr u.a., *Physically Based Rendering* 4e · Lagarde/de Rousiers, *Moving Frostbite to PBR* |
| **Prozedural** | Ebert/Musgrave/Perlin/Worley, *Texturing & Modeling* — der Kanon für „Aussehen ist eine Funktion" |
| **C++** | Stroustrup/Sutter, *C++ Core Guidelines* · Meyers, *Effective Modern C++* · Pikus, *The Art of Writing Efficient Programs* |
| **Physik** | Ericson, *Real-Time Collision Detection* · Bridson, *Fluid Simulation for Computer Graphics* |

## Host

emsdk in `~/Git/emsdk`, `nproc`-Shim in `~/.local/bin`. Container: `podman machine start`, dann
`tiles/up.sh` (:8081), `sim/up.sh` (:8080). Native Builds: `sim/vendor/.compat-headers`; **macOS hat
kein `timeout(1)`**. Baumvorlage: `~/Git/wasm-tree` (16 Arten als JSON).
