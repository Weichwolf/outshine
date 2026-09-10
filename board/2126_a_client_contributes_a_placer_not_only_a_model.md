Type: debt
State: active
Area: include, generators, engine
Tags: architecture, ownership, generation
Parent: 2188
Depends: 2150, 2194

# Clients contribute native generated assets and placement rules

## Befund

`include/generation/Generate.h` bietet Generator::make(Request, Geometry &),
optionale Terrain-Stamps und eine Registry geliehener Producer. Die internen
Making/Yield/Claim/Ground/Rank-Verträge sind kein öffentlicher Placement-Zugang.
Ein Client kann Geometrie liefern, aber keine räumliche Belegungsregel registrieren.

make liefert nur bool und mutiert fremden Output ohne Rollbackvertrag. Structures
ignoriert Ground/Coarseness, setzt CornerAslM auf null und AnchorEcef auf Nullvektor;
das ist keine gültige weltweit platzierte Terrain-Anbindung. Engine::declare prüft
unbekannte Registrierungen bereits vorab; generated weist sie ebenfalls zurück.
Die dokumentierten Ist-Grenzen sind keine Abnahme dieses Verhaltens.

API-Audit: Georeference::RadiusM (Default Erdradius) wird Request::ExtentM; der
Regionsvertrag bleibt uneindeutig. Gebäudegröße ist jetzt ein eigener widthM-Parameter;
unbekannte Einstellungen werden verweigert, native Views bleiben nur im Aufruf gültig.
Gemeinsame Schema-/API-Migration und Abnahmen in 2131; keine reine Umbenennung.

## Entscheidung

Native Generatorprodukte als owned expected liefern. Meshdaten, Instanzen und
Terrainänderungen ausdrücklich trennen; 2150 stellt gemeinsame Asset-/Instanztypen.
Terrain-/Detailbedarf prüfen, fehlende Providerdaten diagnostizieren und mehrere
Produkte atomar publizieren. Keine Ersatzwelt auf Nullhöhe bei fehlendem Terrain.
Generierungsfehler unterscheiden von einem gültigen leeren Ergebnis.

Platzierungsregeln erhalten Region, Seed und geliehene Provider; sie liefern begrenzte
Belegungsansprüche mit Priorität und stabiler Identität. Bestehende Forest::Occupy-
Fähigkeiten nutzen; interne OccupancySink-Speicherung nicht öffentlich machen.
Deterministische Konfliktauflösung unabhängig von Worker-Abschlussreihenfolge.
RDR2/GTA5 sind visuelle Referenzen, kein Beleg proprietärer API- oder Dateiverträge.

## Abnahme

- [ ] Öffentlicher Client registriert Assetproducer und Placement-Regel unabhängig.
- [ ] Zwei Producer liefern verschiedene native Assets und Instanzen in einer Region.
- [ ] Geneigtes Terrain, fehlende Höhen, Pole und ungültige Requests explizit geprüft.
- [ ] Unbekannte Registrierung und Fehler nach Teilaufbau publizieren keinen Teilzustand.
- [ ] Leere gültige Region ist erfolgreich; vorhandener Output bleibt bei Fehler erhalten.
- [ ] Seed, unveränderte Provider und wechselnde Worker-Reihenfolge liefern dieselbe Belegung.
- [ ] Abmeldung entfernt Beiträge ohne hängende geliehene Zugriffe; Negativkontrolle wirksam.
- [ ] Kosten-/Speichergrenzen und Abbruch für Streaming geprüft; make lint und API-Tests.

## Gebäudemesher: Fehlergrenze

Mesh hängt an Raised an; catch leert bisher auch vorherige Gebäude. Größen aller
vier Ausgabepuffer vor dem Versuch sichern und bei Ablehnung nur den neuen Anhang
verwerfen. expected unterscheidet ungültigen Plan, inkompatiblen Scratch und
Aufbaufehler; Scratch-Typ am Job-Eingang prüfen statt unchecked static_cast.
Allokationsfehler früh/mittig/spät gezielt injizieren und vorherige Geometrie erhalten.
Catch erst nach begrenztem Scratch-/Output-Aufbau aus 2194 entfernen, nicht ersatzlos.
StructureBake reicht Mesher-/Clusterfehler typisiert bis StructureBakes::Lands weiter;
fehlgeschlagene Jobs publizieren keine Kachel. Gegenprobe mit verschlucktem Fehler rot.
Weitere Befunde: Site::Index nutzt Hash als Identität und multipliziert signed int64;
Kollisionen/Überlauf dürfen keine verschiedenen Vertexpositionen zusammenlegen.

