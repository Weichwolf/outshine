Type: feature
State: active
Area: scenario, engine, generators
Tags: isolation, lifecycle
Parent: 2169
Depends: 2187

# Scenarios will disable features before they request resources

## Problem und Entscheidung

Der Terrain-Diagnoselauf `build/terrain-skirt-zero-diagnosis.log` endete am
2026-09-08 mit Make-Exit 2: Gelände geladen, 28 Kronenprototypen warteten noch.
Kein PNG, daher keine Aussage zur Skirt-Hypothese. Quellkonstante wiederhergestellt.
Tests müssen Features über Szenarien isolieren können, ohne Quellcodeänderung.

Unreal bietet separate Foliage-Show-Flags:
https://dev.epicgames.com/documentation/en-us/unreal-engine/viewport-show-flags-in-unreal-engine
Das belegt Sichtbarkeitssteuerung, nicht das Unterbinden von Erzeugung/IO.
Outshine braucht ausdrücklich einen Ressourcenvertrag über den gesamten Lebenszyklus.
Eine entsprechende interne RAGE-Struktur ist hier nicht belegt.

Erster vollständiger Pfad: `world vegetation="no"`, standardmäßig eingeschaltet.
Das unterbindet Artenladen, Waldplatzierung, Atlasvorbereitung, Cachezugriffe und
Kronen-Draws. Gelände-Landcover und Gebäude bleiben unabhängig nutzbar.
Lesen/Schreiben erhält den Wert; erneutes Declare behandelt den Wechsel als
Ressourcenänderung und gibt bisherige Kronen vor einem Katalogwechsel frei.
Der Katalogwechsel wird atomar aufgebaut; fehlgeschlagenes Aktivieren zerstört
keinen zuvor gültigen Katalog. Kein universeller Schalter ohne tatsächlichen Consumer.

Vorhanden: WorldSettings, XML-Lesen/Schreiben, Shipping-Registries, WorldCrowns-
Lebenszyklus. Compositor.On steuert bereits andere deklarierte Bereiche, ist aber
kein Beleg für einen durchgängigen Ressourcenvertrag.

## Abnahme und weitere Pfade

- [ ] Vegetation an/aus durch XML-Roundtrip und wiederholtes Declare erhalten.
- [ ] Aus mit absichtlich ungültigem Artenpfad erfolgreich, keine Flora-Registry,
      kein TreeFor-Ergebnis; Gebäude verfügbar. Wiederan funktioniert.
- [ ] Aktivieren mit ungültigem Pfad verweigert atomar; vorheriger Zustand gültig.
- [ ] Reales Place-Rendering aus erzeugt ein PNG ohne Crown-Wartebedingung.
- [ ] Negativkontrolle ignoriert den Schalter und lässt die Ressourcenprüfung rot werden.
- [ ] Weitere Features inventarisieren und gleiche Verträge implementieren:
      Gebäude, Verkehrsgeometrie, Wasser, Atmosphäre, Schatten und Postprocessing.
      Logisches Navigationsnetz bleibt unabhängig von Darstellungsabschaltung.

Ein isoliertes PNG ist keine Abnahme des vollständigen Places. Aktivierte und
deaktivierte Konfigurationen im Ergebnis eindeutig nennen.
