Type: defect
State: active
Parent: 2194
Area: base, include, build
Tags: architecture, audit, memory
Depends:

# Engine allocation does not replace the host allocator

## Beleg und Auswirkung

src/base/io/Heap.cpp definiert globale operator new/new[]/delete-Overloads in der
Engine-Bibliothek. Damit betrifft die Instrumentierung auch fremden Host-/Bibliothekscode.
Ein eigener globaler Allocator ist für eine bewusst konfigurierte Anwendung legitim;
als implizite Nebenwirkung einer einbettbaren Engine ist er kein passender Vertrag.
Skalares nothrow-new zählt jetzt wie die Array-Variante über Counted; zuvor zog delete
nicht gezählte Bytes ab. Direkte Aufrufe mit 0/1/257 Bytes und Nullfreigabe prüfen die
Symmetrie; der Test scheitert am Altstand. Die globale Host-Übernahme bleibt offen.
Telling veröffentlicht derzeit prozessweite C++-Allokationen; nach der Trennung keine
uninstrumentierten Nullwerte als Engineverbrauch ausgeben. HeapProbe misst ebenfalls
prozessweiten Heap, keine engine-eigene Ownership.

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

- [ ] Externer Host mit eigenem new/delete lässt sich linken und behält seinen Allocator.
- [ ] Scalar/Array, throwing/nothrow, aligned/sized delete: symmetrische gezählte Bytes.
- [x] Gezielter Test des skalaren nothrow-Pfads zeigt vor Fix den Zählerfehler.
- [ ] Engine-Budget umfasst Engine-Speicher; fremde Host-Allokationen separat ausweisen.
- [ ] OOM-/Budgetfehler nach 2194, Lint und Instrumentierungskosten nach 2108 prüfen.
