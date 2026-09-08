Type: bug
State: active
Area: include, generators
Parent: 2123
Depends:

# LOD error checks will reject invalid projection inputs

## Befund und Entscheidung

Generate.h::Unseen gibt bei NaN und negativen errorM true zurück, weil
!(errorM > 0) diese Fälle mit null zusammenfasst. Unendliche Entfernung kann eine
nicht endliche Projektion ebenfalls akzeptieren. Ein unbekannter Fehler darf
keine gröbere Darstellung rechtfertigen.

Vertrag: geometrischer Fehler endlich und nichtnegativ; Brennweite in Pixeln und
Entfernung in Metern endlich und positiv. Sonst false. Für gültige Eingaben gilt
unverändert errorM * focalPx <= kErrorPx * awayM. Nullfehler braucht ebenfalls eine
 gültige Projektion. Kein Clamp ungültiger Daten auf scheinbar gültige Werte.

Statische Beispieldaten gehören in einen eigenen Konventionstest, nicht in jedes
Public-Header-Include. Bestehende Beispiele bleiben erhalten; dazu NaN, ±Inf,
negative Werte, null, exakte Ein-Pixel-Grenze und Werte unmittelbar daneben.

## Abnahme

- [ ] Alle bisherigen gültigen Beispiele unverändert, einschließlich Nullfehler.
- [ ] Ungültiger Wert in jedem Parameter rechtfertigt keine Vereinfachung.
- [ ] Ein Pixel akzeptiert, nächster größerer repräsentierbarer Fehler verweigert.
- [ ] Die bisherige Negation akzeptiert NaN/negative Fehler und verletzt die Kontrolle.
- [ ] make lint und Konventionssuite; keine Unterdrückung von Tidy-Befunden.

Bildwirkung für gültige Szenarien unverändert; dies ersetzt keine LOD-/Terrain-
Abnahme aus 2123/2166. Verbraucher bleiben für Fehler-Messung und Bounds verantwortlich.
