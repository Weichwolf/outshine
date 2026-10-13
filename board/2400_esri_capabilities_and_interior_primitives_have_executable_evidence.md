Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators
Tags: buildings, esri, reference, interiors

# Esri architecture primitives have inspectable and executable evidence

## Ergebnis und Ist
Besitzer: Gebäudeagent/Python; Gebäude-Gesamtvertrag in 2173.
Vorrangig Ghidra einsetzen: Esri-Fähigkeiten/Verfahren einschließlich Innenräumen als Referenz katalogisieren.
SDK 3.4.12206: 72 Operations-, 70 Funktions-, 10 Attribut-, 49 Bibliotheksseiten, keine Rezeptzahlen.
Fünf externe Regelpakete inventarisiert; Candler auf drei echten Grundrissen Nah/Gegen/Fern gerendert.
Ein externes Architekturregelwerk ist keine eigene Rezeptfamilie; Alias-Seiten zählen keine Algorithmen.
PyPRT 1.12/PRT 3.3.11669 führt Referenzen aus; SDK 3.4 separat untersuchen, Versionen nicht mischen.
Innenraumdarstellung ist bei CityEngine belegt; automatische Raum-/Wegeplanung in lokalen Libs/RPKs unbewiesen.
Ghidra 12.1.2: 79 Funktionen gezielt untersucht: Offset/Skelett/Convexify, Split/Extrusion/Innenrechteck, Komponenten/Instanzen, Dächer/Fassadenscopes, Materialien und Scope-Transformationen.
Offset-Kern nutzt CGAL Polygon-Offset; Achsenteilung ruft Mesh-Cuts auf. Split-Allokation 004d6120: feste Restbreite, gewichtete Floating-Anteile, Repeat-Floor/Ceil nach Streckungsabstand zu 1.
ELF 00805258/00805278: 1e−8/1; 761 Instruktionen, 13 indirekte Stellen. Verschachtelte Wiederholungen und Grenzfälle bleiben offen; 2480 prüft 2D/3D getrennt.
`computeInnerRects` nutzt Sweep/rekursive vertikale Zerlegung; Daniels-Blattverfahren ruft Zwei-/Drei-/Vierkontaktfälle auf.
`continueRecursion` bildet Koordinatenextrema, Rechteckfläche und `isBetterThan`; Typen und Vollständigkeit bleiben ungeprüft.
Feste Treppen-/Schachtmaße zuerst gültig innerhalb des Quellkörpers platzieren (2420), kein eigener Globaloptimalitätsanspruch.
`comp` delegiert Auswahl; `i` lädt Assets, korrigiert Aufwärtsachsen und nutzt Bounds. `getGeometryAsset` (00497950, 965 Instruktionen) löst Assetkeys und Cache-Proxies auf; indirekte Decoder/Typen bleiben ungeprüft.
`transformInPlace` ruft sichere inverse Transponierung und Flächennormalen auf; `addInstances` teilt Face-Material-Meshes und kombiniert Materialien.
Material: RGB/RGBA warnen und begrenzen [0,1]; Hex-RGB erhält Opazität, Hex-RGBA setzt sie. Reset ersetzt Shape-/Geometriematerial getrennt.
`getMaterial` kombiniert used aus Shape/Geometrie; Roughness/Metallic-Setter ohne beobachtete Begrenzung. Isolierte PRT-Regeln/Formatierung/Shadergleichheit offen.
Walm-/Satteldach rufen denselben `roofSkeleton`-Pfad auf; Pultdach delegiert an den noch ungeprüften `roofRidge`-Kern.
`createSkeletonBasedRoof` nutzt CGAL-Innenskelett, Lochpolygone, Giebelklassifikation, Schnittkorrektur und Flächennormalen.
Dachvorbereitung bereinigt Löcher, transformiert/verschweißt Bauteile; 3/11/1 indirekte Aufrufe in den drei großen Dachfunktionen bleiben offen.
`extractCompComponents` delegiert Flächen/Kanten, übernimmt Loch-/Tagbeziehungen und ruft glatte Vertexnormalen auf; vier indirekte Ziele bleiben offen.
`extractFaceComponents` nutzt Lochbeziehungen/Kantenabbildung und hat einen indirekten Aufruf; automatische Fenster-/Raumplanung folgt daraus nicht.
Insert: `adjustScopeSizeZerosToGeometry` ergänzt Punkt-/Linien-/Flächenscopes; zwei vorhandene Skalierungsverhältnisse werden gemittelt.
Nullachsen <0,1 mm, Nenner ≤1e−8 m mit Faktor 1; `getTrafoToSizeMatrix` begrenzt ausgegebene Abmessungen auf ±0,01 mm. Acht ELF-Konstanten geprüft.
`MaterialImpl::opacity` ruft numerisches `Material::set` auf; `opacitymap_mode` Stringvergleich und String-Set.
Ghidras fünf unaufgelöste Mode-PLT-Stellen unabhängig per ELF-Disassemblierung als 3×`wcslen`/2×`wmemcmp` bestätigt.
Referenzierte UTF-32-Konstanten `blend`/`mask`/`opaque` gegen SDK-Dokumentation bestätigt; andere dynamische Ziele bleiben offen.
`Material::combine` liest Zahlen-/Textur-/Stringarrays und kopiert Materialcontainer; genaue Überschreibpriorität bleibt ungeprüft.
Extrusionsbeleg e42c3763a: Welt-/Flat-Top-/Vertexdispatch ruft Seiten-UVs, glatte Normalen und neue Scope-Ausrichtung auf.
Seiten-UVs: aktive Kanäle, Eckfolge A/B/B/A; glatte Normen summieren und normieren angrenzende Flächennormalen.
Neue Scope-Y-Achse: alte X-Achse orthogonal projizieren, bei Kollaps Ersatzachse, Z aus Kreuzprodukt.
Diese drei Helper haben keine indirekten Stellen/außerhalb liegenden Blöcke; SDK-Typen und Ersatzkonstante bleiben nicht laufzeitverifiziert.
Komponentenbeleg eb6938bbf: Front/Links prüfen größte signierte Normalkomponente mit 1e−14 Toleranz; Side prüft |Y| ≤ cos(11,25°).
Richtungsdispatch berücksichtigt negative Scope-Größen; Neigungsgrenzen haben geringfügig verschiedene positive/negative 11,25°-Konstanten.
`createShape` ruft Geometrieausrichtung bzw. Edge-/Vertex-Pivothelper und Bounding-Box-Anpassung auf; Verbindung nutzt Quellmesh-/Indexbeziehungen.
Komponentenpfade enthalten indirekte Aufrufe/außerhalb liegende Blöcke; Details im Export, vollständige Semantikabdeckung unbewiesen.