Schlüsselkorrektur in zwei geprüften Schritten: FlatMap um vollständige Key-Typen
mit separatem Hasher erweitern; bestehender uint64-Pfad bleibt unverändert. Künstlich
konstanter Hash muss Wachstum, Find, Duplikate und Clear mit verschiedenen Keys
bestehen. Danach BuildingScratch auf vollständige Positions-/Vertexschlüssel migrieren
und tatsächliche Gebäude-/Places-PNGs vergleichen; ein Hashwert ist keine Identität.

Wien-Render deckte Regression auf: MassOf-Ablehnung kleiner/kollabierter Footprints
war als InvalidPlan fatal propagiert. UnsupportedFootprint separat zählen und den
Tile-Aufbau fortsetzen; nichtendliche Eingaben, fremder Scratch und Aufbaufehler
bleiben Fehler. UnsupportedMeshes im Bake-Ergebnis und Gebäude-Log ausweisen.
Vollständige Unterstützung kleiner/clippingbedingter Footprints bleibt offen.
Wien rendert nach Korrektur mit deaktivierter Vegetation wieder; Vorherstand für die
Schlüsselkorrektur: build/shots/reference/building-keys/Wien-before.png, visuell geöffnet.
Mit Vegetation scheitert der Shot aktuell am 15-s-Residency-Limit für Crown-Prototypen,
nicht mehr an einem Gebäudefehler. Keine visuelle Gesamt- oder Vegetationsabnahme.

Gebäudemesher: volle Vertex-Identität, Koordinaten-/Höhen-/Quantisierungsgrenzen,
zyklische Schnittklassifikation und einheitliches Dachseitenverhältnis sind korrigiert.
Bestehende Fehler-/Retry-/Geometrietests behalten; Budgets und vollständige Clipping-
Robustheit bleiben offen. Frühere Einzelresultate stehen in Git.

Structures: widthM als einziger optionaler Größenparameter (Default 12 m), vollständig
endlich/positiv parsen; Duplikate, unbekannte Namen und Zahlenreste verweigern.
Request::ExtentM beeinflusst Objektmaße nicht mehr. Direkte Meshvergleiche bei anderen
Regionen, expliziten Breiten und Fehlererhalt; alter Extent-Pfad als Gegenprobe.

Structures dekodiert StoredVertex jetzt typisiert über uv()/norm() zu Geometry;
der fehlerhafte Cast von fünf gepackten Wörtern auf acht Floats ist entfernt.

Nachweise: Breiten 12/24/240 m, Regionsunabhängigkeit und Fehlererhalt bestehen;
Wandseiten statt Dachhüllbox messen (0,42 m Überstand bzw. 0,16 m Gesims). Typisierter
Decoder prüft Position/Index exakt, UV/Normalen nach 16-Bit-Quantisierungsgrenze.
Beide Gegenproben scheitern; lint 186/330, 32 Claims grün. Isolierter GLB-Clientrender
build/shots/reference/building-width/structure.png zeigt weiterhin falsche Ausrichtung
und Flächendiagonalen: Null-Anker/ECEF und Normalen-/Präzisionspfad vor Abnahme korrigieren.

## Nächster Schritt: nativer Gebäuderaum

GLB: X = 6378136,5 … 6378153 m als Float; ULP = 2^(22−23) = 0,5 m.
Sechs von 82 Dreiecken degenerieren; 18 verbleibende Ecknormalen haben dot < 0,9
zur Flächennormale. Bilddiagonalen nicht allein der Achsenausrichtung zuschreiben.
Vorhanden: GeoToEcef/EnuAxesEcef, RenderFrame::Of, Double-Anker, Packed-Decoder.
Vor Float-Packen AnchorEcef auf den geodätischen Request-Ursprung setzen.
Decoder erhält explizite ECEF→lokal-Basis: Positionen und dekodierte Normalen nach
East/Up/South drehen, ohne erneutes Packen oder Abhängigkeit von content/shade.
Lokales Asset und Terrain-Platzierung unterscheiden; fehlendes Ground bleibt ein
ungelöster Placement-Vertrag, kein implizites Nullterrain.
Prüfung: mehrere Breiten/Seeds/geografische Ursprünge; lokale Meterbounds, Y-up,
CCW/Normalenkonsistenz, erhaltene Submeterdetails. Null-Anker und fehlende
Normalendrehung als Gegenproben. GLB vor/nach öffnen; OSM-Place Wien prüfen.
Terrain-Sitz, Pole/Datumsgrenze und atomare Publikation bleiben eigene Abnahmen.
