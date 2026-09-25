Type: feature
State: active
Parent: 2169
Area: world, render
Tags: webcam, measured
Depends: 2173, 2166, 2179
Priority: P1

# Procedural surfaces carry Khronos materials at every distance

## IST

590696be3: Bodenpalette erhält Roughness und die BRDF ihren dielektrischen Glanzanteil.
Native/Ground-Vergleich samt Fallback und Slope-Mix grün; Malcesine-8dd84aa7 visuell geprüft.

Native Masked-/DoubleSided-Farbmaps greifen auch bei instanzierten Pieces;
Mip-Alpha-Coverage, Normalmaps und Blatttransmission bleiben offen (2111).

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
   Relief gemeinsam erzeugen. Feinstruktur direkt prozedural in GLSL auswerten:
   Poren, Asphaltkörnung, Holzfasern und Rindenfurchen verwenden dieselben stabilen
   Materialkoordinaten und Seeds für Farbe, Roughness und Bump-/Normalableitungen.
   Makroform bleibt DEM/OSM, Mesorelief darf plausibel erfunden sein.
3. Bodenklassen mit finaler Neigung/Krümmung, Höhe, Exposition, Feuchte und Nutzung mischen.
   Konstruierte Wände erhalten ihren eigenen Baustoff. Fels mit Schichtung, Brüchen,
   Schuttfuß und Vegetationsinseln; keine exakte Geologie ohne zusätzliche Quelldaten behaupten.
4. Relief amplitudenbegrenzt und in 2166s finalem Fehlermaß enthalten; nahe Silhouetten
   brauchen Geometrie. Subpixelstruktur gefiltert in Normal/Roughness überführen, keine
   periodischen Streifen, kein World-Origin-Schwimmen. Wetterfeuchte aus 2172 später einspeisen.
5. Native Materialien als deduplizierte Rezepte aus MR-Faktoren, Schichten, metrischem Maßstab,
   Seed und Wetterzustand verwalten; Instanzen referenzieren Rezept plus kleine Parameterdeltas.
   Ein Stadtpark mit Boden, Arten, Ausstattung, Menschen und Tieren benötigt bereits Hunderte
   Varianten; weltweit Tausende. Tausende Varianten bedeuten weder Tausende Shader noch Draws:
   Shaderfamilien und Renderzustand bündeln, Instanzen und GPU-Parameter tabellarisch binden.
   Keine fest eingebauten Texturassets für generierte Weltoberflächen. Gefilterte prozedurale
   Tiles/Mips sind versionierte, verwerfbare Caches derselben Funktionen, wenn Messung sie
   billiger als direkte GLSL-Auswertung zeigt. Importierte glTF-Texturen bleiben Quelldaten.
6. `decay` als dimensionsloser Alterungsgrad im Materialzustand; jedes Rezept definiert
   seine Reaktion. Regenlauf, Exposition, Feuchte, Temperatur, Bewuchs und Nutzung
   lokalisieren Ablagerung/Wasserflecken, Korrosion, Flechten, Moos und Abrieb.
   Rost benötigt oxidierbares Metall, Moos Feuchte/Licht; kein universelles Schmutzrauschen.
   Farbe, Rauheit, Normal/Relief und Materialmischung folgen demselben stabilen Feld.

## Abnahme

- [ ] Native und importierte Material-Fixtures stimmen unter derselben Beleuchtung überein;
      Normal-/Roughness-/Metallkanal-Tausch geht rot. Vorhandene rote Vendor-Orakel bleiben offen.
- [ ] Malcesine und Koerbersee zeigen strukturierte Seitenflächen; Husums Quai bleibt Baustoff,
      Wasser horizontal. Distanzen 1/10/100/1000 m und bewegte Kamera auf Flimmern prüfen.
- [ ] Parametervariation bleibt deterministisch und regional plausibel; kein Foto wird Input.
      Katalog mit >1000 Rezepten und vielen Instanzen: stabile IDs, Deduplizierung,
      Streaming-/Cache-Eviction ohne Bildsprung, p95/p99 und Bytes messen.
- [ ] Gleiche Betonkante trocken/feucht, neu/gealtert und um 180° gedreht:
      Ablaufspuren folgen Schwerkraft; Holz, Metall und Stein altern unterscheidbar.
      `decay=0` entfernt Alterung, Material-/Wetterwechsel ändert nur passende Effekte.

Referenzen: [Filament](https://google.github.io/filament/main/filament.html), [glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

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

Holz/Stein/Blatt/Asphalt sind Dielektrika; MR ersetzt weder Blattstreuung noch Wasserabsorption.

- [ ] Coverage-Report nennt jeden Generator und jede ausgegebene Oberflächenklasse samt
      Materialbindung; fehlende/ungültige ID geht rot. Bewusstes Debugmaterial ist markiert.
- [ ] Materialatlas und Nah-/Fernbilder aller Tabellenzeilen, einschließlich Brückenunterseite
      und Tunnelinnenwand. Alle Generatoren nutzen denselben Khronos-Vertrag, keine privaten
      RGB+Glanz-Abkürzungen. Einheitliches Defaultmaterial als Mutation verletzt das Oracle.
## Verbleibender Filtervertrag

Native Farb-/Normal-/MR-Bilder und flächenintegrierte Mips sind implementiert.
Der rote externe Filter-/Wiederholungsnachweis bleibt in 2179. Noch offen:
Alpha-Coverage bei Mips/Bewegung, Normalvarianz und Rauheit, Anisotropie,
Speicher-/Uploadbudget. Boxfilter und ein Materialatlas allein nehmen keine Welt ab.

## Plausibler Boden vor Einzelpflanzen
Zuerst Gelände ohne Vegetationsgeometrie visuell abnehmen. Aus OSM/DEM, Höhe,
Neigung, Exposition, geografischer Lage, Klima und Jahreszeit plausible Anteile von
Fels, Erde, Sand, Gras und Schnee ableiten; Feuchtigkeit/Temperatur zeitlich führen.
Geologie, Wasser und Nutzung beeinflussen den Zustand: Höhe/Klima bestimmen ihn
nicht eindeutig. Fehlende Daten deterministisch plausibel ergänzen, kein digitaler Zwilling.
Drei Ebenen: Bodenzustand, räumliche Materialmischung, PBR-Darstellung. Große Farbflächen,
mittlere Strukturen und gefilterte Mikrodetails trennen; nicht nur grünes Normalrauschen.
Natürlicher Boden ist dielektrisch (Metallic=0); BaseColor/Roughness/Normal erzeugen,
Nässe/Schnee/Gras mit passenden Lichtreaktionen statt falscher Metalness darstellen.
Gemeinsame Standort-/Dichtedaten verbinden Bodenmaterial und Vegetationsplatzierung.
Abnahme: kahle Testlandschaft mit Fels/Wiese/Erde, Nah-/Fernansicht, flacher Blick,
Gegenlicht, Tag/Jahreszeit/Wetterwechsel und Bewegung; stimmiger künstlerischer Look
ist zulässig. Streamingnähte, Wiederholungen und Flimmern bleiben Fehler.
Einzelhalme erst ergänzen, wo projizierte Größe, Blickwinkel und Silhouette beitragen;
keine pauschale Metergrenze. Vegetationsgeometrie folgt in 2137/2176.
