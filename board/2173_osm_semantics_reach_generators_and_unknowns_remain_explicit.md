Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, world, engine
Tags: buildings, roofs, facades, semantics

# Complete buildings preserve source form and gain spatial detail

## Ergebnis und Ist
Vollständige Grundrisse/Höfe/Parts, plausible Dächer/Sonderbauten und räumliche Nahfassaden.
Native Höhenintervalle, Footprints, BuildingMesh und Terrain-Stempel bestehen. Klassen-/
Dachpläne bleiben lückenhaft; prozedurale Formfamilie ist keine belegte Nutzung.
Quellfarben erreichen den Renderer. Öffnungspläne existieren; die visuelle Abnahme bleibt offen.
Baukörper und Öffnungsplan sind getrennt; der native Rohling speichert beide. Breite unbekannte
Hall-/Turmkörper erhalten regelmäßige Öffnungen, schmale Schächte bleiben geschlossen.
Gelieferte Nutzung geht vor; Sonderformen und weitere Rezeptfamilien bleiben auszubauen.

## Besitzer und Lieferung
OSM-Erweiterung normalisiert tatsächliche Eigenschaften; building besitzt Form-/Kontaktpläne.
world/Renderer konsumieren native Assets ohne Tags/Generator-Eingabeverträge. 2336 besitzt LOD.
Zuerst vollständige Formen/Höhen/Sonderklassen in Wien/Rosenheim, danach Dächer und Nahöffnungen.
Asset-Rohlinge (2280) enthalten ergänzten Typ, Höhe, Dach, Material und Fassadenplan/Seed.
Cachehits laden diese Basis; Nahdetails entstehen budgetiert daraus ohne Quellen oder neues Raten.

## Form und Quellsemantik
- [Inventur und vollständiger Lieferkatalog](../doc/references/data/osm/inventory.md): 2341
  besitzt zusätzliche Layer/Zuordnung. Gelieferte Grundrisse/Höhen/Farben sofort nutzen;
  fehlende Typen aus Form, Lage und Umfeld stabil ergänzen, ohne Quellenblocker.
- Klassen besitzen getrennte Rezepte: Haus/Reihe (Geschosse, Achsen, einfaches Dach),
  Wohnblock/Büro/Hotel (Höfe/Parts, regelmäßige Öffnungen), Halle/Lager (Tore, wenige Öffnungen),
  Schornstein (geschlossener, meist runder/verjüngter Schaft), Silo/Tank (Behälter), Wasserturm
  (Schaft plus Behälter), Kirche (Langhaus plus Turm), Garage/Schuppen (Tor), Gewächshaus
  (Tragwerk plus Glas), erhöhte Parts/Brücken (freie Unterkante). Unbekannte Nutzung bleibt
  ein einfacher Ersatz. Gelieferte Form/Material schlagen Ergänzungen; Baujahr beweist kein Material.
- Weitere Sonderrezepte: Kühlturm (offene gekrümmte Schale), Windrad (Mast/Gondel/Rotor),
  Solarpark (gebündelte geneigte Modulreihen), Strommast/Leitung und technische Anlagen.
  Alle gelieferten Attribute persistieren; relevante Eigenschaften vor Ergänzungen auswerten.
  Die Beispiele sind nicht abschließend. Fehlende Klassen heuristisch aus Form/Lage/Umfeld
  ergänzen; Rendervergleiche verbessern allgemeine Rezepte, keine Place-Sonderfälle.
- Rosenheim: native Lieferdaten der beiden hohen kleinen Grundrisse enthalten ausschließlich
  `render_height`/`render_min_height` (65/80 m), keine Nutzung oder Materialangabe. Der 80-m-Körper
  hat einen rundlichen achteckigen Grundriss; der 65-m-Körper einen viereckigen. Alle gelieferten
  Tags bleiben im nativen OSM-Produkt. Unklassifizierte schmale Schächte bleiben konservativ
  geschlossen; explizite Wohnnutzung geht vor. Das ist ein Ersatz, keine belegte Schornsteinklasse.
- Dächer einfach und robust halten. Flensburg rechts erhielt fälschlich eine Kuppel, weil
  Füllgrad/Seitenverhältnis konkave Grundrisse als rund einordneten. Die gespeicherte
  Rundheitsprüfung schließt konkave Grundrisse aus; Dach und Mantel teilen dieselbe Wahl.
  Bewährte Dachformen erhalten.
  Komplexe Formen nur mit belegten Parametern und vollständigen, überschneidungsfreien Flächen.
