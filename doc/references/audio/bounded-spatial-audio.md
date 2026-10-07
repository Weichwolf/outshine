# Räumliches Audio mit begrenzter Arbeit

Codeprüfung: 2026-10-07. Besitzer: 2336 (native Abfragen), 2136 (Audio).
Native Terrain-Abfrage integriert; Cubemap, Stimmenauswahl und GPU-Pfad bleiben Entwurf.

## Geprüfte GitHub-Verfahren

- [Steam Audio: Unity-Integration](https://github.com/ValveSoftware/steam-audio/blob/480dd64f513cc8a6437e7d5b9eb0d3f1d30c2fac/unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Scripts/Runtime/SteamAudioManager.cs):
  `ClosestHit` und `AnyHit` nutzen `Physics.RaycastNonAlloc`. Der Simulator konfiguriert
  `maxNumSources`, `maxNumRays` und `maxNumOcclusionSamples`. Wiederverwendete Abfragen
  und explizite Grenzen, kein Beleg für kostenlose Simulation beliebig vieler Quellen.
- [Steam Audio: Integration](https://github.com/ValveSoftware/steam-audio/blob/480dd64f513cc8a6437e7d5b9eb0d3f1d30c2fac/core/doc/integration.rst):
  vorhandene Engine-Raytracer einbinden; teure Reflexion/Wege vom Mixer entkoppeln.
- [Resonance Audio: Raumparameter](https://github.com/resonance-audio/resonance-audio/blob/4556a46afd4ffae092aa281bfd072eb0279d3a29/platforms/common/room_effects_utils.cc):
  sechs Raumflächen, materialabhängige Reflexionskoeffizienten und frequenzabhängige RT60.
  Geeignetes Vorbild für gemeinsamen einfachen Hall; keine allgemeine Außenwelt-Verdeckung.

## Entscheidung für Outshine

1. Panning/Entfernung brauchen kein Mesh. Verdeckung nutzt vorhandene native Kontaktflächen
   entlang hörbarer Quelle-Hörer-Segmente; final deformiertes Terrain, nicht ursprüngliches DEM.
   Kein vollständiger Terrain-Dreiecksaufbau und keine getrennte akustische Weltkopie.
2. Grobe Richtungsfelder/Cubemaps als Experiment: Dämpfung in wenigen Frequenzbändern,
   Weglänge/Laufzeit und wenige Ankunftsrichtungen. Entfernung mit abbilden: dieselbe Richtung
   kann vor einer Wand frei und dahinter verdeckt sein. Ein Tap bildet keine mehreren Echos ab;
   direkte Wege und gemeinsamer parametrischer Hall ergänzen den groben Transfer.
3. Eine geteilte Umgebung am Hörer, begrenzte wichtige Einzelstimmen und feste Hallbusse.
   Quellen ohne hörbaren Beitrag virtualisieren; ähnliche Ferngeräusche räumlich bündeln.
   Keine Quelle-Quelle-Paare und kein Hallprozessor/Cubemap je deklarierter Weltquelle.
4. Bei linearer Filterung gilt H(sum x) = sum H(x): Quellen mit gleichem Transfer vor dem
   gemeinsamen Filter summieren. Unterschiedliche Verzögerungen/HRTFs vorher berücksichtigen.
   Linearität des Schalls ist kein Beleg für von der Quellenzahl unabhängige Rechenkosten.
5. Für N einzeln ausgewertete Quellen kostet schon die Bewertung O(N). Teure Akustik mit
   festen Strahl-/Schrittbudgets R und Stimmenlimit K begrenzen; keine unbeschränkte BVH-
   Traversierung unter einem bloßen Strahllimit. Räumliche Kandidaten, Ereignisse und
   inkrementelle Auswahl vermeiden einen Vollscan pro Frame; bei Überlast vergröbert ausgeben.
6. CPU-Prototyp gegen asynchronen GPU-Compute messen, einschließlich Upload/Readback und
   Konkurrenz zum Bildbudget. Mixer konsumiert den letzten fertigen geglätteten Snapshot,
   wartet auf keine GPU-Fence. Zeit, Speicher, Aktualisierungsalter und hörbare Fehler messen.

`Laying.cpp::BuildGroundResidency` publiziert Höhen ohne globalen CPU-Dreiecksaufbau.
`TerrainResidency::HeightMAt` fragt den vorhandenen Seitenindex ab und interpoliert die nativen
Zellen einschließlich ungleichmäßiger Posting-Abstände. Audio nutzt maximal 64 Wegproben;
schmale Hindernisse auf langen Wegen können fehlen. Keine konservative Fehlerschranke,
kein Physikkontakt-Ersatz. Zusätzliche importierte Dreiecke nutzen weiter `TriangleBvh`;
dessen Abfrage und die gesamte Stimmenauswahl sind noch nicht fest budgetiert.
Weitergehende Raumakustik bleibt nach dem visuellen Meilenstein in 2136.
