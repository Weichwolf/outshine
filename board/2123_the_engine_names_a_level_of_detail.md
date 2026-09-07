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

## Konkreter Baum-LOD-Befund

Nativer Birkenrender nach 2177: selbst Rank 3 hält 272934 Rinden- und 80640 Blattdreiecke.
TreeMesher::Draw entscheidet Astpräsenz nach Shoot.Reach, also Reichweite statt projizierter
Dicke/Abweichung; viele subpixelbreite Äste bleiben. TreePrototype reduziert Blattanker durch
stride = 16 << (2*rank) und vergrößert Blattfächer über LAI-Erhaltung. Im PNG bleiben dadurch
wenige große Büschel. Erforderlich: konservativer Silhouetten-/Coveragefehler, räumliche
Aggregation statt bloßem Index-Stride, gefilterte Ferndarstellung ohne riesige Einzelblätter.
Messartefakt build/tree-native/birch.png; Aufbaukosten/Overdraw plus Waldinstanzkosten separat.

## Ast-Dicken-LOD implementiert und gegengeprüft

TreeMesher berücksichtigt zusätzlich zur Reach den maximalen Ast-Durchmesser relativ
zum deklarierten Pixelmaß. Nicht auflösbare Astgeometrie entfällt; Eltern sichtbarer Äste
werden rückwärts markiert und bleiben als Träger bestehen. Bei Pixelmaß 0 bleibt die
unbegrenzte Geometrie. Das ist Geometrieauswahl, noch kein gefilterter Ersatz für die
kollektive subpixelbreite Ast-Coverage und kein vollständiger Projektionsfehlerbeweis.

Birke Rank 3: Rinde 272934 → 2184 Dreiecke, Reduktion
(272934 - 2184) / 272934 = 99,2 %. Blätter unverändert 80640 Dreiecke. Gesamt damit
2184 + 80640 = 82824 statt 353574. PNG vorher `build/tree-native/birch-before-lod.png`,
nachher `build/tree-native/birch.png`, beide visuell geprüft: wesentlich weniger feines
Astgeflimmer, weiterhin spärliche große Blattbüschel und fehlende natürliche Kronendeckung.
Keine Wald-Framerate aus diesen Geometriezahlen ableiten.

`TreeLodPreservesVisibleBranchThickness`: langer dünner Ast entfällt bei grobem Pixelmaß,
kehrt bei feinem zurück; ein dicker Ast gleicher Länge bleibt. Dickenbegrenzung als echte
Mutation entfernt: genau dieses Oracle rot (build/tree-lod-negative.log, 11 PASS/1 FAIL).
Korrekte Fassung wiederhergestellt: `make suite SUITE=outshine/conventions`, 12/12 PASS,
Exit 0 (build/tree-lod-restored.log). Abschließender nativer Baumrender erneut geöffnet.

Offen: räumliche Kronenaggregation/Blatt-Coverage, feinere Stufen/Nahansicht, kontinuierliche
kameraabhängige Wahl, Hysterese und Gesamtbudget mit Waldinstanzen. Nächster Schritt bleibt
Blattdarstellung; keine Erhöhung der Instanzgrenze und kein Place-spezifischer Ersatzbaum.

## Nächster begrenzter Schritt: Blattanker erhalten

Unreal-/RAGE-Distanzleiter bleibt der Zielvertrag; dieser Schritt repariert den nativen
Adapter, ohne eine konkrete interne Foliage-Technik dieser Engines zu behaupten. Die
Wachstumspositionen existieren bereits in TreeFoliage. Der Adapter setzt dagegen 16
Blätter auf einen einzigen behaltenen Anker. Gleiche Größenordnung an Blattdreiecken auf
16-mal mehr vorhandene Anker verteilen; je Anker ein Blatt. Erwartung vor Render:
keine isolierten radialen Fächer mehr, gleichmäßigere Kronenbelegung. Regression: Die
Birke auf Rank 3 besitzt unterschiedliche Blattansätze; ein Rückfall auf gemeinsame
Fächerursprünge muss rot werden. Bild und Dreieckzahl beurteilen getrennt.
Dies ersetzt noch keine räumliche Aggregation, physische Nahblattgröße oder Coverage-LOD.

## Blattanker-Schritt geprüft

Native Birke Rank 3: 45 Anker mit je 16 Blättern wurden durch 716 einzelne Anker ersetzt.
Blattdreiecke 45 × 16 × 112 = 80640 → 716 × 112 = 80192; Rinde unverändert 2184.
Gesamt 82376 statt 82824 Dreiecke. Keine daraus abgeleitete Frame-/Speicheraussage.
Die Karten-Metadaten werden größer; GPU-Blattgeometrie bleibt praktisch gleich groß.

PNG vor/nach selbst geöffnet: zusammenhängendere, räumlich verteilte Krone, keine isolierten
radialen Fächer. Noch deutlich zu große Einzelblätter, harte unzureichende Kronenbeleuchtung,
fehlende gefilterte Ast-/Blatt-Coverage. Rank 3 überspringt weiterhin 63 von 64 Blättern und
vergrößert die verbleibenden zur LAI-Erhaltung ungefähr um sqrt(64) = 8. Dieser Schritt ist
keine Abnahme physischer Blattgrößen oder photorealistischer Waldqualität.

