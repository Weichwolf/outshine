Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators
Tags: buildings, recipes, roofs, facades, variety

# Distinct architecture recipes produce a visible building catalogue

## Ergebnis und fehlender Vertrag
Besitzer: Gebäudeagent/Python; 2173 liefert Quelle/Nutzung, 2420 konstruktive Raum-/Öffnungspläne.
Erkennbare Bauformen mit passenden Fassaden, Dächern, Erdgeschossen und Innen-/Außenanschlüssen.
Tausende nachvollziehbar verschiedene Varianten; Familien, Varianten und Instanzen getrennt zählen.
Aktuell wenige Fassadenparameter auf extrudierten Grundrissen, keine belegte Architekturvielfalt.
2420 liefert einen nutzbaren Python-Raum-/Geschoss-/Portalvertrag; native Integration und Nutzungsprogramme bleiben offen.
Aktiver Ausbau: Dachflächen/Traufe/First auf denselben Quellgrundrissen, zuerst 2D-Schnitte, danach 2.5D und äußere Ansichten.
2400 untersucht dafür Esri-Dachkerne mit Ghidra; allgemein gültige Verfahren gegen metrische analytische Dächer vergleichen.
Gesamtoberkante/Unterkante bleiben geliefert; Dachraum vor Geschossplanung innerhalb dieser Höhe reservieren.
Gelieferte Geschoss-/Dachangaben erhalten, fehlende ergänzen und kennzeichnen; zu knappe Intervalle explizit ausweisen.
Skelettflächen für konkave/Hof-Dächer gegen analytische metrische Rechteckdächer vergleichen; Quelle/Höfe unverändert.
Zusätzliche externe Python-Bibliothek `py_straight_skeleton==0.1.0` in `.venv`; kein Fremdcode im Repository.
Ungewichtetes Skelett ist ein Vergleichspfad, keine vollständige Esri-Dachentsprechung; Gewichte/überhängende/even-Dächer bleiben offen.

## Familien und Komponenten
- Haus/Bungalow/Reihenhaus: Geschosse/Räume/Eingang, Fensterachsen und Dach-/Traufkonstruktion.
- Wohnblock/Büro/Hotel/Mischbau: Höfe/Parts, Erschließungskern, gegliederte Fassaden/Teilnutzungen.
- Laden/öffentlicher Bau: Schaufenster/Portale, Säle/Korridore und passende getrennte Eingänge.
- Halle/Lager/Hangar/Garage: Tragstruktur, Tore/Ladezugang, große Innenflächen und passende Öffnungen.
- Sakralbau/Gewächshaus, Silo/Tank, Schornstein/Wasserturm und weitere gelieferte Sonderklassen:
  passende Körper/Nutzung/Wartung/Details; keine Wohnfassade auf jedem Typ.
- Flach-/Pult-/Sattel-/Walm-/weitere belegte Dächer: Löcher/Parts, Traufe/First/Parapet/Entwässerung/Zugang.
- Sockel/Geschossbänder, Rahmen/Leibungen/Stürze/Fensterbänke/Brüstungen, Türen/Tore.
  Balkone/Vordächer/Außenfluchten/Dachaufbauten funktional an den gemeinsamen Plan anschließen.
- Materialien/Proportionen kohärent je Familie; Quellfarbe und Herkunft ergänzter Architektur erhalten.

## Umsetzung und Abnahme
Eine Familie vollständig über 2D → 2.5D → Tag/Nacht/Nah/Gegen/Fern, dann nächste Bildlücke wählen.
Form-/Teilungs-/Komponentenverfahren bei gleichen Anforderungen vergleichen; gültige einfache Rezepte behalten.
Mehrere reale Eingaben je Familie, passende Esri-Referenzen bei gleicher Kamera/Licht optisch prüfen.
Varianten nach tatsächlich verschiedenen Form-/Bauteil-/Raumkonfigurationen zählen, nicht nach Seeds/Instanzen.
Konstruktive Funktion, Bildgewinn und Erzeugungs-/Detailkosten gemeinsam prüfen; Tausende Varianten bleiben Ziel.
Fremde Modelle/Code bleiben externe Referenzen (2400); eigener Generator unter `test/experiments/buildings/`.

## Quellen
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
[Held/Palfrader 2016](../doc/references/buildings/geometry/2016-weighted-skeleton-roofs-and-terrains.pdf): Dachflächen aus Wavefront; Seiten 1/6/8 optisch geprüft, CGA-Shape-Seite 4 ebenso.
[Python-Skeleton-Bibliothek](https://pypi.org/project/py_straight_skeleton/),
[Esri Walm](https://doc.arcgis.com/en/cityengine/latest/cga/cga-roof-hip.htm),
[Esri Sattel](https://doc.arcgis.com/en/cityengine/latest/cga/cga-roof-gable.htm).
[Esri-Pakete c9ebe92](https://github.com/Esri/pyprt-examples/tree/c9ebe92f27f6355179c9d7792f430a3c133b9214).
Gemeinsame Arbeits-/Bildregeln in AGENTS, Gebäude-/Quellvertrag in 2173.
