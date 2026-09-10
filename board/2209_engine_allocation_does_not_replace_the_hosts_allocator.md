Type: defect
State: active
Parent: 2194
Area: base, include, build
Tags: architecture, audit, memory
Depends:

# Engine allocation does not replace the host allocator

## Beleg und Auswirkung

Globale new/delete-Overloads liegen jetzt in src/diagnostics/ProcessHeap.cpp, außerhalb
beider Bibliotheksarchive. Client und eigene Diagnose-Testgruppe linken sie ausdrücklich.
Ein Host mit eigenen Operatoren scheitert am Altstand am Linker und funktioniert nach
Trennung. Instrumentierung wird in der Compile-Datenbank weiter analysiert.
Skalares nothrow-new zählt symmetrisch; direkte Aufrufe mit 0/1/257 Bytes und Nullfreigabe
sind geprüft. Ausgerichtetes nothrow-new brach bei SIZE_MAX mit SIGABRT ab;
eigene Scalar-/Array-Overloads nutzen jetzt einen nichtfatalen posix_memalign-Pfad.
Nullfehler und passende Cleanup-Overloads erhalten Zähler. 64/256/4096-Byte-Alignment,
Zero-size und unmögliche Größe prüfen; vollständige Overload-/OOM-Matrix bleibt offen.
Vertrag: [C++ new.delete.single](https://eel.is/c++draft/new.delete.single).
Telling/Advancing geben instrumentierte Prozess-C++-Bytes als solche aus; ohne Modul
fehlen diese Messwerte statt unbelegter Nullen. HeapProbe bleibt eine Prozess-Heap-Probe.
Engine-eigene Ressourcen/Arenen samt vollständiger Bilanzierung sind noch umzusetzen.

## Entscheidung

Benchmark: explizite Allocator-/Arena-Zuständigkeit und symmetrische Instrumentierung.
Engine-Speicher über eigene Ressourcen/Arenen bzw. injizierten Allocator bilanzieren;
prozessweite Instrumentierung nur als bewusst gelinktes Diagnose-/Client-Modul.
Runtime-OOM-Vertrag aus 2194 erhalten, Host-new_handler/Allocator nicht heimlich ersetzen.
Bytezähler und Allokations-/Freigabezuordnung prüfen, Alignment und Zero-size definieren.

## Umsetzungsschritte

Zuerst den belegten skalaren nothrow-Zählfehler mit direkten Allocator-Aufrufen
reproduzieren und die symmetrische Freigabe prüfen; null und Zero-size einschließen.
Danach globale Overloads als ausdrücklich gelinktes Diagnosemodul aus der Bibliothek
lösen und Engine-/Host-Bilanzierung trennen. Der erste Schritt schließt dieses WI nicht.

## Abnahme

- [x] Host-Test mit eigenen new/delete-Operatoren linkt mit Engine-Objekten und behält sie.
- [x] Externer Client mit kopierten öffentlichen Headern und liboutshine.a läuft außerhalb
      des Checkouts; Engine-Lebensdauer erhält Host-Allocator und new_handler.
      Zusätzliche globale Operatoren scheitern als Linker-Negativkontrolle.
- [ ] Scalar/Array, throwing/nothrow, aligned/sized delete: symmetrische gezählte Bytes.
- [x] Gezielter Test des skalaren nothrow-Pfads zeigt vor Fix den Zählerfehler.
- [ ] Engine-Budget umfasst Engine-Speicher; fremde Host-Allokationen separat ausweisen.
- [ ] OOM-/Budgetfehler nach 2194, Lint und Instrumentierungskosten nach 2108 prüfen.

## Besitzende Heap-Tags
TagRow hält bisher geliehene char-Zeiger und vergleicht Identität statt Inhalt.
Namen in festen 96-Byte-Slots besitzen; gleiche Texte zusammenführen. 32 Slots
inklusive other/untagged, überlange/unregistrierbare Namen nach other. Registrierung
unter Mutex beim Scope-Eintritt; Hot-Path zählt direkt über threadlokalen Slotindex.
Namen erst nach vollständiger Kopie veröffentlichen; verschachtelte Scopes stellen
vorigen Index wieder her. Tests: mutierte/freigegebene Namen, gleiche Texte, Threads,
Überlauf und Scope-Restore. Kein Anspruch auf vollständige Engine-Speicherbilanz.
