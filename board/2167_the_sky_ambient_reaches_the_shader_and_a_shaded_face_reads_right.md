Type: feature
State: active
Parent: 2169
Area: world, render
Tags: webcam, measured
Depends:

# Directional sky light and visibility make shaded surfaces plausible

## Bereits repariert / noch offen

12ceb790 bindet die tatsächlich berechnete GPU-Sky-Irradiance an den Shader; GLSL-
Binding-/Synchronisationsfehler und mehrere Kamera/Licht-Konventionen sind repariert.
Das offene Problem ist nicht mehr „Ambient erreicht den Shader nicht“. Diffuses Umgebungslicht
ist noch zu grob richtungsabhängig; lokale Sichtbarkeit/Bounce fehlen. Alle neun Bilder
zeigen flache graue Wände und harte dunkle Felspartien. AO wird laut aktuellem Renderplan
auf neutral aliasiert (`stage_without_a_body ambientOcclusion`).

## Implementierung

- `skyIrradiance.glsl`, `subjectLighting.glsl`, `groundLit.glsl` und zugehörige Stages:
  richtungsabhängige diffuse Sky-Integration (z. B. SH mit 9 Koeffizienten) aus derselben
  bewölkten Atmosphäre wie 2140; Prefilter/DFG für energieverträgliches specular IBL.
- Himmelssichtbarkeit/Horizont für Terrain; kontaktnahe AO mit Bent-Normal bzw. begrenzter
  Screen-space-Lösung für Gebäude. AO auf indirekte Beiträge, nicht als pauschaler Faktor
  auf direkte Sonne. Fehlende Off-screen-Occluder ausdrücklich messen.
- Begrenzten lokalen Bounce über räumlich aktualisierte Irradiance-Probes untersuchen;
  Sky nicht gleichzeitig als zusätzliche konstante Fill-Lampe doppelt zählen. Weißabgleich/
  Exposure getrennt diagnostizieren; Wolken dämpfen direkten Anteil und ändern den diffusen.
- Radiance-/Albedo-/Direct-/Indirect-/Visibility-AOVs durch denselben öffentlichen Renderpfad
  für attributierbare Vergleiche; keine Place-spezifische Aufhellung.

## Abnahme

- [ ] Weißer Würfel, Schlucht, Dachüberstand, offenes Feld: schattige Fläche reagiert auf
      sichtbaren Himmel, verborgene Fläche bleibt dunkler; Energie-/Rotationsoracle mit
      unabhängig integrierter Hemisphäre. Konstant-Fill-Negativkontrolle scheitert.
- [ ] Darmstadt/Husum bei bedecktem Wetter, Koerbersee bei direkter Sonne visuell plausibel;
      keine universelle Foto-Hell/Dunkel-Quote (Webcam-Exposure unbekannt).
- [ ] Kosten/Leakage bei Bewegung sowie Tag/Nacht mit 2092; 2140s Integration bleibt offene
      gemeinsame Abnahme, verhindert aber nicht die erste Clear-sky-Implementierung.

Wahl: [Filaments IBL-Modell](https://google.github.io/filament/main/filament.html) als
lesbare Referenz; Unreal-GI ist Vergleich für Sichtbarkeit/Bounce, kein Lumen-Versprechen
auf SDL_GPU. RAGEs Look begründet keine unbelegte konstante Ambient-Zahl.

## Numerischer Vorläufer: Sonnenpole

sky.frag und aerialPerspective.frag normalisieren die Sonnenprojektion auf die
Horizontebene ohne Nullprüfung. Für sunDir parallel worldUp ist diese Projektion
(0,0); eine Richtung mit Länge eins existiert dafür nicht. Khronos normalize-Vertrag:
https://raw.githubusercontent.com/KhronosGroup/OpenGL-Refpages/main/gl4/normalize.xml
Die gemeinsame Sky-LUT-Koordinate muss den Pol explizit behandeln: dort ist der
Azimut beliebig, da das Medium rotationssymmetrisch um die Vertikale ist. Endlichen
kanonischen Azimut verwenden; keinen NaN-Wert nachträglich im fertigen Bild verdecken.
Der Device-Test aus 2190 verwendet Zenitsonne. Seine NaNs verschwanden durch die
Korrektur des Temporal-Eingangs, ohne GLSL-Änderung: keine belegte Folge des Sonnenpols.
Den unabhängig vorhandenen Nullvektor-Verstoß mit Zenit/Nadir und Annäherung prüfen.
Diese Reparatur ersetzt keine Abnahme von gerichtetem IBL, Sichtbarkeit oder Bounce.

## Offene Sonnenhöhenprüfung
`test/outshine/integration/places/ScoreWhichWaysTheSunMovesTheGround.cpp` bleibt rot:
unteres Bildviertel bei 5°/30°/75°: 37,022 / 35,774 / 71,616. Der Test verlangt
Monotonie, misst aber komplexes Gelände nach Belichtung/Tonemapping. Das beweist
noch keinen Fehler der direkten Beleuchtung: sin(Höhe) gilt für eine horizontale,
unverschattete diffuse Fläche bei konstantem einfallendem Direktlicht. Den Vertrag
mit isoliertem Empfänger, fester Belichtung und linearem Direct-AOV prüfen;
Geländenormalen, Sichtbarkeit und Belichtungsverlauf separat eingrenzen. Keine
Toleranzanhebung oder Shaderkorrektur aus dieser ROI-Zahl allein ableiten.

## Clear-sky-Messreferenz
Egbert/ASTM-Manifeste erhalten, score_clear_sky.py neu anbinden: derzeit falscher
JSON-Pfad files statt subjects[].files, veralteter Fetchpfad und kein Engine-Aufruf.
ASTM-Global-Tilt und Egbert-Tagesmaximum sind keine identischen Messbedingungen;
pauschale 2%-Gleichheit entfernen und durch fachlich begründete Orakel ersetzen.
Messgeometrie, Sonnenstand, Atmosphäre, Spektralbereich und radiometrische/photometrische
Einheiten angleichen. Lineare GPU-Ausgabe vor Belichtung/Tonemapping vergleichen;
Messunsicherheit und Modellabweichung ausweisen. Keine Parameteranpassung nur an Egbert.
Mindestens unabhängige Sonnenhöhen/Empfängerrichtungen und Negativkontrolle; Anschluss 2218.