Bildänderung 37193 / (640 × 720) = 8,07 %; Bounding Box der Differenz
[x=196..447, y=95..591], also Krone, nicht Stammfuß oder Hintergrund.
Vorher `build/tree-native/birch-before-foliage.png`, SHA256
`27e7439e0bb3742707ff691db8a2e17cb948130e29251b6c2ee0a22ef1d61f97`;
nachher `build/tree-native/birch.png`, SHA256
`1cb70507c5b28af5af16bac0baae7ff24dd46bb90bf9563e27c5d77b79d9e4d2`.

Echte Mutation im Adapter legt je 16 Blätter wieder auf einen Ursprung: exakt der neue
Attachment-Check fällt, übrige 11 Tests grün, Make Exit 2
(`build/tree-foliage-negative.log`, Detail `build/tree-foliage-negative-case.log`).
Wiederhergestellte Fassung: `make suite SUITE=outshine/conventions`, 12/12 PASS, Exit 0
(`build/tree-foliage-restored.log`), finales PNG erneut geöffnet und Hash identisch.
Die Probe gilt für die native Birke; dieser Prototyppfad wird von Places noch nicht
gerendert. Keine veränderten Place-Bilder behauptet. Räumliche Aggregation mit begrenztem
Projektionsfehler, physische Nahblätter und gemeinsame Waldinstanzen bleiben nächste Arbeit.

## Vergleich vor der nächsten LOD-Entscheidung

Die Distanzleiter braucht eine tatsächlich gerenderte feine Referenz. GeometryAt(0)
existiert bereits, wurde aber nach Reparatur des Wachstums noch nicht visuell beurteilt.
Die native Fixture rendert deshalb Rank 3 und Rank 0 mit identischer Kamera und Licht.
Erwartung: physische Blattgröße auf Rank 0, höhere räumliche Kronenfrequenz; verbleibende
Kronenlücken dort sind kein Beleg für einen Fehler ausschließlich in Rank 3. Blattmaß
und Geometrieaufbaukosten ausweisen. Unreal-/RAGE-LOD bleibt das Ziel, die Messung des
eigenen feinen Generators entscheidet über dessen brauchbare Referenz. Das ist ein
Bild-/Kostenvergleich, kein neues Konformitätsoracle und keine Wald-Framerate.

## Feine Referenz und Nahansicht gemessen

Die Fixture rendert jetzt beide Ranks nacheinander (gleiche Gesamtansicht), zusätzlich
Rank 0 als 3 × 3,375 m großen orthographischen Kronenausschnitt. Keine Produktänderung
oder Place-Bildverbesserung in diesem Schritt. Die bereits vorhandenen Material-/Maßstabs-
Checks gelten auch für Rank 0; das Attachment-Oracle von Rank 3 bleibt unverändert.

| Native Birke | Rank 3 | Rank 0 |
|---|---:|---:|
| deklarierte Blattlänge m | 0,799573 | 0,100000 |
| Rindenvertices / -dreiecke | 1094 / 2184 | 54945 / 109882 |
| Blattvertices / -dreiecke | 62292 / 80192 | 3982425 / 5126800 |
| CPU-Aufbau GeometryAt ms, einzelner finaler Lauf | 0,974 | 94,936 |

Rank 0 erhält das deklarierte Blattmaß; 10 cm sind hier der vorhandene Species-Default,
keine neu validierte botanische Referenz. 3982425 / 87 = 45775 Blätter, je 112 Dreiecke.
Reine native Position-/Normal-/UV-/Indexdaten Rank 0:
(54945 + 3982425) × 32 + (109882 + 5126800) × 12 = 192036024 Bytes.
Dies ist Nutzlastarithmetik, weder Spitzenheap noch GPU-Residency. Aufbauzeit ist ebenfalls
keine Framezeit, und Einzelmessungen belegen keine Perzentile.

Alle drei PNGs selbst geöffnet. Gesamtansicht fein: wesentlich kleinere Einzelblätter,
zusammenhängende, aber stark feinkörnige Krone; Standbild belegt kein zeitliches Flimmern.
Nahansicht: Blattansätze/Verzweigungen vorhanden, flache gleichförmige Blattflächen und
glatte helle Rinde, fehlende plausible Kronenlichtverteilung. Die feinste vorhandene Stufe
ist damit eine Generatorreferenz, keine akzeptierte photorealistische Wahrheit.

Rank-3-/Rank-0-Bilddifferenz: 50416 / (640 × 720) = 10,94 %,
BBox x=196..447, y=75..641, Krone und feinere Rindenkontur. Hashes SHA256:
- `build/tree-native/birch.png`: `1cb70507c5b28af5af16bac0baae7ff24dd46bb90bf9563e27c5d77b79d9e4d2`
- `build/tree-native/birch-fine.png`: `1f46e4e6d442a711abc47a844a68d43ec76e54c0246d64dea235cd44a0de8ee0`
- `build/tree-native/birch-fine-close.png`: `d1e2d6df0330bd686a8aa2eb421dda58cc3a9d43ec1d01b38b118f4f7a23bb70`

`make suite SUITE=outshine/conventions`: 12/12 PASS, Exit 0, 38 Checks im nativen Baumfall;
`build/tree-fine-reference-final.log`. Kein neues Negativoracle für die Rate beansprucht.
Nächste Umsetzung: Blattmorphologie nach begrenztem projiziertem Fehler vereinfachen und
Blatt-/Kronen-Coverage räumlich filtern, statt Blattflächen blind aufzublasen. Gemeinsame
GPU-Prototypdaten bleiben Voraussetzung der Waldanbindung; Millionen expandierter
Dreiecke pro Instanz sind ausdrücklich kein Implementierungsweg. Materialdetail und
indirektes Kronenlicht bleiben bei 2171/2167, Artenmaße bei 2176.
