# Outshine

## Ziel

Ich baue eine weltweite, datengetriebene Open-World-Sandbox. Aus OpenFreeMap, Mapterhorn,
Open-Meteo, Szenariozeit und prozeduralen Rezepten entsteht eine plausible, funktionale Erde.
Ich entwickle und prüfe die erste Welt aus bodennahen Perspektiven zu Fuß und im Auto.
Ich rendere physikalisch basiert und orientiere Architektur, Vegetation, Materialien
und Licht an der realen Region.
GTA VI gibt Orientierung für Umgebungsqualität, Fallout 4 für Himmel und offene Erkundung.
Datierte Aufnahmen sind meine Bildreferenz für reale Orte, Jahreszeiten, Tageslicht und Wetter.
Die [Konzeptbilder](doc/images/concept/) zeigen exemplarische Fähigkeiten. Die Abnahme nutzt
wechselnde Orte, Tageszeiten und Spielsituationen.

Ich liefere zuerst die statische Welt inklusive Bewegung, Wasser, Vegetation und Wetter.
Danach folgen Fahrzeuge, NPCs, Physik, Skripte, Audio und Persistenz. Ein LLM Game Master führt
eigene Geschichten, Quests, Dialoge und NPC-Absichten. Die Simulation validiert und speichert
akzeptierte Aktionen und deklarative Rezepte. [Produktziel #1974](https://github.com/Weichwolf/outshine/issues/1974)

## Budget und Abnahme

| Gerät | Darstellung | p99 |
|---|---|---|
| A18 Pro / Metal | 1280×720 bei 30 fps | ≤ 33,33 ms |
| i5-1135G7 / Iris Xe / Vulkan | 1280×720 bei 30 fps | ≤ 33,33 ms |
| RTX 3060 12 GB / Vulkan | 1920×1080 bei 60 fps | ≤ 16,67 ms |

Ich implementiere und messe auf dem verfügbaren A18 Pro. Die übrigen Geräte haben ihre eigene
Abnahme. Alle Profile zeigen vollständige Inhalte und bis zu 240 km Sichtweite, soweit Wetter
und Aussicht sie zulassen. Dichte Stadt und dichter Wald teilen sich das gleiche Gesamtbudget für
GPU-Zeit, Bandbreite und Speicher. Ich stecke Detail in die sichtbare Bildwirkung.

- Ich halte zwölf wechselnde Places als Vergleich, auch unabhängige Gegenbeispiele. Ich prüfe
  Gehen, Blickdrehungen und Fahrten aus Autoperspektive. Ich öffne aktuelle PNGs und Zwischenbilder. Pro Place bleibt unter
  `build/shots/places/` das neueste PNG. Bilder publiziere ich nur auf ausdrücklichen Auftrag.
- Ich messe CPU-/GPU-p50 und -p99, Passzeiten, IO, Uploads sowie physischen Speicher. Vorher/Nachher
  gelten bei drei gepaarten Läufen auf demselben Gerät und Profil. [Benchmark #1956](https://github.com/Weichwolf/outshine/issues/1956)
- Ich erweitere in sichtbaren vertikalen Schritten. Bei hohen Kosten prüfe ich zuerst den
  dominanten Pass und seine Arbeit. Nach zwei erfolglosen Ansätzen ändere ich das Verfahren.

## Neubau

Ich baue den neuen Darstellungs- und Ladepfad neben dem lauffähigen Produktstand. Der
[Vergleichscommit](https://github.com/Weichwolf/outshine/tree/0932b78323ebf23016f723fdfb3fa31da2e28337) hält den
bisherigen Vergleichsstand. Ich nutze seine funktionierenden Generatoren als Datenquelle
für den Vergleich und übernehme Verfahren aus `lib/` mit ihren Tests und Eingabefällen.
Ich ersetze Module, sobald Bewegung, Bild und Budget auf denselben Eingaben belegt sind.

1. Ein Renderer erzeugt einen nativen Metal-Frame aus einem externen Szenario.
2. Für die Ferne ersetze ich Objektgeometrie durch eine gefilterte Tiefen-Cubemap oder
   einen Skydome. Für mittlere Entfernung nutze ich Gruppen-Impostors. Im Nahbereich
   baue ich detaillierte Geometrie. Der Aufwand sinkt mit der Entfernung.
3. Eine gemeinsame Kachelauswahl entscheidet vor IO, Decode und Upload. Ein grobes
   Produkt deckt seinen Bereich ab, bis der Ersatz vollständig sichtbar ist.
4. Quellen, Generatoren, SSD-Index, Residency und Renderer haben eigene Zuständigkeiten.
   Generatoren liefern versionierte Produkte. World hält generische Produkte und
   Abdeckung. Bild-, Schatten- und Funktionsbedarf sind unabhängig. Der Frame verarbeitet
   residente Daten. Cache-Hits laden fertige Daten, Misses erzeugen sie asynchron.
5. Ich löse Infrastruktur gegen grobe Höhen und leite daraus glatte Terrainanschlüsse ab.
   Gebäude nutzen die gelösten Standortkontakte, Vegetation folgt danach. Die Darstellung
   nutzt geeignete Repräsentationen desselben Weltinhalts.

Umsetzungsreihenfolge, Abhängigkeiten und Abnahmen stehen in der
[Roadmap #1972](https://github.com/Weichwolf/outshine/issues/1972). Details gehören ins Issue.

## Arbeitsweise

- Ich verantworte Architektur, Art Direction, Forschung, Code und Abnahme. Die aktuellen
  Vorgaben des Regisseurs gehen vor.
- Ich halte einen aktiven Issue und Arbeitsbranch je Host nach `hostname/issueid_issuename`.
  Kleine vollständige Commits mit Issuebezug pushen. Integration per PR nach `master`,
  nach eigener Abnahme mergen. Archivbranch von der laufenden Arbeit getrennt halten.
- Issues sind englisch, kompakt und enthalten Priorität, Abhängigkeiten, Verfahren,
  Literatur und sichtbare oder messbare Abnahme. Aktuelle Arbeit trägt `active`.
- Ich verwende etablierte Verfahren aus den Primärquellen in `doc/references/` und bewährte
  Bibliotheken mit echtem Nutzen. Neue Algorithmen prüfe ich zuerst in Python/GLSL an
  echten Daten, Gegenfällen, Geschwindigkeit und Speicher. GPU-Kosten messe ich nativ.
- C++23, CMake-Targets, native Metal/Vulkan, SDL3 für Fenster, Eingabe und Audio nach Bedarf.
  Klare Namen und Zuständigkeiten, RAII, Composition, Fehler an Grenzen per
  `[[nodiscard]] std::expected`, Warnings als Fehler. Header beim Modul und C++-Quellen
  ohne Kommentare. Funktionen höchstens 120 Zeilen. CPU/GPU-Layouts per `static_assert`.
- Die Anwendung kennt keine fest eingebauten Places. Ich teste typisches Spielverhalten,
  Quellen- und Asset-Misses, Geometriequalität und CPU/GPU-Layouts. Für kleine Änderungen
  laufen einschlägige Tests und Lint der betroffenen Dateien. Breite Läufe bündle ich.
- Builds und Lint laufen mit `nice -n 10`, Messungen ohne Parallellast. Builds und Bilder
  liegen unter `build/`, Logs und Versuchsausgaben im System-Tempverzeichnis.
  Eigene überholte Versuchsausgaben entfernen. Berichte kurz auf Deutsch.