- Gelieferte Attribute erhalten; ausdrücklich gelieferte Schlüssel schlagen normalisierte Aliase.
  Höhe/Unterkante/Geschosse, Klasse/Nutzung, Dach, Material/Farbe und Parts nutzen, soweit vorhanden.
  [OpenMapTiles](https://github.com/openmaptiles/openmaptiles/blob/master/layers/building/building.yaml)
  liefert nicht alle OSM-Tags. Unbekanntes und deterministische Ergänzungen bleiben unterscheidbar.
- Gelieferte oder plausibel abgeleitete Sonderbauten erhalten passende Baukörper ohne
  Wohnhausfassade; die Herkunft der Klasse bleibt unterscheidbar.
  Schlankheit allein beweist keine Klasse; 19 m Höhe allein macht keinen Turm. Unbekannte schlanke
  Bauten bekommen keine erfundenen Spitzdächer. Gelieferte Dachangaben gehen vor.
- Multipolygone/Höfe/Parts erhalten, Eltern/Parts nicht doppeln. Unterkanten sind vorzeichenbehaftet
  relativ zum Gelände; erhöhte/unterirdische Parts erhalten ihr Intervall und falsche Bodenstempel
  entfallen. Widersprüchliche Höhen bleiben von Ergänzungen unterscheidbar.
- Kompakte Regeln bestimmen Geschosse/Achsen/Öffnungen, Eingangsseite, Dach und Kontakt.
  Regionale Grammatik ist Ergänzung, keine Fotorekonstruktion/Place-Geometrie. Kontakte ohne
  Fassadenmesh erzeugen; alle LODs teilen denselben Plan. Straßenqualität bleibt erhalten.
- Massierung und Öffnungsplan trennen. Form bestimmt Silhouette/Dach, belegte Nutzung bestimmt
  Sonderbau-/Öffnungsregeln; unbekannte breite Körper bekommen plausible Wohn-/Büroöffnungen.
  Bekannte Industrie-/Versorgungstürme bleiben geschlossen. Regel-/Produktversion gezielt ändern;
  alle LODs und native Treffer verwenden denselben Plan, keine pauschale Shader-Fensterfreigabe.
- Rundliche geschlossene Schächte erhalten elliptische Mantelnormalen ohne zusätzliche Geometrie.
  Grundriss, Höhe und geometrische Treffer bleiben erhalten; Fernflächen interpolieren dieselben
  Ecknormalen wie das Nahmesh. Viereckige Schächte und belegte Wohnfassaden bleiben eben.
- Gemeinsame randtreue Polygontriangulierung für Gebäude/Wasser mit Löchern: GEOS ≥3.10 über
  System-C-API; Library-Typen privat. Ungültige Polygone nicht durch Flächenverlust kaschieren.

## Nahdetail und Erscheinung
- Öffnungen nur aus Nutzung/Geschossen/Achsen und verfügbarem Bildbedarf. Kontaktwände/Giebel,
  belegte Hallen/Versorgungstürme geschlossen erhalten; kein gleichförmiges Fensterraster.
- Laibungen, Rahmen, Traufen, Balkone/Gauben erhalten nahe Tiefe; wiederholte Teile instanzieren.
  Fernhüllen filtern denselben Öffnungsplan ohne einzelne Detaildreiecke. Nahglas und Hüllen
  erhalten mittlere Coverage/Helligkeit. Fine bedeutet keine pauschale Expansion aller Details.
- Bounds umfassen Form/Details; Bewegung/Projektion und zusätzlicher Fehler begrenzen Wiederverwendung.
  Bildfelder haben eigene Gültigkeit; CPU-Vergleiche allein verkleinern keine LOD-Schranke.
- Gemeinsamer CssColour-Parser: alle CSS-Namen/Hexfarben, `building:colour` vor Alias `colour`.
  OSM-Gebäudefarben zusätzlich als sechs Hexziffern ohne `#` lesen; Originalwert erhalten.
  Ungültige/teiltransparente Wandfarben melden. Lineare Wandfarbe ersetzt Palette, keine Glas-/
  Rahmen-/Tür-/Dachfarbe. Fernverbände mischen flächengewichtet in linearem RGB; Herkunft bleibt klar.
- Konstante Baukörperfarbe kompakt in Material-/Batchdaten, nicht vier Floats je Ecke. Optionale
  Streams reservieren keine fremden Lücken (2188). Metrische UVs bis zum Shader erhalten (2171).

## Bewährte Verfahren
[Instant Architecture, SIGGRAPH 2003](../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf):
begrenzte Regeln aus wenigen Parametern, keine universelle Grammatikengine. [Weitere Quellen](../doc/references/README.md).

## Abnahme
Wien/Feldkirch: vollständige Formen/Höfe/Parts. Rosenheim/Flensburg: korrekte gelieferte Sonderklassen,
plausible Dächer. Nahöffnungen räumlich, Fernstadt stabil/gebündelt; kein Aufbau unsichtbarer Details.
Assethit wiederholt keine Anreicherung. Bildgewinn und Kosten gemeinsam im AGENTS-Profil prüfen.
