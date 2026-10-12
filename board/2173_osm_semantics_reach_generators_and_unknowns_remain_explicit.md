Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, world, engine
Tags: buildings, roofs, facades, semantics, interiors

# Buildings become functional architecture with coherent interiors

## Ergebnis und Ist
Ziel: Esri-CityEngine-Qualität/-Vielfalt in Python, konstruktiv ebenso gründlich wie das Straßennetz. Auftrag bleibt offen.
Jedes Gebäude wird begehbar: funktionaler Zugang, Räume/Geschosse/Wege; räumlicher Hintergrund hinter Fenstern und kohärentes Nachtlicht.
Zuerst 2D-Grundriss/Fassade/Schnitt, danach 2.5D-Prüfansichten derselben 3D-Geometrie; Licht-/Materialbilder folgen.
Native Höhenintervalle, Footprints, BuildingMesh, Quellfarben und Terrain-Stempel bestehen.
Gelieferte Form/Attribute bestimmen die Geometrie. Frühere Bauhaus-/PV-/Balkonversuche sind keine Gestaltungsvorgabe.
16 Providerlayer/137 POI-Klassen/476 Unterklassen; 3.411 Zuordnungen zu wenigen Fassadenmustern. Architektur/Begehbarkeit/Innenräume fehlen.

## Getrennte Arbeitsblöcke
Alle WIs gehören zu 2169; 2173 besitzt Quelle/Form und den Gebäude-Gesamtvertrag.
[2400](2400_esri_capabilities_and_interior_primitives_have_executable_evidence.md): vorrangig Ghidra/Esri und ausführbare Referenzen.
[2420](2420_building_plans_connect_access_rooms_and_vertical_circulation.md): funktionale Raum-/Zugangspläne, 2D → 2.5D.
[2440](2440_building_recipe_families_have_distinct_architecture_and_examples.md): konstruktive Architekturrezepte und belegte Vielfalt.
[2460](2460_building_lods_preserve_interiors_night_light_and_cached_plans.md): Innenraum-LOD, Nachtlicht, Cache/Kosten.
Kleine Grund-/Gegenfälle vor echten Stadtfenstern; jede Anpassung/ungelöste Teilaufgabe mit Grund, gültige Teile erhalten.

