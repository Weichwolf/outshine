Type: feature
State: active
Priority: P0
Architecture: ready
Parent: 2169
Depends:
Area: client, engine
Tags: places, screenshots

# Complete Place loading precedes a fixed frame measurement

## Ergebnis und Entscheidung

`outshine-client shots` lädt die Welt vollständig für die deklarierte Ansicht,
misst genau 120 Frames für p50/p95/p99 und speichert das Bild. Die Messung darf
nicht auf bis zu 6144 Frames wachsen, während die Welt noch verfeinert wird.
Bei 60 Hz benötigen 120 Frames 120/60 = 2 Sekunden; langsamere Frames bleiben
als Überschreitung sichtbar. Laden und Messzeit werden getrennt ausgewiesen.
Warmes Laden ab Cache soll höchstens ein bis zwei Sekunden benötigen. Der aktuelle
Cache enthält Quelldaten; Generierung und GPU-Upload folgen erneut. Diese Schritte
getrennt messen und den belegten Engpass beheben, keine Ladegrenzen erhöhen.

## Implementierung

`client/PlaceCamera.cpp` besitzt Preload, feste Messung und Capture. Vor der Messung
muss Refined erreicht sein; ein Ladefehler liefert kein unvollständiges Erfolgsbild.
`engine/Engine.cpp` besitzt den vorhandenen qualitätsabhängigen Preload-Vertrag.
Der Preload darf neue Ground-Revisionen nicht von fertigen Details der alten Revision
abhängig machen: Koerbersee wartet bei Footprint-Revision 3/4 ohne Kandidat und ohne
laufenden Bake. `Grounds` prüft wie im Runtime-Pfad selbst Revisionswechsel und
Unchanged; alte Details sind keine Eintrittsbedingung für diese Prüfung.
Die feste Messung ist kein Ersatz für Laden und führt keine Refined-Warteschleife.
Screenshot-Capture und notwendige Renderer-Initialisierung explizit abgrenzen.
Vorhandene Hash-PNGs erhalten; vollständige Inhalte und Bildänderungen selbst prüfen.

## Nächster Engpass

Graz erhält identische Terrain-Bytes aus derselben Quelle erneut. `TilePool` vergibt
jedes Mal eine neue Lieferrevision; bereits gehaltene Kandidatenfelder werden dadurch
stale und ihre Gebäudeaufträge endlos zurückgestellt. `TerrainRevisionIndex` muss
identische Lieferungen bei noch vorhandener Registrierung idempotent erkennen.
`TilePool` liefert dafür einen SHA-256-Fingerprint aus angefragter/gelieferter Adresse,
Quellidentität, Abwesenheitsstatus und Payload. Geänderte Bytes oder Provenienz vergeben
weiterhin eine neue Revision. Unbekannte/eviktierte Registrierungen bleiben konservativ;
keine Freigabe durch bloß gleiche Koordinaten oder deaktivierte Zertifikatsprüfung.
Metadaten bleiben im bestehenden begrenzten Index, ohne Terrain-Bytes dort zu halten.
Zusätzlich zeigt ein Stack-Sample wiederholte Speicherzählung und Patchwork-Anfragen
innerhalb `FlushPreloadGround` → `Grounds` → `RingWanted`; erst den Stillstand schließen.

## Abnahme

Format, fokussierte Client-/Preload-Prüfung, alle Places und vollständiger Lint.
Je erfolgreicher Messung genau 120 Samples; fehlende Weltprodukte verhindern den
Beginn der Messung. Ladezeit separat prüfen. Ein schnelles leeres Bild oder fehlende
Gebäude widerlegen die Lieferung. Zielgeräte-Budget bleibt separat nachzuweisen.
