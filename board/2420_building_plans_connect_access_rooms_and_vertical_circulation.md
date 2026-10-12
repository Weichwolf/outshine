Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, world
Tags: buildings, interiors, access, geometry

# Building plans connect entrances, usable rooms and every storey

## Ergebnis und fehlender Vertrag
Besitzer: Gebäudeagent/Python; 2173 besitzt Quelle/Form, 2281 Straßen-/Zugangsbezüge.
Kameraunabhängiger konstruktiver 3D-Plan: Außenhülle, Raum-/Geschossflächen, Öffnungen und Verkehrswege.
Jedes Gebäude mindestens ein funktionaler Zugang; von dort erreichbare Innenräume/Geschosse.
Native Hüllen/Repeats besitzen noch keine integrierten Türen oder nutzbaren Innenraum-/Wegepläne.
Python-Prototyp: acht Gebäude/39 Geschosse/31 konstruierte Geschossverbindungen; drei echte Esri-Vergleichsgrundrisse.
Wand-/Boden-/Deckenstärken, offene Türportale und Fenster in tatsächliche Räume; drei Gegenfälle mit Auslassungsgrund.
Begrenzte achsparallele Streifen gegen BSP-Teilung: gleiche Mindestflächen, Türbreiten, Clearance und Raumgraphprüfung.
Maximal zwölf Räume/256 Quellvertices; sechs Schnittkandidaten pro BSP-Versuch, ungeeignete Teilungen offene Räume.
Grundriss/Schnitt aus tatsächlichem Mesh; Blender-Workbench für 2.5D mit ausgeblendeter Decke und Tiefenpuffer.
Matplotlibs Dreieckssortierung erzeugte falsche räumliche Überlagerung; der Pfad ist entfernt.
Jede Nutzfläche hat direkten Flurzugang; sieben Treppenkerne mit Stufen, Podesten, Geländern und echten Deckenöffnungen.
Vorherige Erdgeschoss-/Reservepläne bleiben als Bildvergleich; nutzungsgerechte Raumprogramme, Aufzüge und native Begehbarkeit offen.
Logische Welt braucht stabile Raum-/Portal-IDs, Höhen, Wege und Kollision unabhängig vom Render-LOD.

## Komponenten und Umsetzung
- Gelieferte Footprints/Höfe/Parts/Höhen erhalten; fehlende Typ-/Geschoss-/Raumangaben kohärent ergänzen.
- Haupteingang/Nebeneingang, Windfang, Schwelle, barrierefreier Anschluss; Anlagen mit Wartungszugang.
  Öffnung verbindet Außenweg, Wanddurchbruch und Innenboden; erhöhte Parts benötigen Höhenanschluss.
- Nutzungsspezifisch: Garagenzufahrt/Tor, Ladebucht/Servicetor, Schaufenster und Laden-/Hofeingang.
- Geschossböden/Decken, Wandstärken, Innenräume/Trennwände/Korridore; Fenster sehen in passende Räume.
- Treppen/Podeste/Geländer, Schächte/Aufzüge; nutzbare Türhöhen, Kopffreiheit und Geschossanschlüsse.
- Fluchtwege/Notausgänge, passende Feuerleitern, Dachtreppe/-luke; Balkone/Terrassen mit realem Zugang.
- Durchgänge/Arkaden/Hofzugänge; Tragteile, Boden-/Dachränder und Infrastrukturkontakte schließen.
  Bauteile nach Nutzung/Form wählen; keine vollständige Ausstattung auf jedem Gebäude.

## Gestufte Entwicklung und Abnahme
1. Rechteck, L-Form, Hof, schmaler Flügel, erhöhte/unterirdische Teile als Grund-/Gegenfälle.
2. Metrische 2D-Grundrisse, Fassadenabwicklungen und Schnitte der tatsächlichen 3D-Geometrie.
   Raum-/Portalgraph, Geschosse/Öffnungen, Wand-/Deckenstärken und Konflikte sichtbar machen.
3. Dieselbe Geometrie in einfachen 2.5D-Ansichten öffnen; Anschlüsse/Kopffreiheit/Höhen prüfen.
4. Unverändertes Verfahren auf reale Stadtfenster; Anpassungen/ungelöste Teile mit Grund erhalten.
   Gültiges einfaches Rezept vor unbegrenzter Suche; Zeit/Varianten/Speicher/Wachstum begrenzen und messen.
