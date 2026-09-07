Type: bug
State: open
Area: world, render
Tags: webcam, measured
Depends: 2166

# Adjacent terrain patches share the final boundary

## IST und Lösung

Skirts und unabhängige LOD-/Stamp-Auswertung können sichtbare Seitenstreifen produzieren.
Die dunklen Stellen allein beweisen kein Loch: Schatten, Deckung, Winding und fehlende
Flächen durch Depth-/Face-ID-/Normalbilder auseinanderhalten. Einstieg:
`src/world/ground/tiles/TerrainGrid.*`, GroundLattice und `src/engine/Laying.cpp`.

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
