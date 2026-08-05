#!/usr/bin/env python3
"""mig23 — die VERMESSUNG. Nur Zahlen und ihre Herkunft, keine Blender-Abhaengigkeit.

    ALLES METRISCH, DEZIMALSYSTEM. Eignerentscheid 2026-08-05, projektweit (doc/conventions.md).

WAS DAS IST. MiG-23MLD "Flogger-K", der haeufigste Gegner der F-22-Kampagne
(mods/f22/doc/substitutions.md: alle acht MiG-27-Sorties werden auf diese Zeile abgebildet).
Registry-Schluessel `mig23`, Katalogzeile doc/modules/air/catalogue.md §mig23,
Flugmodell-Anker sim/test/modules/air/FBAirAnchors.h.

QUELLEN
  [PUB]  Brassey's world aircraft & systems directory 1996/97, S. 73-75, ueber
         [WEB https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23] Abschnitt
         "Specifications (MiG-23MLD)". Das ist DIESELBE Quelle, aus der die Katalogzeile
         dieses Baums ihre A1..A8-Anker zieht — Netz und Flugmodell haengen an einem Nagel.
  [BP]   Dreiseitenriss MiG-23M aus einer russischen Modellbauzeitschrift, 4000 x 2785 px,
         [WEB https://drawingdatabase.com/wp-content/uploads/2014/03/mikoyan-gurevich-mig-23-mpd-2.png]
         (Uebersicht: [WEB https://drawingdatabase.com/mikoyan-gurevich-mig-23/]).
         Jede [BP]-Zahl nennt ihre PIXELKOORDINATE im Original, damit ein Nachfolger sie
         nachschlagen und widerlegen kann. Der Riss traegt einen Massstabsbalken (s. kPxM).
  [DOC]  aus diesem Baum: doc/modules/air/catalogue.md, sim/test/modules/air/FBAirAnchors.h.
  [SET]  von mir gesetzt, weil keine Quelle es hergibt. Jede solche Zahl steht in DEFECTS.md.

WARUM DER RISS DIE MiG-23M ZEIGT UND DAS MODELL DIE MLD IST. Es gibt keinen bemassten
MLD-Riss in gleicher Aufloesung. M und ML/MLD teilen Rumpf, Fluegel, Einlaeufe, Leitwerk und
alle veroeffentlichten Hauptmasse; die Unterschiede sind benannt und einzeln behandelt
(kDorsalFin, kGearStance — s. dort und DEFECTS.md #2).

KOORDINATEN (Blender): +X rechts (Steuerbord), +Y vorwaerts, +Z oben, 1 Einheit = 1 m.
  y = 0  ist die DREHZAPFENSTATION der Schwenkfluegel  — der einzige Punkt des Risses, der in
         beiden Ansichten und in beiden gezeichneten Pfeilstellungen unabhaengig belegt ist.
  z = 0  ist die TRIEBWERKSACHSE (Mitte der Schubduese im Seitenriss).
  x = 0  ist die Symmetrieebene.
Der Boden liegt bei kGroundZ; ein Platzierer setzt ihn auf das Terrain.
"""

import math

TAU = 2.0 * math.pi


# ================================================================ Kalibrierung des Risses

# [BP] Bildkoordinaten des Risses. Grundriss: Nase LINKS, x_px waechst nach ACHTERN.
#      Seitenriss: Nase RECHTS, x_px waechst nach VORN. Beide Ansichten haengen ueber
#      kPxViewSum zusammen: x_seite + x_grund = konstant.
kPxPlanCentre = 792.0                    # Symmetrieachse im Grundriss, aus dem Bugkegel
#      (x=400: 781..803 -> 792.0; x=450: 763..821 -> 792.0)
kPxPivot = 2251.0                        # Drehzapfenstation im Grundriss (s. kPivotY unten)
kPxEngineAxis = 2432.0                   # Duesenmitte im Seitenriss (x=190: 2307..2557)
kPxViewSum = 3797.0                      # Seiten- + Grundriss-Station derselben Spantstelle:
#      Radomspitze Seite 3408 / Grund 388 -> 3796; Pitotspitze Seite 3626 / Grund 168 -> 3794.
#      Genommen wird 3797, der Mittelwert ueber beide Marken (Streuung 2 px = 10 mm).

