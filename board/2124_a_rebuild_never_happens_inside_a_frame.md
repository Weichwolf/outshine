Type: bug
State: active
Area: engine, world
Parent: 2169
Depends:

# Rebuilds will leave the frame path and update only changed regions

## Befund und Ziel

Updates erreicht Grounds(false); Ringbewegung und ankommende Ergebnisse können
weltweite Arbeit auslösen. Statische Place-Frames messen diese Kosten nicht.
Geänderte Kacheln und ihre Randabhängigkeiten bestimmen die Arbeit, nicht der Ring.

Vorhanden: Worker-Bakes, TilePool-Felder und geteilte DecodedCache-Daten.
Ergebnisse tragen ihre Bytes statt sie nach Übergabe erneut aus einem LRU zu lesen.
Diese Schritte nehmen weder das gesamte Rebuild noch dessen bewegten Framepfad ab.
Historische Messungen und korrigierte Fehlinterpretationen stehen in Git.

**Benchmark**: Unreal trennt FIoDispatcher und asynchrone Aufbauarbeit; RAGE trennt
Streaming-Threads und Compute. Übernommen: abgeschlossene Produkte per Snapshot,
begrenzte Übergabe, bisherige vollständige Welt bleibt bis zum Austausch sichtbar.

## Umsetzung

1. Graph, Alignment, Render- und Kontaktprodukte getrennt versionieren. Sicht-LOD
   darf Navigation/Physik nicht neu erzeugen; alle nutzen konsistente Strukturdaten.
2. Ground-/Straßen-/Wasseraufbau und Pressing auf Worker aus unveränderlichem Snapshot.
   Tileänderungen invalidieren nur betroffene Produkte plus direkte Randabhängigkeiten.
3. Persistente Cluster-/Instanzbereiche; Upload nur geänderter Bereiche. Retable darf
   bei Ankunft einer Kachel nicht alle Cluster neu bauen. Jobs nach Budget übernehmen.
4. Beim Rezentrieren nur verlassene Ressourcen freigeben und neue Arbeit anfordern.
   Abgebrochene/überholte Jobs dürfen keine alte Generation veröffentlichen.
5. Frame übernimmt vollständigen Snapshot atomar und blockiert weder auf IO noch Worker.

## Abnahme

- [ ] Eine Kachel Bewegung kostet proportional deren Änderungen, nicht den ganzen Ring.
- [ ] Laufende Kamera in allen Places: 720p60 nach 2092, Spitzen und Perzentile angeben.
- [ ] Während Aufbau bleibt vorherige vollständige Welt sichtbar, ohne Teil-Snapshot.
- [ ] Brückenrampe, Tunnelportal und gestapelter Knoten: Sim/Render/Kontakt konsistent.
- [ ] Keine Frame-Allokationen, unbeschränkten Uploads oder Speicherzunahme im Dauerlauf.
- [ ] Synchrone Rebuild-Mutation verletzt das Frame-/Blockierungsoracle beim Tilewechsel.

Nächster Schritt: tatsächliche Aufruf-/Kostenkette beim Rezentrieren erfassen und
verbliebene weltweite Arbeit durch persistente, versionierte Produkte ersetzen.

## Job-Vertrag an der Übergabe

Tasks::Post (src/base/io/Tasks.cpp) wächst Queue_ ohne Kapazitätsablehnung; Done_ hält
Ergebnisse bis zum konsumierenden Poll/Wait. Wait wartet nur auf Done_.contains und
besitzt keinen Fehler für unbekannte oder bereits konsumierte Handles. Work ruft Jobs
ohne definierten Fehler-/Abbruchabschluss auf. Gegenbeispiel zum pauschalen Leak-Vorwurf:
WorldCrowns konsumiert seinen einzelnen Job; ein Leak dieses Consumers ist nicht belegt.
Job-Slots/Queue begrenzen, ungültige Handles explizit ablehnen, Generation/Abbruch und
Fehlerabschluss modellieren. Worker-Warten außerhalb des Framepfads ist legitim.
- [ ] Sättigung, alter Handle, konsumierter Handle und Shutdown mit laufendem Job
      enden definiert; Frameübernahme bleibt nichtblockierend und budgetiert.

## Download-/Query-Pfad

Place-Rerender: Stack-Sampling belegt Hauptthread in WaterField::Ingest ->
GroundStream::At -> Oracle::Take -> BytesBlocking -> Fetching::Await.
Damit blockiert DEM-Nachladen Fortschritt und Preload-Frist trotz IO-Workern.
Oracle liefert jetzt über TilePool::Bytes Pending und löst später erneut auf.
Verzögerte Quelle prüft Worker-Zuständigkeit, Pending und spätere korrekte Höhe;
Altpfad scheitert. Fetching bricht aktive Requests über libcurl-XFERINFO ab,
auch beim Shutdown. Lokaler HTTP-Test mit einem Worker prüft Freigabe vor Timeout.
Drei Tests grün, beide Negativkontrollen rot. Lint: unverändert 96 Tidy-Befunde
und offene Writer-Inventur; keine neuen Diagnosen.
Offen: Carrier-Serialisierung, Queue-Budgets, Mainthread-Decodierung/Aufbau und
OSM-Gesamtdurchsatz bei kaltem Cache. Der Fix ist keine Streaming-Gesamtabnahme.
