Type: feature
State: active
Area: world, render
Tags: webcam, measured
Depends: 2173, 2166

# Procedural surfaces carry Khronos materials at every distance

## IST

Alle neun Render zeigen weitgehend einfarbige Dächer/Wände/Boden. Die jüngste
Slope-Regel in `groundClass.glsl`/`groundLit.glsl` mischt steile Flächen Richtung Rock;
sie erzeugt noch keine Felsstruktur. Malcesines graue Falten und Husums weiße Böschungen
bleiben falsch. Steilheit ist ein Indiz für freiliegenden Untergrund, kein Beweis für
Fels: Betonquai, Mauer, Straßeneinschnitt und ein steiler Erdhügel sind Gegenbeispiele.
`include/scene/Material.h` erklärt Metallic-Roughness und Maps; die native Map-Anbindung,
normalScale/occlusionStrength und Alpha-Verhalten sind noch durchgängig nachzuweisen.
GLSL versteht glTF-Materialien nicht automatisch: Upload, Texturkanäle und BRDF sind Engine-Code.

## Implementierung

1. Ein Khronos-kompatibler Materialpfad für importierte und generierte Oberflächen:
   lineare Faktoren; sRGB nur Farb-/Emissionsmaps, Roughness/Metalness/Normal/AO linear;
   Kanalbelegung, UV-Sets/Transform, Alpha Mask/Blend, DoubleSided und Tangentenhandedness
   durch die öffentliche Tür. Nichtuniforme Skalierung über inverse-transpose behandeln.
2. Prozedurale Oberflächenschichten in Weltmetern: Triplanar für Terrain/Seitenwände,
   objektgebundene Fassaden-/Dachkoordinaten. Albedo, Normal, Roughness, AO und begrenztes
   Relief gemeinsam erzeugen. Makroform bleibt DEM/OSM, Mesorelief darf plausibel erfunden sein.
3. Bodenklassen mit finaler Neigung/Krümmung, Höhe, Exposition, Feuchte und Nutzung mischen.
   Konstruierte Wände erhalten ihren eigenen Baustoff. Fels mit Schichtung, Brüchen,
   Schuttfuß und Vegetationsinseln; keine exakte Geologie ohne zusätzliche Quelldaten behaupten.
4. Relief amplitudenbegrenzt und in 2166s finalem Fehlermaß enthalten; nahe Silhouetten
   brauchen Geometrie. Subpixelstruktur gefiltert in Normal/Roughness überführen, keine
   periodischen Streifen, kein World-Origin-Schwimmen. Wetterfeuchte aus 2172 später einspeisen.
5. Wiederverwendbare Parameter/Seeds/Materialtabellen statt Texturdownload oder Webcam-Bake;
   gebackene prozedurale Tiles/Mips amortisieren Shaderkosten.

## Abnahme

- [ ] Native und importierte Material-Fixtures stimmen unter derselben Beleuchtung überein;
      Normal-/Roughness-/Metallkanal-Tausch geht rot. Vorhandene rote Vendor-Orakel bleiben offen.
- [ ] Malcesine und Koerbersee zeigen strukturierte Seitenflächen; Husums Quai bleibt Baustoff,
      Wasser horizontal. Distanzen 1/10/100/1000 m und bewegte Kamera auf Flimmern prüfen.
- [ ] Parametervariation bleibt deterministisch und regional plausibel; kein Foto wird Input.
      Passzeit/Bytes/Overdraw mit 2092 messen, nicht nur Materialkugel akzeptieren.

