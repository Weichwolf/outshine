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
Ghidra 12.1.2: 27 Funktionen gezielt untersucht: Offset/Skelett/Convexify, Split/Extrusion/Innenrechteck, Komponenten/Instanzen, Dächer und Fassadenscopes.
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

## Umsetzung
- Für die nächste sichtbare Fassadenlücke (2440): Komponenten-/Flächenextraktion, Scope-Ausrichtung, Größen-/Skalierungsdispatch und Einfügungsanpassung vertiefen.
  Bibliothekssymbole zuerst mit Ghidra gegen tatsächliche Aufrufe prüfen; Rahmenmodule an vorhandene Raumöffnungen anschließen.
- Aktiver Materialvergleich: `MaterialImpl::opacity`, `opacitymap_mode` und `util::Material::combine` gezielt analysieren.
  Lokale SDK-Dokumentation trennt Deckkraft, Reflexion/Zweiseitigkeit sowie blend/mask/opaque; Speicherpfade belegen keinen Renderer.
- Ghidra headless: versioniertes Binary, exportierte Symbole, gezielte Disassemblierung/Funktions-/Aufrufanalyse.
  Offset/Skelett/Unterteilung/Booleans/Komponenten/Instanzen und innenraumrelevante Bausteine prüfen.
  Symbolname, Dokumentation, analysierte Funktion und ausführbares Ergebnis als Evidenz unterscheiden.
- Regelpakete/CGA/externe Assets untersuchen; Raumrezepte von generischen Geometrieoperationen trennen.
  Innenwand, Decke/Boden, Raumteilung, Durchbruch, Tür/Fenster, Treppe und Innenmaterial katalogisieren.
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
[CGA InnerRectangle](https://doc.arcgis.com/en/cityengine/latest/cga/cga-inner-rectangle.htm),
[CGA Inline](https://doc.arcgis.com/en/cityengine/latest/cga/cga-inline.htm): Spiralstufen als rekursive Bauteile plus 3D-Vereinigung, lokales SDK-Beispiel optisch geprüft.
[CGA Insert](https://doc.arcgis.com/en/cityengine/latest/cga/cga-i.htm): fremde Bauteilgeometrie im gemeinsamen Scope, kein automatisch erschlossener Geschossplan.
[Daniels/Milenkovic/Roth, Autorenpreprint 1993](../doc/references/buildings/geometry/1993-maximum-area-axis-parallel-rectangle.pdf): Kontakte und Polygonzerlegung, Seiten 1/5/8 optisch geprüft.
[Esri-Pakete c9ebe92](https://github.com/Esri/pyprt-examples/tree/c9ebe92f27f6355179c9d7792f430a3c133b9214).
[Esri Tutorial 15](../doc/references/buildings/esri/2013-interior-floors-web-scenes.pdf): Dev-HQ-Innenraum-/Geschossansichten, Seiten 3/7 optisch geprüft.
[CitySample a0981eb](https://github.com/fate-apple/CitySample/tree/a0981eb52322eab75db2507a23879de9f30110a0): HLOD/Instanzen, Houdini-Graphen fehlen.
[Building Tools 0216534](https://github.com/ranjian0/building_tools/tree/0216534645b15b3f8aac0bd1d7e6674e2972b9b3): Python-Bauteile/Skelettdächer.
[Labs Patterns](https://www.sidefx.com/docs/houdini/nodes/sop/labs--building_from_patterns-1.0.html): Module/Overrides.