# [DERIVED] Massstab. Der Balken des Risses (0..5 m ueber 1007.0 px, Strichmitten x = 99.5
# und 1106.5 bei y = 540) gibt 4.96524 mm/px. Genommen wird NICHT er, sondern die
# Anpassung an die ZWEI veroeffentlichten Spannweiten, weil die den Riss an zwei weit
# auseinanderliegenden Stellen festnageln statt an einer:
#   gepfeilt  7.779 m [PUB] / (792.0 - 12.0) px  = 4.98654 mm/px
#   gespreizt 13.965 m [PUB] / (2194.0 - 792.0) px = 4.98003 mm/px
# Beide liegen 0.27 % bzw. 0.30 % ueber dem Balken — der Riss ist also gleichmaessig um
# 0.3 % geschrumpft (Scan/Papier), und genau das gleicht dieser Faktor aus.
kPxM = 0.0049814
kPxMScaleBar = 0.00496524                # zum Vergleich, s. DEFECTS.md #1
kPxMBarDelta = kPxM / kPxMScaleBar - 1.0        # +0.328 %


def y_side(px):
    """Seitenriss-Station -> y (vorwaerts positiv)."""
    return (px - (kPxViewSum - kPxPivot)) * kPxM


def y_plan(px):
    """Grundriss-Station -> y (vorwaerts positiv)."""
    return (kPxPivot - px) * kPxM


def z_side(py):
    return (kPxEngineAxis - py) * kPxM


def x_plan(py):
    return (py - kPxPlanCentre) * kPxM


# ================================================================ Hauptmasse [PUB]

kLength = 16.7                           # [PUB] "length m = 16.7"
kSpanSpread = 13.965                     # [PUB] "span m = 13.965 (fully spread)"
kSpanSwept = 7.779                       # [PUB] "7.779 m fully-swept"
kHeight = 4.82                           # [PUB] "height m = 4.82"
kWingAreaSpread = 37.35                  # [PUB] "wing area sqm = 37.35 (fully-spread)"
kWingAreaSwept = 34.16                   # [PUB] "34.16 m2 fully-swept"
kAirfoilRootThk = 0.065                  # [PUB] "root: TsAGI SR-12S (6.5%)"
kAirfoilTipThk = 0.055                   # [PUB] "tip: TsAGI SR-12S (5.5%)"

# [DERIVED] Probe des Risses gegen [PUB]: Radomspitze bis Hoehenleitwerksspitze.
# Seitenriss 3408 px (Kegelspitze) bis 66 px (hinterste Leitwerksecke) = 3342 px.
kLengthMeasured = 3342.0 * kPxM                   # 16.648 m
kLengthErrRel = kLengthMeasured / kLength - 1.0   # -0.31 %
# DIE ENTSCHEIDUNG, DIE DARAN HAENGT: die 16.7 m sind OHNE Pitotrohr. Mit Rohr misst der
# Riss 17.735 m; waeren die 16.7 m MIT Rohr gemeint, muesste der Laengenrest -5.8 % sein,
# waehrend beide Spannweiten auf demselben Massstab -0.3 % liegen. Ein Massstab kann nicht
# gleichzeitig zwei Spannweiten auf 0.3 % treffen und eine Laenge um 5.8 % verfehlen.
kLengthOverall = (3626.0 - 66.0) * kPxM           # 17.735 m, mit Pitotrohr

# [DOC] doc/modules/air/catalogue.md §mig23 / FBAirAnchors.h — dieselben [PUB]-Zahlen,
# hier nur zur Gegenprobe, dass Netz und Flugmodell aus einer Quelle kommen.
kDocWingArea = 37.35
kDocGrossKg = 14840.0


