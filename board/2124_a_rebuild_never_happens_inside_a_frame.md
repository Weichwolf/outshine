Type: bug
State: active
Architecture: ready
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
VegetationStreaming konsumiert seinen einzelnen Job; ein Leak dieses Consumers ist nicht belegt.
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

## Water admission: bounded height sampling

Rosenheim 8e6642f9: der längste erfolgreiche `GroundStack::Restand` braucht
23.58 ms, davon Wasser 23.54 ms. `WaterField::Ingest` prüft in einem Aufruf
484 Punkte in 21.09 ms; die längste `GroundStream::At`-Abfrage braucht 5.44 ms.
Die Validierung fragt bei Pending erneut und die Materialisierung ein zweites Mal. Besitzer sind
`WaterField` (Aufnahmezustand und staged Höhen) und `GroundStream` (Höhenquelle).

Wasser-Tiles über mehrere Frames mit gemeinsamem Zeit- und Punktbudget für die
bis zu vier Kandidaten prüfen. Pro gültigem Ring Höhe oder Loch in stabiler
Punktreihenfolge merken; Pending wiederholt nur den betroffenen Punkt. Erst nach
vollständiger Prüfung aus den gemerkten Höhen atomar Courses/Surfaces/Levels
publizieren und `TileWatermark::Take` ausführen. OSM-Generation verwirft den
Aufnahmezustand, alte Revisionen dürfen nichts veröffentlichen. Kein grober
`Resident`-Fallback für definitive Wasserhöhen. Negativkontrolle: Pending nach
mehreren erfolgreichen Punkten darf keine Teilprodukte und keine doppelten
Punktabfragen erzeugen. Gleicher Input muss dieselben Profile und das gleiche
Place-Bild liefern; längste Wasseraufnahme und ganze Framezeit erneut messen.

## Worker-Phasen

NextJob besitzt priorisierte Queue-Entnahme und Shutdown-Warten, RunJob die Mesh-/
Field-Ausführung, PublishResult die gesperrte Ergebnisübergabe. Work koordiniert
Abhängigkeiten und Zeitmessung. Priorität, Locks und Fehlerzustände bleiben erhalten.
Drei TilePool-/Terrain-Tests grün; Wien ohne Vegetation pixelgleich (0/921600).
Lint: 94 Tidy-Befunde, keiner mehr in TilePool; Writer-Inventur weiter offen.
Diese Strukturkorrektur nimmt keine neue Streaming-Fähigkeit ab.

## P0: begrenzte Patchwork-Abdeckung

Die alte Maske benötigte 16*4^(Levels-1) Bytes und entsprechende Flächenscans;
unbegrenzte Zoom-/Levelwerte konnten ungültige Shifts auslösen. Jetzt hält die
Abdeckung disjunkte dyadische Tile-Bereiche: fertige Eltern ersetzen enthaltene
Kinder, abgedeckte Fläche ist die Summe der Kinderflächen. Maximal
16*(kZoomLevels-1) = 368 Bereiche/Provider-Aufrufe, kein weltflächiges Raster.
Zoom 1..23, Levels positiv und auf vorhandene Zoomstufen begrenzt, Grid>=2,
Koordinaten endlich; ungültige Eingaben erreichen keinen Provider.
Anfrage, Antwortzählung und Abdeckung sind getrennt. Reihenfolge und Fallback
bleiben erhalten, Anfragen ohne Mesh verdecken keine Eltern. Ein unabhängiges
Zellenorakel prüft gemischte Antworten, ungültige Ready-Meshes, Anfragebetrieb,
Datumsgrenze/Polgrenzen und Maximalfall: 187 Checks grün. Pending als Abdeckung
injiziert: 35 Checks rot. Wien geöffnet, 0/921600 Pixel verändert.
Lint vollständig: 60 Befunde, keiner in GroundPatchwork; Writer weiter rot.
Warmaufnahme p50/p95/p99 5.26/6.00/6.27 ms, 0/120 über 16.67 ms, sim p99 0.53 ms.
Kein allgemeiner Geschwindigkeitsnachweis aus einer Aufnahme; Bewegung und
Dauerlauf bleiben offen. Fremde Provider können intern weiterhin blockieren.
