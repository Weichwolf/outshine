# Outshine

> **Eine Game Engine, die auf PS4- und A18-Pro-Hardware in 720p60 läuft, mit der Technologie von Days
> Gone und Horizon Forbidden West, und in der sich Spiele wie Witcher 3, Fallout 4 und GTA 5 optisch,
> inhaltlich und funktionell rein über deklarative `scenarios/` abbilden lassen. Grundlage der prozeduralen
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
— sie gilt für alles Sichtbare. **Kommentare des Eigners schlagen alles.** Und **der Weg ist das Ziel**:
Monate, kein Abnahmetermin — eine Runde, die etwas gelernt hat, ist auch ohne Lieferung eine gute Runde.

**Erfinderisch sein, auf Bewährtes aufbauen.** Der Stand der Technik ist geschrieben — siehe
`## Referenzen`. Der etablierte Weg ist der Ausgangspunkt, **die Abweichung braucht einen Grund**, und
der steht bei ihr.

**Der Rahmen steht, der Code ist im Wandel.** Fest sind **wasm32 und WebGPU** — eine virtuelle Konsole,
und ihre Grenzen sind die Grenzen. Alles andere im Baum ist **Material**: Formate, Verzeichnisse,
Algorithmen, Schnittstellen, Build, Werkzeuge. Wir bauen etwas Neues; nichts hier ist Besitzstand, und
was die Vision verlangt, wird gebaut oder geändert.

**Fehlt etwas, ist das eine Aufgabe und keine Grenze.** „Diese Zahl gibt es nicht" endet mit „also wird
das Werkzeug gebaut", nicht mit „also ist es nicht entscheidbar". Unterscheide **nicht messbar** (die
Sache gibt keine Zahl her) von **noch nicht gemessen** (das Werkzeug fehlt) — das zweite hat einen
Aufwand, keine Grenze. Bleibt ein Entwurf an etwas Vorhandenem hängen, lautet die Frage nicht „wie
arbeite ich darum herum", sondern **„gehört das Vorhandene geändert"**, samt der Angabe was es kostet.

**Kein Freibrief:** revidierbar ist jede getroffene *Entscheidung*, nicht die Messpflicht, nicht die
Herkunft jeder Zahl, nicht das Löschen des Ersetzten in derselben Runde — das sind die Werkzeuge, mit
denen revidiert wird.

## Wo was steht

| Ort | Inhalt |
|---|---|
| **der Code** | was das Ding kann. **Nur Korrektes wird committed** — es gibt keinen zweiten Ort, an dem Korrektheit behauptet wird |
| **`git log`** | was war. Kein Journal, kein Verlauf in einer Datei |
| [`doc/vision.md`](doc/vision.md) | wofür, und wo die Latte hängt |
| [`doc/architecture.md`](doc/architecture.md) | wie Outshine gebaut sein soll — Entscheidungen, keine Prosa |
| [`doc/todo.md`](doc/todo.md) | die nächsten Schritte, in Reihenfolge |
| `.claude/agents/` | **`engine-developer`** baut und misst · **`engine-architect`** plant und urteilt, nur lesend |
| diese Datei | die Regeln. Höchstens **200 Zeilen** |

`doc/` hat **drei** Dateien — Zweck, Bauform, Reihenfolge — und bekommt keine vierte. Ein Dokument, das
beschreibt, was der Code **tut**, ist dasselbe in zwei Sprachen, und die zweite kann lügen. Ein
verworfener Versuch wird nicht aufbewahrt: die Ausgangslage ändert sich laufend, und eine konservierte
Messung führt später in die Irre.

**Kommentare fallen fast vollständig weg.** Es bleibt EINE Aufgabe: **das lokale Warum am
Entscheidungspunkt**, eine Zeile. Nie, was der Code tut.

## Prinzipien (nicht verhandelbar)

