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

## Verbleibender Engpass: Renderarbeit während der Drehung

Absolute Frame-Zeitplanung und GPU-Vorbereitung innerhalb des Preloads beseitigen
Anlaufausreißer mehrerer Stadtansichten. Jura überschreitet das Framebudget weiterhin
bei bereits residenter Welt. Die Phasenmessung lokalisiert die Spitze im Host-Fence-Warten;
CPU-Vorbereitung und Simulation erklären sie nicht. Das beweist noch keine GPU-Passzeit.
Den nächsten Profilvergleich auf identische Welt, Kamera und Messframes begrenzen:
Das getrennte Abschalten von Terrain-Klassifizierung und prozeduralem Felsdetail
beseitigt die Budgetverletzung nicht. Diese Materialpfade deshalb nicht ohne weiteren
Nachweis umbauen. Als Nächstes Zeichenlast und Überzeichnung je Geometrieklasse isolieren;
Host-Fence-Warten weiterhin nicht einer einzelnen GPU-Passzeit zuordnen. Diagnosevarianten
mit verändertem Bild sind keine Abnahme und dürfen keine Baseline ersetzen. Erst den
gemessenen dominanten Pfad optimieren; Weltinhalt und Materialgrenzen erhalten.

Die idempotente Terrain-Lieferrevision ist vorhanden; geänderte Bytes oder Provenienz
vergeben weiterhin eine neue Revision. Stadt-Ladefehler bleiben WI 2319, stationäre
Wiederholungsarbeit WI 2124. Keine weitere Cachekampagne.

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
Die Initialisierung bleibt vollständig im Ladebudget. Verbleibende Renderkosten während
der Drehung separat beheben; die GPU-Vorbereitung ist kein Beleg für allgemeine Budgettreue.

## Abnahme

Format, fokussierte Client-/Preload-Prüfung, alle 14 Places und vollständiger Lint.
Hockenheimring wird als statische Übersicht gerendert; Routentests ersetzen dieses Bild nicht.
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
