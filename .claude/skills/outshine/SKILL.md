---
name: outshine
description: Wie an Outshine gearbeitet wird — der OSM-basierten Open-World-Engine (C++17, WebGPU/WASM auf Chromium/Edge, weltweiter Kachelserver in C). Wo was liegt, wie eine Runde läuft, wer entwirft und wer baut, und die Fallen, in die dieses Projekt nachweislich tappt. Laden, wenn an Outshines Code, Architektur, Szenarien, Generatoren, dem Renderer oder der Welt gearbeitet wird, oder wenn zu beurteilen ist, ob eine Änderung passt.
---

# Outshine

> **Eine weltweite Sandbox auf GTA-5-Niveau: man läuft überall hin, und alles strömt, entsteht und wird
> platziert, während man geht. Einzige Eingabe ist, was der Kachelserver liefert.**

## Was bindend ist

| Ort | Inhalt |
|---|---|
| **`CLAUDE.md`** | die Regeln. Bindend, und ein Verstoß ist falsch, auch wenn er funktioniert. **Lies es zuerst.** |
| `doc/vision.md` | wofür, und wo die Latte hängt |
| `doc/architecture.md` | warum der Schnitt so ist |

**`doc/` hat zwei Dateien und bekommt keine dritte.** Kein Spec, kein State, kein Gaps, kein Journal —
ein Dokument, das beschreibt, was der Code tut, ist dasselbe in zwei Sprachen, und die zweite kann lügen.
Was war, steht in `git log`. Was ist, steht im Code. **Nur Korrektes wird committed**, also gibt es
keinen zweiten Ort, an dem Korrektheit behauptet wird.

## Der Baum

```
sim/src/  clients · core · generators · render · units · world
tiles/    fb-tiles, der C-Kachelserver
scenarios/  die deklarierten Welten
```

**Kern** ist die nackte Welt — Gelände, Klassifizierung, Atmosphäre, Wolken, Gestirne, Renderer, und
*wo* Wasser steht. **Generatoren** machen daraus Inhalt — Vegetation, Bauwerke, Infrastruktur, das
*Aussehen* von Wasser — und sind austauschbar, weil sie dieselbe Eingabe lesen. Ein Generator ist eine
reine Funktion `(Region, Ground) → Yield`: er zeichnet nicht, kennt keine Kamera, keinen Frame, kein
Gerät. Der Scheduler weiß, wo das Auge ist, der Generator nie.

**Ein Programm, ein Eintrittspunkt.** `clients/Outshine` besitzt Welt und Renderer und ist das Einzige,
was eine Szene baut; ein Client ist `main()` plus Ausgabemedium. Gebaut wird nur über Make-Targets;
`verify-layers` und `verify-clients` sind die Tore.

## Wie eine Runde läuft

| Wer | Was |
|---|---|
| **`engine-architect`** | entwirft, bevor gebaut wird, und urteilt danach. Nur lesend. Für eine gegnerische Prüfung **frisch** aufrufen, ohne den Planungslauf |
| **`engine-developer`** | baut und misst. Genau einer im Baum — Entwicklung läuft strikt seriell |

Ein Auftrag nennt **Ziel, Zwang und Abnahmezahl**. Einen Mechanismus nennt er nur, wenn der belegt ist —
sonst heißt es „finde heraus, wie X es löst, und schlage vor". Ein konkreter falscher Mechanismus im
Auftrag schlägt jede richtige Parole daneben.

## Die Fallen, in die dieses Projekt tappt

Alle gemessen, keine erfunden:

- **Flüssigkeit ist verdächtig.** Ein plausibler Satz über einen Streamer entsteht schneller, als die
  Prüfung dauert, ob er stimmt. Vor dem Bauen das Problem im **Vokabular der Sache** benennen — „Level
  Load", „Streaming", „LOD-Übergang", „Resektion". Findet sich kein solcher Name, kennst du das Feld
  nicht, und dann wird recherchiert statt improvisiert.
- **Messung vor Griff.** Fünf Vermutungen kosteten hier mehr als die eine Messung, mit der man hätte
  anfangen sollen. Eine Messung, die deine Vermutung widerlegt, ist das Ergebnis der Runde.
- **Die teuren Fehler sind Bedeutungsfehler, keine C++-Fehler.** Ein absoluter Wert in einem
  kamerarelativen Puffer, 16 von 24 Hashbits, ein Weißpunkt aus dem sRGB-Container, ein Trieb mit 3 cm
  Mindestradius — jede dieser Zeilen hätte jedes Review bestanden. **Einheit und Bezugssystem gehören
  zur Herkunft einer Zahl.**
- **Ein Kommentar ist eine Behauptung ohne Test.** Sechs davon logen in einer einzigen Sitzung. Nur das
  lokale, nicht offensichtliche *Warum*, eine Zeile; nie, was der Code tut; nie eine Messung — die
  verfällt, der Kommentar bleibt.
- **Ein grünes Tor beweist nur, was es prüft.** Zehn Runden lang bewies `make wasm`, dass der Browser
  *übersetzt* — nicht, dass er dasselbe *zeigt*. Ein Tor, das Struktur misst, hätte es sofort gefunden.
- **Ein konfundierter Befund kostet eine Runde.** „Kein Richtungslicht" war eine Szene bei Sonnenstand
  −3,6°. Vor jedem Defekt aktiv die harmlose Erklärung suchen und sagen, warum sie ausscheidet.
- **Das Standbild ist die Vergleichsauflösung, nicht die Abnahme.** Popping, Ghosting, ein Ruckler beim
  Nachladen, eine Streuung mit Radius — ein Einzelbild zeigt keins davon.

## Wenn du nicht weiterkommst

Mach es wie die Etablierten. Der Kanon steht in `CLAUDE.md ## Referenzen`, die **C++ Core Guidelines
sind verbindlich**. Suche die Quelle, nenn sie in einer Zeile am Entscheidungspunkt, und weiche nur mit
einem Grund ab, der bei der Abweichung steht.

Und prüfe die Quelle, statt sie zu zitieren: Microsoft Flight Simulator trägt **nicht** als Beleg für
Laufzeiterzeugung — dort ist alles vorab in der Cloud erzeugt worden. Für eine Welt, die entsteht,
während man läuft, ist Guerrillas *Horizon Zero Dawn* der Beleg.
