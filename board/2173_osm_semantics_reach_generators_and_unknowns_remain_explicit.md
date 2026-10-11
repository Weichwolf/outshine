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
Ziel: Esri-CityEngine-Qualität/-Vielfalt in Python; Providerform erhalten, fehlende Architektur kohärent ergänzen. Auftrag bleibt offen.
Native Höhenintervalle, Footprints, BuildingMesh, Quellfarben und Terrain-Stempel bestehen.
Gelieferte Form/Attribute bestimmen die Geometrie. Frühere Bauhaus-/PV-/Balkonversuche sind keine Gestaltungsvorgabe.
16 Providerlayer/137 POI-Klassen/476 Unterklassen berücksichtigt; 3.411 Gebäudezuordnungen zu wenigen Fassadenmustern, keine Rezeptvielfalt. Architekturqualität fehlt.

## Python-Entwicklung auf buildings
Besitzer dieses Branches: Gebäudeagent, ausschließlich Python in `.venv`; native Übernahme offen.
[Experimente](../test/experiments/buildings/facade_plan.py): Quellen → Massierung/Fassade/Ausstattung → fertige Hülle → Kamerabedarf.
Gepinnte z14-Tiles Wien/Tokyo/Central Park mit SHA-Prüfung; Client-Pref `sources/building-experiments/`, Erwerb nur `samples.py --fetch`.
Schema 4: fertige Hülle, Form/Höfe/Höhen/Farbe, Kontextzeugen/Teilnutzungen und Rezept; Schema-3-Referenzen bleiben lesbar.
`preview.py --cached-plan <Plan>` spielt echte Rohlinge quellenfrei ab; Hüllen-Coverage aus gespeicherten Parametern.
Kompakte Repeats; Form/Höhen/Farbe bleiben erhalten. Körper ≤1,9 m Breite ohne direkte Nutzung bleiben geschlossen; Kachelränder als unvollständig kennzeichnen.
Hülle/Leibungen teilen metrische Achsen/Box-Coverage; Facing/Frustum, 1 m Bewegungsreserve und projizierte Tiefe vor Expansion.
0,5 px Detailgrenze ist eine Hypothese, kein Gerätegate; Drehung deckt alle Kanten ab. Über 50.000 Öffnungen: Hülle samt Grund.
Ungültige Quellen/Rezepte protokollieren; schmale Wandstreifen erhalten keine durchstoßende Leibung.
`verify_provider.py`: 13.955 Quellkörper/Volumen, 39 `hide_3d`-Auslassungen, ein ungültiges Höhenintervall; PRT-Achsen/Höfe/sRGB.
[`context.py`](../test/experiments/buildings/context.py): exakte [STRtree](https://shapely.readthedocs.io/en/stable/strtree.html)-Punkt-/Flächenzuordnung; Parts getrennt, keine pauschale nächste POI-Zuordnung.
Landuse ab 80 % Überdeckung; verschachtelte Teilfläche vor Umgebung, widersprüchliche Klassen bleiben gemischt.
POI/Hausnummer nur mit eindeutigem Host; Höfe, gemeinsame Grenzen und Erdgeschoss unter erhöhtem Part beachten. POI bleibt Teil-/Geschossnutzung.
[`provider_compare.py`](../test/experiments/buildings/provider_compare.py): Live-TileJSON, Inventur/frische Tiles; freie eigene CGA-Attribute aus Quelle/Teilnutzungen, keine Esri-Regelzuweisung.
OpenFreeMap `building`: `render_height`/`render_min_height` → Höhe/Unterkante, `colour` → sRGB-Wandfarbe, `hide_3d` → Auslassung mit Grund.
Höhen sind provideraufbereitet; sie beweisen keine originalen `height`-/Geschosstags. Dachform/Nutzung/Material fehlen in diesem Layer.
[`recipes.py`](../test/experiments/buildings/recipes.py): Wohn-/Gewerbe-/Industrie-/Institution-/Mischfassaden; direkte Angaben vor Kontext/Schlankheit, Unbekanntes bleibt unbekannt.
Verkehr/Wasser/Aeroway bis 12 m Umgebung; Bridge/Tunnel/Ebenen/Zugang begrenzen Frontage. Keine erfundene Straßenbreite/Dachform/Ausstattung.
[`context_catalog.py`](../test/experiments/buildings/context_catalog.py): alle Layer/Felder bekommen eine Rolle; regionale Namen/Höhen/Schutzgebiete bleiben Quellmetadaten.
[`verify_context.py`](../test/experiments/buildings/verify_context.py): alle drei Kacheln; 3 × 64 Hosts exakt gegen Vollsuche, Rezeptfamilien/acht Cache-Posen ohne Quellen/Join/Hüllenaufbau.
Multipart-Zerlegung erhält gültige Einzelgeometrien; gleiche Flächenklasse vereinen, ungültige Teile mit Grund erhalten. Python-Kontextvorbereitung bisher 1,6–2,4 s/Kachel.
[Showcase](../test/experiments/buildings/showcase.py): `--publish` prüft PNG/Report-Hash; `/tmp/outshine-buildings-showcase/`, Port 8766, Varianten/Kontext/Kosten samt Commit.
Jeder geprüfte Fortschritt: Commit/Push, dann Publish. Esri-Fähigkeitskatalog/ausführbare PRT-Referenzen zuerst; [VisusCity](https://visusarts.itch.io/visuscity-vol2) ergänzt den Esri-Bildmaßstab.

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
  (Tragwerk plus Glas), erhöhte Parts/Brücken (freie Unterkante). Unbekannt: einfacher Ersatz; gelieferte Form/Material gehen vor, Baujahr beweist kein Material.
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
[Esri-Katalog](../test/experiments/buildings/esri_catalog.py): SDK 3.4.12206, 72 Operations-/70 Funktions-/10 Attribut-/49 Bibliotheksseiten; Alias-Seiten zählen keine Rezepte.
[PRT-Orakel](../test/experiments/buildings/esri_oracle.py): externe [Esri-Pakete c9ebe92](https://github.com/Esri/pyprt-examples/tree/c9ebe92f27f6355179c9d7792f430a3c133b9214), PyPRT 1.12/PRT 3.3.11669; echte Form/Höfe, eigene Kamera/Licht. Candler-Dachhöhe überschreitet seinen Körperparameter.

## Abnahme
Wien/Feldkirch: Infrastruktur/Baukörper/Höfe/Parts; Rosenheim/Flensburg: Sonderklassen/Dächer. Nahöffnungen räumlich,
Fernstadt stabil/gebündelt; kein Aufbau unsichtbarer Details.
Bildgewinn/Kosten gemeinsam im AGENTS-Profil; Assethit wiederholt keine Anreicherung.
