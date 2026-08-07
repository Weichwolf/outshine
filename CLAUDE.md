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

**Das laufende Ziel steht in [`doc/goal.md`](doc/goal.md), bindend bis widerrufen; Kommentare des
Eigners schlagen es.**

## Der Dreiklang

> **`doc/` = was wir wollen · `src/` = was wir können · `test/` = was wir beweisen.**
Jede Aussage hat **genau einen Ort**; `doc/` spiegelt **Verzeichnisse** von `sim/src/`. **Beschreibt ein
Dokument etwas Gelöschtes, fliegt es** — für die Historie ist Git zuständig.

**Kommentare fallen fast vollständig weg** — keine Kopfblöcke, keine Benutzungshinweise, keine
Beschreibung des *was*. Es bleibt EINE Aufgabe: **das lokale Warum am Entscheidungspunkt.**

**Spec zuerst.** Jede Runde ändert ZUERST das `## Spec` ihrer Themendatei, baut, bis `## State` es
erfüllt, führt `## State`/`## Gaps` nach und schreibt eine Zeile ins `journal.md`. Verworfenes bleibt mit
seiner **Messung** in `## Gaps`. Dokumente enthalten Gegenwart und Zukunft, nie Vergangenheit.

Diese Datei bleibt **unter 100 Zeilen**. Einstieg [`doc/INDEX.md`](doc/INDEX.md).

## Prinzipien (nicht verhandelbar)

1. **Rein deklarativ, und die Sprache ist JSON.** Ein Titel bringt **keine `.cpp` und keine Welt**. JSON
   ist schema-prüfbar, diffbar und **erzeugbar**; ein Eigenformat wäre ein Parser, den niemand bestellt
   hat. Shader für das Aussehen eigener Entitäten sind erlaubt — Aussehen ist kein Wissen.
   [`doc/mods.md`](doc/mods.md)
2. **Die Engine ist texturfrei.** Zulässig sind nur der **Cache einer berechenbaren Funktion** (Sky-,
   Transmissions-LUT) und **Messdaten, die naturgemäß ein Raster sind** (DEM, Luftbild, Sterne) — **nie
   autoriertes Aussehen**; es gibt keine Artists. Der Nebengewinn ist der größere: Mip-Abhängigkeit,
   Zoomsprünge, Abtastgitter und Filterartefakte **können in einer Funktion nicht auftreten**.
3. **Die Physik ist unsere eigene und deklarativ.** Ein Körper sind fünf Teile — Segmente, Gelenke,
   Kontakte, Kraftquellen, Medium — plus Modell, Materialien, Gehirn; dasselbe Format trägt Möbel,
   Mensch, Wolf, Panzer, Flugzeug. [`doc/body-format.md`](doc/body-format.md). **Sie muss für die
   Darstellung reichen, nicht mehr.**
4. **Outshine weiß alles, ein Mod kennt nur, was er kennt.** Prüfbar: *braucht dieses Ding Wissen, das
   kein Teilnehmer haben könnte?* Ja → Engine, nein → Mod. **Mit LLM-Akteuren ist das die tragende
   Regel**: ein Gehirn sieht nur über Sensoren und wirkt nur über simulierte Systeme; ein Kontakt trägt
   keine Identität.
5. **Alles läuft IM Client.** Physik, Welt und Bild sind ein Prozess, ein Adressraum, WASM wie nativ.
6. **Server-seitig nur zwei Container:** `fb-tiles` (`tiles/`, :8081) und `fb-sim` (`sim/`, :8080).
   Der Kachelserver liefert DEM, OSM, Luftbild, Wetter und Sternenkarte — sonst nichts.
7. **Die Mathematik ist deterministisch.** Gibt das Tempo das Ergebnis, ist die Kopplung ein Bug.

## Architektur

```
fb-tiles (DEM/OSM/Luftbild/Wetter/Sterne)  ──HTTP──▶  Client = Physik + Welt + Renderer + KI
                                                      als EIN Prozess (WASM | native)
```

`sim/src/` hat **fünf** Verzeichnisse: `clients`, `core`, `render`, `units`, `world`. **Zwei Clients auf
derselben Quellenliste** — **`gpu_walk`** (nativ, das Frame-Orakel) und **wasm**; beide lesen
`mods/demo/scene.json` und sonst nichts. Kampfschicht, `core-lib`, `fb-gym`, `.fbm` und die
Testmaschinerie sind gelöscht.

## Build

Nur über Make-Targets. `sim/`: `walk` | `wasm` | `worker` | `image` | `up`. Tore: `verify-layers` |
`verify-trees` | `verify-types`. **Der wasm-Client baut in JEDER Runde mit.** Warnings = Errors
(`-Wall -Wextra -Wpedantic`) · Frame-Beweis oder Messung · vendor read-only.

**Leistung ist eine Verteilung über eine bewegte Kamera**, nie Mittelwert, nie Minimum:
`tools/walkbench.py` (vier Geschwindigkeiten, p50/p95/p99), `tools/determinism.py` — **jede Messung
pinnt ihr Binary.**

## Harte Regeln im Code

- **Keine verstreuten Ausgaben.** `Log` für Ereignisse, `TelemetryBus` für Zustand. Core ist I/O-frei.
- **Jede Zahl trägt ihre Herkunft** — hergeleitet, gemessen oder `[SET]`.
- **Was ersetzt wird, wird in derselben Runde gelöscht** — ein toter Pfad, der noch feuern kann, ist
  schlimmer als eine Zeile zu viel. Rückfalltüren sind tote Pfade, Diagnosen nicht.
- **Es gibt eine Fassung.** Keine Qualitätsstufen während der Grundentwicklung.
- **Entwicklung läuft strikt seriell** — ein Agent im Baum. Dateitrennung verhindert Überschreiben, nicht
  Störung: Baum und Compiler sind gemeinsam.
- **Nach JEDEM abgenommenen Entwicklungsschritt wird committed.** „Git holt es zurück" gilt nur, wenn es
  drin ist.
- `core/` zeigt nie nach oben. Peers rufen sich nie gegenseitig.
- C++17, **kein Präfix**, PascalCase, **`namespace outshine`**, Klasse pro Datei. Ausnahmen:
  `world/terrain/` (kleingeschriebene C-ABI-Bibliothek, weil `tiles/` denselben DEM-Dekoder ruft),
  `FBWX` (Formatname).

## Host

emsdk in `~/Git/emsdk`, `nproc`-Shim in `~/.local/bin`. Container: `podman machine start`, dann
`tiles/up.sh` (:8081), `sim/up.sh` (:8080). Native Builds brauchen `sim/vendor/.compat-headers`;
**macOS hat kein `timeout(1)`**. Baumvorlage: `~/Git/wasm-tree` (16 Arten als JSON).