# ================================================================ Schwenkfluegel

# [PUB]/[WEB] Drei belegte Stellungen. "The 23-11 featured variable-geometry wings which could
# be set to angles of 16, 45 and 72 degrees" [WEB en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23].
# Die MLD hat eine vierte bei 33 Grad ("A strengthening of the wing pivot in the MiG-23MLD
# allowed the addition of a fourth wing sweep position of 33 deg", ibid.) — sie ist gebaut,
# weil dieselbe Kinematik sie kostenlos hergibt, und benannt.
kSweepDetents = (16.0, 33.0, 45.0, 72.0)
kSweepMin, kSweepMax = 16.0, 72.0
kSweepDefault = 16.0                     # [SET] Bauzustand: gespreizt, wie am Boden

# [BP] Drehzapfen. Der Zapfenbolzen ist im Grundriss als Kreis gezeichnet; seine Lage folgt
# hier aber NICHT aus dem Kreis (zu unscharf), sondern aus einer Bestimmung, die beide
# veroeffentlichten Spannweiten ERZWINGT und danach am Riss geprueft wird:
#   Spitze(Lambda) = Zapfen + r * (cos(theta0 + Lambda - 16), sin(...))
#   b/2(16) = kPivotX + r cos(theta0)      = 6.9825
#   b/2(72) = kPivotX + r cos(theta0 + 56) = 3.8895
# Zwei Gleichungen, drei Unbekannte -> kPivotX kommt aus dem Riss, r und theta0 werden geloest.
kPivotX = 1.5144                         # [BP] 304 px seitlich der Achse (Zapfenkreise
#                                          Grundriss y = 488 / 1096, x = 2251)
kPivotY = 0.0                            # [DERIVED] Definition des Nullpunkts
kPivotZ = 0.16                           # [SET] Zapfenhoehe ueber der Triebwerksachse,
#                                          s. DEFECTS.md #5


def _solve_panel():
    """r und theta0 aus den ZWEI veroeffentlichten Spannweiten. Geschlossen, nicht iterativ.

    a = r cos(t), b = r cos(t + d) mit d = 56 Grad:
        b = r (cos t cos d - sin t sin d) = a cos d - r sin t sin d
        -> r sin t = (a cos d - b) / sin d
        -> tan t   = (a cos d - b) / (a sin d)
    """
    a = kSpanSpread / 2.0 - kPivotX
    b = kSpanSwept / 2.0 - kPivotX
    d = math.radians(kSweepMax - kSweepMin)
    t = math.atan2(a * math.cos(d) - b, a * math.sin(d))
    return a / math.cos(t), math.degrees(t)


kPanelTipR, kPanelTipTheta = _solve_panel()      # 5.5343 m, 8.536 Grad
# [BP] GEGENPROBE, und sie ist der Grund, dieser Konstruktion zu glauben: die so bestimmte
# Spitze wird an BEIDEN gezeichneten Stellungen mit dem Riss verglichen.
#   72 Grad: gerechnet Grundriss (3258.5, 10.2) — gemessen (3255, 12)  -> 3.5 / 1.8 px
#   16 Grad: gerechnet (2416.5, 2200.3)          — gemessen (2420, 2194) -> 3.5 / 6.3 px
# 6 px sind 30 mm auf einer 7-m-Halbspannweite.
kPanelCheckPx = ((3258.5, 10.2, 3255.0, 12.0), (2416.5, 2200.3, 2420.0, 2194.0))

