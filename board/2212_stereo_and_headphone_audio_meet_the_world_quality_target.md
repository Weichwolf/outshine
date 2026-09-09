Type: feature
State: active
Area: audio, engine, client, test
Parent: 2169
Depends: 2191

# Stereo and headphone audio meet the world quality target

## Ziel und Priorität

Nutzermaßstab: hochwertige Stereoanlage und gute Kopfhörer zuerst. Klangfarbe,
Dynamik, stabile Ortung und glaubwürdige Tiefe vor Kanalzahl. Kein eigenes Surround-Ziel.
Reihenfolge: saubere Wiedergabe/Dynamik → Lautsprecher-Stereo → binaurale Kopfhörer
→ verfeinerte Raumakustik. Fehler-/Ownership-Fundament gemeinsam mit 2194 bearbeiten;
dessen vollständige Exception-Migration blockiert die Backend-Evaluation nicht.

Eine native akustische Szene mit Quellen, Listener und Schallwegen; erst am Ausgang
in Lautsprecher-Stereo oder binaurales Stereo umsetzen. Keine doppelte Weltlogik.
Stereo-Musik und nicht räumliche Inhalte erhalten ihre beabsichtigte Abbildung.
Keine globale künstliche Verbreiterung oder dauernde Lautheitsmaximierung.
Leise-Spiel-Kompression optional; pegelgleiche Vergleiche statt Lauter-ist-besser.

## Prozedurale Quellen und virtueller Hörkopf

Quellen hauptsächlich synthetisch aus Weltzustand erzeugen: Motor aus Drehzahl/Last,
Reifen aus Tempo/Untergrund, Wind aus Strömung/Vegetation, Kontakte aus Material,
Impuls und Resonanzen. Deterministische Seeds und kontinuierliche Parameterübergänge;
keine hörbaren Wiederholungsschleifen oder Sprünge beim Streaming. Aufnahmen bleiben
ergänzende Anregungen/Klangbausteine, keine Voraussetzung für jede Weltvariation.
Synthese produziert native Quellsignale; Ausbreitung und Wiedergabe bleiben getrennt.
Virtueller Hörkopf berücksichtigt Laufzeit-, Pegel- und spektrale Richtungsmerkmale.
Lautsprecherübersprechen verhindert eine pauschale Garantie binauraler Rückwärtsortung;
kalibrierte Übersprechkompensation optional evaluieren, keine Pflicht für Stereo.
https://resource.isvr.soton.ac.uk/FDAG/VAP/html/vasi.htm
KHR_audio_graph ist eine Entwurfsreferenz, kein verbindlicher Synthesizerstandard:
https://github.com/KhronosGroup/glTF/issues/2561
Formatadapter dürfen daraus native Syntheseprodukte erzeugen, nicht die Runtime binden.

## Bestand und Entscheidung

Mixer erzeugt Synthese, einfachen Tiefpass, Delay, Panning, Doppler und Hall.
Setup ist transaktional; numerische Parameter und Delay-Ringe werden vorbereitet.
Datei-/Streamingquellen fehlen im Wiedergabepfad. Der als Biquad benannte Prozessor
ist ein Einpol-Tiefpass. Graphkanten sind beim Setup validiert und kompiliert;
Signal-Scratch wird beim Setup reserviert, variable Ausgabeblöcke intern geteilt.
Engine::prepareAudio(sampleRateHz) bereitet außerhalb der Ausgabe vor; mix(stereo)
verwendet ausschließlich diesen Zustand. Erfolgreiche Vorbereitung setzt DSP zurück,
fehlgeschlagene erhält ihn. Neue deklarierte Inhalte invalidieren die Vorbereitung.
Initialen Quellsnapshot ohne Simulationstick publizieren; Engine-Aufrufe serialisieren.
Öffentlicher API-Test: Rate, erste Samples, Fehler/Zustandserhalt und Redeclare.
Hallwerte und Ringbudget sind validiert.
Quellbindung On wählt noch den ersten freistehenden Körper statt der benannten
Entität. Ortsgebundene Abnahme benötigt die gemeinsame Entity-/Transform-Bindung
aus 2191; Synthese-/Backend-Arbeit kann unabhängig davon weitergehen. Kein Ersatz
durch parallele Deklarationsindizes: unplatzierte Bodies verändern deren Zuordnung.
Der Mixer verwendet noch nur den ersten aktiven Bus-Hall: Bus-spezifische Effekte
und unabhängige RT60-/Spektralprüfung fehlen; Worst-Case-Messung bleibt offen.
Diese Defizite nicht durch konservierte Alt-Ausgaben als richtig deklarieren.