## Umsetzung
- Kantenbeleg b976043b4: `extractEdgeComponents` mittelt adjazente Normalen, projiziert orthogonal zur Kante, nimmt bei Kollaps eine Quellnormale.
  Edge-Pivot: x=Kante, z=orthogonale Normale, y=z×x; Pivot am Anfang, Scope=(Länge,0,0).
  Vertex-Pivot: Scope=(0,0,0), optional gelieferte Tangente bzw. orthogonale Ersatzachsen; UNIT-BSS-Werte nicht laufzeitverifiziert.
  ELF-Konstanten: 1e−25 Richtungswache, 0,999999 Achsenschwelle, 6,4e−7 Kantenlänge²; eigene Kollapsäquivalenz bleibt offen.
- Kantenextrusionsbeleg 5513b3e1f: aktuelle Polygon-/Kantencontainer je Endpoint-ID für `calculateVertex2Faces`; glatte Normalen summieren aktuelle Nachbarn.
  `setSideFace` kopiert Quell-/Kanten-/Endpunkttags und aktualisiert Eckbeziehungen; konkrete C++-Feldtypen bleiben ungeprüft.
  Extrusionsdispatchs besitzen eigene Zweipunktzweige: eine Seitenfläche pro Kante, Vertexmodus teilt verschobene Endpoint-IDs.
