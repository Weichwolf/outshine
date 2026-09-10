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
das ist keine gültige weltweit platzierte Terrain-Anbindung. Engine::generated
überspringt unbekannte Registrierungen still und setzt mehrere Outputs zusammen.
Die dokumentierten Ist-Grenzen sind keine Abnahme dieses Verhaltens.

API-Audit: Georeference::RadiusM (Default Erdradius) wird Request::ExtentM und dort
Gebäudeseitenlänge. Parameter erreichen make jetzt als geliehene native Views;
Region/Featuregröße bleiben zu trennen; Builtin verweigert nicht unterstützte Parameter.
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

BuildingScratch vergleicht vollständige Millimeterpositionen und Rendervertex-Keys
(Position, gepackte Normale, gepackte UV). Hasharithmetik unsigned; Spiegelkollision
und UV-/Normalennähte direkt geprüft, entfernte UV-Identität lässt Gegenprobe scheitern.
Wien-Nachher: build/shots/places/Wien-d89be31c.png, beide PNGs geöffnet; Fernsilhouette
unauffällig, 28 zusätzliche Dreiecke. Kein Nachweis der Nahbildqualität.
Offen: Zahlenbereich vor llround, Index-/Scratch-Budgets und Generator-Nahaufnahmen.

Nächste Eingangsgrenze: alle Ringkoordinaten vor MassOf auf gültige geodätische
Breite/Länge prüfen. SeedOfPlace konvertiert Mikrograd in int32; finite allein
schützt diesen Pfad nicht. Ungültige Koordinate an jeder Ringposition muss ohne
Allokation und Outputmutation InvalidPlan liefern. Höhen-/Ableitungsgrenzen separat.

Höhenkonvertierung: MassOf muss ungültige Höhen typisiert melden, statt sie mit
nicht unterstütztem Grundriss zu vermischen. Vor Scratch-/Polygonaufbau endliche
Höhe gegen min(Geschosspräferenzen, TallFloor) * (INT_MAX - 1) prüfen; Reserve schützt
Rundung. Das ist eine Repräsentationsgrenze, kein hinreichendes Echtzeitbudget.
NaN/Inf/extreme endliche Höhen ohne Allokation ablehnen; Fehler bis Mesh durchreichen.

Vertex-Konvertierung: gerundete Millimeter vor llround auf [-2^63, 2^63) prüfen.
Site hält einen typisierten Fehlerstatus; nach Fehler keine weiteren Dreiecke.
Mesh verwirft den eigenen Anhang auch bei diesem Fehler. Extreme endliche Sockelhöhen
müssen InvalidPlan liefern, vorherige Geometrie erhalten und denselben Scratch wieder
verwendbar lassen. Float-/Normalen-Repräsentation und Arbeitsbudgets bleiben separat.

CutPiece: zyklische Seitenklassifikation vom Polygonaufbau trennen. Genau zwei
Vorzeichenwechsel erlauben einen zusammenhängenden Schnitt; Nullwerte überspringen,
mehrere Ein-/Austritte ablehnen. Reine span-Funktion mit zyklisch rotierten Tests für
Tangente, Schnitt durch Ecke und mehrere Intervalle prüfen. Das beweist nur die
Schnittklassifikation; Flächenerhaltung und vollständige Clippingrobustheit bleiben offen.

Dachauswahl: Seitenverhältnis aus BuildingShape ableiten statt als zweiten,
vertauschbaren double-Parameter übergeben. Nur PitchedShare ist externe Vorgabe;
Geometrie und abgeleitete Proportion dürfen sich nicht widersprechen. Bestehenden
Gebäudeaufbau inklusive Fehler-/Retry-Verträgen unverändert nachweisen.

Structures: widthM als einziger optionaler Größenparameter (Default 12 m), vollständig
endlich/positiv parsen; Duplikate, unbekannte Namen und Zahlenreste verweigern.
Request::ExtentM beeinflusst Objektmaße nicht mehr. Direkte Meshvergleiche bei anderen
Regionen, expliziten Breiten und Fehlererhalt; alter Extent-Pfad als Gegenprobe.