1. **Rein deklarativ, und die Sprache ist JSON.** Ein Titel bringt **keine `.cpp` und keine Welt**. JSON
   ist schema-prüfbar, diffbar und **erzeugbar**; ein Eigenformat wäre ein Parser, den niemand bestellt
   hat. Shader für eigenes Aussehen sind erlaubt — Aussehen ist kein Wissen.
2. **Die Engine ist texturfrei.** Zulässig sind nur der **Cache einer berechenbaren Funktion** (Sky-,
   Transmissions-LUT) und **Messdaten, die naturgemäß ein Raster sind** (DEM, Luftbild, Sterne) — **nie
   autoriertes Aussehen**; es gibt keine Artists. Nebengewinn: Mip-Abhängigkeit, Zoomsprünge,
   Abtastgitter und Filterartefakte **können in einer Funktion nicht auftreten**.
3. **Die Physik ist unsere eigene und deklarativ.** Fünf Teile — Segmente, Gelenke, Kontakte,
   Kraftquellen, Medium — plus Modell, Materialien, Gehirn; dasselbe Format trägt Möbel, Mensch, Wolf,
   Panzer, Flugzeug. **Sie muss für die Darstellung reichen, nicht mehr.**
4. **Outshine weiß alles, ein Mod kennt nur, was er kennt.** Prüfbar: *braucht das Wissen, das kein
   Teilnehmer haben könnte?* Ja → Engine, sonst Mod. **Mit LLM-Akteuren ist es die tragende Regel**: ein
   Gehirn sieht nur über Sensoren, wirkt nur über simulierte Systeme; ein Kontakt trägt keine Identität.
5. **Alles läuft IM Client.** Physik, Welt und Bild sind ein Prozess, ein Adressraum, WASM wie nativ.
6. **Server-seitig nur zwei Container:** `fb-tiles` (`tiles/`, :8081) und `fb-sim` (`sim/`, :8080).
   Der Kachelserver liefert DEM, OSM, Luftbild, Wetter und Sternenkarte — sonst nichts.
7. **Die Mathematik ist deterministisch.** Gibt das Tempo das Ergebnis, ist die Kopplung ein Bug.

## Architektur & Build

`fb-tiles` liefert per HTTP an den Client aus Prinzip 5. `sim/src/` hat **fünf** Verzeichnisse:
`clients`, `core`, `generators`, `render`, `units`, `world` — **Kern** ist die nackte Welt (Gelände,
Klassifizierung, Atmosphäre, Wolken, Gestirne, Renderer), **Generatoren** liefern daraus Inhalt
(Vegetation, Bauwerke, Infrastruktur, Wasser) und sind austauschbar, weil sie dieselbe Eingabe lesen.

**EIN Programm, zwei Übersetzungen, EIN Eintrittspunkt.** `clients/Outshine` besitzt World und
Renderer und ist das Einzige, was eine Welt aufbaut; ein Client ist `main()` plus Ausgabemedium
darüber — **`gpu_walk`** (nativ, Frame-Orakel, Bank `WalkBench`) und **wasm** (Browser, `Walker`).
Beide bekommen zwei Wörter: welcher Mod, welche Szene. Eine gemeinsame Quellenliste allein deckte den Drift zehn
Runden: sie beweist, dass beide *übersetzen*, nicht, dass beide dasselbe *zeigen* — das tut
`verify-clients`.

Gebaut wird nur über Make-Targets. `sim/`: `walk` | `wasm` | `worker` | `image` | `up`. Tore:
`verify-layers` | `verify-clients` | `verify-trees` | `verify-types`. **Der wasm-Client baut in JEDER
Runde mit.**
Warnings = Errors (`-Wall -Wextra -Wpedantic`) · Frame-Beweis oder Messung · vendor read-only.
**Jede Stufe misst sich selbst, laufend, und das Ergebnis geht in die Telemetrie** — Kachelabruf,
Dekodierung, Upload, Residenz, jeder Pass, der Frame. Es gibt keinen Messmodus: eine Bank ist ein
deklarierter Lauf, kein anderer Codepfad. Ausgewertet wird über die Zeitreihe; eine Kachelankunft im
Frame ist ein **Feld**, kein Grund, den Lauf zu verwerfen. **Leistung ist eine Verteilung über eine
bewegte Kamera** — p50/p95/p99, nie Mittelwert, nie Minimum. Jede Zeile trägt Mod, Szene, wasm-Hash und
Browserversion.

