Type: feature
State: open
Parent: 2169
Area: world, render
Tags: webcam, measured
Depends: 2152

# Terrain and local lights cast budgeted shadows

## IST und Umsetzung

Aktuelle GLSL-Pfade ersetzen die früher hier genannten MSL-Dateien. Der bestehende Pfad
hat eine begrenzte Lichtliste und einen Sonnen-Shadow-Atlas; `GroundLattice::Cast` ist
noch an den tatsächlichen Schattenpfad anzuschließen. Bergschatten fehlen als eigener
Beitrag, wenn Terrain nicht gerendert wird. Erst Draw-/Caster-Counter nachweisen.
Live setzt die Caster-Grenze weiterhin auf Joined_. Der Sonderpfad für initiale
Built-Subjects entfällt mit 2150; Shadow-Casting dennoch ausdrücklich je Objekt/
Geometrie deklarieren, Quelle nicht als Policy verwenden; Kamera-/Caster-Auswahl und
Radius gemeinsam budgetieren. LightVisibilityStage::Cast zeichnet außerdem hart eine
Instanz je DrawBatch, statt batch.Instances zu übernehmen. Instanzbereiche einschließlich
partieller Caster-Masken korrekt behandeln und mit mehreren unabhängigen Castern prüfen.
Der Device-Lebenszyklustest deklariert seine Caster-Menge ausdrücklich; er nimmt diese
Welt-/Instanz-Policy und den fehlenden Terrain-Aufruf nicht ab.

1. Terrain und Subjects in denselben Lichtkoordinaten, Bias in Welt-/Texeleinheiten und
   korrekte Reverse-Z-/Cull-Konventionen; stabile Sonnenkaskaden nach Sichtvolumen wählen.
2. Cluster/Froxel-Listen und deklarierte Überlaufpolitik; bei wenigen Lichtern gemessenen
   Fast-Path behalten. Punctual-Lights nach Khronos-Einheiten, lokale begrenzte Shadow-Slots,
   Updatepriorität nach Einfluss/Bewegung und Hysterese. Keine still verlorenen Lichter.
3. Vegetation/Alpha-Cutouts und 2140s Cloud-Transmittance für Boden/Subjects einbeziehen;
   überlappende Sichtbarkeit physikalisch kombinieren. Schattenbudget pro Frame messen.

- [ ] Terrain verdeckt Sonne auf einer bekannten Empfängerfläche; Entfernen des Casters
      muss das Sichtbarkeitsoracle brechen. Bias-Testreihe trennt Acne von Peter-Panning.
- [ ] 100 lokale Lichter ohne 16-Licht-Abschneiden; Referenz gegen vollständige Lichtsumme.
      Cluster abschalten ist ein Korrektheitsvergleich, nicht automatisch ein Performancefehler.
- [ ] 720p60 bewegte Tag-/Nachtszenen nach 2092 mit allen aktiven Passes.

Wahl: [Filament](https://google.github.io/filament/main/filament.html), portable Cluster und
Schattenkarten. Unreal/RAGE sind Bildbenchmarks. Metal-Imageblocks oder herstellerspezifische
Tile-Shading-Experimente sind aus dem Auftrag entfernt: SDL_GPU ist die verbindliche Grenze.