SDL3 für Geräte/Streams; SDL3_mixer für ergänzende Wiedergabe/Dekodierung evaluieren.
Synthesequalität und akustische Szene bestimmen die Backendwahl, nicht Dateidekodierung.
Lokal Version 3.2.4 vorhanden, Integration noch offen. MIX_CreateMixer/MIX_Generate
ermöglichen gerätelose Tests des bestehenden Engine::mix-Vertrags.
https://wiki.libsdl.org/SDL3_mixer/MIX_Generate
SDL3_mixer-3D liefert weder Doppler noch frei wählbare Distanzmodelle und konvertiert
räumliche Quellen zu Mono; kein vollständiges Raumakustik-Backend.
https://wiki.libsdl.org/SDL3_mixer/MIX_SetTrack3DPosition

Steam Audio für HRTF, Transmission und Reflexionen evaluieren, nicht vorab festlegen.
Plattformen, Lizenz, SIMD/CPU-Kosten, Streaminggeometrie, Ownership und Threadmodell
prüfen. HRTF-Profile müssen auf Kopfhörern vergleichbar und übersteuerbar sein;
binaurale Ausgabe nicht ungeprüft an Stereo-Lautsprecher senden.
https://valvesoftware.github.io/steam-audio/doc/capi/guide.html

## Umsetzung und widerlegbare Abnahme

- [ ] Prozedurale Quellen reagieren reproduzierbar und klanglich kontinuierlich auf
      Last, Geschwindigkeit, Wetter und Material; Pegel-/Spektral- und Hörtests.
- [ ] Versionierte Backend-Abhängigkeiten und Init-/Shutdown-Ownership festgelegt.
- [ ] Unabhängige PCM-/WAV-Fixtures über öffentliche API: korrekte Kanäle, Gain,
      Blockkontinuität, Start/Stop/Loop, Resampling und Fehler ohne Teilzustand.
- [ ] Quellenzahl, Decoder, Queues, Scratch und Delay/Hall budgetiert; kein Datei-IO
      oder routinemäßiges Allokieren im Audioblock. Aussetzer und Latenz messen.
- [ ] Stereo: Phantommitte, seitliche Bewegung, Nah/Fern, Transienten und leise
      Details erhalten. Headroom/Clipping messen; keine pauschale Klangqualitätsbehauptung.
- [ ] Kopfhörer: vorne/hinten/oben/unten, Bewegung und Außenkopflokalisation prüfen;
      HRTF-Wahl und einfacher Stereo-Modus, keine doppelte Spatialization.
- [ ] Akustische Szenen: Freifeld, Wald, Straße, Zimmer, Türdurchgang, Tunnel und
      Fahrzeug; Direktschall, frühe Reflexionen, Nachhall und Verdeckung getrennt prüfen.
- [ ] Geräuschquellen und Reflexionen bleiben bei Weltstreaming und Kamerawechsel stabil.
- [ ] Pegelgleiche A/B-Dateien und reproduzierbare Hörszenarien für den Regisseur.
      Automatische Signaltests ersetzen seine Hörabnahme nicht.
- [ ] Bestehende Synthese-/Szenariofähigkeiten erhalten oder fachlich korrekt migriert;
      falsche Filterbezeichnung und unklare Delay-Zeitkonvention beseitigt.
- [ ] Lint, clang-tidy, unabhängige Tests und wirksame Negativkontrollen abgeschlossen.