- Gruppenbeleg e6fb5a155: `GroupedComponent::isOriented` prüft Mitglieder bis zum ersten Treffer; sieben virtuelle Aufrufstellen.
  `createShape` baut Quellmeshes und passt BBox an oder delegiert Z-Up-Ausrichtung an Geometrie mit Mesh-/Faceindex 0.
  `Material::operator<` vergleicht Shader und bool/int/float/string/Texture-/Pointercontainer, keine bloße Palette-ID.
  Gesamte Defaults-/Texturgleichheit bleibt ungeprüft; Export trennt interne/indirekte und außerhalb liegende Aufrufe.
  Komponenten-Dispatch bündelt Materialwerte über Quellen hinweg; Lochzweig invertiert Mesh vor Auswahl der Lochbeziehungen.
- Schnittbeleg fa54f57cd: Mesh-Cut behält Face-Materialtabellen/UVs und schließt in der Ebene; generischer Clip behandelt koplanare Kanten.
  Closure prüft überlappende/identische Koplanarflächen und hält Vertex-/Edgetags konsistent; Typen/Kappensemantik nicht laufzeitverifiziert.
  SplitArea projiziert zu 2D-Multipolygonen und AreaSplit; `createMeshes` übernimmt Material-/UV-Container und berechnet Flächennormalen.
  Plane-Helper summiert logische Normalen unter Lochbeziehungen und prüft cos(30°); Achsenwinkel ist orientiert, Mindestwinkel 30°.
  Fünf ELF-Doubles und drei UTF-32-Fehlertexte direkt geprüft (`esri_area_reference.py`); C++-Normalengewichte und SDK-Flächen-/UV-Gleichheit offen.
  Eigene tatsächliche Dreiecksflächen und Schnittentscheidung samt verworfener Manifold-Koplanarvariante stehen in 2480.
- Nach dem Modellierkern (2480): Rahmenmodule über Komponenten-/Flächenscopes und Insert an vorhandene Raumöffnungen anschließen (2440).
- Materialvergleich: lokale SDK-Dokumentation trennt Deckkraft, Reflexion/Zweiseitigkeit sowie blend/mask/opaque.
  UV-Pfade: affine Array-Edits/Assetcopy; normalizeUV entfernt ungenutzte Koordinaten und nutzt HoleRelation. 1e−25 UV-Schwellwert im ELF geprüft; Warn-/Typgleichheit unbewiesen.
- Dokumentation, Ghidra-Aufrufbeleg und ausführbares Ergebnis unterscheiden; indirekte Ziele bleiben offen.
- Regelpakete/CGA/externe Assets untersuchen; Raumrezepte von generischen Geometrieoperationen trennen.
  Innenwand, Decke/Boden, Raumteilung, Durchbruch, Tür/Fenster, Treppe und Innenmaterial katalogisieren.
  Aktuell Candler-Ladenfenster/-eingang sowie vorhandenes `envelope2002` mit Grundriss-/Nutzungs-/Nachtglasparametern prüfen.
  Regeln/Assets bleiben extern in Downloads; 7z-RPK mit System-libarchive lesen, keine eigene Dekompression.
- Ausführbare Beispiele als Orakel: Achsen/Löcher/Höhen/Farben/Bounds, kontrollierte Kamera/Licht.
  Fremde Regeln/Materialien kennzeichnen; fehlende Baupläne/Regeln offen halten.
- Eigene Python-Verfahren ableiten und vergleichen; Ghidra-Ausgabe ist Referenz, keine kopierte Implementierung.

