Type: feature
State: active
Area: scenario, engine
Tags: architecture, owner, ai-first
Depends: 2131, 2136

# Ein deklaratives Spiel mit wiederholbarem Zustand

## Ziel
Eine Szenariodatei beschreibt ein minimales RPG: ein Ort, drei NPCs, eine Quest.
Spielregeln sind Inhalt; Engine hält Identitäten, Besitz, Zustandsdaten und Zeit.
Kein spielabhängiger Sondercode. Gespielter Zustand lässt sich speichern und fortsetzen.

## Vorhandene Grundlage
- Kinds/Instances und Script::Program; TableBook hält typisierte Daten mit eigenen Strings.
- Tabellen prüfen eindeutige nichtleere IDs/Spalten, mindestens eine Spalte, exakte
  Zeilenbreite, höchstens 4096 Zeilen und vollständige endliche Dezimalzahlen.
  Fehlende trailing Types bedeuten Text; erste unveränderte Zelle ist der eindeutige
  Schlüssel, auch bei numerischer Schreibweise oder leerem Text. Keine Typkoerzierung.
- Tabellenaufbau trennt Schema, Zahlenkonvertierung und Publikation; Zellspeicher wird
  mit geprüftem Produkt reserviert. Assembly publiziert nur vollständige Kandidaten.
- Trigger halten höchstens 256 Volumes, 65536 Ereignisse, 256 Occupants pro Volume
  und 256 gepufferte Ereignisse. Katalogauflösung verwendet kurzlebige string_views.
  Ereignis-/Feldnamen sind nichtleer und eindeutig je Katalog/Ereignis; Groß-/
  Kleinschreibung unterscheidet Namen. Leere Feldlisten sind gültig.
- Volume-Geometrie ist endlich, Ausdehnung nichtnegativ; geschlossene Box/Kugel,
  Halbausdehnung bzw. Radius X. std::hypot verhindert Quadrierungsüber-/unterlauf.
- Probe lehnt Sentinel, nichtendliche Position/Zeit, negative/rückläufige Zeit vor
  Mutation ab; gleiche Zeit für mehrere Bodies ist gültig. Engine prüft Lebendigkeit
  und propagiert Fehler. Bereits integrierte Physik wird nicht zurückgerollt.
  Enter/Exit/Dwell sind getrennte Occupant-Übergänge; Dwell einmal je Aufenthalt.
- Writer erhält Events/Carries und alle Volume-Felder einschließlich Reihenfolge,
  Escaping und endlichen Double-Werten. Leeres When bleibt ungültig.
  Dokumentierte Table-/Event-/Volume-Verträge sind noch keine vollständige SOLL-Abnahme.

## Offene Arbeit
1. Besitzmodell für Owner/Item/Count; kontrollierte give/take-Operationen, keine
   impliziten Inventare in NPC-Texten. Unbekannte Assets/fehlender Besitz ergeben Fehler.
2. Veränderbare Tabellen und deklarierte Ereignisregeln für Quest-/Spielzustand.
   Carries benennt aktuell nur Felder; Trigger liefern Index/Entity, keine Payloadwerte.
3. Simulationszeit und NPC-Schedules aus einem expliziten Clock-Vertrag; Replay nach 2142.
4. Gespielten Zustand separat von der Ausgangsdeklaration speichern: Besitz, Tabellen,
   Körperpositionen, Uhr und benötigte Trigger-Zustände. Vollständiger Import nach 2131.
5. Region-Bindung von Volume.In implementieren oder explizit ablehnen; aktuell ignoriert.
   Volume.Id ist Layer-Schlüssel, Assembly prüft ihn noch nicht auf Eindeutigkeit.
6. Despawn-Freigabe, öffentlich sichtbare Überlast und schnelle Durchquerung lösen.
   Aktuell Mittelpunktabtastung pro Tick, keine Körper-/Sweep-Intersection; interne
   Overflow/Unseated-Zähler reichen als öffentlicher Fehlervertrag nicht aus.

## Nächster Schritt: deklarierte Tabellen schreiben
Eigene WriteTables-Phase erhält Tabellen-/Spalten-/Zeilenreihenfolge und Zellschreibweise.
XML stellt pro Spalte einen Typ dar; fehlende native Typen werden als Text materialisiert.
Escaping einschließlich Tabs/Zeilenwechseln; leere Textzellen und Nullzeilen erhalten.
Unabhängiger Read/Write-Test prüft konkrete Werte und typisierte TableBook-Abfragen;
Altstand muss wegen fehlender Tabellen scheitern. Keine Runtime-Savegame-Fähigkeit behaupten.

## Abnahme
- Ein Ort, drei NPCs, eine Quest vollständig deklarativ und über die Public API spielbar.
- N Schritte, speichern/laden, weitere N Schritte: gleicher Zustand und gleiche Bilder
  wie ununterbrochen, mit identischen Eingaben und aufgezeichneten NPC-Antworten (2142).
- Jede genutzte Sektion bleibt nach Import/Export semantisch gleich; unabhängige Fixtures.
- Negative Fälle für unbekannte Assets, unzulässige Besitzwechsel, fehlerhafte Tabellen,
  Event-/Volume-Definitionen und Probes erhalten den jeweils zugesicherten alten Zustand.
- Vorhandene Tests unter test/outshine/src/scenario/{Tables,Triggers,ScenarioWrite} und
  Public-API-Tests für Assembly-Erhaltung bleiben grün; Negativkontrollen bleiben wirksam.
