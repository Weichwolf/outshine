Type: feature
State: active
Area: scenario, engine, generators
Tags: isolation, lifecycle
Parent: 2169
Depends:

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

Vorhanden: WorldSettings, XML-Lesen/Schreiben, Shipping-Registries und WorldCrowns-
Lebenszyklus. Nicht ausgeführte Szenario-Metadaten gehören nicht in den Vertrag.

## Abnahme und weitere Pfade

- [x] Vegetation an/aus durch XML-Roundtrip erhalten, Default bleibt an.
- [ ] Wiederholtes Engine-Declare mit residenten Kronen im gesamten Weltpfad prüfen.
- [x] Katalog aus mit absichtlich ungültigem Artenpfad erfolgreich, keine Flora-Registry,
      kein TreeFor-Ergebnis; Gebäude verfügbar. Wiederan funktioniert.
- [x] Katalogaktivierung mit ungültigem Pfad verweigert atomar; vorheriger Zustand gültig.
- [x] Wien aus rendert ohne Crown-Wartebedingung; Wiederholung pixelgleich.
- [ ] Negativkontrolle ignoriert den Schalter und lässt die Ressourcenprüfung rot werden.
- [ ] Weitere Features inventarisieren und gleiche Verträge implementieren:
      Gebäude, Verkehrsgeometrie, Wasser, Atmosphäre, Schatten und Postprocessing.
      Logisches Navigationsnetz bleibt unabhängig von Darstellungsabschaltung.

Ein isoliertes PNG ist keine Abnahme des vollständigen Places. Aktivierte und
deaktivierte Konfigurationen im Ergebnis eindeutig nennen.

Katalogvertrag: 25 Checks, drei Umschaltzyklen; vorzeitiges Löschen alter Generatoren
erzeugt drei Fehler, restauriert grün. Eigenständiger Test unter generators/Shipped.
Composes propagiert Katalogfehler; Public-API-Test erhält die bisherige Simulation
und prüft Erholung. Alte Kronen werden vor Katalog/Live-Wechsel freigegeben.
GPU-Manager: drei Lade-/Freigabezyklen, 33 Checks; ohne Release fünf Fehler,
restauriert grün. Vollständige Engine-Wiederdeklaration und Speicherbudgets bleiben offen.
Wien 26834bdb wiederholt pixelgleich; gegenüber vorher 685/921600 Pixel höchstens
ein Farbwert anders, Ursache nicht isoliert. Keine visuelle Gesamtqualitätsabnahme.