# [BP] Fluegelgrundriss im PANEELRAHMEN (u = spannweitig ab Zapfen, v = nach achtern),
# und der Paneelrahmen IST die 16-Grad-Stellung: dort steht der Randbogen parallel zur
# Rumpfachse (gemessen 1.08 Grad Abweichung), also ist es der Rahmen, in dem der Fluegel
# konstruiert wurde. Alle Punkte sind am 72-Grad-Fluegel des Risses abgelesen und um
# -56 Grad in diesen Rahmen gedreht.
kPanelLeRootU, kPanelLeRootV = 0.3160, -0.9405   # Grundriss (2198, 296)
kPanelLeTipU, kPanelLeTipV = 5.4723, 0.8309      # Grundriss (3255, 12)
kPanelTeTipU, kPanelTeTipV = 5.4928, 1.9142      # Grundriss (3380, 190)
kPanelTeRootU, kPanelTeRootV = 0.0385, 0.7891    # Grundriss (2346, 615)
kPanelRootU = 0.42                       # [SET] Trennfuge Paneel/Handschuh, s. DEFECTS.md #6

kPanelLeSweep = math.degrees(math.atan2(kPanelLeTipV - kPanelLeRootV,
                                        kPanelLeTipU - kPanelLeRootU))     # 18.97 Grad
kPanelTeSweep = math.degrees(math.atan2(kPanelTeTipV - kPanelTeRootV,
                                        kPanelTeTipU - kPanelTeRootU))     # 11.66 Grad
# [BP] GEGENPROBE ZWEI: 18.97 Grad ist die Vorderkantenpfeilung in der "16-Grad"-Stellung.
# Die Literatur nennt fuer die MiG-23 zu den Zapfenwinkeln 16/45/72 die Vorderkantenwinkel
# 18 Grad 45' / 47 Grad 40' / 74 Grad 40'. 18.97 gegen 18.75 = +1.2 %, und am 72-Grad-Riss
# misst dieselbe Kante ueber eine 1000-px-Basis 74.97 gegen 74.67 Grad.
kLeSweepDocDeg = 18.75

kPanelTipChord = math.hypot(kPanelTeTipU - kPanelLeTipU,
                            kPanelTeTipV - kPanelLeTipV)                   # 1.083 m


def panel_le_v(u):
    return kPanelLeRootV + math.tan(math.radians(kPanelLeSweep)) * (u - kPanelLeRootU)


def panel_te_v(u):
    return kPanelTeRootV + math.tan(math.radians(kPanelTeSweep)) * (u - kPanelTeRootU)


def panel_chord(u):
    return panel_te_v(u) - panel_le_v(u)


kPanelRootChord = panel_chord(kPanelRootU)                                 # 1.771 m
kPanelExposedArea = 0.5 * (kPanelRootChord + kPanelTipChord) * (kPanelLeTipU - kPanelRootU)
# [DERIVED] beide Paneele zusammen; der Rest der Bezugsflaeche steckt im Handschuh und im
# Rumpfdurchgang. 2 * 7.36 = 14.7 m2 von 37.35 m2 [PUB].

kPanelDihedralDeg = -1.0                 # [SET] leichte V-Stellung nach unten, s. DEFECTS.md #7

# [SET] Vorfluegel und Landeklappe belegen feste Sehnenanteile. Die MiG-23 hat Vorfluegel
# ueber die ganze Paneelspannweite und einteilige Landeklappen; keine Quelle nennt die
# Sehnenanteile (DEFECTS.md #8).
kSlatChordFrac = 0.16
kFlapChordFrac = 0.26
kSpoilerChordFrac = 0.14
kSpoilerU = (1.55, 4.55)                 # [SET] Spannweitenbereich der Stoerklappe
kSlatMaxDeg = 20.0                       # [SET]
kFlapMaxDeg = 25.0                       # [SET]
kSpoilerMaxDeg = 45.0                    # [SET]


def tip_lateral(sweep_deg):
    """Halbspannweite bei gegebener Zapfenstellung — die EINE Formel hinter der Kinematik."""
    return kPivotX + kPanelTipR * math.cos(math.radians(kPanelTipTheta + sweep_deg - kSweepMin))


# ================================================================ Rumpf

