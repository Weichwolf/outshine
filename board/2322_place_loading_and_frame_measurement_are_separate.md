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
Die Place-Drehung deklariert vor assemble einen festen Katalog aus Ausgangsblick und
59 Zwischenblicken. `Engine::setView` wählt diese vor jedem advance; keine neue Kamera-API
und keine Renderer-Sonderbewegung. Bearing/Pitch-Kameras ändern nur den Bearing; explizite
LookAt-Kameras drehen Ziel und Up um die lokale geodätische Hochachse durch den Kamerastandort.
Die letzte Auswahl verwendet unverändert den Ausgangsblick. Die Drehung wird nur von `Take`
angefordert; allgemeine Szenario-Aufnahmen behalten ihre Kamera. Capture pinnt nach dem letzten
Messframe ausschließlich den Readback, ohne weitere Renderframes.
Screenshot-Capture und notwendige Renderer-Initialisierung explizit abgrenzen.
Vorhandene Hash-PNGs erhalten; vollständige Inhalte und Bildänderungen selbst prüfen.

## Nächster Engpass: erste vollständige Renderbereitschaft

Die absolute Frame-Zeitplanung verhindert aufsummierte Sleep-Verzögerungen. Mehrere
Stadtansichten überschreiten dennoch p99; ihr langsamster Frame ist häufig Frame 2.
`SceneRenderer` erlaubt zwei Frames gleichzeitig und wartet dann erstmals auf ein altes
GPU-Fence. Darmstadts Phasenmessung lokalisiert den größten Ausreißer im Fence-Warten,
nicht in der Simulation. Das ist Host-Wartezeit, noch keine GPU-Ausführungszeit.

`PrepareFrame` erzeugt Tabellen, Placements und Schattenvorbereitung erst beim Rendern;
`SubjectResidency` reicht unmittelbare Kopien ohne Abschluss-Fence ein. Zuerst trennen,
welcher Anteil aus diesen Initialisierungen und welcher aus den ersten GPU-Pässen stammt.
Vorbereitbare Tabellen/Uploads gehören in die bestehende Zehn-Sekunden-Ladefrist, mit
nichtblockierender Fence-Abfrage, Fehlerabschluss und gültiger Weltgeneration. Keine
zusätzlichen Renderframes zum Verbergen des Ausreißers und kein pauschales GPU-Idle-Warten
im Framepfad. Tatsächliche Arbeit bleibt gemessen; unvollständige GPU-Produkte sind nicht
renderbereit. Bestehende 60 Messframes, Rundum-Abdeckung und letztes Capture erhalten.

Die idempotente Terrain-Lieferrevision ist bereits vorhanden; geänderte Bytes oder
Provenienz vergeben weiterhin eine neue Revision. Die Stadt-Ladefehler bleiben WI 2319,
stationäre Wiederholungsarbeit WI 2124. Keine weitere Cachekampagne.

## Aktive Umsetzung

Für Refined-Preload einer assemblierten Ground-Szene mit aktiver Kamera bereitet
`SceneRenderer::PrepareWorldResources` vorhandene Tabellen/Placements vor und reicht
noch ausstehende Uploads mit einem eigenen OwnedFence ein. Die Engine fragt dessen
Abschluss innerhalb derselben Ladefrist ab und gibt zwischen Abfragen CPU-Zeit frei.
Währenddessen wird keine neue Welt publiziert; das Fence gehört zum vorbereiteten
Stand. Fehler/Timeout geben die Diagnose zurück und behalten gültige Ressourcen.
Die vorhandene gecachte Schattenkarte wird im eigenen LightVisibility-Pass ebenfalls
vorbereitet und erst nach erfolgreicher GPU-Submission als gültig markiert. Sie ist ein
GPU-Vorbereitungsprodukt innerhalb der Ladefrist, kein zusätzlicher Szenenframe.
Keine Zeit-/Kamerafortschaltung oder Manipulation der Frame-Fences/Capture-Historie.
Groundlose und Playable-Preloads behalten ihren bisherigen Vertrag.
Das Experiment muss zeigen, welcher Teil des Anlaufausreißers danach noch besteht;
es verspricht keine bereits gemessene vollständige GPU-Entlastung.

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
