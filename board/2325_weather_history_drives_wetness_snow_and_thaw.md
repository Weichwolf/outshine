Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends: 2172, 2171
Area: world, render, generators
Tags: weather, seasons, materials

# Nässe, Schnee und Schmelze verändern dieselbe Welt

## Sichtbares Ergebnis

Koerbersee zeigt im Winter zusammenhängenden Schnee, freiliegende Steilwände und bläuliche
Schatten; im Frühling Schneereste in Rinnen neben freiem Boden. Rosenheims nasse Dächer
und Husums nasser Kai reagieren anders als trockene Flächen. Kein Jahreszeiten-Farbfilter.
Archivanker: Koerbersee 15.02.2025/15.04.2025/15.10.2024 jeweils 12:00;
Rosenheim 15.01.2025 und 15.07.2025. Quelle/Metadaten in WI 2324.

## Vorhanden und Umsetzung

WeatherProvider hat Wind, Wolken und Sichtweite; Temperatur/Niederschlag, Historie und
Oberflächenzustand fehlen als durchgängiger Vertrag. Materialparameter existieren in 2171.
world/weather erweitert den Snapshot in SI-Einheiten um belegte Temperatur/Niederschlag
und eine deklarierte Zeitspanne; keine Renderer-Interpretation roher Providerwerte.

Ein begrenztes world-Oberflächenfeld hält Feuchte, Schneewasseräquivalent und Schmelzwasser.
Feste grobe Updates integrieren Wettergeschichte; Höhe, Neigung, Exposition, Material und
Sonnenenergie beeinflussen Ablagerung, Ablauf und Schmelze. RAM-Zustand bleibt resident;
persistent nur originale Wetterantworten. Historie fehlt: explizit plausibler Initialzustand,
keine Behauptung realer Schneehöhe und kein frei erfundener ortsspezifischer Winterwert.

Render erhält kompakte Zustandsfelder für Ground und Subjects: Feuchte verändert Albedo/
Rauheit, Schnee Bedeckung und Normalantwort. Zuerst Materialwirkung ohne neue Geometrie;
Akkumulation auf Dächern bleibt konsistent mit Gelände. Dachneigung und Abfluss verhindern
weiße senkrechte Fassaden. Derselbe Zustand informiert Wasser; Eis erst mit belegtem
Temperaturverlauf und eigenem sichtbarem Folgeschritt, nicht automatisch im Dezember.

## Grenzen und Fertig-Kriterien

Keine Welt-Neugenerierung bei Wetterwechsel. Revisionsgebundene Updates, begrenzte Felder
und stetige Übergänge; trockener Nullzustand erhält bisherige Materialien.
Winter/Schmelze/trocken/nass am selben Ort zeigen plausible Verteilung und Reaktion auf
Sonne/Wolken. Neustart mit gleicher Historie/Seed reproduziert den Zustand.
Wasser-/Schneemengen bleiben nichtnegativ; keine Schneeauflage allein durch weißen Shader.
Keine neue Vegetationspflicht vor dieser Lieferung. Acht Places, Zeitwechsel und
720p60-/Speicherbudget prüfen; make format, fokussierte Wetter-/Materialtests, make lint.
