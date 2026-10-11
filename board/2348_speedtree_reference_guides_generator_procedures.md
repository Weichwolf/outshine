Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2111
Depends:
Area: generators, render
Tags: vegetation, python, reference, ghidra

# SpeedTree reference guides own generator procedures

## Ergebnis und Besitzer
Ghidra ist auf Nutzerauftrag das primäre Analysewerkzeug für die vorhandene SpeedTree-10-Referenz.
Verfahren, Parameter und Aufrufer nachvollziehen und daraus eigene prüfbare Python-Verfahren ableiten.
2346 besitzt Organ-/Pflanzenarchitektur, 2347 begrenzte Gruppenprodukte, 2111 vollständige Wälder.
Papers bleiben ergänzende Primärquellen unter `doc/references/vegetation/{siggraph,eg,egsr,tvcg}`;
das zentrale Quellenregister verlinkt alle acht verwendeten Vegetationspapers.

## Referenz und Analyse
Nutzerpaket `/tmp/SpeedTree_Modeler_v10.2.0_Linux.tar.gz`, Arbeitsreferenz
`~/Downloads/SpeedTree_Modeler_v10.2.0`; keine fremden Assets oder Codes im Repository.
Symbolisierte Modeler-Binärdatei: `linux/SpeedTree_Modeler`; lesbarer Windcode im selben Paket.
Ghidra 12.1.2 aus Snap, Java 21; vorhandenes Projekt in
`~/Downloads/outshine-speedtree-analysis/projects/VegetationReference` wiederverwenden.
Gezielte Funktionskörper aus ELF-Symbolgrößen, begrenzte Dekompilierung und Aufrufer prüfen.
Diagnosen/Dekompilate bleiben bei der Referenz in Downloads, Logs im System-Tempverzeichnis.
Belegte Operationen von vermuteten Feldnamen und nicht rekonstruierten Verfahren unterscheiden.