## Python-Entwicklung auf buildings
Besitzer dieses Branches: Gebäudeagent, ausschließlich Python in `.venv`; native Übernahme offen.
[Experimente](../test/experiments/buildings/facade_plan.py): Quellen → Massierung/Fassade/Ausstattung → fertige Hülle → Kamerabedarf.
Gepinnte z14-Tiles Wien/Tokyo/Central Park mit SHA-Prüfung; Client-Pref `sources/building-experiments/`, Erwerb nur `samples.py --fetch`.
Schema 4: fertige Hülle, Form/Höfe/Höhen/Farbe, Kontextzeugen/Teilnutzungen und Rezept; Schema-3-Referenzen bleiben lesbar.
`preview.py --cached-plan <Plan>` spielt echte Rohlinge quellenfrei ab; Hüllen-Coverage aus gespeicherten Parametern.
Kompakte Repeats; Form/Höhen/Farbe bleiben erhalten. Körper ≤1,9 m Breite ohne direkte Nutzung bleiben geschlossen; Kachelränder als unvollständig kennzeichnen.
Hülle/Leibungen teilen metrische Achsen/Box-Coverage; Facing/Frustum vor Detail. Vorschau ergänzt grobe Schattenflächen sichtbarer Körper aus dem Cache, keine versteckten Fensterdetails.
0,5 px Detailgrenze ist eine Hypothese, kein Gerätegate; Drehung deckt alle Kanten ab. Über 50.000 Öffnungen: Hülle samt Grund.
Ungültige Quellen/Rezepte protokollieren; schmale Wandstreifen erhalten keine durchstoßende Leibung.
`verify_provider.py`: 13.955 Quellkörper/Volumen, 39 `hide_3d`-Auslassungen, ein ungültiges Höhenintervall; PRT-Achsen/Höfe/sRGB.
[`context.py`](../test/experiments/buildings/context.py): exakte [STRtree](https://shapely.readthedocs.io/en/stable/strtree.html)-Punkt-/Flächenzuordnung; Parts getrennt, keine pauschale nächste POI-Zuordnung.
Landuse ab 80 % Überdeckung; verschachtelte Teilfläche vor Umgebung, widersprüchliche Klassen bleiben gemischt.
POI/Hausnummer nur mit eindeutigem Host; Höfe, gemeinsame Grenzen und Erdgeschoss unter erhöhtem Part beachten. POI bleibt Teil-/Geschossnutzung.
[`provider_compare.py`](../test/experiments/buildings/provider_compare.py): Live-TileJSON/Tiles → freie CGA-Eingaben, keine Esri-Regelzuweisung.
OpenFreeMap: `render_height`/`render_min_height`, `colour`, `hide_3d`; provideraufbereitete Höhen, originale Geschosse/Dach/Nutzung/Material fehlen.
[`recipes.py`](../test/experiments/buildings/recipes.py): Wohn-/Gewerbe-/Industrie-/Institution-/Mischfassaden; direkte Angaben vor Kontext/Schlankheit, Unbekanntes bleibt unbekannt.
Verkehr/Wasser/Aeroway bis 12 m Umgebung; Bridge/Tunnel/Ebenen/Zugang begrenzen Frontage. Keine erfundene Straßenbreite/Dachform/Ausstattung.
[`context_catalog.py`](../test/experiments/buildings/context_catalog.py): alle Felder behalten Rolle/Metadaten; gültige Multipartteile/gleiche Flächenklassen vereinigen, ungültige Teile mit Grund.
[`verify_context.py`](../test/experiments/buildings/verify_context.py): drei Kacheln, 3 × 64 Hosts gegen Vollsuche; acht quellenfreie Cache-Posen. Kontext bisher 1,6–2,4 s/Kachel.
[Showcase](../test/experiments/buildings/showcase.py): Server/HTML/CSS/JS eingecheckt; `.venv/bin/python test/experiments/buildings/showcase.py --publish --serve`, Port 8766, Webroot `build/experiments/buildings/showcase/site/`.
Vier Unterseiten: Innenräume, einzelne Gebäude, Gebäudegruppen, Städte; Ansichtsebenen, keine behaupteten nativen LOD-Grenzen. Nur `current-showcase/snapshot.json` veröffentlichen; überholte öffentliche PNGs entfernen, Referenzen/Orakel erhalten.
`showcase_snapshot.py` rendert aktuelle gespeicherte Prüfgebäude plus echte Provider-Nachbarn; einfache Quellkörper ausdrücklich zählen. PNG/Report-Hash/Profil/sauberer Commit prüfen; fehlende Bilder sichtbar ausweisen.
Jeder geprüfte Fortschritt: Commit/Push, dann Publish. Esri/PRT/Ghidra zuerst; [VisusCity](https://visusarts.itch.io/visuscity-vol2) ergänzt den Esri-Bildmaßstab.

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

## Abnahme
Wien/Feldkirch: Infrastruktur/Baukörper/Höfe/Parts; Rosenheim/Flensburg: Sonderklassen/Dächer. Nahöffnungen räumlich,
Fernstadt stabil/gebündelt; kein Aufbau unsichtbarer Details.
Raum-/Portalgraph, Geschosswege und tatsächliche Öffnungen prüfen; Tag/Nacht/Nah/Gegen/Fern, echte Innenräume hinter Fenstern.
Logische Begehbarkeit/Kollision unabhängig vom Render-LOD. Bild/Kosten nach AGENTS; Assethit ohne neue Anreicherung; native Integration offen.
