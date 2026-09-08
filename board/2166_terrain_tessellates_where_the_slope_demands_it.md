Type: feature
State: active
Area: world, render
Tags: webcam, measured
Depends: 2123

# Terrain refines the final surface, including cliff faces

## IST, 2026-09-07 / 12ceb790

Fehlergesteuerte Verfeinerung existiert gegen eine verschachtelt resampelte DEM-Referenz.
`TerrainGrid::FlattenOutliers` wurde entfernt: Der Medianfilter löschte in Malcesines
Tile 11/1084/731 29 903 / 65 536 = 45,6 % der gültigen Samples. Konventionssuite 9/9 grün.
Die Verfeinerung läuft jedoch vor `Press`, behandelt virtuelle Ferngitter nicht gleichwertig
und deckt die finale Geometrie nicht ab. 33×33-Grids, Skirts und feste virtuelle Ringe bleiben.
Neue Slope-Materialmischung färbt steile Flächen felsig; sie verfeinert keine Seitenfläche.

Visuell: Malcesine-46e4db5c zeigt einen senkrechten Faltenvorhang mit gezahntem oberen Rand;
Koerbersee-53e84b69 gerundete/aufgeblähte Felsformen; Feldkirch-297a9d23 eine abrupte Nahwand.
Die konkrete Entstehung jeder Wand ist noch durch Höhen-/Stamp-/Sheet-Diagnostik zu lokalisieren.

## Implementierung

Diagnose 2026-09-08: Malcesine-46e4db5c erneut geöffnet und unter
`build/shots/reference/terrain-20260908/` gesichert. `GroundConstants.inc` senkt
Skirts um 16 Gitterabstände ab; `HeightSheets::TileOf` erweitert die Bounds entsprechend.
Hypothese: sichtbare Skirts verursachen den Ufer-Vorhang. Gegenprobe: ausschließlich
die Skirt-Absenkung temporär auf null setzen, Malcesine rendern und PNG öffnen,
danach Quellwert wiederherstellen. Verschwindet der Vorhang nicht, ist die pauschale
Skirt-Ursache widerlegt. Das Abschalten ist Diagnose, keine zulässige Nahtreparatur.

1. Pro sichtbarem Patch Roh-DEM, finales gestempeltes Höhenfeld, Quellzoom, Sampleabstand,
   Höhenänderung, Patch-/Skirt-ID und geometrische Normale separat ausgeben. Faltenursache
   belegen, nicht eine Materialänderung als Geometriefix melden.
2. Fehlerhierarchie über die finale Oberfläche nach Straße/Gebäude/Wasser aufbauen bzw.
   lokal invalidieren. Volle residente Quelldetails berücksichtigen, Grenzen konservativ
   propagieren; virtuelle Fernflächen in dieselbe Entscheidung aufnehmen.
3. Projektierter Fehler statt Entfernung allein: focal_px = H/(2 tan(vfov/2));
   e_px ≈ e_m * focal_px / d ist die Fernfeldnäherung. Für nahe/seitliche Wände konservative
   View-/Clip-space-Bounds einschließlich Near-Plane und horizontaler Abweichung verwenden.
   Refine bis deklarierter Fehler ≤ 1 px oder Quell-/Budgetgrenze; unerfüllten Fehler ausweisen.
4. Breaklines/Ufer/Einschnitte als echte Kanten erhalten, Seitenflächen entlang UND quer zur
   Wand adaptiv triangulieren. Ein steiles planares Stück braucht keine zusätzlichen Dreiecke;
   Felsrelief dagegen schon. Reine Höhenfelder können keine Überhänge/vertikalen Mehrfachhöhen:
   dort lokale generierte 3D-Patches mit geschlossenen Anschlüssen, begrenztem plausiblen Relief
   aus 2171. Kein Anspruch auf im DEM nicht vorhandene reale Felsspalten.
5. Normale aus finaler Geometrie und getrennter Mikrostruktur; keine Glättung über konstruktive
   harte Kanten. Nachbarkanten gemeinsam wählen (2144), Morph/Hysterese beim LOD-Wechsel,
   Off-thread-Aufbau und begrenzter Upload (2124).

## Abnahme

- [ ] Plane einschließlich steiler Plane hat Nullfehler; versetzter Peak, schmale Rinne,
      nachträglicher Stamp und seitlich gesehene Reliefwand lösen notwendige Verfeinerung aus.
      Unendlich große Toleranz lässt das Fehleroracle rot werden.
- [ ] Malcesine: kein Vorhang, keine Zähne; seitliche Details tragen Licht/Silhouette auch bei
      Kamerabewegung. Koerbersee: Grate/Rinnen bleiben erhalten. Feldkirch: keine erfundene Nahwand.
- [ ] 1-px-Ziel getrennt gegen residente Daten und gegen Quellauflösung berichten; zusätzliche
      Dreiecke allein gelten nicht als Beweis. 2092 misst Kosten und maximale Framezeit.

Wahl: Cesium-artige geometrische Fehlerselektion plus portable Compute-/Index-Geometrie,
kein SDL-unverfügbares Hardware-Tessellation-/Nanite-Versprechen. Unreal/RAGE sind visuelle
Referenzen; Quellauflösung und finales Fehlermaß entscheiden die Umsetzung.
