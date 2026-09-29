Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Area: client, world, render
Tags: webcam, visual
Depends:

# Acht Webcam-Szenen treiben die sichtbare Entwicklung

## Ergebnis und Auswahl

Der Standardkatalog enthält genau acht Szenen mit realer Foto-Webcam und Archiv.
Jede Lieferung wird dort auf Bildgewinn, Vollständigkeit und 720p60 geprüft.

| Place | Kamera bei foto-webcam.eu/webcam/ | Bildaufgabe |
|---|---|---|
| Rosenheim | rosenheim | Dächer, Schornstein/Kirchtürme, Stadt vor Alpen, Nachtlicht |
| Flensburg | flensburg | Backstein, Kirchturm, Hafen, Laubwechsel und Fernsicht |
| DarmstadtWest | darmstadt-west | Nahe Dachlandschaft, unterschiedliche Fassaden, flacher Horizont |
| Wien | wien | Dichte vollständige Stadt, Donau, Brücken, Fern-LOD |
| Husum | husum-hafenklappbruecke | Kai, Giebelhäuser, Hafenwasser, Brücke und Nässe |
| Feldkirch | feldkirch | Stadt im Tal, Fluss und Straßen, Hanganschlüsse und Tiefenstaffelung |
| Malcesine | malcesine | See, Insel/Ufer, felsige Gegenküste, Wasserreflexion und Dunst |
| Koerbersee | koerbersee | Alpine Grate, Schnee/Schmelze, Bergsee, tiefe Wolken; Wald zuletzt |

## Referenzbefund und Grenze

Archivsichtung: Winter/Frühling/Sommer/Herbst sowie Morgen/Mittag/Abend/Nacht bei
Rosenheim; Jahreszeiten bei Flensburg und Koerbersee; Sommeransichten aller acht Kameras.
Gesicherte Bilder und Einzelmetadaten: build/shots/reference/webcams/archive-review-20260929/.
Koerbersee 15.01.2025 ist verdeckt und kein Geometrieoracle; 15.02.2025 zeigt klaren Winter.
Temperatur am Kameragehäuse, Belichtungsautomatik, Tropfen und Sensorflecken sind keine
verlässlichen Weltparameter. Archivzeit, Abrufzeit und Wetterinterpretation getrennt halten.

## Implementierung

src/assets/places besitzt acht deklarative Szenarien und ein Quellenmanifest.
Bestehende Szenarien außerhalb der Auswahl bleiben unter src/assets/diagnostic-places
für explizite Regressionen erhalten; alte rote Befunde werden dadurch nicht grün.
PlaceCamera lädt weiterhin Verzeichnisinhalt; keine fest kodierte Ortsliste im Renderer.
ClientShot nimmt für erhaltene Diagnosen einen expliziten Katalog; acht Render-Fälle
bleiben in integration/places. Erhaltene Diagnose-Renderfälle liegen unter
test/outshine/integration/world_regressions; die Engine verweist nicht auf ihre Tests. Neue Flensburg-Kamera nutzt veröffentlichte Position/Bearing;
Höhendatum, Pitch und aus Bildwinkel abgeleiteter FOV bleiben bis WI 2170 vorläufig.

2170 fixiert Pose/Intrinsics vor geometrischen Bildurteilen. Archivbilder sind Referenz,
niemals Skybox, Textur, Höhenkarte oder Quelle für ortsspezifische Bauwerke.
World-Inputs bleiben OSM, DEM, Wetter/Zeit und Kamera; fehlende Details sind plausible
allgemeine Generatorentscheidungen. Schiffe, Autos und exakte Wolken sind nicht rekonstruierbar.

## Fertig-Kriterien

Client places zeigt genau acht Namen; jede Szene hat erreichbare Archivquelle und
proveniente Kameraangaben. Roundtrip und negative Katalogfälle bestehen. Acht echte
Render-Fälle halten vollständigen Preload bis zehn Sekunden und 60 Frames/360° in einer
Sekunde; p99 <= 1000/60 ms. Bilder selbst öffnen und Quellenfit gesondert beurteilen.
make format; test_place_catalog.py; betroffene Place-Suite; vollständiger make lint.