# [BP] Rumpfquerschnitte. Je Station: (y, z_oben, z_unten, halbe Breite).
# z aus dem Seitenriss (obere/untere Huellkurve, spaltenweise Extremwerte in y in
# [2270..2520] bzw. [2440..2648]), halbe Breite aus dem Grundriss. Die Bauchflosse und die
# Aussenlasten sind aus der unteren Huellkurve entfernt — sie sind eigene Koerper.
#   Seitenriss-Rohwerte (px): 3408:(2529,2529) 3300:(2487,2565) 3200:(2456,2581)
#   3100:(2436,2594) 3000:(2421,2608) 2900:(2397,2606) 2800:(2392,2625) 2700:(2348,2630)
#   2600:(2314,2615) 2400:(2297,2614) 2200:(2295,2636) 2000:(2296,2635) 1800:(2296,2609)
#   1600:(2289,2607) 1400:(2293,2607) 1200:(2292,2608) 1000:(2294,2609) 800:(2301,2612)
#   400:(2315,2609) 300:(2324,2579) 190:(2307,2557)
#   Grundriss-Halbbreiten (px): 385:0 450:29 550:55 650:74 750:86 850:93 1000:97 1150:93
#   1300:107 1400:117 dann [SET]-Fortschreibung bis zur Duese, s. DEFECTS.md #4.
kFusStations = (
    #  x_px_seite, z_oben_px, z_unten_px, halbe Breite px
    (3408.0, 2529.0, 2529.0, 0.0),
    (3350.0, 2503.0, 2549.0, 20.0),
    (3300.0, 2487.0, 2565.0, 34.0),
    (3200.0, 2456.0, 2581.0, 55.0),
    (3100.0, 2436.0, 2594.0, 70.0),
    (3000.0, 2421.0, 2608.0, 81.0),
    (2900.0, 2404.0, 2610.0, 89.0),
    # KOCKPITBUCHT: die obere Huellkurve des Seitenrisses IST hier die HAUBE, nicht der
    # Rumpfruecken — eine spaltenweise Extremwertsuche kann die beiden nicht trennen. Die
    # erste Fassung uebernahm sie als Rumpfoberkante, und die Haube verschwand im Rumpf
    # (im ersten Bild gesehen: kein Kanzelumriss). Der Ruecken liegt hier auf der
    # Bruestungslinie (Riss 2352..2400 px), die Haube darueber. [BP] mit [SET]-Anteil,
    # s. DEFECTS.md #19.
    (2800.0, 2400.0, 2618.0, 94.0),
    (2700.0, 2382.0, 2624.0, 96.0),
    (2600.0, 2366.0, 2620.0, 97.0),
    (2500.0, 2352.0, 2616.0, 99.0),
    (2450.0, 2338.0, 2615.0, 101.0),
    (2400.0, 2297.0, 2614.0, 104.0),
    (2300.0, 2295.0, 2620.0, 110.0),
    (2200.0, 2295.0, 2634.0, 118.0),
    (2100.0, 2295.0, 2638.0, 126.0),
    (2000.0, 2296.0, 2635.0, 132.0),
    (1900.0, 2296.0, 2624.0, 137.0),
    (1800.0, 2296.0, 2612.0, 141.0),
    (1700.0, 2292.0, 2607.0, 145.0),
    (1600.0, 2289.0, 2607.0, 148.0),
    (1500.0, 2291.0, 2607.0, 150.0),
    (1400.0, 2293.0, 2607.0, 150.0),
    (1300.0, 2293.0, 2607.0, 149.0),
    (1200.0, 2292.0, 2608.0, 147.0),
    (1100.0, 2293.0, 2608.0, 144.0),
    (1000.0, 2294.0, 2609.0, 141.0),
    (900.0, 2297.0, 2610.0, 136.0),
    (800.0, 2301.0, 2612.0, 130.0),
    (700.0, 2303.0, 2612.0, 124.0),
    (600.0, 2305.0, 2611.0, 118.0),
    (500.0, 2310.0, 2610.0, 112.0),
    (400.0, 2315.0, 2606.0, 106.0),
    (330.0, 2320.0, 2590.0, 100.0),
    (260.0, 2314.0, 2578.0, 96.0),
    (190.0, 2307.0, 2557.0, 92.0),
)

