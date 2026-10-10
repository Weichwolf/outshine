Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2342
Area: generators, world
Tags: infrastructure, solver, recipes

# Flat transport recipes share complete junction ports

## Ergebnis und fehlender Vertrag
2342 liefert den quellbezogenen ebenen Graph samt Nutzungsbilanz.
Aus ihm entstehen einfache zusammenhängende Bänder, Kurven und Anschlussflächen ohne Terrain.
Besitzer: generators/road. Ergebnis ist der native Bauteilplan für 2281, noch ohne Höhen/Ebenen.
[Python-Flächenplan](../test/experiments/infrastructure_flat_surface_network.py) teilt gemeinsame
Randsegmente im selben Mesh; die gewählten Ebenen-0-Fenster bestehen die Flächen-/Kantenprüfung.
Fahrkurven, Verkehrsregeln, Kollisionsprodukt und Budgetgrenzen bleiben offen; gegliederte Flächen
allein liefern noch kein plausibles fertiges Netz. Kleine Beispiele sind mit `--small` renderbar.
[Bordbögen](../test/experiments/infrastructure_flat_corners.py) runden Innenkanten tangential,
ohne bestehende Verkehrsfläche zu entfernen. Ungültige Bogenstücke entfallen lokal samt Grund;
ein Konflikt verwirft keine ganze Komponente. Brauchbare Fahrtrajektorien fehlen weiterhin.
[Anschlusskerne](../test/experiments/infrastructure_flat_junctions.py) verwenden endliche Bänder
mit geraden Schnitten; kurze überlappende Kerne vereinigen sich. Ganze Fenster behalten ihre Fläche.
[Schnittprüfung](../test/experiments/infrastructure_flat_ports.py) erkennt in realen Fenstern noch
gebogene und geschlossene innere Schnittlinien. Gerade vollständige Ports bleiben damit offen;
eine lückenlose Fläche allein erfüllt den Bauteilvertrag nicht.
[Direkte Querschnitte](../test/experiments/infrastructure_flat_cuts.py) grenzen die tatsächliche
Verkehrsfläche per Randschnitt/Suchindex ab und polygonisieren gemeinsame Grenzen.
Gleichartige Nachbarstücke werden vereinigt. Kollidierende Schnitte rücken begrenzt nach außen;
kurze Zwischenabschnitte gehen in den gemeinsamen Anschluss ein. Die drei vollständigen
Ebenen-0-Fenster bestehen damit auch die Querschnittprüfung. Fahrkurven und Verkehrsregeln fehlen.
Quellteilung darf keine künstlichen Bauteile erzeugen; Kosten und weltweite Vielfalt weiter prüfen.

## Umsetzung und Abnahme
1. Gerade/Ende/Kurve, T/X und Einfädelung als kleine Rezepte mit gemeinsamen vollständigen Querschnitten.
2. Überlappende kurze Knoten zu einem Bauteil vereinigen; Außenports gemeinsam speichern.
   Kurven bei zu engem Radius vereinfachen/verschieben, dann messen; ungelöste lokale Teile ausweisen.
3. Mesh, Kontakt und Kollision aus demselben Bauteilplan; Fläche einmal besitzen und triangulieren.
4. Kleine Fälle, dann alle verwendeten Bauteile der Wiener/Zürcher/Basler Fenster rendern und öffnen.
5. Weltweit hunderte bis tausende Beispiele nach Topologie, Klassen und gelieferten Tags untersuchen.
   [Survey](../test/experiments/infrastructure_world_survey.py) nutzt gespeicherte Originalkacheln,
   schreibt kompakte Messwerte und nachstellbare Defizite; Bilder nur nach Auswahl gemäß AGENTS.
   Nahansichten müssen Anschlüsse, Fahrkurven und lokale Auslassungen plausibel zeigen.
   Native Übertragung erst nach annähernd fehlerfreien vollständigen 2D-/2,5D-Netzen gemäß Goal.
Gemeinsame Portpositionen/Normalen, plausible Breiten/Kurven, keine Risse/Faltungen/inneren Doppelstücke.
Verwendungsgrad und Kosten gehören zur Qualität; ungültige Teile bleiben außerhalb des Plans.
Überflüssige Zweierknoten ohne Richtungs-/Breitenwechsel erhalten kein eigenes Modul.
Ruhige Kurven und zusammenhängende Knotenformen im Bild entscheiden; Originalknicke dürfen weichen.
[Gemeinsame Ports](../test/experiments/infrastructure_ported_junction.py),
[OSM2World/SUMO/CARLA](../doc/references/infrastructure/README.md).
