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
Aktueller Auftrag: OSM-Gebäude/Architektur so genau wie die Provider-API ermöglicht; Esri/CGA-Rezepte als Python-Basis.
Native Höhenintervalle, Footprints, BuildingMesh, Quellfarben und Terrain-Stempel bestehen.
Gelieferte Form/Attribute bestimmen die Geometrie. Frühere Bauhaus-/PV-/Balkonversuche sind keine Gestaltungsvorgabe.
Providerprüfung: vier Gebäudefelder; 13.955 gültige Quellkörper, 39 `hide_3d`-Auslassungen und ein ungültiges Höhenintervall. Native Budgets unbewiesen.

## Python-Entwicklung auf buildings
Besitzer dieses Branches: Gebäudeagent, ausschließlich Python in `.venv`; native Übernahme offen.
[Experimente](../test/experiments/buildings/facade_plan.py): Quellen → Massierung/Fassade/Ausstattung → fertige Hülle → Kamerabedarf.
Gepinnte z14-Tiles Wien/Tokyo/Central Park mit SHA-Prüfung; Client-Pref `sources/building-experiments/`, Erwerb nur `samples.py --fetch`.
Schema 3: Form/Höfe/Höhen/Quellattribute/Farbe/Achsen und fertige Mantel-/Terrassen-/Unterseiten-/Ausstattungspuffer ohne Hit-Neubau.
`preview.py --cached-plan <Plan>` spielt echte Rohlinge quellenfrei ab; Hüllen-Coverage aus gespeicherten Parametern.
Gleiche Öffnungen als Split-/Voll-Liste oder 48-Byte-Repeat; Quellaufbereitung separat, frische Prozesse/Peak-RSS.
Hülle/Leibungen teilen metrische Achsen/Box-Coverage; Facing/Frustum, 1 m Bewegungsreserve und projizierte Tiefe vor Expansion.
0,5 px Detailgrenze ist eine Hypothese, kein Gerätegate; Drehung deckt alle Kanten ab. Über 50.000 Öffnungen: Hülle samt Grund.
Ungültige Quellen/Rezepte protokollieren; schmale Wandstreifen erhalten keine durchstoßende Leibung.
`verify_provider.py`: alle Quellkörper/Volumen, PRT-Achsen/Höfe/sRGB, 48 Rohlinge × acht Cache-Posen; frühere Fassaden-/Massierungstests bleiben.
Frühere Diagnosehüllen mischen Wand-/Rahmen-/Glas-BSDFs nach [Coverage](https://docs.blender.org/manual/en/4.3/render/shader_nodes/shader/mix.html).
Standardrezept: Grundriss/Höfe und geliefertes Höhenintervall erhalten, keine erfundenen Staffelungen oder Ausstattung.
Frühere Stilrezepte bleiben explizite Vergleiche. Numerische Nulllängen bis 1e-8 m normalisieren.
[`provider_compare.py`](../test/experiments/buildings/provider_compare.py): Live-TileJSON, 352-Tile-Inventur und drei frische Kontrollkacheln.
Esri `InitialShapeBuilder`: freie Regelattribute nach Name/Typ; keine universelle Gebäude-Tagtabelle. Grundriss/Höfe als Startgeometrie.
OpenFreeMap `building`: `render_height`/`render_min_height` → Höhe/Unterkante, `colour` → sRGB-Wandfarbe, `hide_3d` → Auslassung mit Grund.
Höhen sind provideraufbereitet; sie beweisen keine originalen `height`-/Geschosstags. Dachform/Nutzung/Material fehlen in diesem Layer.
POI/Landuse/Transport sind Kontext: keine pauschale Übertragung auf ein ganzes Gebäude; Straßenbreite braucht konstruierten Straßenplan.
Früherer `massing_benchmark.py`: direkte Bandflächen gegen [Manifold](https://github.com/elalish/manifold)-Union gleicher Prismen; direkte gewählt (2,2–2,4×, gleicher Payload).
[Showcase](../test/experiments/buildings/showcase.py): `--publish` prüft PNG/Report-Hash und publiziert nach
`/tmp/outshine-buildings-showcase/`; Port 8766 zeigt letzte Varianten und interaktive Körper-/Fassadenkosten samt Commit.
Jeder geprüfte Fortschritt: Commit/Push, dann Publish. [VisusCity](https://visusarts.itch.io/visuscity-vol2) ist Bildmaßstab.

## Besitzer und Lieferung
OSM-Erweiterung normalisiert tatsächliche Eigenschaften; building besitzt Form-/Kontaktpläne.
world/Renderer konsumieren native Assets ohne Tags/Generator-Eingabeverträge. 2336 besitzt LOD.
Asset-Rohlinge (2280): Typ/Höhe/Dach/Material/Fassade/Seed; budgetierte Nahdetails daraus ohne neue Quellen.
2281 liefert Zugänge/Straßenkanten/Bezugshöhen; nativ fehlen Korridorflächen/Höhenintervalle/überdachte Verkehrsfreiräume.
Grundriss gegen geplante Fahrbahn/Gehwege/Seitenraum räumlich vorauswählen; konfliktfreie Bauten unverändert.
GEOS-Differenz gegen Weglassen: betroffener Flächenanteil und Restform entscheiden; Randkontakt löscht nichts.
Nur überlappende Höhen schneiden. Brücken/Tunnel und freie Verkehrsports in Bahnhallen erhalten.
Durchfahrten schneiden betroffene Geschosse; Boden, Leibungen/Decke und Portale schließen.
Innenringe/Blöcke/Wegzyklen liefern Höfe, Tunnel/Überdeckung/Wegefortsetzung Durchgänge. Zugang/Verkehrsart
erhalten; brauchbare Reste samt Höfen/Parts/Tags behalten, unbrauchbare dürfen entfallen.
Infrastrukturplan im Rohling binden, Terrain darunterlegen; Treffer wiederholt keine Differenz.
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
- Rosenheim: `render_height`/`render_min_height` 65/80 m, ohne Nutzung/Material; 80 m achteckig, 65 m viereckig.
  Tags erhalten; unbekannte schmale Schächte geschlossen, Wohnnutzung geht vor; Form beweist keine Klasse.
- Flensburgs falsche Kuppel: gespeicherte Rundheitsprüfung schließt konkave Grundrisse aus.
  Dach/Mantel teilen die Wahl, bewährte Dächer erhalten. Komplexe Formen nur mit belegten Parametern,
  vollständigen überschneidungsfreien Flächen; einfache robuste Dächer bevorzugen.
- Gelieferte Attribute erhalten; ausdrücklich gelieferte Schlüssel schlagen normalisierte Aliase.
  Höhe/Unterkante/Geschosse, Klasse/Nutzung, Dach, Material/Farbe und Parts nutzen, soweit vorhanden.
  [OpenMapTiles](https://github.com/openmaptiles/openmaptiles/blob/master/layers/building/building.yaml): nicht alle OSM-Tags; Ergänzungen unterscheidbar.
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
motiviert wahlfreien Zugriff auf kompakte Fassaden.
[CitySample a0981eb](https://github.com/fate-apple/CitySample/tree/a0981eb52322eab75db2507a23879de9f30110a0): HLOD aus Kollisionskörpern,
Mesh-/Materialinstanzen; Houdini-Graphen fehlen. [Building Tools 0216534](https://github.com/ranjian0/building_tools/tree/0216534645b15b3f8aac0bd1d7e6674e2972b9b3): Python-Bauteile/Skelettdächer.
[Labs Patterns](https://www.sidefx.com/docs/houdini/nodes/sop/labs--building_from_patterns-1.0.html): Module/Overrides.
[CityEngine-SDK](https://github.com/Esri/cityengine-sdk) unter `~/Downloads/esri_ce_sdk-3.4.12206`, Archiv-SHA `1cad08b973a1`; PRT-PDF optisch geprüft.
PRT-Core als Referenz: Offset/Skelett/Triangulierung, Kantenattribute, Material-/Geometrieinstanzen; Runtime braucht kompilierte CGB-Regeln.

## Abnahme
Wien/Feldkirch: Infrastruktur/Baukörper/Höfe/Parts; Rosenheim/Flensburg: Sonderklassen/Dächer. Nahöffnungen räumlich,
Fernstadt stabil/gebündelt; kein Aufbau unsichtbarer Details.
Bildgewinn/Kosten gemeinsam im AGENTS-Profil; Assethit wiederholt keine Anreicherung.
