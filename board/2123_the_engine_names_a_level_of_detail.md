Type: debt
State: active
Area: world, render
Tags: webcam, measured
Depends: nothing

# Every visual representation obeys a measured screen error

## IST / Umsetzung

Terrain hat bereits Fehlerselektion; Gebäude wählen Details teilweise beim Ingest. Das
ist noch keine gemeinsame kontinuierliche Qualitätsleiter für Straßen, Wasser, Bäume und
Gebäude. Einstieg `include/generate/Generate.h`, Unseen/Detail-Auswahl, GroundLattice,
Generator-Bakes und Render-Cluster. 2166 besitzt den finalen Terrainfehler.

Pro Renderprodukt geometrischen/visuellen Fehler und Bounds tragen. Auswahl nach projiziertem
Fehler mit konservativem Nahfall; Hysterese/Morph und stabiler Material-/Feature-ID. Proxies
asynchron vorbereiten, selektiv verfeinern. Makrosilhouette und wesentliche Öffnungen erhalten.
Wasser darf nicht pauschal zum konvexen Hull werden: Inseln/konkave Buchten blieben sonst falsch.
Straße/Schiene dürfen vereinfacht werden, ohne logisches Netz, Alignment oder Kontakt mitzunehmen.
Fernvegetation erhält Kronenvolumen und Coverage, nahe Blattgeometrie ein Overdrawbudget.

- [ ] 1/10/100/1000-m-Leiter plus Fernblick und Bewegung; Mesh-/Materialwechsel ohne
      wandernde Gebäude, geschlossene Unterführungen oder verdeckte Wasserinseln.
- [ ] Gegen unabhängige feine Referenz Fehler messen; forced-coarse verletzt Qualitätsoracle.
      Forced-fine ist Kostenmessung, nicht automatisch ein garantiert rotes Performanceoracle.
- [ ] 2092/2104 messen Gesamtzeit und Speicher; Graph-/Kontaktgrößen unabhängig ausweisen.
      Bestehende Speicherobergrenzen bleiben erhalten, keine unbegründete Anhebung.

Wahl: Cesium-artiger Fehlervertrag, Unreal-HLOD als Clusterkonzept; RAGE visuelle Distanzleiter.
Kein Hardware-Nanite-Versprechen, da SDL_GPU die verwendbaren Mechanismen vorgibt.

## Aktueller Baum-LOD, dev/codex

TreeMesher wählt Äste nach Reach und maximalem Durchmesser relativ zum deklarierten
Pixelmaß; Eltern sichtbarer Äste bleiben erhalten. Pixelmaß 0 erhält die unbegrenzte
Astgeometrie. Native Birke Rank 3: 2184 Rindendreiecke statt zuvor 272934, also
(272934 - 2184) / 272934 = 99,2 % weniger. Kollektive subpixelbreite Ast-Coverage fehlt.
`TreeLodPreservesVisibleBranchThickness` samt echter Dicken-Mutation hält diesen Schritt;
Belege `build/tree-lod-negative.log` und `build/tree-lod-restored.log`.

TreePrototype erhält jetzt auf allen Ranks die vollständigen vorhandenen Blattansätze und
das auf Rank 0 erklärte Blattmaß. TreeFoliage wird einmal aufgebaut. Der frühere Stride
ließ auf Rank 3 nur 716 von 45775 Blättern stehen und vergrößerte sie zur LAI-Erhaltung
von 0,10 auf 0,80 m. Dieser Ersatz ist entfernt. 10 cm bleiben der Species-Default,
keine neu validierte botanische Messung (2176).

TreeLeaf vereinfacht die drei Längsreihen eines Blades im lokalen Raum unter einer
Abstandsschranke. Erfasste Formen: Broad sowie die Blades von Pinnate/PalmateCompound;
Palmate und Needle bleiben geometrisch unverändert. Normals und UVs behalten die feinen
Werte an übernommenen Stationen. Rank 0 bleibt unveränderte Geometriereferenz; höhere
Ranks erhalten halbes deklariertes Pixelmaß, durch CardLeafM in lokale Koordinaten geteilt.
Dies ist eine Geometrieschranke, keine Coverage-, Schattierungs- oder Perspektivabnahme.

### Entscheidung und Herleitung