Wahl: Filaments Metallic-Roughness/IBL als lesbare physikalische Referenz; prozedurale
Schichten erfüllen die Generierungsanforderung. Unreal/RAGE sind visuelle Vergleichsbilder.
[Filament](https://google.github.io/filament/main/filament.html),
[glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

## Vollständigkeit pro Geometrieerzeuger

Materialerzeugung ist für jede sichtbare Oberfläche Pflicht, nicht nur für Terrain oder
importiertes glTF. Generator-Ausgang → Material-ID → Upload → Draw separat inventarisieren:

| Geometrie | erforderliche Materialunterscheidung |
|---|---|
| Terrain/Fels/Seitenfläche/Schutt | Substrat, Relief, Rauheit, Feuchte |
| Gebäude/Dach/Fenster/Fundament | Putz, Ziegel, Stein, Holz, Glas, beschichtetes/blankes Metall |
| Straße/Weg/Platz/Markierung | Asphalt, Beton, Pflaster, Erde, Farbe; trockene/nasse Oberfläche |
| Gleis/Weiche/Schwelle/Schotter | blanker Schienenkopf, oxidierte Seiten, Holz/Beton, Gestein |
| Brücke/Tunnel/Quai/Geländer | jeweiliger Baustoff, Fahrbelag, Abrieb/Nässe; Unterseite eingeschlossen |
| Baum/Strauch/Gras/Totholz | Rinde/Holz/Blattober-/unterseite/Nadel, artspezifisch in 2176 |
| Wasser/Schnee/Eis | dielektrisches MR plus passende Transmission/Volumen-/Streumodelle |
| generierte Props/Partikel | Oberflächenmaterial bzw. expliziter Emissions-/Volumenvertrag |

Metalness beschreibt das exponierte Material: Holz/Stein/Blatt/Asphalt sind Dielektrika;
Lack und Rost werden nicht allein wegen metallischem Untergrund zu blankem Metall.
Roughness, Normalmaßstab und Albedo dürfen nicht sämtlich auf dem Fallback bleiben.
MR allein beschreibt weder Blattstreuung noch Wasserabsorption oder Wolken: ergänzende
physikalische Modelle sind explizit, nicht als falsche Metalness versteckt.

- [ ] Coverage-Report nennt jeden Generator und jede ausgegebene Oberflächenklasse samt
      Materialbindung; fehlende/ungültige ID geht rot. Bewusstes Debugmaterial ist markiert.
- [ ] Materialatlas und Nah-/Fernbilder aller Tabellenzeilen, einschließlich Brückenunterseite
      und Tunnelinnenwand. Alle Generatoren nutzen denselben Khronos-Vertrag, keine privaten
      RGB+Glanz-Abkürzungen. Einheitliches Defaultmaterial als Mutation verletzt das Oracle.

## Konkreter vorhandener Baum-Materialvertrag, 2026-09-07

`TreePrototype.h::Row` verwendet 20 Floats für BarkRgb/BarkDark/BarkFreq/BarkRidge,
LeafRgb/LeafShape usw. und setzt die Zeilenbreite per static_assert mit kMaterialRowFloats
gleich. Gleiche Breite bedeutet keine gleiche Semantik wie das Khronos-Materiallayout.
Bei Integration in den Renderpfad getrennte prozedurale Tree-Parameter und echte MR-
Materialien für Rinde/Blätter erzeugen. Keine vorhandene Float-Zeile direkt als MR hochladen.

## Erster nativer Baum-Materialpfad

TreeGeometry erzeugt getrennte Material-Objekte für Rinde/Blätter: Metalness 0, lineare
Artenfarben, Blätter DoubleSided, echte Blattsilhouette statt ungefiltertem Deckungsrechteck.
TreeSpecies liest bark_roughness/leaf_roughness mit [0,1]-Validierung. Defaults 0.9/0.6 sind
plausible Startwerte, keine gemessenen artspezifischen Materialdaten. Normal-/Rindenrelief,
Blatttransmission und saisonale Änderungen bleiben offen. Die neue Fixture prüft getrennte
Bindungen, explizite Rauheiten und ungültige Eingaben; 11/11 Konventionsfälle grün.

## Einstieg native Bilder, Voraussetzung für 2111/2123

Live::CarriesBuilt/der native Anhang in StandsSubjects übernehmen Materialwerte, aber keine
Geometry.addImage-Daten in SubjectTexture. Vorhandene Renderer-Sockets für BaseColour,
Normal, MetalRough, Emissive und Specular stehen bereit. Diese Übergabe gemeinsam binden,
mit Image-Indexprüfung, UV-Transform, UV-Set und Sampler; Geometry hält die Bildbytes bis
zum Upload wie bereits die Mesh-Spans. Kein zweiter Textur-Renderer. Occlusion besitzt
noch keinen Socket und bleibt separat offen.

Unreal-/RAGE-Prototyp-/Atlasprinzip bleibt das Ziel für Wald; native Geometry/SurfaceMap
ist die vorhandene Tür. GPU-Fixture: prozedurales 2×2-Farbbild auf einer nativen Fläche,
quadrantenweise Farboracle im linearen Render und PNG. Fehlende Bindung muss rot werden.
Sampler/UV-Metadaten und weitere vorhandene Sockets gezielt prüfen; kein vollständiger
MR-Konformitätsnachweis allein aus dieser Farb-Fixture.