## Belege und Abnahme
[`esri_catalog.py`](../test/experiments/buildings/esri_catalog.py), [`esri_oracle.py`](../test/experiments/buildings/esri_oracle.py),
[`verify_esri.py`](../test/experiments/buildings/verify_esri.py): sechs analytische Kontrollen, drei reale Extrusionen,
drei deterministische Architekturvarianten; Geometrie-/Bildprovenienz erhalten.
Ghidra-Belege mit Binary-/Werkzeugversion, überprüfbaren ausgewählten Funktionen/Aufrufen und Grenzen.
[`esri_ghidra.py`](../test/experiments/buildings/esri_ghidra.py): begrenzte gezielte Analyse; C++-Typen aus Decompiler nicht verifiziert.
Aufrufstatistik aus disassemblierten Kontrollflussblöcken innerhalb der ELF-Symbolgrenzen; vollständige Funktionsabdeckung unbewiesen.
Decompiler kann Blöcke außerhalb der Funktion behaupten; diese zählen nicht als Funktions-/Aufrufbeleg.
[`verify_ghidra.py`](../test/experiments/buildings/verify_ghidra.py): frischen Script-Export verlangen; Headless-Status 0 allein reicht nicht.
Snap hat privates `/tmp`: Analyseprojekte vorübergehend in Snap-common, fertige Belege nach `build/experiments/buildings/`; Logs System-Temp.
Katalog zeigt Fähigkeit, eigene Entsprechung und fehlende Verfahren; daraus nächste Bild-/Funktionslücke wählen.

## Primärquellen
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
[Esri SDK](https://esri.github.io/cityengine/cityenginesdk),
[CGAL Skeleton/Offset](https://doc.cgal.org/latest/Straight_skeleton_2/index.html),
[CGA Split](https://doc.arcgis.com/en/cityengine/latest/cga/cga-split.htm),
[CGA Extrude](https://doc.arcgis.com/en/cityengine/latest/cga/cga-extrude.htm): vier Flächenarten, negative Scope-Y-Größe, Flat-Top-Mindestspanne und unverbundene Vertexnormalen.
[CGA Comp](https://doc.arcgis.com/en/cityengine/latest/cga/cga-comp.htm),
[Komponententags](https://doc.arcgis.com/en/cityengine/latest/cga/cga-component-tags.htm): Punkt-/Mehrfachsubtag-Wildcards und Ersatz früherer Auto-Tags bei erneuter Operation.
[CGA InnerRectangle](https://doc.arcgis.com/en/cityengine/latest/cga/cga-inner-rectangle.htm),
[CGA Inline](https://doc.arcgis.com/en/cityengine/latest/cga/cga-inline.htm): Spiralstufen als rekursive Bauteile plus 3D-Vereinigung, lokales SDK-Beispiel optisch geprüft.
[CGA Insert](https://doc.arcgis.com/en/cityengine/latest/cga/cga-i.htm): fremde Bauteilgeometrie im gemeinsamen Scope, kein automatisch erschlossener Geschossplan.
[Daniels/Milenkovic/Roth, Autorenpreprint 1993](../doc/references/buildings/geometry/1993-maximum-area-axis-parallel-rectangle.pdf): Kontakte und Polygonzerlegung, Seiten 1/5/8 optisch geprüft.
[Esri-Pakete c9ebe92](https://github.com/Esri/pyprt-examples/tree/c9ebe92f27f6355179c9d7792f430a3c133b9214).
[Esri Tutorial 15](../doc/references/buildings/esri/2013-interior-floors-web-scenes.pdf): Dev-HQ-Innenraum-/Geschossansichten, Seiten 3/7 optisch geprüft.
[CitySample a0981eb](https://github.com/fate-apple/CitySample/tree/a0981eb52322eab75db2507a23879de9f30110a0): HLOD/Instanzen, Houdini-Graphen fehlen.
[Building Tools 0216534](https://github.com/ranjian0/building_tools/tree/0216534645b15b3f8aac0bd1d7e6674e2972b9b3): Python-Bauteile/Skelettdächer.
[Labs Patterns](https://www.sidefx.com/docs/houdini/nodes/sop/labs--building_from_patterns-1.0.html): Module/Overrides.
