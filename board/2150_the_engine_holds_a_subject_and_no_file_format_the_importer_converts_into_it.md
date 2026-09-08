Type: debt
State: active
Area: engine, import, scene, render
Tags: architecture, ownership, audit
Parent: 2188
Depends:

# Importers and generators deliver one engine-owned geometry model

## Befund und Entscheidung

Quellprüfung 2026-09-08: `Declaring.cpp` konvertiert öffentliche `Geometry` über
`Gltf::Subject::Assemble`. `Asset` hält Document, Subject, Pose und VariantSelection
im Import-Namespace. `Live::Declaration::Built` und `Restand` verlangen Gltf::Subject.
`Subject` mischt dekodierte Meshdaten, Importzugriff, Skinning und Rückkonvertierung
über `Handed`. Ein Namespace-Wechsel würde diesen Designfehler nicht beheben.
`Live` setzt außerdem den Schatten-Casterbereich anhand importiert/gebaut (2128).

Ein kanonisches engine-eigenes Geometriemodell. Importer und Generatoren liefern
identische native Produkte. glTF ist ein beliebiges unterstütztes Importformat;
Document, Accessors, Dateinodes und Extension-Dispatch enden am Importadapter.
Bestehende Geometry-, Material-, Transform- und GPU-Packing-Fähigkeiten nutzen,
aber redundante CPU-Modelle und Rückkonvertierungen vollständig ablösen.

## Datenverträge

- Mesh-Assets besitzen lokale Vertex-/Indexdaten, Submeshes, Bounds und Material-
  referenzen. Attribute, Topologie, Indexbreite und Validierung explizit definieren.
- Material-/Textur-Assets sind engine-eigen; Metallic-Roughness, Farbräume und
  Samplersemantik erhalten. Keine glTF-Indizes als langlebige Runtime-Identitäten.
- Instanzen referenzieren gemeinsame Assets über validierbare Handles; lokale
  Transformationen, Hierarchie und Weltposition sind unabhängig vom Dateibaum.
  Weltpositionen Double, lokale Mesh-/GPU-Daten Float mit dokumentierten Grenzen.
- Native Skeletons, Clips, Morphziele, Kameras und Varianten getrennt besitzen;
  Animation darf kein Importdokument zur Frameauswertung benötigen.
- Weltzellen halten Instanzen und Residency, nicht mehrfach kopierte Meshes.
  Generationen schützen Handles und verworfene Streaming-Ergebnisse.
- GPU-Packing, LOD, Kollision und Navigation sind abgeleitete Produkte mit eigener
  Residency. Sie sind keine konkurrierenden Quellen des Weltzustands.
- Schatten, Materialbehandlung, Instancing und Sichtbarkeit folgen Komponenten und
  expliziten Eigenschaften; niemals der Herkunft importiert/generiert.
- Importfehler transaktional als expected, geliehene Spans mit Lebensdauervertrag;
  keine Importarbeit oder unbeschränkten Allokationen im Framepfad.

## Vorbedingungen im nativen Besitzer

`Geometry::wellFormed` muss aktive Parts statt zurückbehaltener Kapazität prüfen.
`clear` entfernt auch Bilder; Attributsetter verweigern nichtendliche Werte ohne
Mutation. Winding-Diagnostik muss bei unvollständigen Attributen sicher bleiben.
Regression: größerer Aufbau → clear → kleinerer Aufbau, wiederholtes clear,
NaN/Inf je Attributkanal und Dreiecke mit fehlenden Normalen in jeder Ecke.

Die öffentliche Geometry-/Manager-Dokumentation beschreibt jetzt auch verbleibende
Grenzen: unvalidierte Material-/Transformwerte, lokale wiederverwendbare Integer-
Indizes, unbenutzbare Move-Quellen und allokierende Setter. Vor nativer Runtime-
Abnahme: validierte Asset-Publikation, echte Handle-Generationen und 2194-Fehlervertrag.
Image-Import verlangt geprüfte Größenrechnung und exakte RGBA8-Quelllänge;
Überlauf und erschöpfte Bildindizes vor Kopie ablehnen. Fehler verändern weder
Bilder noch Indexvergabe. Dokumentation verbleibender Lücken akzeptiert sie nicht.

## Direkter Renderer-Zulauf