5. Gemeinsamen Plan mit 2440/2460 verbinden; Tag-/Nachtbilder und begehbarer Pfad folgen daraus.
Zugang/Raum-/Geschossgraph gegen tatsächliche Geometrie prüfen; Türen/Treppen kollisionsfrei.
Keine versiegelten Zugänge, durchstoßenden Leibungen oder schwebenden Böden; Quelle/Höfe/Parts erhalten.
Mehrere reale Eingaben optisch prüfen; native Kollisions-/Spielintegration bleibt ausdrücklich offen.
[`building_plan_probe.py`](../test/experiments/buildings/building_plan_probe.py): `--circulation` Flur/Kernreserve, `--vertical` vollständige Quellhöhe mit Treppen.
[`verify_layout.py`](../test/experiments/buildings/verify_layout.py): geschlossene Prismen, freier Türkörper und Wege aus tatsächlichem Mesh.
Außenwand 0,24 m/Trennwand 0,12 m am Mesh prüfen; nur gemeinsame Raumgrenzen erhalten Trennwandstreifen.
GEOS repariert ausschließlich prozedurale Raumringe nach Koordinatentransformation; höchstens 1e−6 m² Flächenänderung, sonst Alternative verwerfen.
Abgeleitete Raum-/Wand-/Wegepolygone nutzen ein 1e−8-m-Präzisionsraster, maximal 1e−6 m² Flächenänderung je Operation.
Quellfootprint bleibt unverändert; obere Böden/Decken erhalten deklarierte Treppenöffnungen statt der bisherigen Vollplatten.
Erdgeschoss-/Dachabschluss behalten die vollständige Quelle. Mesh-Projektion folgt vor Clearance demselben erklärten Raster;
sonst erzeugen fast kollineare Boolesche Nähte falsche Mitre-Schnitte. Topologie-/Öffnungs-/Breitenprüfungen bleiben streng.
[`GEOS/Shapely-Präzision`](https://shapely.readthedocs.io/en/stable/reference/shapely.set_precision.html): gültige abgeleitete Geometrie, keine Änderung der Quelle.
0,30-m-Radius/1,90-m-Körperhöhe sind lokale Prüffigur, kein baurechtlicher Nachweis.
Alle Wege-/Raum-Insetprüfungen nutzen dieselbe konservative Mitre-Approximation; gesamte begehbare Raumfläche muss verbunden sein.
Feste IDs, gelieferte Geschosszahlen und Frontage geprüft; erhöhte/unterirdische Landings ohne Geländeanbindung bleiben offen.
[`building_core.py`](../test/experiments/buildings/building_core.py): feste Kernmaße vollständig innen; Zentrum gegen 9×9-Anker je Orientierung.
Technikreserve 2,4×2,4 m bei einem Geschoss; Treppenreserve 3 m breit, Länge aus Geschosshöhe und zweiläufigem Konzept.
0,18 m maximale Steigung/0,28 m Auftritt/1,10 m Podest steuern Reservierung und konstruierte Stufen.
Bis drei räumlich verschiedene Kernpositionen; vollständige Rechteckfläche prüfen, vier gültige Ecken reichen bei Höfen nicht.
[`building_corridor.py`](../test/experiments/buildings/building_corridor.py): zwei rechtwinklige Direktwege gegen orthogonalen Koordinatengraph.
Graph verbindet weitere Zugänge mit vorhandenen Ästen durch Mehrquellen-Dijkstra; keine globale Fluroptimalität.
Maximal 40×40 Knoten/3.120 Kanten, zwölf Versuche aus zwei Raumteilungen × drei Kernen × zwei Wegverfahren; maximal 24 Flächen.
Anker 0,87 m innerhalb der Raumgrenze. Große Restflächen mit engen Hälsen in getrennte nutzbare Flächen zerlegen;
Teilflächen ab 5 m² erhalten, kleine unbrauchbare Reste öffnen. Quellkontur/Höfe bleiben erhalten, ungeeignete Alternativen ausweisen.
Türportale nur auf geraden gemeinsamen Grenzabschnitten und mit voller Breite auf beiden Seiten; keine Türen um Ecken.
[`verify_circulation.py`](../test/experiments/buildings/verify_circulation.py): Mesh-Clearance nur mit Flur/Eingang, Türbesitz und Kernmaße prüfen.
0,75 m Achsabstand zum tatsächlichen Flurrand ergibt 1,50 m Zielbreite; versiegelte Tür als Gegenfall, feste IDs und Lochkontrolle.
Synthetischer Hof/Tokyo-Kontrolle: Flur höchstens 30 % der Innenfläche, damit die Erschließung keine ganzen Flügel absorbiert.
Flure/Räume sind generisch; kompakte Proportionen, Nutzung und passende Raumprogramme bleiben offen.
[`building_stair.py`](../test/experiments/buildings/building_stair.py): zweiläufiges geschlossenes Stufenprofil mit geneigter Unterseite,
mindestens 0,20 m vertikale Dicke; Antritt an vorhandener Bodenplatte gekappt. Keine massive Unterseite im darunterliegenden Lauf.
Ein metrisches Bauteil gedreht/platziert und je Geschoss wiederholt; Ghidra-Komponenten-/Transformbelege und Esri-Inline-Beispiel in 2400.
Kernzugang nur am freien Antrittspodest; 1 m hohe Balustraden an inneren Laufkanten, Zwischenpodest und oberen Öffnungsrändern.
[`building_vertical.py`](../test/experiments/buildings/building_vertical.py): gleiche Quellhöhe, stabile Geschoss-/Raum-/Portal-IDs,
gemeinsamer Flur pro Geschoss; generischer Raumplan wiederholt, keine belegte Rekonstruktion. Budget maximal 64 Geschosse.
[`verify_vertical.py`](../test/experiments/buildings/verify_vertical.py): geschlossene Bauteile, tatsächliche Auftritte/Deckenöffnungen,
Zugangsgraph aller 399 Flächen und geometrische An-/Austritte. Verschlossene Decke/fehlende Stufen als echte Mesh-Gegenfälle.
Spaltenprüfer gilt für vertikal zusammenhängende geschlossene Prismen/Profile, nicht beliebige nichtkonvexe Assets.
16 Radialproben plus Zentrum, 0,10 m Fußradius; Torso 0,30 m Radius/0,40–1,90 m über den Füßen.
Übergänge höchstens 0,10 m horizontal abtasten, Füße beim Steigen anheben; Stichprobe, keine kontinuierliche native Kapselphysik.
Deckenöffnung folgt getrennt An-/Austritt: längerer unterer Lauf braucht frühere Öffnung; obere Austrittstufe behält ihr Podest.
Oberes Abschlussgeländer folgt diesem Absatz; keine kleinere Prüffigur. Erhöhte/unterirdische Außenanschlüsse weiterhin ungelöst.
Profil gegen einzelne gleich geformte Stufenbauteile: gleiche analytische Volumen/Auftrittshöhen; je 21 warme Python-Läufe,
p50/p95, Bauteile und Dreiecke offenlegen. Geschlossenes Profil benötigt lokal weniger Aufbauarbeit/Dreiecke; kein nativer Framebeleg.
Grundriss und zwei überlagerte parallele tatsächliche Treppenschnitte zuerst; Blender-Ansichten desselben Meshs danach.
Kern-Nahansicht blendet übrige Bauteile/Böden aus; vollständiges Prüfmesh und Ausblendungsmetadaten erhalten.
2440 reserviert Dachhöhe vor Geschossplanung; `body_top_m` getrennt von erhaltener `source_top_m`, direkte Geschosszahl unverändert.
Kernkandidaten dürfen den Start nicht in ihrer Mitre-Clearance blockieren; notwendige Voraussetzung vor den zwölf teuren Raum-/Wegealternativen.
Bis fünf Antrittstüren je vier Kernorientierungen; nach jeder versetzten Tür vollständige Raum-Clearance erneut verbunden verlangen.
Bauteil-IDs append-only monoton, letztes Element statt wiederholtem globalem Maximum; lokale Körpermeshes bitgleich zum vorherigen Emitter.
WKB-Plan/NPZ-Prüfmesh in Build sind Experimentausgaben, kein integrierter Asset-Cachevertrag (2460).

## Verfahren
[Instant Architecture, SIGGRAPH 2003](../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf).
[Residential Building Layouts, SIGGRAPH Asia 2010](../doc/references/buildings/siggraph/2010-computer-generated-residential-building-layouts.pdf): Raumprogramm/Adjazenzen → Geschossplan → 3D.
2400 ergänzt Esri-/Ghidra-Belege; begrenzte Layoutverfahren bei festem Quellkörper vergleichen; Seiten 1/3/6/7/8/9 optisch geprüft.