# [SET] Formexponent der Superellipse je Station (oben, unten). 2 = Ellipse. Der Vorderrumpf
# der MiG-23 ist rund, der Mittelrumpf zwischen den Einlaeufen kastig, der Heckrumpf wieder
# rund. Kein Riss bemasst das; die Schnitte A-A / B-B / W-W / G-G des Risses zeigen es
# qualitativ und danach sind die Exponenten gesetzt (DEFECTS.md #3).
kFusShapeKnots = ((190.0, 2.0, 2.0), (900.0, 2.2, 2.2), (1500.0, 2.9, 2.6),
                  (2200.0, 2.9, 2.4), (2700.0, 2.2, 2.2), (3408.0, 2.0, 2.0))

kNoseConeStart = 2900.0                  # [BP] Radomfuge im Grundriss/Seitenriss
kPitotTipPx = 3626.0                     # [BP] Spitze des Pitotrohrs, Seitenriss
kPitotRootDia = 0.075                    # [SET] Wurzeldurchmesser des Rohrs
kPitotTipDia = 0.030                     # [SET]


# ================================================================ Einlaeufe

# [BP] Rechteckeinlauf mit senkrechter Rampe, das Kennzeichen der MiG-23 neben dem
# Schwenkfluegel. Lippe im Seitenriss bei x = 2420 px, Kanal bis x = 1500 px; Hoehe der
# Lippe 2380..2560 px; im Grundriss reicht die Aussenwand bis 195 px von der Achse
# (Rumpfseite dort 117 px), also 78 px lichte Breite.
kInletLipPx = 2420.0
kInletTopPx = 2378.0
kInletBotPx = 2562.0
kInletOuterPx = 195.0
kInletRampGapPx = 34.0                   # [SET] Grenzschichtspalt zwischen Rampe und Rumpf
kInletDuctDepth = 1.35                   # [SET] Tiefe des dunklen Kanals, s. DEFECTS.md #9
kInletLipRadius = 0.035                  # [SET] Lippenradius


# ================================================================ Handschuh (fester Innenfluegel)

# [DERIVED] Die Handschuh-Vorderkante ist KEINE eigene Messung: bei 72 Grad flieht die
# Paneelvorderkante mit ihr (das ist der Sinn der Stellung), also ist sie die Verlaengerung
# der Paneel-VK aus dem 72-Grad-Riss nach innen bis zur Einlauf-Aussenwand.
kGloveLeSweep72 = 74.97                  # [BP] 1000-px-Basis, Grundriss 2200..3200
kGloveRootX = kInletOuterPx * kPxM       # 0.971 m — Aussenwand des Einlaufs
kGloveTeY = -1.60                        # [SET] Hinterkante des Handschuhs, s. DEFECTS.md #10
kGloveSawtoothU = 0.62                   # [SET] Lage des MLD-Saegezahns an der Handschuh-VK
kGloveSawtoothDepth = 0.18               # [SET] Tiefe des Saegezahns
# "the wing's notched leading edge roots were 'saw-toothed' to act as vortex generators"
# [WEB en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23, Abschnitt MiG-23MLD] — DASS er da ist,
# ist belegt; WIE gross er ist, nicht.


# ================================================================ Leitwerk

# [BP] Seitenflosse, Seitenriss. Spitze (LE) 305/1853 px, Spitzen-HK 145/1878 px,
# VK-Steigung 0.525 px/px ueber die Basis 340..740 -> Pfeilung 62.3 Grad gegen die Senkrechte.
kFinTipLePx = (305.0, 1853.0)
kFinTipTePx = (145.0, 1878.0)
kFinRootPx = 2290.0                      # z-Station, an der die Flosse in den Ruecken laeuft
kFinLeSlope = 0.525                      # dy/dx im Seitenriss
kFinRootTePx = 150.0
kFinThickRoot = 0.22                     # [SET] Dicke am Fuss
kFinThickTip = 0.09                      # [SET]
kRudderChordFrac = 0.26                  # [SET] Sehnenanteil des Ruders
kRudderMaxDeg = 25.0                     # [SET]