Die 2D-Kurvenvereinfachung in base/curve/Fit trägt keine gefaltete 3D-Blattfläche.
Unreal-/RAGE-Distanzleiter bleibt der Rahmen; die konkrete Schranke folgt dem hiesigen
parametrisierten Generator. Eine bilineare Fehlersumme war zu konservativ. Stattdessen:
Die Differenz zweier stückweise affiner Dreiecksflächen ist auf ihrer UV-Überlagerung
affin. Ihre Norm nimmt das Maximum an einer Ecke der Überlagerung an. Prüfen: feine
Reihenenden und Schnittpunkte der groben Diagonale mit feinen Querlinien/Diagonalen.
Bei n zusammengefassten Intervallen trifft die feine Diagonale i die grobe bei t=i/(n-1),
Querlinien liegen bei t=i/n. Nur Intervalle innerhalb desselben Budgets zusammenfassen.

Konturvereinfachung allein reduzierte die aufgeblähten Birkenblätter nicht
(`build/tree-leaf-lod-overlay.log`). Erst das erhaltene physische Blattmaß lässt die
Kontur unter dem unveränderten Fehlerbudget sinnvoll vereinfachen. Das kostet gegenüber
dem alten falschen Proxy mehr Geometrie. Keine Wald-/Frame-/Heapobergrenze wurde erhöht.

### Messung und visuelle Beurteilung

| Native Birke | vor Flächenschranke (d2dafe3d) | aktueller Rank 3 | Rank 0 |
|---|---:|---:|---:|
| Blattlänge m | 0,100000 | 0,100000 | 0,100000 |
| Blattanzahl | 45775 | 45775 | 45775 |
| Blattvertices | 411975 | 823950 | 3982425 |
| Blattdreiecke | 366200 | 915500 | 5126800 |
| Rindenvertices / -dreiecke | 1094 / 2184 | 1094 / 2184 | 54945 / 109882 |
| einseitige Blattfläche m² | 265,987351 | 283,682236 | 284,193300 |
| CPU-Aufbau GeometryAt ms, finaler Einzelaufruf | — | 22,968 | 86,434 |

Blattdreiecke pro Blatt: fein 5126800/45775 = 112, aktuell 915500/45775 = 20.
Reduktion gegenüber fein: 1 - 20/112 = 82,14 %. Gegenüber d2dafe3d steigen sie
um den Faktor 915500/366200 = 2,5. Gesamt Rank 3: 915500 + 2184 = 917684 Dreiecke.
Native Vertex-/Index-Nutzlast Rank 3: (823950 + 1094) × 32 + 917684 × 12 = 37413616 Bytes;
Rank 0 weiterhin 192036024 Bytes. Das sind weder Spitzenheap noch GPU-Residency.
CPU-Aufbau ist keine Framezeit, Einzelaufrufe belegen keine Perzentile.

Alle finalen PNGs selbst geöffnet: Riesenblätter verschwunden, kleine räumlich verteilte
Blätter und erkennbare Kronenform. Rank 3 wirkt gegenüber Rank 0 dunkler/feinkörnig;
Kronendeckung und kollektive subpixelbreite Äste bleiben ungelöst. Nahansicht Rank 0:
Blattansätze/Verzweigungen vorhanden, glatte helle Rinde und flache gleichförmige Blätter,
unzureichende plausible Kronenlichtverteilung. Standbilder belegen kein zeitliches Flimmern.
Keine photorealistische Abnahme und keine geänderten Place-Bilder: dieser Prototyppfad
ist noch nicht in den Places-Wald integriert.

Bildwechsel beim Blattmaß in d2dafe3d, vor der Flächenschranke: 47884 / (640 × 720) = 10,39 % geänderte Pixel,
BBox x=196..447, y=75..591 (Krone; Stammfuß unverändert). SHA256:
- `build/tree-native/birch-before-leaf-lod.png`: `1cb70507c5b28af5af16bac0baae7ff24dd46bb90bf9563e27c5d77b79d9e4d2`
- `build/tree-native/birch-before-area-bound.png`: `abcada76040e225519ecaa4987a710bb140553e5b25a6854f950d95ce8d84b24`
- `build/tree-native/birch-fine.png`: `1f46e4e6d442a711abc47a844a68d43ec76e54c0246d64dea235cd44a0de8ee0`
- `build/tree-native/birch-fine-close.png`: `d1e2d6df0330bd686a8aa2eb421dda58cc3a9d43ec1d01b38b118f4f7a23bb70`

