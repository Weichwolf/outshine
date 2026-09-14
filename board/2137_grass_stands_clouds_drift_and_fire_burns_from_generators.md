Type: feature
State: open
Parent: 2169
Area: generators, render, engine
Tags: architecture, vegetation, effects
Depends: 2126, 2123, 2171

# Ground cover and particle effects use native generation and rendering contracts

## Zuständigkeit und Reihenfolge

Nach 2169 erst P5 Vegetation, P6 Partikeleffekte. Wolken gehören ausschließlich
2140/2172. Vorhandene Scatter-/Instanz-/Materialpfade prüfen und verwenden;
keine vorgeschriebene Verzeichnisstruktur als Ersatz für einen Datenvertrag.

## Umsetzung

- Bodenmaterial nach 2171 ohne Pflanzengeometrie abnehmen. Gräser/Stauden/Büschel
  besitzen einen krautigen Generator; verholzte Sträucher teilen Wachstumsverfahren
  mit 2176. Beide liefern native Geometrie und Materialien.
- Standort-/Nutzungsmasken, Gelände sowie Gebäude-/Verkehrsfreiräume bestimmen Placement.
  Seeds in Weltkoordinaten, stabile Tilegrenzen und deterministische Wiederkehr.
  Gemeinsame Artenparameter, Wind, Instancing, LOD und räumliches Streaming.
- Bodenzustand liefert Standort und Dichte, kennt keine einzelnen Halme. Mikrodetail
  unter einem Pixel geht in gefilterte Materialwirkung über. Geometrie nur für sichtbaren
  Zusatznutzen: Silhouette, Gegenlicht, flacher Blickwinkel; Entfernung allein genügt nicht.
- Deklarierte Emitter für Feuer/Rauch/Niederschlag: begrenzte Lebenszeit, feste Partikel-/
  Uploadbudgets, Abbruch/Freigabe, gemeinsame Licht-/Mediumverträge. Wetter aus 2172
  liefert den Antrieb. Kein Screen-Space-Ersatz für räumliche Effekte.
- Öffentliche Generator-/Szenarioverträge nach 2126; Features einzeln schaltbar.
  Keine bestimmten Pflanzen oder Feuer nur für ein einzelnes Webcam-Bild erzeugen.

## Abnahme

- [ ] Boden-only gegen Büschel bei Bewegung/Jahreszeitenwechsel: Mehrwert, Übergänge,
      Overdraw, CPU/GPU und Speicherspitzen getrennt messen, PNGs selbst öffnen.
- [ ] Tileeintritt, Grenzen und Seeds prüfen; keine Pflanzen in Gebäuden/Fahrbahnen.
- [ ] Gegenlicht/Nah-/Fernsicht ohne flackernde Übergänge oder unbeschränkte Arbeit.
- [ ] Emitter stoppen vollständig; Replay/Teilschritte stimmen innerhalb eines vorher
      festgelegten Fehlerbudgets. Sättigung bleibt kontrolliert.
- [ ] Fehlende Belegungsmasken, entkoppelte Windzeit und fehlende Freigabe werden
      durch unabhängige Prüfungen erkannt. Cloud-Abnahme bleibt in 2140.
