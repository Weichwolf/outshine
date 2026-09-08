Type: feature
State: active
Area: generators, world, render
Parent: 2169
Depends: 2123, 2124, 2132, 2171, 2184, 2185

# Forests will populate suitable ground within the streaming budget

## Ziel und aktueller Befund

Dichte, plausible Wälder und Stadtbäume aus Daten und deterministischen Regeln.
Priorität und Integrationsreihenfolge stehen in 2169. Erst isolierte ebene Waldszene,
danach Weltintegration und Artenausbau in 2176. Keine Place-Sonderbehandlung.

Vorhanden: Species-Identität, geografische Instanzmatrizen, native Materialien,
geteilte Piece-Geometrie, Kronenatlas und begrenzter Dateicache. Der uncommittete
WorldCrowns-Consumer zeichnet Kronenkarten, ist aber keine abgenommene Waldpipeline.
Koerbersee-c3a0846c zeigt vergrößerte pixelige Nahkarten und unpassende Arten;
Rosenheim-4fda4e24 übergroße Kronen im Gebäudebereich. Visuell abgelehnt.
Aktuelle Vergleichsbilder: `build/shots/reference/terrain-20260908/`.

Platzierung verarbeitet bislang nur die Augenkachel und bleibt danach stehen.
Der Regionspool begrenzt gemeinsame Gebäude-/Flora-Platzierungen. Es fehlen
räumlicher Wiedereintritt, Freigabe, vollständige Distanzleiter und Ökologie.
Kronenkarten sind aktuell auch im Nahbereich aktiv; acht horizontale Ansichten
reichen für steile Blickwinkel nicht. Alpha-Coverage über Mips und Übergänge offen.

## Umsetzung

1. Isoliertes Waldszenario mit deklarierter Fläche, Dichte, Sichtweite, Seed und
   Kamerafahrt; Vegetation über 2185 schaltbar. Zahlen vor dem Lauf festlegen.
2. Geteilte Prototypen, hierarchisches Culling und instanzierte Nah-/Mittelgeometrie;
   Fern-Impostoren nach projiziertem Fehler, Übergänge mit stabiler Coverage.
   Keine vollständige Kopie jedes Blattes pro Baum und kein Billboard direkt am Auge.
3. Räumliche Residency mit begrenzten Jobs, Uploads und Speicher. Daten und GPU-
   Ressourcen beim Verlassen freigeben; Rückkehr reproduziert denselben Bestand.
   Prototypmaterialien nicht für jede Kachel neu registrieren.
4. Geeignete Landnutzung, Höhe, Neigung und Klima steuern Bestände. Gebäude, Wege
   und Wasser aussparen; Überschneidungen entlang gemeinsamer räumlicher Daten prüfen.
5. 2176 erweitert Arten/Morphologie/Phänologie; 2171 trägt Rinde, Blatt und Nadeln.
   Blatttransmission, Wind und Schatten erst im vollständigen Kostenvertrag abnehmen.

**Benchmark**: Unreal-HISM/HLOD und veröffentlichte SpeedTree-Verfahren liefern
Instancing, räumliche Cluster und Distanzrepräsentationen. RAGE sowie Arma/Far Cry/
DayZ/KCD/RDR liefern den sichtbaren Qualitätsmaßstab. Konkrete Strukturen nur aus
belegten Verfahren übernehmen; aus Bildern keine Implementierung ableiten.
Der bestehende Code ist ersetzbar, das vollständige Waldsystem ist die Abnahmeeinheit.

## Abnahme

- [ ] Isolierter dichter Wald bei Kamerabewegung: PNGs aller Distanzen geöffnet,
      plausible Silhouetten, keine Nahkarten, kein Flimmern/auffälliges LOD-Popping.
- [ ] Framezeiten p50/p95/p99, Überschreitungen, CPU/GPU, Overdraw, Uploads und
      Speicherspitzen auf Zielhardware; 2092/2143 samt Bewegung und Dauerlauf erfüllen.
- [ ] Tilewechsel, Rückkehr und Kamerawechsel: stabile Seeds, begrenzte Residency,
      keine verlorenen Bestände oder unbeschränkt wachsenden Materialkataloge.
- [ ] Gebäude/Wege/Wasser bleiben frei; Arten und Dichte standortgerecht in 2176.
- [ ] Weltintegration in allen relevanten Places visuell prüfen, besonders
      Koerbersee, Wien, Rosenheim, Olympiaturm und Feldkirch.
- [ ] Falsche Instanzmatrix, fehlender Handoff und deaktiviertes Culling verletzen
      jeweils das passende Korrektheits- bzw. Budgetoracle.

Historische Implementierungsschritte und Messreihen: Git-Historie dieses WIs.
Standframes mit fehlender oder ungeeigneter Vegetation belegen keinen vollen Wald.