Beide Rank-0-Hashes unverändert gegenüber 67dc6610. Gleiche Gesamtansicht auf beiden Ranks;
zusätzlich 3 × 3,375 m orthographischer Kronenausschnitt auf Rank 0.

### Oracles und Negativkontrollen

`TreeLeafSimplificationBoundsTheSurface` wertet beide Dreiecksflächen unabhängig per
baryzentrischer UV-Interpolation auf einem 65×65-Raster aus. Glatte und stark gefaltete,
gekrümmte, asymmetrische, gezackte Blätter; Budgets 0,01/0,04/0,10 lokale Einheiten.
Das Raster ist ein Regressionsnetz; die durchgehende Schranke folgt der Herleitung oben.

2178 korrigiert eine nachweislich falsche Test-Spezifikation: 147853 Wachstumsansätze
enthalten schon 3 identische Positionen. Auswahl: 45775 Blätter, 45774 eindeutige Ursprünge.
Globale Eindeutigkeit war falsch; der Adapter muss die Eingabepositionen samt Multiplizität
unter dem Meter-Transform erhalten. Multiset-Vergleich statt willkürlicher Duplikattoleranz.
Diagnose `build/tree-leaf-attachment-diagnosis.log` und `...-case.log`.

Gemeinsamer Mutationslauf mit drei gezielt unabhängigen Defekten:
- Fehlerschranke übersprungen: alle 6 Abstandsprüfungen rot, Fehler bis 0,825609 lokal.
- Blattmaß ×8 auf groben Ranks: Maße-Oracle rot (0,80 statt 0,10 m).
- Je 16 Ansätze auf einen Ursprung gelegt: Multiset-Oracle rot, also kein akzeptierter Fächer.

`build/tree-leaf-lod-negative.log`: 11 PASS/2 FAIL, Exit 2. Details in
`build/tree-leaf-lod-negative-native.log` und `...-surface.log`.
Alle Mutationen entfernt: `make suite SUITE=outshine/conventions`, 13/13 PASS, Exit 0,
40 Checks im nativen Baumfall; `build/tree-leaf-lod-restored.log`. Finale PNGs erneut geprüft.
2178 damit erledigt und aus dem offenen Board entfernt; kein Geometrieoracle gelockert.

### Nächste Arbeit

Räumliche Blatt-/Kronenaggregation mit gemessener projizierter Coverage und zeitlicher
Stabilität; tatsächliche Kameraauswahl/Hysterese; Nah-/Fernbewegung und unterschiedliche
Beleuchtung. Den dunkleren Rank 3 nicht allein wegen kleinerer Geometriezahlen akzeptieren.
Gemeinsame GPU-Prototypdaten/Instanzen bleiben Voraussetzung der Waldanbindung (2111);
expandierte Geometrie nicht pro Weltinstanz kopieren. Materialdetail/Kronenlicht bei
2171/2167, Artenmaße bei 2176. WI bleibt active.

## Blattfläche getrennt von Pixelbild: Vertrag und Abnahme

Die tatsächlich exportierte Blattfläche und ihre Summenprojektionen auf X/Y/Z werden
in der nativen Fixture gemessen. TreeGeometry erzeugt beide Netze bereits; native
Positionen/Indizes lesen, keine zweite Baumimplementierung. Summe der einseitigen
Dreiecksflächen ist die geometrische LAI-Größe, Summe absoluter Projektionen ist ein
Richtungsmaß ohne gegenseitige Verdeckung. Beides ist nicht die sichtbare Kronen-Coverage.
Die vorher erklärte Hypothese war Flächenverlust trotz erfülltem Abstandsbounds;
nahezu gleiche Werte hätten dagegen gesprochen. Normals, Licht und Rasterung bleiben
separat zu prüfen. Unreal-/RAGE-Coveragevertrag bleibt das Ziel; die Messung
des vorhandenen Netzes entscheidet den nächsten Eingriff. Kein neues Gate aus einer Rate.

