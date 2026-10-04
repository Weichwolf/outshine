Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends:
Area: generators, world, render
Tags: water, terrain, coastline, contacts

# Water has coherent levels, shorelines and its own geometry

## Ergebnis und Ist
Meer/Fluss/See mit richtiger Höhe, Ufern/Inseln, Wellen und Transparenz. WaterField,
WaterSurfaceBuilder/WaterDepth und Terrain-Stempel bestehen; Flensburgs falsche Pegel/
überflutete Gebäude bleiben ein Geometriefehler. Husums Hafen zeigt Pegelstufen,
Malcesine unplausible Uferflächen. Erkannte Meeresflächen teilen den mittleren ASL-Pegel
ohne Terrain-Abfragen; getrennte Hafenflächen und ihre Bett-/Uferstempel bleiben falsch.
Wasser ist kein Terrain-Klassenersatz.

## Besitzer und nächste Lieferung
OSM-Erweiterung besitzt Wasser-/Küstenringe und Randidentität; Wasser-/Terrain-Generatoren
Körper, Bett und Ufer; world native WaterBody-Produkte, Renderer Licht/Animation.
Zuerst Flensburgs Pegel-/Kontaktursache vom gelieferten Ring und Höhendatum bis zum Bild
korrigieren. Vorhandene Quellen erlauben die Diagnose; kein Warten auf Abschluss von 2280.
OpenMapTiles liefert `class=ocean`, der Adapter normalisiert zu `kind=ocean`.
WaterField legt diese Meeresflächen auf den gemeinsamen mittleren ASL-Pegel 0 m.
Unbekannte Tide bleibt unmodelliert. Seen behalten die eigene Höhenermittlung;
ein Hafenbecken ist nicht automatisch Meer. Nächster Schritt: tatsächliche Verbindungen
zwischen Hafenbecken und Meer sowie die daraus abgeleiteten Bett-/Uferstempel korrigieren.
Keine pauschale Nullhöhe für Flüsse, Seen oder Schleusenbecken.
WaterField erhält die vorhandene `ProviderFeatureId` samt Quell-/Layer-Namensraum.
Fragmente desselben stehenden Gewässers teilen bei überlappenden Wasserabschnitten an
Tile-Grenzen einen Pegel aus gemeinsamen Höhensamples; Wassergeometrie und Bett nutzen ihn.
Gleiche ID allein verbindet keine getrennten Komponenten; Flussflächen behalten Gefälle.
Gegenüberliegende vollständige Außenkanten verbinden auch verschiedene Quellobjekte desselben
Quell-/Layer-Namensraums. Eine verbundene Meeresfläche bestimmt den gemeinsamen mittleren Pegel.
Husums sichtbarer Hafen besteht aus angrenzenden `lake`-Objekten mit Kantenpfad zum Meer,
nicht aus den westlich gelegenen `dock`-Flächen. Quellklasse allein beweist keine Meer-Verbindung.
Nächster Schritt: Ufer und Bett folgen den Körpergrenzen; interne Objekt-/Clipping-Kanten
sind keine Ufer. Inseln und trockene Lücken erhalten. Explizite Sperren müssen Verbindungen
unterbrechen; dafür fehlt noch der Barrierenvertrag. Fehlende Sperrendaten sind kein Sperrennachweis.
Der weltweite Abschluss offener Küsten ist noch zu entscheiden, daher `planned`.

## Verfahren
- Höhendatum von DEM, Kamera und Wasser abgleichen. NoData ist kein Nullboden, DSM-Dach/
  Bewuchs kein nackter Boden. Keine pauschale Stadtglättung als Korrektur.
- Ringe/Höfe/Inseln über Tile-Ränder zu Komponenten verbinden; Nachbar-/Abschlussbedarf
  explizit. Clipped MVT-Ringe beweisen keinen vollständigen Gewässerkörper.
- Ein Pegelmodell je Körper: Meer im passenden Datum, Seen mit begründeter Höhe, Flüsse
  entlang Gefälle. Unbekannte Tide benennen. Keine zufälligen Tile-Pegel/Grundwassermeshes.
- Wasserfläche, Bett und Ufer gemeinsam konstruieren; Terrain schneidet Wasser am Ufer.
  Gebäude-/Straßenstempel respektieren Wasser und schwebende Bauwerke. Grenzen teilen
  Kontaktrelief; Kapazitätsfehler melden, keine Ringe oder Wasserflächen still verlieren.
- Windwellen, Fresnel, Tiefenabsorption, Reflexion/Transmission und Uferschaum auf demselben
  Wasserprodukt. Licht aus 2155, Wetter/Eis aus 2172 später anschließen, keine zweite Lichtwelt.

## Abnahme
Zuerst Flensburg ohne Wasserfälle/überflutete Gebäude, dann Husum/Malcesine/Koerbersee.
Korrekte Pegel, Ufer/Inseln und Wasser unter Brücken; Wellen ändern keine Grundpegel/Abdeckung.
Vorher/Nachher öffnen und AGENTS-Budget halten; Küstenabschluss weltweit gesondert belegen.
