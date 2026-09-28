Type: debt
State: active
Architecture: planned
Priority: P1
Area: include, engine, render, world
Tags: architecture, audit
Parent: 2169
Depends: 2093, 2094, 2096, 2124, 2130, 2131, 2132, 2149, 2150, 2151, 2185, 2190, 2191, 2194, 2207, 2208, 2209, 2210, 2214

# Engine contracts match the streaming sandbox architecture

## Entscheidung

Provider besitzen Quellen; Generatoren native CPU-Produkte; Simulation veränderlichen
Weltzustand; Rendering/Audio konsumieren Snapshots. Integration koordiniert Lebensdauer,
Versionsannahme und Budgets. Kein fremder Algorithmus in Engine::State, kein paralleler
Geometrievertrag, kein generisches Framework ohne konkreten Consumer.
RAII-GPU-Wrapper, TilePool, Worker, Renderplan, Registry und native Materialien erhalten,
soweit ihre Verträge tragen. Strukturänderungen brauchen einen belegten Fehlernutzen.

Öffentliche API bleibt formatunabhängig; Importtypen enden am Adapter (2150).
SDL-Fenster-/Event-Adapter sind legitim, SDL in Generatoren/Szenariomodell nicht.
Navigation, Kontakt und Render-LOD teilen Raumreferenzen, besitzen getrennte Produkte.
Weltpositionen Double, GPU kamera-relativ Float mit gemeinsamem Frame-Ursprung.
Queues, Abbruch, Arbeit und Speicher begrenzen; alte Ergebnisse ersetzen keine neuen.

## Kleine Coding-Reserve

| Rang | WI / Besitzer | Ausführbarer Schritt und Grenze |
|---|---|---|
| P0 zuerst | 2311 / engine streaming | Place-Absturz, Owner-Domänen und tatsächliche Refined-Bereitschaft beheben |
| P0 unabhängig | 2228 / engine + render | Olympiaturm: residenten CPU/GPU-Bestand erfassen und begrenzen |
| P0 unabhängig | 2092 / client diagnostics | Alle Places: gemessene Stadt-/Terrain-/Himmelkosten statt Hockenheim allein |
| P0 danach | 2312 / engine streaming | Bewiesene adaptive CPU-Fähigkeit in gepaarte BuildTask-Phasen integrieren |
| P0 danach | 2298 / render | Source-matching residentes LOD unter Bild-/Zeit-/Speicherabnahme auswählen |
| P1 | 2172 / world + engine declaration | Wetterzustand integrieren; danach Wolken/Licht/Materialien vorantreiben |
| P2 zuletzt | 2111 / generators + vegetation streaming | Vegetation implementieren und dann Stadt/Wald im selben Budget abnehmen |

2313 liefert die CPU-Grundlage; weitere isolierte Detailbeweise brauchen einen konkreten
Integrationsfehler. Jeder Code-/Shader-/Build-Schritt umfasst Places-Suite UND alle zehn
Place-Renderings mit geöffneten PNGs. Hockenheim/Kamerarunde ist nur der erste Test.
2170 ist P0-Architekturarbeit: Kamera-/Datum-/Verdeckungsursache belegen, keine freien
Place-Offsets oder ausgeblendete Geometrie. 2230 prüft vollständige Snapshot-Bereitschaft.

2140 Wolken und 2314 gemeinsamer Budgetplaner bleiben Architecture: planned, bis die
jeweiligen Eingangs-/Kostenverträge feststehen. Keine scheinbar ausführbare Reserve.
Fehlgeschlagene Gates zuerst korrigieren. Ein blockiertes WI beendet weder Reserve noch Ziel.

## Weitere Vertragsaufträge

| Befund / Abnahme | Besitzer-WI |
|---|---|
| Vollständige Gate-, öffentliche Header- und Shader-Belege | 2094, 2093, 2152 |
| GPU-Submit/History/Retirement und Kandidatenpublikation | 2190, 2191, 2223 |
| Begrenzte Jobs, Simulation und Produkt-Streaming | 2124, 2130, 2132 |
| Natives Importmodell, Schema-/Parsing- und Providergrenzen | 2150, 2151, 2214, 2194 |
| Kein allocator replacement in Library; atomisches Save, begrenzte Reader | 2209, 2210 |
| Installierbare Ressourcen und host-eigene Diagnostik | 2207, 2208 |
| Atomare Weltkronen, vollständige Residency, begrenzte Restphasen | 2225, 2228, 2234 |
| Geländeform, logisches Netz und räumliche Anschlüsse | 2166, 2133, 2175 |
| Konkrete Modul-/Namensdefekte, keine große Rename-Kampagne | 2139 |

Historische Quellaudits vom 2026-09-08 stehen in Git; ihr Fehlerstatus muss im
zuständigen WI neu geprüft werden. Quellenprüfung ersetzt keine Race-/Bild-/Backend-Abnahme.
Depends dieses Parent-WI betrifft die Gesamtvertragsabnahme. 2139 ist kein technischer
Blocker dieser Abnahme; gezielte Ownership-Defekte liegen bei ihren ausführbaren Kindern.

## Abnahme

- [ ] Externer Client nutzt installierte öffentliche Header/Library; Fenster, Offscreen,
      mehrere Engines, Redeclare, Fehler und Shutdown sind tatsächlich geprüft.
- [ ] Ownership/Threading/Schema mit unabhängigen Orakeln und wirksamen Negativkontrollen.
- [ ] Bewegung und Dauerlauf zeigen begrenzte CPU/GPU-Arbeit, Queues und Residency.
- [ ] Strukturelle Änderungen bewahren Verhalten; fachliche Fixes erhalten neue Bildabnahme.
- [ ] Vollständiges Lint samt API-Dokumentation und clang-tidy grün; reale Limits offen nennen.

Lokale Referenzen und Stand gehören in den fachlichen WI. Unreal/RAGE/Filament/Cesium
sind Vergleiche, keine unbelegte interne Spezifikation. Messungen im Projekt entscheiden.
