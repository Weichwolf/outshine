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
Ghidra 12.1.2: 47 Funktionen gezielt untersucht: Offset/Skelett/Convexify, Split/Extrusion/Innenrechteck, Komponenten/Instanzen, Dächer/Fassadenscopes, Materialien und Scope-Transformationen.
Offset-Kern nutzt CGAL Polygon-Offset; Achsenteilung ruft echte Mesh-Cuts auf. Dynamische Aufrufziele bleiben offen.
`computeInnerRects` nutzt Sweep/rekursive vertikale Zerlegung; Daniels-Blattverfahren ruft Zwei-/Drei-/Vierkontaktfälle auf.
`continueRecursion` bildet Koordinatenextrema, Rechteckfläche und `isBetterThan`; Typen und Vollständigkeit bleiben ungeprüft.
Feste Treppen-/Schachtmaße zuerst gültig innerhalb des Quellkörpers platzieren (2420), kein eigener Globaloptimalitätsanspruch.
`comp` delegiert Komponentenwahl; `i` lädt Assets, korrigiert die Aufwärtsachse und nutzt Bounding-Box-Dimensionen im Einfügungsdispatch.
`transformInPlace` ruft sichere inverse Transponierung und Flächennormalen auf; `addInstances` teilt Face-Material-Meshes und kombiniert Materialien.
Instanzausgabe hat vier unaufgelöste indirekte Aufrufe; vollständige Komponenten-/Treppenplanung bleibt ungeprüft.
Walm-/Satteldach rufen denselben `roofSkeleton`-Pfad auf; Pultdach delegiert an den noch ungeprüften `roofRidge`-Kern.
`createSkeletonBasedRoof` nutzt CGAL-Innenskelett, Lochpolygone, Giebelklassifikation, Schnittkorrektur und Flächennormalen.
Dachvorbereitung bereinigt Löcher, transformiert/verschweißt Bauteile; 3/11/1 indirekte Aufrufe in den drei großen Dachfunktionen bleiben offen.
`extractCompComponents` delegiert Flächen/Kanten, übernimmt Loch-/Tagbeziehungen und ruft glatte Vertexnormalen auf; vier indirekte Ziele bleiben offen.
`extractFaceComponents` nutzt Lochbeziehungen/Kantenabbildung und hat einen indirekten Aufruf; automatische Fenster-/Raumplanung folgt daraus nicht.
Skalierungsdispatch schreibt absolute/relative Größen; Einfügungshelper ruft Asset-Bounds und `adjustScopeSizeZerosToGeometry` auf.
Scope-Ausrichtung und Einfügungshelper haben außerhalb der ELF-Grenzen wiedergewonnene Decompilerblöcke; diese belegen keine weiteren Operationen.
`MaterialImpl::opacity` ruft numerisches `Material::set` auf; `opacitymap_mode` Stringvergleich und String-Set.
Ghidras fünf unaufgelöste Mode-PLT-Stellen unabhängig per ELF-Disassemblierung als 3×`wcslen`/2×`wmemcmp` bestätigt.
Referenzierte UTF-32-Konstanten `blend`/`mask`/`opaque` gegen SDK-Dokumentation bestätigt; andere dynamische Ziele bleiben offen.
`Material::combine` liest Zahlen-/Textur-/Stringarrays und kopiert Materialcontainer; genaue Überschreibpriorität bleibt ungeprüft.
11/63/1964 begrenzte Instruktionen, keine indirekten Stellen; Mode/Combine haben 12/4 außerhalb liegende Decompilerblöcke.
Extrusionsbeleg e42c3763a: Welt-/Flat-Top-/Vertexdispatch ruft Seiten-UVs, glatte Normalen und neue Scope-Ausrichtung auf.
3715 begrenzte Instruktionen, keine indirekten Stellen; 55 außerhalb liegende Blöcke zählen nicht als Beleg.
Seiten-UV-Helper: aktive Kanäle, Eckfolge A/B/B/A; 137 Instruktionen. Normenhelper: angrenzende Flächennormalen summieren und normieren; 204 Instruktionen.
Neue Scope-Y-Achse: alte X-Achse orthogonal projizieren, bei Kollaps Ersatzachse, Z aus Kreuzprodukt; 244 Instruktionen.
Diese drei Helper haben keine indirekten Stellen/außerhalb liegenden Blöcke; SDK-Typen und Ersatzkonstante bleiben nicht laufzeitverifiziert.
Komponentenbeleg eb6938bbf: Front/Links prüfen größte signierte Normalkomponente mit 1e−14 Toleranz; Side prüft |Y| ≤ cos(11,25°).
18/18/6 Instruktionen, keine indirekten/außerhalb liegenden Stellen. IEEE-Konstanten unabhängig im versionierten ELF gelesen.
Richtungsdispatch berücksichtigt negative Scope-Größen; Neigungsgrenzen haben geringfügig verschiedene positive/negative 11,25°-Konstanten.
`createShape` ruft Geometrieausrichtung bzw. Edge-/Vertex-Pivothelper und Bounding-Box-Anpassung auf; Verbindung nutzt Quellmesh-/Indexbeziehungen.
Oriented/Create/Connected/Split: 793/1927/969/1150 Instruktionen, 0/1/0/5 indirekte Stellen, 2/20/22/13 außerhalb liegende Blöcke; keine vollständige Semantikabdeckung.

