Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, world, engine
Tags: buildings, roofs, facades, semantics

# Buildings fit the infrastructure and gain coherent spatial detail

## Ergebnis und Ist
Plausible, vollständige Baukörper mit Höfen/Parts, passenden Dächern/Sonderformen und räumlichen Nahfassaden.
Native Höhenintervalle, Footprints, BuildingMesh, Quellfarben und Terrain-Stempel bestehen.
Körper-/Öffnungspläne im Rohling, schmale Schächte geschlossen; Nutzung geht vor. Dachfamilien/Abnahme offen.
Baseline 325bbe1c3: 13.993 Pläne; Repeat-Payload 1,17–2,04 MiB/37–64 ms; RSS 96–133 MiB. Native Budgets unbewiesen.

## Python-Entwicklung auf buildings
Besitzer dieses Branches: Gebäudeagent, ausschließlich Python in `.venv`; native Übernahme offen.
[Experimente](../test/experiments/buildings/facade_plan.py): `samples`, `facade_plan`, `codec`,
`shell`, `demand`, `facade_geometry` trennen Quellen, fertige Hülle, vollständigen Plan und Kameraemission.
Je ein echtes z14-Tile Wien/Tokyo/Central Park aus der Lieferinventur; SHA-Prüfung vor Nutzung.
Quellcache unter Client-Pref `sources/building-experiments/`; Erwerb nur `samples.py --fetch`.
Form/Höfe/Höhen/Quellattribute/Farbe und alle Fassadenachsen persistieren kameraunabhängig.
Schema 2 speichert fertige Mantel-/Dach-/Unterseitenpuffer; Treffer ohne Provider/Rezept/Triangulierung.
`preview.py --cached-plan <Plan>` spielt echte Rohlinge quellenfrei ab; Hüllen-Coverage aus gespeicherten Parametern.
Vergleich: explizite Split-Liste, vektorisierte Voll-Liste und 48-Byte-Repeat je Fassadenkante;
gleiche vollständige Öffnungen, gemeinsame Quellaufbereitung separat, frische Prozesse/Peak-RSS.
Direkte Hüllen und budgetierte Leibungen teilen metrische Achsen und exakte Box-Coverage.
Bedarf vor Expansion: Facing/Frustum, 1 m Bewegungsreserve im Preview und projizierte Leibungstiefe.
0,5 px Detailgrenze ist eine Versuchshypothese, kein Welt-/Gerätegate. Drehung deckt alle Kanten ab.
50.000 Öffnungen je Körper begrenzen lokale Versuchsemission; Überlauf erhält Hülle samt Grund.
Ungültige Quellen/Rezepte protokollieren; schmale Wandstreifen erhalten keine durchstoßende Leibung.
`verify.py`: Flächenoracle, Hof/L-Form, Unterkante, Farb-/Höhenvorrang, Cachehit und frischer Offline-Prozess.
Renderings/Messprodukte: `build/experiments/buildings/{preview,measurements}/`; Blender-Eevee via CLI.
Diagnosehüllen mischen dieselben Wand-/Rahmen-/Glas-BSDFs nach Coverage ([Blender](https://docs.blender.org/manual/en/4.3/render/shader_nodes/shader/mix.html)).
Ausbau: CGA-Pavillon/Staffelung, bis drei Geschossbänder; Basis/Maximalhöhe/Höfe erhalten.
GEOS-Mitres strikt innen; nur nutzbare Obergeschosse, sonst gültige Basis. Teile/Tile-Nähte offen.
Direkte Bandflächen/2D-Differenzen gegen [Manifold](https://github.com/elalish/manifold)-3D-Union gleicher Prismen.
[Showcase](../test/experiments/buildings/showcase.py): `--publish` prüft PNG/Report-Hash und publiziert nach
`/tmp/outshine-buildings-showcase/`; `--serve --port 8766` zeigt Bilder/Kosten und deren Commit, Polling 5 s.
Jeder geprüfte Fortschritt: Commit/Push, dann Publish. Nutzungskontakte/Verdeckung/Schatten/BRDF bleiben offen.

## Besitzer und Lieferung
OSM-Erweiterung normalisiert tatsächliche Eigenschaften; building besitzt Form-/Kontaktpläne.
world/Renderer konsumieren native Assets ohne Tags/Generator-Eingabeverträge. 2336 besitzt LOD.
Asset-Rohlinge (2280): Typ/Höhe/Dach/Material/Fassade/Seed; budgetierte Nahdetails daraus ohne neue Quellen.
2281 liefert Infrastruktur vor Gebäudeplatzierung: Zugänge, Straßenkanten und Bezugshöhen.
Fehlender Vertrag: native Korridorflächen/Höhenintervalle und überdachte Verkehrsfreiräume aus 2281.
Grundriss gegen geplante Fahrbahn/Gehwege/Seitenraum räumlich vorauswählen; konfliktfreie Bauten unverändert.
GEOS-Differenz gegen Weglassen: betroffener Flächenanteil und Restform entscheiden; Randkontakt löscht nichts.
Nur überlappende Höhen schneiden. Brücken/Tunnel und freie Verkehrsports in Bahnhallen erhalten.
Durchfahrten schneiden betroffene Geschosse; Boden, Leibungen/Decke und Portale schließen.
Innenringe/Blöcke/Wegzyklen liefern Höfe, Tunnel/Überdeckung/Wegefortsetzung Durchgänge. Zugang/Verkehrsart
erhalten; brauchbare Reste samt Höfen/Parts/Tags behalten, unbrauchbare dürfen entfallen.
Infrastrukturplan im Rohling binden, danach Terrain darunterlegen; Treffer wiederholt keine Differenz.
[2D-Vergleich](../test/experiments/infrastructure_building_clearance.py): GEOS mit räumlicher Vorauswahl
gegen globale Schnittfläche, Schnitt gegen Weglassen; echte Stadtstichproben. Höhen-/Hallplan noch offen.

## Form und Quellsemantik
- [Lieferkatalog](../doc/references/data/osm/inventory.md), 2341: weitere Layer/Zuordnung.
  Grundrisse/Höhen/Farben nutzen; fehlende Typen aus Form/Lage/Umfeld stabil ergänzen.
- Klassen besitzen getrennte Rezepte: Haus/Reihe (Geschosse, Achsen, einfaches Dach),
  Wohnblock/Büro/Hotel (Höfe/Parts, regelmäßige Öffnungen), Halle/Lager (Tore, wenige Öffnungen),
  Schornstein (geschlossener, meist runder/verjüngter Schaft), Silo/Tank (Behälter), Wasserturm
  (Schaft plus Behälter), Kirche (Langhaus plus Turm), Garage/Schuppen (Tor), Gewächshaus
  (Tragwerk plus Glas), erhöhte Parts/Brücken (freie Unterkante). Unbekannte Nutzung: einfacher Ersatz.
  Gelieferte Form/Material schlagen Ergänzungen; Baujahr beweist kein Material.
- Weitere Sonderrezepte: Kühlturm (offene gekrümmte Schale), Windrad (Mast/Gondel/Rotor),
  Solarpark (gebündelte geneigte Modulreihen), Strommast/Leitung und technische Anlagen.
  Attribute persistieren und vor Ergänzung auswerten; fehlende Klassen aus Form/Lage/Umfeld ergänzen.
  Rendervergleiche verbessern allgemeine Rezepte; die Liste ist offen, keine Place-Sonderfälle.
- Rosenheim: zwei kleine hohe Grundrisse mit `render_height`/`render_min_height` (65/80 m),
  ohne Nutzung/Material; 80 m rundlich achteckig, 65 m viereckig. Tags im nativen Produkt erhalten.
  Unklassifizierte schmale Schächte konservativ geschlossen; Wohnnutzung geht vor, kein Klassenbeweis.
- Flensburgs falsche Kuppel: gespeicherte Rundheitsprüfung schließt konkave Grundrisse aus.
  Dach/Mantel teilen die Wahl, bewährte Dächer erhalten. Komplexe Formen nur mit belegten Parametern,
  vollständigen überschneidungsfreien Flächen; einfache robuste Dächer bevorzugen.
- Gelieferte Attribute erhalten; ausdrücklich gelieferte Schlüssel schlagen normalisierte Aliase.
  Höhe/Unterkante/Geschosse, Klasse/Nutzung, Dach, Material/Farbe und Parts nutzen, soweit vorhanden.
  [OpenMapTiles](https://github.com/openmaptiles/openmaptiles/blob/master/layers/building/building.yaml)
  liefert nicht alle OSM-Tags. Unbekanntes und deterministische Ergänzungen bleiben unterscheidbar.
- Sonderbauten: passende Körper ohne Wohnfassade, Klassenherkunft unterscheidbar.
  Schlankheit/19 m Höhe beweisen keine Klasse. Unbekannte Schächte ohne erfundene Spitzdächer;
  gelieferte Dachangaben gehen vor.
- Multipolygone/Höfe/Parts erhalten, Eltern/Parts nicht doppeln; vorzeichenbehaftete Unterkanten relativ
  zum Gelände und erhöhte/unterirdische Intervalle erhalten. Falsche Bodenstempel entfallen; Höhenkonflikte markieren.
- Kompakte Regeln: Geschosse/Achsen/Öffnungen, Eingang/Dach/Kontakt. Regionale Grammatik ergänzt;
  keine Fotorekonstruktion/Place-Geometrie. Kontakte ohne Fassadenmesh, gemeinsame Pläne/Straßenqualität erhalten.
- Massierung/Öffnungsplan trennen. Form bestimmt Silhouette/Dach, Nutzung Sonderbau/Öffnungen;
  unbekannte breite Körper mit Wohn-/Büroöffnungen, belegte Versorgungstürme geschlossen.
  Gezielte Regel-/Produktversion, alle LODs/Treffer teilen Plan; keine pauschale Shader-Fensterfreigabe.
- Rundliche geschlossene Schächte: elliptische Mantelnormalen ohne Zusatzgeometrie, Grundriss/Höhe/Treffer erhalten.
  Fernflächen interpolieren dieselben Ecknormalen. Viereckige Schächte/Wohnfassaden bleiben eben.
- Gemeinsame randtreue Polygontriangulierung für Gebäude/Wasser mit Löchern: GEOS ≥3.10 über
  System-C-API; Library-Typen privat. Ungültige Polygone nicht durch Flächenverlust kaschieren.

## Nahdetail und Erscheinung
- Öffnungen nur aus Nutzung/Geschossen/Achsen und verfügbarem Bildbedarf. Kontaktwände/Giebel,
  belegte Hallen/Versorgungstürme geschlossen erhalten; kein gleichförmiges Fensterraster.
- Laibungen/Rahmen/Traufen/Balkone/Gauben nahe mit Tiefe; Wiederholungen instanzieren. Fernhüllen filtern
  denselben Öffnungsplan ohne Detaildreiecke mit mittlerer Coverage/Helligkeit; Fine expandiert gezielt.
- Bounds umfassen Form/Details; Bewegung/Projektion und zusätzlicher Fehler begrenzen Wiederverwendung.
  Bildfelder haben eigene Gültigkeit; CPU-Vergleiche allein verkleinern keine LOD-Schranke.
- CssColour: CSS-Namen/Hex, sechs Hexziffern ohne `#`; `building:colour` vor `colour`, Originalwert erhalten.
  Ungültig/teiltransparent melden. Lineare Wandfarbe ersetzt Palette, keine Glas-/Rahmen-/Tür-/Dachfarbe.
  Fernverbände flächengewichtet in linearem RGB mischen; Herkunft erhalten.
- Konstante Baukörperfarbe kompakt in Material-/Batchdaten, nicht vier Floats je Ecke. Optionale
  Streams reservieren keine fremden Lücken (2188). Metrische UVs bis zum Shader erhalten (2171).

## Bewährte Verfahren
[Instant Architecture](../doc/references/buildings/siggraph/2003-instant-architecture.pdf), SIGGRAPH 2003;
[CGA Shape](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf), SIGGRAPH 2006:
begrenzte Split-Regeln; [F-shade, EGSR 2010](../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf)
motiviert wahlfreien Zugriff auf kompakte Fassaden. Übertragung ist ein Repeat-Pilot, kein Grammatikinterpreter.

## Abnahme
Wien/Feldkirch: vollständige, an Infrastruktur angepasste Baukörper/Höfe/Parts. Rosenheim/Flensburg: passende Sonderklassen,
plausible Dächer. Nahöffnungen räumlich, Fernstadt stabil/gebündelt; kein Aufbau unsichtbarer Details.
Bildgewinn/Kosten gemeinsam im AGENTS-Profil; Assethit wiederholt keine Anreicherung.
