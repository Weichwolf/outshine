Type: feature
State: open
Parent: 2169
Area: world, render, scenario
Tags: atmosphere, astronomy, measured
Depends: 2172, 2128, 2155

# The astronomical sky matches world time and location

## Ziel

Sonne, Mond und Sterne bilden weltweit zu jeder Tages- und Jahreszeit einen
glaubwürdigen Himmel. Beide Hemisphären, Polartag/-nacht und hohe Beobachterstandorte
gehören zum allgemeinen Vertrag. Referenzbilder sind keine austauschbaren Skyboxen.

## Umsetzung

- Eine deklarierte UTC-Zeit und geodätische Beobachterposition mit eindeutigem Höhenbezug
  verwenden. Sonnen-/Mondrichtung, scheinbare Scheibengröße, Mondphase und beleuchtete
  Mondseite konsistent ableiten. Astronomische Modelle und ihre Genauigkeit belegen;
  etablierte Ephemeriden als unabhängiges Vergleichsoracle nutzen.
- Sternpositionen/-helligkeiten aus einem versionierten, lizenzierten Datenkatalog;
  Eigenrotation des Himmels, geographische Breite und Datum berücksichtigen. Keine
  zufällige Kameradekoration. Milchstraße und Sternhelligkeiten als gemeinsamen
  Himmelsinhalt behandeln; Sichtbarkeit hängt von Atmosphäre und Belichtung ab.
- Gemeinsame Atmosphäre aus 2172 bestimmt Extinktion und Streuung. Sonnen-/Mondlicht
  beeinflusst Oberflächen und Volumen einschließlich Geländeabschattung konsistent.
  2155 hält Farb-/Belichtungsantwort. Wolkenverdeckung und deren Negativkontrolle
  besitzt die nachfolgende Integration 2140; kein Blocker für diesen klaren Himmel.
- Astronomische Zustandsupdates, vorberechnete Atmosphärendaten und Renderauswertung
  getrennt budgetieren. Räumliche/zeitliche Auflösung nach sichtbarer Wirkung wählen;
  Fehler bei Bewegung, Zeitraffung und Wetterwechsel messen statt Reprojektion vorauszusetzen.

## Abnahme

- [ ] Richtungen und Ereigniszeiten gegen unabhängige Ephemeriden prüfen; tolerierte
      Winkel-/Zeitfehler vor Messung festlegen und begründen. Zeitzonen beeinflussen
      Darstellung der Uhrzeit, nicht die zugrundeliegende UTC-Simulation.
- [ ] Nord-/Südhemisphäre, Äquator, Polarregion und Hochgebirge über Jahreszeiten:
      Auf-/Untergang, Dämmerung, Nacht, Polartag/-nacht und mehrere Mondphasen prüfen.
- [ ] PNGs zeigen stimmige Mondphase/-orientierung, Sternhimmel,
      Luftperspektive und Landschaftsbeleuchtung; keine sichtbaren Sprünge/Geisterbilder.
      Sonne/Mond dürfen bei Horizontdurchgang nicht durch Gelände sichtbar bleiben.
      Eine gepinnte NASA-Mond-Albedokarte darf die einzige fest eingebaute Bildtextur der
      generierten Welt sein; Lizenz, Projektion, Farbraum und Phase/Orientierung belegen.
      Ohne Asset muss der prozedurale Fallback weiterhin funktionieren. Importierte
      Szenarien dürfen eigene Texturen mitbringen; generierte Caches zählen nicht als Assets.
- [ ] A18-Pro-Projektziel 720p60 in bewegter Gesamtszene prüfen: Framebudget
      1000 ms / 60 = 16,67 ms. CPU/GPU p50/p95/p99, Speicher und Streaming mit 2092;
      Himmelbudget als Teil des Gesamtbudgets aus Messungen ableiten. Kein isolierter
      Himmel-Benchmark als Nachweis für die vollständige Welt.
- [ ] Falsche Zeit, vertauschte Hemisphäre und entkoppelte Geländeabschattung werden durch
      unabhängige Richtungs-/Bildprüfungen erkannt; reine Selbstvergleichshashes genügen nicht.
