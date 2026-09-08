Type: debt
State: open
Area: include, engine, render, world
Tags: architecture, audit
Parent: 2169
Depends: 2194, 2193, 2096, 2093, 2094, 2124, 2130, 2131, 2132, 2139, 2149, 2150, 2151, 2185, 2189, 2190, 2191

# Engine contracts will match the streaming sandbox architecture

## Entscheidung und Umfang

Quellprüfung 2026-09-08, keine vollständige Race-/Backend-Abnahme.
Best Practice heißt überprüfbare Zuständigkeit, Lebensdauer, Datenverträge und Kosten.
Keine komplette Neuschreibung und kein ECS-/Framegraph-Umbau ohne konkreten Befund.
Bestehende RAII-GPU-Wrapper, TilePool, Worker, Renderplan, Registry und native
Materialpfade weiterverwenden, sofern ihre Verträge halten.

| Befund im aktuellen Code | Verantwortliches WI |
|---|---|
| Outshine.h: rohe Engine-Zeiger in Renderer/SwapChain, bewegliche Engine | 2189 |
| Engine.cpp: fremde SwapChain nur auf Größe geprüft | 2189 |
| Engine.cpp: Targeted vor erfolgreichem DrawsInto gesetzt | 2191 |
| Declaring.cpp: nicht behandeltes Event als Fehler mit gemeinsamem Error | 2191 |
| SceneRenderer.cpp: CommandBuffer ungeprüft; NULL-Swapchain als Fehler | 2190 |
| EngineHeld.h: gemeinsame Sim-/Render-/Audio-Daten, Booleans für Phasen | 2130, 2191 |
| EngineHeld.h/Live/Asset: Gltf-Typen außerhalb Importgrenze | 2150 |
| ScenarioRead/Write: getrennte Schema-Walks, yes/no-Lücke | 2151, 2131, 2187 |
| Grounds/Laying: globaler Aufbau und nachträgliche Terrainänderung | 2124, 2166, 2144 |
| WorldCrowns: statische Residency und unvollständige Distanzleiter | 2111, 2123, 2132 |
| Rote Format-/Tidy-/Repository-Gates | 2093, 2094 |

Ziel: Plattformadapter → Engine-Fassade → Simulation/Streaming/Rendering.
Provider liefern versionierte Daten; Generatoren liefern native Produkte;
Simulation besitzt veränderlichen Weltzustand; Renderer liest fertige Snapshots.
Navigation, Kollisionsprodukte und sichtbare LOD teilen räumliche Referenzen,
bleiben aber unabhängig resident und versioniert (2133, 2175, 2127).
Öffentliche SDL-Fenster-/Event-Adapter sind legitim. Generatoren und Szenariomodell
benötigen keine SDL-Typen. Öffentliche API darf keine Importimplementierung verlangen.

## Referenzen und Grenzen

**Benchmark**: Unreal dient mit veröffentlichten Verfahren als Architekturvergleich;
RAGE/Arma/Far Cry/DayZ/KCD/RDR liefern Qualitäts- und Skalierungsziele. Nicht öffentlich
belegte interne Klassen, Kernzuordnungen und Algorithmen sind keine Spezifikation.
Filament: Rendering, Materialphysik und API-Verträge, keine komplette Sandbox-Vorlage.
https://github.com/google/filament/blob/main/filament/include/filament/Engine.h
Cesium: Georeferenzierung, asynchrone Tile-Produkte, Refinement/Residency; weder
Verkehrssimulation noch alleinige Vorlage für prozedurales Weltstreaming.
https://cesium.com/learn/cesium-native/ref-doc/rendering-3d-tiles.html
https://cesium.com/learn/cesium-native/ref-doc/selection-algorithm-details.html
SDL3: normative Plattformverträge, insbesondere Threadbindung und GPU-Lebensdauer.
https://wiki.libsdl.org/SDL3/CategoryGPU
Khronos glTF: Asset-/Metallic-Roughness-Konventionen; GLSL implementiert die BRDF
explizit. SpeedTree: Vegetationsreferenz erst in P4; CARLA/SUMO: Netze/Verkehr.
Webcams bleiben Plausibilitätsmaßstab, kein Soll für einen echten Weltzustand.

## Reihenfolge und Abnahme

P0 zuerst 2189/2190/2191, dann 2124 und 2130/2132 entlang ihrer Abhängigkeiten.
2096/2139 nur zusammen mit tatsächlichen Vertragsverbesserungen; kein großer Rename
als Ersatz für Bildqualität. 2150/2151 schrittweise pro vollständigem Consumer.
P1–P5 aus 2169 unverändert; Architektur muss deren Umsetzung erleichtern.

- [ ] Minimaler externer Client nutzt nur installierbare öffentliche Header/Library.
- [ ] Fenster, Offscreen, mehrere Engines, Fehler/Redeclare und Shutdown geprüft.
- [ ] Thread-/Ownership-/Schema-Orakel der Kinder inklusive Negativkontrollen grün.
- [ ] Bewegte Places und Dauerlauf zeigen begrenzte Arbeit und CPU/GPU-Residency.
- [ ] PNGs bleiben bei rein strukturellen Änderungen gleich; fachliche Fixes gegen
      unabhängiges Oracle und visuell abnehmen, keine falschen Altbilder konservieren.
- [ ] make lint einschließlich clang-tidy und relevante Make-Tests tatsächlich grün.
