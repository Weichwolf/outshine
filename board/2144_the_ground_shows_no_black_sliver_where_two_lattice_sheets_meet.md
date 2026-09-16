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

Malcesine ohne Vegetation: 1598 Kantenmaxima lokalisiert; PNG 46e4db5c unverändert
und geöffnet. Globales Maximum 183,594 m liegt außerhalb des Kamerablicks. Große
in den Frustum projizierte Werte liegen überwiegend weit entfernt und können verdeckt
sein; Frustum ist keine Sichtbarkeitsprüfung. Ein wandnaher Kandidat bei
10,722656250° E / 45,771115228° N wechselt 422,675 → 433,882 m (11,208 m),
zwischen Quellzoom 13 und 12. Keine bewiesene Erklärung des gesamten Faltenvorhangs.
Diagnose entfernt. Rohhöhen, finale Oberfläche und Nahtkorrektur müssen getrennt
gegen Bild-/Geometriefehler geprüft werden. Temporäre Logs `outshine-seam-edges-*`
liegen im System-Tempverzeichnis. Skirt-Gegenprobe steht in 2166.

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