## Umsetzung
- Kantenbeleg b976043b4: `extractEdgeComponents` mittelt adjazente Normalen, projiziert orthogonal zur Kante, nimmt bei Kollaps eine Quellnormale.
  Edge-Pivot: x=Kante, z=orthogonale Normale, y=z×x; Pivot am Anfang, Scope=(Länge,0,0).
  Vertex-Pivot: Scope=(0,0,0), optional gelieferte Tangente bzw. orthogonale Ersatzachsen; UNIT-BSS-Werte nicht laufzeitverifiziert.
  1093/289/421 Instruktionen, 1/0/0 indirekte Stellen, 5/0/0 außerhalb liegende Blöcke.
  ELF-Konstanten: 1e−25 Richtungswache, 0,999999 Achsenschwelle, 6,4e−7 Kantenlänge²; eigene Kollapsäquivalenz bleibt offen.
- Aktueller Kernvertrag (2480): Kanten-/Punktkomponenten mit eigenem Python, danach Kantenextrusion; Ghidra-Ziele gegen den fehlenden Vertrag wählen.
- Für die nächste sichtbare Fassadenlücke (2440): Komponenten-/Flächenextraktion, Scope-Ausrichtung, Größen-/Skalierungsdispatch und Einfügungsanpassung vertiefen.
  Bibliothekssymbole zuerst mit Ghidra gegen tatsächliche Aufrufe prüfen; Rahmenmodule an vorhandene Raumöffnungen anschließen.
- Materialvergleich: lokale SDK-Dokumentation trennt Deckkraft, Reflexion/Zweiseitigkeit sowie blend/mask/opaque.
  Analysierte Speicher-/Kombinationspfade belegen weder numerische Clamp-Regeln noch einen Glasrenderer.
- Ghidra headless: versioniertes Binary, exportierte Symbole, gezielte Disassemblierung/Funktions-/Aufrufanalyse.
  Offset/Skelett/Unterteilung/Booleans/Komponenten/Instanzen und innenraumrelevante Bausteine prüfen.
  Symbolname, Dokumentation, analysierte Funktion und ausführbares Ergebnis als Evidenz unterscheiden.
- Regelpakete/CGA/externe Assets untersuchen; Raumrezepte von generischen Geometrieoperationen trennen.
  Innenwand, Decke/Boden, Raumteilung, Durchbruch, Tür/Fenster, Treppe und Innenmaterial katalogisieren.
  Aktuell Candler-Ladenfenster/-eingang sowie vorhandenes `envelope2002` mit Grundriss-/Nutzungs-/Nachtglasparametern prüfen.
  Regeln/Assets bleiben extern in Downloads; 7z-RPK mit System-libarchive lesen, keine eigene Dekompression.
- Ausführbare Beispiele als Orakel: Achsen/Löcher/Höhen/Farben/Bounds, kontrollierte Kamera/Licht.
  Fremde Referenzregeln/-materialien kennzeichnen; fehlende Baupläne/Regeln als Lücke erhalten.
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
