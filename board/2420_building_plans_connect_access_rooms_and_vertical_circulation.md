Type: feature
State: active
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
Aktuelle Hüllen/Repeats besitzen keine echten Türen oder nutzbaren Innenraum-/Wegepläne.
Python-Prototyp: acht lokal verbundene Erdgeschosspläne, darunter drei echte Esri-Vergleichsgrundrisse.
Wand-/Boden-/Deckenstärken, offene Türportale und Fenster in tatsächliche Räume; drei Gegenfälle mit Auslassungsgrund.
Begrenzte achsparallele Streifen gegen BSP-Teilung: gleiche Mindestflächen, Türbreiten, Clearance und Raumgraphprüfung.
Maximal zwölf Räume/256 Quellvertices; sechs Schnittkandidaten pro BSP-Versuch, ungeeignete Teilungen offene Räume.
Grundriss/Schnitt aus tatsächlichem Mesh; Blender-Workbench für 2.5D mit ausgeblendeter Decke und Tiefenpuffer.
Matplotlibs Dreieckssortierung erzeugte falsche räumliche Überlagerung; der Pfad ist entfernt.
Obergeschosse/Treppen, nutzungsspezifische Raumprogramme und native Begehbarkeit bleiben offen.
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
[`building_plan_probe.py`](../test/experiments/buildings/building_plan_probe.py): gleiche 3D-Pläne, Alternativen/Kosten/Quellgrenzen, elf Ansichten.
[`verify_layout.py`](../test/experiments/buildings/verify_layout.py): geschlossene Prismen, freier Türkörper und Wege aus tatsächlichem Mesh.
Außenwand 0,24 m/Trennwand 0,12 m am Mesh prüfen; nur gemeinsame Raumgrenzen erhalten Trennwandstreifen.
GEOS repariert ausschließlich prozedurale Raumringe nach Koordinatentransformation; höchstens 1e−6 m² Flächenänderung, sonst Alternative verwerfen.
0,30-m-Radius/1,90-m-Körperhöhe sind lokale Prüffigur, kein baurechtlicher Nachweis.
Alle Wege-/Raum-Insetprüfungen nutzen dieselbe konservative Mitre-Approximation; gesamte begehbare Raumfläche muss verbunden sein.
Feste IDs, gelieferte Geschosszahlen und Frontage geprüft; erhöhte/unterirdische Landings ohne Geländeanbindung bleiben offen.
Serielle Durchgangsräume sind erst geometrische Konnektivität; Flure/Privatsphäre und Erschließungskerne fehlen.
WKB-Plan/NPZ-Prüfmesh in Build sind Experimentausgaben, kein integrierter Asset-Cachevertrag (2460).

## Verfahren
[Instant Architecture, SIGGRAPH 2003](../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf).
[Residential Building Layouts, SIGGRAPH Asia 2010](../doc/references/buildings/siggraph/2010-computer-generated-residential-building-layouts.pdf): Raumprogramm/Adjazenzen → Geschossplan → 3D.
2400 ergänzt Esri-/Ghidra-Belege; begrenzte Layoutverfahren bei festem Quellkörper vergleichen; Seiten 1/6/9 optisch geprüft.