## Belegte Entscheidungen und offene Semantik
Phyllotaxis `0x1e296e0`: Schritt durch wirksame Elternlänge normalisieren, vom letzten
zum ersten Standort laufen; Profile steuern Abstand/Roll, eine Anzahl begrenzt die Schleife.
Intervallplatzierung `0x1e2b3a0` verwendet zusätzlich den Kehrwert eines Frequenzparameters.
Mastersegmente `0x1cb7260`: absolute/relative Anteile, Profile und Mindestzahl vier.
Diese Beobachtungen begründen physische Internodien und Elternprofile; unbekannte Felder bleiben unbenannt.
`ComputeLodSkin` der Leaf-Mesh-Knoten (`0x1b3f900`) erzeugt einen angeforderten Skin,
setzt ein Auslassungsflag und schreibt Ruhe-/LOD-Zielpositionen relativ zu einem Bezugspunkt.
Skalierung enthält `1 + w(k−1)`; ein Gewichtsparameter mischt zwischen unverändert und skaliert.
Das ist ein Vergleichsmodell für Vergrößerung/Übergänge, kein Beleg für erhaltene Blattdeckung.
`ComputeLods` (`0x1f0d800`) ruft `ComputeVisibilityInfo` auf und sortiert gewichtete `SLodEntry`-Listen.
Virtuelle Blattaufrufe an `+0x610/+0x898/+0x8a8/+0x8b0` anhand der ELF-Vtable zugeordnet:
`GetArea/GetLodSkins/ComputeLodSkin/GetLodWeight`. Unbekannte Eintragsfelder nicht semantisch erfinden.
`ComputeVisibilityInfo` (`0x24ba610`, Worker `0x24b2810`) tastet an Skin-Vertexpositionen
ein vierkomponentiges 3D-Feld trilinear ab; Mittel, Extremwerte und gewichtete Richtungen ableiten.
`RenderVisibilityLookup` (`0x24c1d90`, fünf Worker) baut dieses Feld aus 64 Tiefenansichten
in einem 8×8-Atlas auf. Tiefenwerte aufbereiten, Lücken mit Nachbar-Minima ergänzen;
Gitterpunkte gegen diese Ansichten prüfen und sichtbare Richtungen aufsummieren/normalisieren.
Eine sigmoidale Abbildung der Richtungsanzahl liefert den vierten Kanal;
gewichtetes 3×3×3-Nachbarschaftsfiltern glättet das Feld. Kein kameragenauer Verdeckungsnachweis.
`GetArea` (`0x1b39f00`) liefert π(0,5 d)² aus einem Größenfeld, keine summierte Meshfläche.
LOD-Property-Aufbau (`0x1b4ed10`) benennt Gewicht und Skalierungsfaktor; konkrete Feldbindung offen.
`GetLodWeight` und `GetCullScalar` lesen Basiswerte plus optionale knotenspezifische Offsets.
Die alten Leaf-`SetDropLodData/SetGrowLodData`-Funktionen kopieren nur Positionswerte;
ihre Namen belegen keine allgemeine Verkleinerungs-/Vergrößerungslogik für Leaf-Mesh-Knoten.
`CLodSortFunctor` (`0x19cf610`) sortiert absteigend nach Eintragsfloat `+0x10`.
Bei Leaf-Mesh-Einträgen ist das der gemittelte Sichtbarkeitskanal, zur Hälfte mit dem
Kreisflächenproxy gemischt, gewichtet und durch einen Zufallsfaktor variiert.
LOD-Anzahl enthält `N f²` mit Profilwert f; Skalierung hängt zusätzlich von einer
Profilbewertung des normalisierten Rangs ab. Keine automatische Flächenerhaltung belegt.
Die Float-Signatur von `expf` im Ghidra-Projekt korrigiert; sie legt die vorher verborgene
Sigmoid-Eingabe frei. ELF-Konstanten und importiertes Symbol korroborieren diese Interpretation.
`GenerateOctahedralImpostor` (`0x1f5fc80`) erzeugt 12×12 Richtungs-/Atlasdatensätze;
das dekompilierte Gitter liegt auf einer Hemisphäre. Orthographische Matrizen/LookAt und
Material-Layer steuern getrennte Draws. Das ist keine vollständige Rundum-Runtime-Implementierung.
`GenerateBillboardLOD` (`0x1f68360`) nutzt `ExportBillboardData` und packt Kartenkoordinaten;
`CStartNode::MakeBillboards` (`0x1aa7e90`) liest Basis-/knotenspezifische Exportparameter.
Diese Referenz begründet kamerafreie unbeleuchtete Ansichten-Atlanten als nächste eigene Alternative.
`UnifyNormals` (`0x1b3b0b0`) summiert zwei Richtungsfelder eines Vertexbereichs, normalisiert
deren Mittel und mischt gewichtet zu ihnen; Kreuzprodukte stellen den Tangentenrahmen wieder her.
`ComputeGlobalLighting` (`0x1b42420`) gestaltet Normalen: ursprüngliche Richtung mit radialen
Richtungen vom Skin-Boundszentrum, mehreren Hierarchieankern, Projektoren und Welt-Z mischen.
Die normalisierte Zielrichtung bestimmt auch eine Rotation der anderen Tangentenrichtungen;
belegte Normalengestaltung, kein BRDF-/Verdeckungsersatz und keine gebackene Lichtfarbe.
`BuildLightingPropertyRecs` (`0x1b50880`) benennt Puffiness, Unify und Eltern-/globale Anpassungen.
`CTreeModel::ComputeGlobalLighting` (`0x1edd850`) speichert gemeinsame Parameter und delegiert
an unterstützende Generator-/Knotenprodukte. Vollständige Feld-/Propertybindung bleibt offen.
Eigene Normalenrezepte benötigen eine getrennte Bildprüfung; Karten müssen deren Antwort erhalten.

## Abnahme und nächster offener Vertrag
33 gezielte Funktionen dekompiliert; alle sieben Läufe terminal erfolgreich, zuletzt vier neue Funktionen.
Adressen/Symbolgrößen, ELF-Vtable und Konstanten korroborieren die beschriebenen Operationen.
Sichtbarkeitsfeld, LOD-Sortierung und Zielanzahl belegt; vollständige Property-/Windbindung bleibt offen.
Eigene Alternativen an demselben Plan gegen Deckung, Schatten, Normale, Aufwand und sichtbare Bilder prüfen.
Einzelne dekompilierte Funktionen belegen keinen vollständig rekonstruierten Wachstums-/LOD-Solver.
Windbindungen, natürliche Gabeln, vollständige Gruppen-/Waldaggregation und native Budgets bleiben offen.
