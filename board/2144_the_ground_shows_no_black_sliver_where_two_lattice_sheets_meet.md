Type: bug
State: active
Parent: 2169
Area: world, render
Tags: webcam, measured
Depends: 2166

# Adjacent terrain patches share the final boundary

## IST und Lösung

Skirts und unabhängige LOD-/Stamp-Auswertung können sichtbare Seitenstreifen produzieren.
Die dunklen Stellen allein beweisen kein Loch: Schatten, Deckung, Winding und fehlende
Flächen durch Depth-/Face-ID-/Normalbilder auseinanderhalten. Einstieg:
`src/world/ground/tiles/TerrainGrid.*`, GroundLattice und `src/engine/Laying.cpp`.

Aktueller Malcesine-Lauf ohne Vegetation: StitchEdges verändert virtuelle Randpunkte
um bis zu 183,594 m, reale um 15,496 m; nach vorheriger Fehlerselektion. Stamping
senkt höchstens 28,161 m ab und hebt höchstens 29,656 m an. Der Skirt-Verdacht ist
in 2166 widerlegt. Ob die maximale Randänderung die sichtbare Felswand trifft,
ist offen. Temporäre Diagnose protokolliert neue Randfehler-Maxima mit Tile, Quellzoom,
Geokoordinate sowie Höhe vor/nach Stitching; Logs ins System-Tempverzeichnis.
Erwartung: räumliche Zuordnung trennt sichtbare Wand von entfernten Nahtfehlern.
Diagnose verändert keine Höhen und wird nach Auswertung wieder entfernt.

Gemeinsame finale Rand-Samples und Edge-IDs je Tile/LOD, Nachbar-LOD beschränken,
Stitch-Indizes oder gemeinsam morphten Rand verwenden. Stamp-/Relief-Änderungen invalidieren
beide Besitzer. Keine verdeckende tiefgezogene Seitenwand als Ersatz für passende Oberflächen.
Unreal-Landscape/Cesium-Terrain sind die geometrischen Vergleichsmodelle; proprietäre
RAGE-Interna sind hierfür kein belegter Vertrag.

- [ ] Positionen gemeinsamer Ränder stimmen nach Stamping/LOD-Wechsel im geometrischen
      Toleranzmaß überein. Licht-/Materialnaht zusätzlich im Bild prüfen.
- [ ] Malcesine, Feldkirch, Husum und der ältere Heidelberg-Fall bleiben von beiden Seiten
      bei Tilewechsel geschlossen. Negativkontrolle: einen Rand versetzen, Oracle wird rot.
- [ ] Offene Weltgrenze von internem Nahtfehler unterscheiden; keine DoubleSided-Kaschierung.