# [SET] Der MiG-23M-Riss zeigt die grosse Rueckenflosse vor der Seitenflosse. Die ML/MLD hat
# sie NICHT: "Aerodynamics were refined for less drag, with the dorsal fin extension removed"
# [WEB en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23, Abschnitt MiG-23ML]. Sie wird deshalb
# nicht gebaut — die eine bewusste Abweichung des Modells vom herangezogenen Riss.
kDorsalFin = False

# [BP] Hoehenleitwerk (Taileron), Grundriss. Spitze 3734/1250 px (Steuerbord),
# Wurzel-VK etwa 3050/952 px. Anhedral aus dem Schnitt G-G des Risses.
# Die SPITZE ist der Punkt groesster seitlicher Ausdehnung, nicht die hinterste Ecke: die
# Hinterkante des Leitwerks ist nach VORN gepfeilt, der hinterste Punkt sitzt an der Wurzel.
# Die erste Fassung verwechselte beides und baute 4.55 statt 5.60 m Spannweite — im ersten
# Bild als zu kleines Leitwerk sichtbar, danach am Riss nachgemessen: bei x = 3650 px reicht
# der Umriss auf 253 bzw. 1378 px, also 539 / 586 px beidseits der Achse. Genommen wird das
# Mittel 562.5 px; die Differenz von 47 px (0.23 m) ist Risverzug und steht in DEFECTS.md #11.
kTailTipPx = (3655.0, 1354.5)
kTailRootLePx = (3055.0, 900.0)
kTailRootTePx = (3731.0, 900.0)
kTailTipChord = 0.55                     # [SET] Randbogensehne, DEFECTS.md #11
kTailAnhedralDeg = -10.0                 # [SET] aus Schnitt G-G abgeschaetzt, DEFECTS.md #11
kTailHingeFrac = 0.28                    # [SET] Drehachse in Sehnenanteilen ab VK
kTailZPx = 2258.0                        # [BP] Hoehe der Leitwerkswurzel im Seitenriss
kTailThkRoot = 0.14                      # [SET]
kTailThkTip = 0.06                       # [SET]
kTailMaxDeg = 20.0                       # [SET] Ausschlag

# [BP]/[WEB] Bauchflosse. "the MiG-23 had a ventral fin ... During take-off and landing, the
# fin hinged sideways when the landing gear was extended to prevent it striking the ground"
# [WEB en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23]. Umriss aus dem Seitenriss
# (x 380..980 px, Unterkante bis 2700 px).
kVentralY = (y_side(380.0), y_side(980.0))
kVentralDepth = 0.95                     # [BP] (2700-2612) px ... auf 2.5 m Tiefe [SET]
kVentralFoldDeg = 90.0                   # [SET] Klappwinkel beim Ausfahren des Fahrwerks
kVentralThk = 0.10                       # [SET]


# ================================================================ Kanzel

# [BP] Seitenriss: Windschutzscheibe von x 2800 bis 2700 px, Haube 2700 bis 2450 px,
# Oberkante 2295..2300 px — die MiG-23 hat eine flache, in den Ruecken eingezogene Haube.
kCanopyFrontPx = 2790.0
kCanopyBowPx = 2700.0
kCanopyRearPx = 2440.0
kCanopyTopPx = 2288.0
kCanopySillPx = 2358.0
kCanopyHalfW = 0.42                      # [SET] halbe Haubenbreite
kCanopyHingeAft = True                   # [BP] die MiG-23-Haube klappt nach HINTEN auf
kCanopyOpenDeg = 42.0                    # [SET]