Vor der Migration ignorierte `import/surface/Shaped.cpp::FillFrom(Geometry)` Part-Platzierungen und
setzte Lichtposition nur auf Matrixtranslation; lokale Lichtposition/-richtung gingen
verloren. Der Subject-Pfad transformierte dagegen korrekt. NativePlacementPreserves-
TheSurface prüfte bisher nur Subject::Assemble; direkte Shape-Bounds und Lichtwerte
müssen dieselben unabhängigen Sollwerte erfüllen. GPU-Aufbereitung gehört in Render,
mit einem nativen Eingangsvertrag und dokumentiertem Besitzer für sämtliche Views.
Meshdaten und Instanztransformation getrennt erhalten; keine zweite Formatwelt bauen.
Reproduktion: 16 Fehler bei 79 Checks im erweiterten Transformationstest.
Render::PrepareShape/AppendGeometry erzeugen nun eigene gepackte Attribute/Namen
und übernehmen statische Part-Platzierung in Modellkoordinaten. CPU-Geometry bleibt
lokal, Welt-/Instanzplatzierung bleibt Runtime-Aufgabe. Gemischte Views erst nach
allen Appends binden. 100 Checks für Bounds, Licht, Normalen, Spiegelung, Rebase und
Quell-Clear bestehen, auch mit ASan/UBSan der beteiligten Packing-Komponenten.
Conventions: 29/30 grün; intermittierendes Mipmap-Problem bleibt in 2179.
Malcesine-PNG geprüft: Geländewände/Materialdefizite bleiben; keine visuelle Abnahme.
Gemeldeter Peak-Heap 593 MB gegenüber 589 MB zuvor; kein isolierter Kostennachweis.

AudioOcclusion.cpp leitet die Audio-BVH aus nativen Parts mit Platzierung ab;
keine Physikkollision. Render- und Audiozustand erst nach erfolgreicher
Vorbereitung gemeinsam veröffentlichen; Fehler dürfen keinen Mischzustand erzeugen.

## Migrationsfolge

1. Native Asset-/Instanzverträge aus vorhandenen Consumern ableiten; Geometry und
   Subject-Felder vollständig zuordnen, Besitz und Invalidierung dokumentieren.
2. Einen vollständigen statischen Pfad migrieren: Generator und glTF-Importer →
   derselbe native Mesh-/Materialbesitzer → Instanz → Renderer. Alten Umweg entfernen.
3. Animation, Varianten, Kameras und Asset-Lebensdauer vollständig migrieren;
   Importdokument nach Konvertierung freigeben. Keine verlorenen Fähigkeiten.
4. Engine-/Render-/Generator-Tiers gegen Importheader sperren. Import/Export nur an
   Werkzeug-/Ladegrenzen orchestrieren; installierbarer Client nutzt öffentliche API.

Nach dem begonnenen Submission-Fix 2190 hat diese Grenze Vorrang vor weiteren
herkunftsspezifischen Reparaturen. 2128 behebt zusätzlich Instanz-/Terrain-Schatten;
2195 nutzt den Importadapter für den direkten Clientpfad. Keine zyklischen Blocker.

## Referenzmaßstab

Engine-Assets, getrennte Instanzen und Importadapter sind das Architekturziel.
Unreal/Filament/Cesium anhand veröffentlichter Asset-, Rendering- und Streaming-
Verträge prüfen; konkrete Übernahme vor Implementierung belegen. RAGE liefert
visuelle und funktionale Ziele, keine behauptete Kenntnis proprietärer Interna.
Keinen kompletten ECS oder neuen Assetcontainer ohne konkreten Consumer erfinden.

## Abnahme

- [ ] Importer und Generator erzeugen nachweislich denselben nativen Meshvertrag.
- [ ] Äquivalente importierte/generierte Fixtures haben gleiche Materialien,
      Instanzen, Schatten und Bounds; mehrere Instanzen teilen den Meshbesitzer.
- [ ] Freigabe des Importdokuments beeinflusst Rendering und Animation nicht.
- [ ] Handles nach Unload/Reload, Teilfehler und veraltete Streaming-Ergebnisse geprüft.
- [ ] Engine/Render/Generator bauen ohne Importheader; absichtlicher Import-Include
      scheitert am Tiervertrag. Format-Corpustests bleiben ausdrücklich erlaubt.
- [ ] Khronos-Corpus erhält Skinning, Morphs, Varianten, Texturen und Kameras.
- [ ] Betroffene Places vorher/nachher rendern und PNGs öffnen; strukturelle Migration
      bildgleich, fachliche Fehlerkorrekturen mit unabhängigem Oracle abnehmen.
- [ ] make lint samt clang-tidy und passende Make-Tests ohne neue Befunde;
      API-Verträge für Einheiten, Ownership, Threads, Fehler und Kosten dokumentiert.
