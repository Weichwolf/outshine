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

`outshine-client shots` erreicht innerhalb höchstens zehn Sekunden die vollständige
Refined-Welt. Danach folgen genau 60 Messframes bei 60 Hz: 60/60 = eine Sekunde für
360 Grad Kameradrehung am unveränderten Standort. Pitch und Höhe bleiben erhalten;
der letzte Frame kehrt exakt zur deklarierten Ausgangsrichtung zurück und wird als
einziges PNG gespeichert. Keine zusätzlichen Capture- oder Einschwingframes danach.
p50/p95/p99 werden aus diesen Frames bestimmt; langsame Ausführung bleibt als
Budgetverletzung sichtbar und darf weder Frames überspringen noch die Drehung verkürzen.
CPU-Framezeit und echte GPU-Zeit getrennt ausweisen; fehlende GPU-Messung nicht umdeuten.
Netzwerkquellen bleiben gecacht; die Welt darf keine persistenten Geometrieprodukte benötigen.

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
Je erfolgreicher Messung genau 60 Samples und 360 Grad; fehlende Weltprodukte verhindern
den Beginn der Messung. Preload maximal zehn Sekunden, gemessene Drehung maximal eine
Sekunde und p99 höchstens 1000/60 ms prüfen; Wartezeit und Arbeitszeit getrennt erfassen.
Mindestens ein echter Place bleibt verpflichtend im Gate. Referenzbilder umgehen keine
Zeit-, Sample- oder Vollständigkeitsprüfung. Ladezeit separat prüfen. Ein schnelles leeres Bild oder fehlende
Gebäude widerlegen die Lieferung. Zielgeräte-Budget bleibt separat nachzuweisen.

## Visuelle Referenz

Passende Places mit `https://www.foto-webcam.eu` vergleichen: Kamera, Blickwinkel,
Bildzeit samt Zeitzone, Jahreszeit und Wetter zuordnen. Unbekannte Bedingungen offenlassen.
Ziel ist größtmögliche sichtbare Annäherung innerhalb 720p60 auf A18 Pro; Host-Zahlen
ersetzen keine Zielgeräte-Abnahme. Referenzbilder erst bei tatsächlicher Recherche zuordnen.