**Erstladung und Nachströmen sind zwei Dinge.** Die Erstladung hält die Welt zurück und zeigt
Fortschritt — dafür ist der Ladebildschirm da; Outshine wärmt nicht auf. **Was während des Spielens
nachströmt, hält die Pipeline NIE an**: Holen und Dekodieren laufen neben dem Renderfaden, das Hochladen
je Frame ist ein Budget, und eine Kachel wird sichtbar, wenn sie fertig ist — nie halb. **Ein Ruckler
beim Nachladen ist ein Fehler**, kein Naturgesetz, und genau die Sorte, die ein Standbild nicht zeigt.

**Das Standbild ist die Vergleichsauflösung, nicht die Abnahme.** Was gegen ein Foto abgestimmt wird,
muss **in Bewegung schnell UND makellos** sein — und die teuersten Fehler sind genau die, die ein
Einzelbild nicht zeigen kann: Popping am LOD-Wechsel, eine Streuung, die an einem Radius endet, Ghosting
und Schlieren im Zeitfilter, ein Ruckler beim Nachladen, eine Schattierung, die beim Netzwechsel
springt. **Ein Beleg aus einem Standbild belegt sie nicht** — bewegte Aufnahme oder es gilt als ungeprüft.

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
- **Die C++ Core Guidelines gelten** (`## Referenzen`); das Folgende sind nur die Hausabweichungen.
- C++17, **kein Präfix**, PascalCase, **`namespace outshine`**, Klasse pro Datei. Ausnahmen:
  `world/terrain/` (C-ABI-Bibliothek, `tiles/` ruft denselben DEM-Dekoder) und `FBWX` (Formatname).

## Referenzen

**Stroustrup/Sutter, [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines) — VERBINDLICH.**
Sie entscheiden Besitz, Lebensdauer, Schnittstelle und Stil; eine Abweichung ist ein Fehler, bis sie mit
Grund danebensteht, und gegen eine Hausmeinung gewinnen sie. Der Rest ist Kanon, kein Gesetz —
Ausgangspunkt statt Eigenerfindung.

| Feld | Titel |
|---|---|
| **Engine** | Gregory, *Game Engine Architecture* 3e · Lengyel, *Foundations of Game Engine Development* I–III |
| **Rendering** | Akenine-Möller u.a., *Real-Time Rendering* 4e · Pharr u.a., *Physically Based Rendering* 4e · Lagarde/de Rousiers, *Moving Frostbite to PBR* |
| **Prozedural** | Ebert/Musgrave/Perlin/Worley, *Texturing & Modeling* — der Kanon für „Aussehen ist eine Funktion", samt seiner Grenzen |
| **C++** | Meyers, *Effective Modern C++* · Pikus, *The Art of Writing Efficient Programs* |
| **Physik** | Ericson, *Real-Time Collision Detection* · Bridson, *Fluid Simulation for Computer Graphics* |
| **Implementierungen** | AAA-Titel · SpeedTree · OSM-Viewer (OSM2World, F4map) · Microsoft Flight Simulator |

## Host

emsdk in `~/Git/emsdk`, `nproc`-Shim in `~/.local/bin`. Container: `podman machine start`, dann
`tiles/up.sh` (:8081), `sim/up.sh` (:8080). Native Builds: `sim/vendor/.compat-headers`; **macOS hat
kein `timeout(1)`**. Baumvorlage: `~/Git/wasm-tree` (16 Arten als JSON).