Messung (`build/tree-leaf-area-diagnosis.log`, 13/13 PASS): Rank 0 284,193300 m²,
Rank 3 265,987351 m²; Verlust (284,193300 - 265,987351)/284,193300 = 6,41 %.
Summenprojektionen XYZ: fein 138,985317/148,051814/139,165144 m², grob
130,099144/138,536894/130,261759 m². Das ist ein Beitrag zur Coverage-Differenz,
kein Nachweis ihrer alleinigen Ursache.

Zusätzlicher Qualitätsvertrag: höchstens 2 % relative einseitige Flächenabweichung je
Blade. 2 % ist ein hier gesetztes Fehlerbudget, kein behaupteter Industriestandard.
Für ein zusammengefasstes Intervall mit k von n Ausgangssegmenten darf der absolute
Flächenfehler höchstens 0,02 × Gesamtfläche × k/n betragen. Die disjunkten Intervalle
partitionieren n; Dreiecksungleichung begrenzt damit auch die Gesamtabweichung auf 2 %.
Abstandsbudget bleibt bestehen, keine nachträgliche Skalierung. Direkte Dreieckssumme
im Test prüft die Fläche unabhängig von der Intervallauswahl. Negativkontrolle entfernt
nur diese zweite Schranke; sie muss die Flächenprüfung verletzen. Bild und Kosten bleiben
separate Abnahmen. Der Vertrag garantiert keine projizierte Union/Verdeckung oder Beleuchtung.

Der erste Flächenlauf (`build/tree-leaf-area-proof.log`) behielt sämtliche Birkensegmente.
Die vorherige Auswahl brach beim ersten unzulässigen Kandidaten ab. Die Fehler sind bei
geänderter Endstation jedoch nicht monoton, insbesondere bei Serration. Alle folgenden
Endstationen prüfen und den weitesten zulässigen Kandidaten behalten; Budgets unverändert.

Finale Messung: (284,193300 - 283,682236)/284,193300 = 0,18 % Flächenverlust statt 6,41 %.
Summenprojektionen XYZ aktuell 138,733171/147,789364/138,913648 m²; nahe an der feinen
Referenz, aber keine garantierte Projektionsunion/Verdeckung. Aktueller Detailpreis siehe
Tabelle oben. Noch keine akzeptierte Wald-Performance.

`TreeLeafSimplificationBoundsTheSurface` prüft nun auch die unabhängig summierte
Dreiecksfläche. `TreeGeometryUsesNativeMaterials` prüft den 2-%-Vertrag bis zur exportierten
nativen Gesamtgeometrie. Nur Flächenschranke als Mutation entfernt, Abstand unverändert:
245,277608 statt 284,193300 m², beide Flächenoracles rot, alle Abstandstests grün.
`build/tree-leaf-area-negative.log`: 11 PASS/2 FAIL, Exit 2; Details `...-negative-native.log`
und `...-negative-surface.log`. Wiederhergestellt: `make suite SUITE=outshine/conventions`,
13/13 PASS, Exit 0; 41 Checks im nativen Baumfall, 19 im Blattfall.
Beleg `build/tree-leaf-area-restored.log`. Keine Oracles gelockert.

Finale drei PNGs selbst geöffnet. Rank 3 zeigt etwas dichtere feine Blattstruktur, bleibt
feinkörnig; Kronenlicht, Material und kollektive Ast-Coverage unzureichend. Rank 0 und
Nahansicht unverändert (Hashes oben). Keine bewegten Frames oder Place-Verbesserung behauptet.
Bildwechsel durch Flächenschranke: 21518/(640×720) = 4,67 %, BBox x=198..447, y=75..590,
innerhalb der Krone. `build/tree-native/birch.png` SHA256 jetzt
`3ca11fd66472a43d1488f56e938a9269c70baa35e838202352b1011592e13906`.
Der Vorzustand bleibt `build/tree-native/birch-before-area-bound.png` (Hash oben).

Nächster Schwerpunkt ist eine gefilterte Ferndarstellung mit gemeinsam gehaltenen
Prototypdaten und echter Waldanbindung. Die nahe geometrische Referenz ist jetzt in
Blattmaß, Abstand und Fläche kontrollierbar; weitere lokale Konturarbeit allein schließt
den fehlenden Wald nicht. Physisch plausible Kronenbeleuchtung und zeitliche Stabilität
bleiben Voraussetzung der visuellen Abnahme.