# ================================================================ Fahrwerk

# [DERIVED] Die Beinlaenge kommt NICHT aus dem Riss: Vorder- und Seitenansicht teilen dort
# keine Hoehenbezugslinie (die Frontansicht gibt 4.515 m Gesamthoehe, [PUB] 4.82 m,
# DEFECTS.md #1). Sie wird deshalb aus der veroeffentlichten Standhoehe GERECHNET:
#   Boden = Flossenspitze - kHeight
kFinTipZ = z_side(kFinTipLePx[1])                 # 2.884 m ueber der Triebwerksachse
kGroundZ = kFinTipZ - kHeight                     # -1.936 m
kTireMainDia = 0.830                     # [WEB] Hauptrad 830 x 225 mm, MiG-23-Bereifung
kTireMainW = 0.225                       # Gegenprobe am Riss: Frontansicht 48 px = 0.239 m
kTireNoseDia = 0.520                     # [WEB] Bugrad 520 x 125 mm (Zwillingsrad)
kTireNoseW = 0.125
kTrack = 2.658                           # [WEB] Spurweite MiG-23
kWheelbase = 5.77                        # [SET] Radstand, s. DEFECTS.md #12
kNoseGearY = 3.35                        # [SET] Bugbeinstation vor dem Drehzapfen
kMainGearY = kNoseGearY - kWheelbase     # [DERIVED]
kGearDoorThk = 0.020                     # [SET]


# ================================================================ Duese und Triebwerk

# [BP] Duesenaustritt Seitenriss x = 188 px, 2307..2557 px -> 1.245 m Durchmesser.
kNozzleExitPx = 188.0
kNozzleExitDia = (2557.0 - 2307.0) * kPxM         # 1.245 m
kNozzleThroatDia = 0.86                  # [SET] engster Querschnitt, s. DEFECTS.md #13
kNozzlePetals = 24                       # [SET]
kNozzleDepth = 0.55                      # [SET] Sichttiefe in die Duese


# ================================================================ Anbauten

kGunY = 1.55                             # [SET] Station der GSh-23L unter dem Rumpf
kGunLen = 1.30                           # [SET]
kPylonGloveX = 1.05                      # [SET] Handschuhpylon, seitliche Lage
kPylonGloveY = -0.35                     # [SET]
kPylonFusX = 0.52                        # [SET] Rumpfpylon
kPylonFusY = 1.10                        # [SET]
kPylonChord = 1.55                       # [SET]
kPylonThk = 0.11                         # [SET]
kIrstY = 4.55                            # [SET] TP-23-Waermepeiler unter dem Bug
kAirbrakeY = -2.60                       # [SET] Bremsklappen am Heck
kAirbrakeMaxDeg = 45.0                   # [SET]


# ================================================================ LOD

# [DERIVED] Sehwinkel eines Pixels im Zielbild: doc/render/visual-target.md nennt 1280 px
# ueber 60 Grad horizontalem Sichtfeld.
kPixelAngle = math.radians(60.0) / 1280.0         # 8.181e-4 rad

kLodSegments = (48, 32, 20, 12)          # Umfangsteilung der Rumpfschnitte je Stufe


def ring_error(r, n):
    """Groesster radialer Fehler einer n-Ecke gegenueber dem Kreis mit Radius r."""
    return r * (1.0 - math.cos(math.pi / n))


def ring_radius(r, n):
    """Umkreisradius der n-Ecke mit GLEICHEM UMFANG wie der Kreis r (Cauchy, s. build)."""
    return r * (math.pi / n) / math.sin(math.pi / n)


def ring_balance(n):
    """(Flaechenfehler, Umfangsfehler) der ring_radius-Korrektur bei n Ecken."""
    k = (math.pi / n) / math.sin(math.pi / n)
    area = k * k * (n / 2.0) * math.sin(TAU / n) / math.pi - 1.0
    per = k * n * math.sin(math.pi / n) / math.pi - 1.0
    return area, per
