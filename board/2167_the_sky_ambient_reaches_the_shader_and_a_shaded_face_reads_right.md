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
