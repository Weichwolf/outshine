Type: feature
State: open
Parent: 2169
Area: world, render
Tags: webcam, measured
Depends: 2152, 2295

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
   Sichtbare Lichter und schattenwerfende Lichter getrennt budgetieren:
   Tausende Emittenten dürfen Licht beitragen, während nur die wichtigsten
   lokale Schattenkarten erhalten. Schattenlos heißt nicht lichtlos. Caster-
   Revision und Lichttransform sind Teil des Cache-Schlüssels; Tag, Dämmerung,
   Nacht und Scheinwerferwechsel dürfen keine fremden/stale Schatten übernehmen.
3. Vegetation/Alpha-Cutouts und 2140s Cloud-Transmittance für Boden/Subjects einbeziehen;
   überlappende Sichtbarkeit physikalisch kombinieren. Schattenbudget pro Frame messen.

- [ ] Terrain verdeckt Sonne auf einer bekannten Empfängerfläche; Entfernen des Casters
      muss das Sichtbarkeitsoracle brechen. Bias-Testreihe trennt Acne von Peter-Panning.
- [ ] 100 lokale Lichter ohne 16-Licht-Abschneiden; Referenz gegen vollständige Lichtsumme.
      Cluster abschalten ist ein Korrektheitsvergleich, nicht automatisch ein Performancefehler.
- [ ] Gleiche Straße bei klarem Mittag, Dämmerung und Nacht mit 1000 sichtbaren
      Emittenten rendern und PNGs öffnen. Sonnenschatten verschwindet mit
      Sonnenlicht; lokale Lichtwirkung bleibt auch ohne Shadow-Slot erhalten.
      Bewegte Scheinwerfer und LOD-/Weltwechsel erzeugen keine geerbten Schatten.
      Shadow-Slots/Atlas-Bytes, Updatezahl und GPU-p95/p99 am Zielgerät messen;
      bei Überlast stabil priorisieren statt pro Frame zu flackern.
- [ ] Nächtliches Auto fährt an einem begehbaren Raum mit Fenster vorbei:
      zwei Scheinwerferkegel und die zugehörigen Schatten wandern kontinuierlich
      über Innenwand und Decke; Wand, Fensteröffnung und Fahrzeug verdecken
      Licht geometrisch korrekt. Eine Figur geht unter einer Straßenlaterne
      durch: eigener Schatten wechselt Richtung und Länge stetig auf Boden und
      Körper. Kamerafahrt und Standbilder bei mehreren Zeiten selbst ansehen;
      falsches Lichtleck, Schatten-Popping oder temporaler Nachlauf sind rot.
- [ ] 720p60 bewegte Tag-/Nachtszenen nach 2092 mit allen aktiven Passes.

Wahl: [Filament](https://google.github.io/filament/main/filament.html), portable Cluster und
Schattenkarten. Unreal/RAGE sind Bildbenchmarks. Metal-Imageblocks oder herstellerspezifische
Tile-Shading-Experimente sind aus dem Auftrag entfernt: SDL_GPU ist die verbindliche Grenze.

Nach korrekten Terrain-/Instanz-Castern: Epic Virtual Shadow Maps als Experiment
für seitenweise Schattenauflösung und Wiederverwendung statischer Seiten prüfen.
Gegen stabile Sonnenkaskaden bei Fahrt, Kamerasprung, bewegter Vegetation und
Zeitraffer vergleichen; Page-Invalidierungen, GPU-p95/p99 und Bytes ausweisen.
Sonnenbewegung invalidiert den Cache, daher nur übernehmen, wenn Gesamtbild und
Kosten auf A18 Pro besser sind. Referenz:
https://dev.epicgames.com/documentation/unreal-engine/virtual-shadow-maps-in-unreal-engine .
