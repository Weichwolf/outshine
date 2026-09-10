Type: debt
State: active
Area: engine, import, scene, render
Tags: architecture, ownership, audit
Parent: 2188
Depends: 2216

# Importers and generators deliver one engine-owned geometry model

## Befund und Entscheidung
Quellprüfung 2026-09-08: setGeometry kopiert nun native Geometry direkt;
Clone erhält aktive Daten und Ownership. Asset hält weiterhin Document, Subject,
Pose und VariantSelection. InitialGeometry wird beim Öffnen nativ kopiert;
Live speichert danach keinen geliehenen Geometriezeiger. Gemischte Parts ohne
Material erhalten einen eigenen Default-Slot; Pixel-Orakel prüft dessen Farbe.
`Subject` mischt dekodierte Meshdaten, Importzugriff, Skinning und Rückkonvertierung
über `Handed`. Ein Namespace-Wechsel würde diesen Designfehler nicht beheben.
`Live` koppelt Carry an Joined_ > 0 und Schatten-Caster an Herkunft (2128).
Draws instanziert dasselbe Subject unabhängig von Body::Asset. Native Asset-/Entity-
Bindung muss auch Physik ohne Mesh und Renderinstanzen ohne Physik erlauben.

UV-Grenze: native Rotation algebraisch definieren (+U nach +V), glTFs visuell
gegenläufige Rotation beim Import konvertieren. Das GLSL-Beispiel der Extension
hat einen bekannten [Vorzeichenfehler](https://github.com/KhronosGroup/glTF/issues/1563).
Vendor-Marker und Cycles bestätigen die bisherige glTF-Darstellung; diese erhalten.
Native Vierteldrehung durch unabhängige GPU-Quadranten prüfen, Import getrennt durch
Vendor-Parameter und grüne Pfeilmarker. Keine Formatkonvention in der öffentlichen API.

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

setPlacement prüft finite affine Matrizen (letzte Zeile exakt 0,0,0,1) vor Mutation.
expected trennt fehlenden Part/ungültige Matrix; Nullskalierung, Spiegelung und Scherung erlaubt.
Mat4::TransformPoint definiert die Algebra ohne Perspektivdivision. Altcode scheitert am
NaN/Inf-/Projektions-Test; Erhaltung, Retry, native Render-/Audio-Platzierung: vier Tests grün.
Image-Import verlangt geprüfte Größenrechnung und exakte RGBA8-Quelllänge;
Überlauf und erschöpfte Bildindizes vor Kopie ablehnen. Fehler verändern weder
Bilder noch Indexvergabe. Dokumentation verbleibender Lücken akzeptiert sie nicht.

## Direkter Renderer-Zulauf
Render::PrepareShape/AppendGeometry besitzen gepackte Attribute und Namen und
übernehmen statische Part-Platzierung in Modellkoordinaten. CPU-Geometry bleibt
lokal, Welt-/Instanzplatzierung bleibt Runtime-Aufgabe. Gemischte Views erst nach
allen Appends binden. NativePlacementPreservesTheSurface prüft Bounds, Licht,
Normalen, Spiegelung, Rebase und Quell-Clear unabhängig vom Importpfad.
Fehlende Normalen werden als getrennte Flächennormalen aufbereitet; 2179 bleibt offen.

AudioOcclusion.cpp leitet die Audio-BVH aus nativen Parts mit Platzierung ab;
keine Physikkollision. Audio-BVH wird nach erfolgreichem Render-Aufbau publiziert.
Vollständiger GPU-Rollback und atomarer Welt-/Render-Austausch bleiben offen.

## Migrationsfolge

Importer-Namen folgen Khronos (Node/Mesh/Primitive/Material/Animation/Skin), Runtime-Namen
bleiben nativ. Vektor-/Matrixmathematik teilen; nur Formatkonvertierung liegt im Adapter.
1. Native Asset-/Instanzverträge aus vorhandenen Consumern ableiten; Geometry und
   Subject-Felder vollständig zuordnen, Besitz und Invalidierung dokumentieren.
2. Einen vollständigen statischen Pfad migrieren: Generator und glTF-Importer →
   derselbe native Mesh-/Materialbesitzer → Instanz → Renderer. Alten Umweg entfernen.
3. Animation, Varianten, Kameras und Asset-Lebensdauer vollständig migrieren;
   Importdokument nach Konvertierung freigeben. Keine verlorenen Fähigkeiten.
4. Engine-/Render-/Generator-Tiers gegen Importheader sperren. Import/Export nur an
   Werkzeug-/Ladegrenzen orchestrieren; installierbarer Client nutzt öffentliche API.

Diese Grenze hat nach Submission-Fix 2190 Vorrang; 2128 behebt Instanz-/Terrain-Schatten;
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
