#!/usr/bin/env python3
"""F-16C **Block 52** — die VERMESSUNG. Nur Zahlen und ihre Herkunft, keine Blender-Abhaengigkeit.

WARUM BLOCK 52 UND NICHT BLOCK 50 (Runde 3, schwerster Befund)
  mods/f16/src/aircraft/f16/f16.xml:245 deklariert <engine file="F100-PW-229"> — Pratt & Whitney,
  also **Block 52**, nicht die GE-getriebene Block 50. CLAUDE.md Prinzip 4: Referenz ist das
  GEFLOGENE Modell. Damit aendern sich zwei silhouettentragende Baugruppen:
      Einlauf   MCID "Grossmaul" 1.40 m  ->  NSI "Kleinmaul"  (s. kInlet*)
      Duese     F110, konisch verjuengt  ->  F100-PW-229, gerade zylindrische Aussenklappen
  Der Riss [KH] traegt genau diesen Titel ("Lockheed Martin F-16C Block 52"); seine Seiten- und
  Draufsichten SIND also die geflogene Variante. Nur seine Untersicht und seine Frontansicht sind
  ausdruecklich als "F-16C Block 50" beschriftet — von dort stammten die falschen Maulmasse.

QUELLEN
  [XML]   mods/f16/src/aircraft/f16/f16.xml — das GEFLOGENE Modell. Nach Prinzip 1 ist sein
          <ground_reactions>-Block byteidentisch mit vendor/jsbsim und damit akzeptiert; die
          Fahrwerksgeometrie des Netzes hat sich IHM zu beugen, nicht der T.O.-Karte.
          Registriert wird auf den dort deklarierten VRP (-180, 0, 0) in.
  [TO]    USAF T.O. 00-105E-9, Blatt "F-16 AIRCRAFT DIMENSIONS" (Blatt 1) und "AIRCRAFT HAZARDS
          AND ACCESS PANELS" (Blatt 3, Fanghaken / Streuwerfer / JFS).
          [WEB https://schultzairshows.com/wp-content/uploads/2020/05/usaf-f-16-emergency-extraction-card.pdf]
          ACHTUNG TOLERANZ: die Karte quantisiert auf 0.1 ft = 30.5 mm. Auf der 7.8-ft-Spur sind
          das 0.39 %. Die 0.5-%-Toleranz des Baums kann diese Quelle bei kleinen Massen nicht
          tragen — fuer [TO]-Zahlen gilt ihre eigene Aufloesung kToQuantum als Grenze.
  [NASA]  NASA TP-1538 (Nguyen et al., 12/1979), TABLE I, Seite 49 des PDF, im Bild gelesen:
          Span 9.144 m | Area 27.87 m2 | MAC 3.45 m | Bezugsschwerpunkt 0.35 c |
          Ausschlaege: HT +-25 (diff +-5.375), Flaperon +-21.5, Ruder +-30, LEF 25, Bremsklappe 60.
          [WEB https://ntrs.nasa.gov/api/citations/19800005879/downloads/19800005879.pdf]
  [KH]    Dreiseitenriss A. W. Chaustow, "Awiazija i Wremja" 3/1999, "Lockheed Martin F-16C
          Block 52", 4458x3160 px, mit acht bemassten Rumpfspanten A-A ... З-З.
          [WEB https://drawingdatabase.com/lockheed-martin-f-16c-block-50/]
          Alle [KH]-Zahlen sind mit numpy aus dem Bild gelesen, nicht geschaetzt.
  [WEB]   einzeln an der Zahl.
  [SET]   von mir gesetzt, weil keine Quelle es hergibt.

KOORDINATEN (Blender): +X rechts, +Y vorwaerts, +Z oben, 1 Einheit = 1 m.
  Stationskoordinate s = Meter HINTER der Radomspitze (nicht hinter der Pitotspitze).
  z = 0 ist die Fluegelsehnenebene = die Wasserlinie der Spantrisse [KH].
  Modellnullpunkt: der VRP aus [XML]. Blender-Y = kS0 - s.
"""

import math

M_PER_IN = 0.0254
FT = 0.3048

# ================================================================ Massstab und Toleranz

# Massstabsbalken des Risses: 822 px = 5 m  ->  0.0060827 m/px.  [KH, gemessen]
# Runde 2 hatte stattdessen auf "49.5 FEET" der T.O.-Karte kalibriert (2477 px = 15.0876 m ->
# 0.0060911 m/px) und den Riss damit um +0.14 % gestreckt. Der Balken ist die genauere Messung:
# 2477 px * 0.0060827 = 15.0669 m, und das trifft die amtliche Laenge 49 ft 5 in = 15.0622 m auf
# 0.03 %. Jede in Runde 2 gemessene [KH]-Zahl traegt deshalb den Korrekturfaktor kKh.
kPx = 0.0060827
_kPxRound2 = 49.5 * FT / 2477.0        # 0.00609107 m/px
kKh = kPx / _kPxRound2                 # 0.9986213


def _kh(v):
    """Eine in Runde 2 mit dem alten Massstab gemessene [KH]-Laenge auf den Balken umrechnen."""
    return v * kKh


def _khg(px):
    """Spalte der FAHRWERKS-Seitenansicht (Blatt rechts oben, Fahrwerk aus, Haube offen) -> s.

    Registrierung: Radomspitze bei x = 2040 px, zwei unabhaengige Belege —
      (a) Kreuzkorrelation der Spalten-Tintenprofile beider Seitenansichten ueber 2540 Spalten,
          Maximum bei +4 px gegen die saubere Ansicht, deren Spitze aus sechs Schnittmarken bei
          2036 +-2.5 px steht  ->  2040 +-3 px (+-18 mm);
      (b) hinterster Zeichnungspunkt bei x = 4437..4440 px gegen 2040 + kFinTeS/kPx = 4436.4 px.
    """
    return (px - 2040.0) * kPx


# Amtliche Gesamtlaenge. Drei publizierte Werte, Streuung 0.06 m = 0.4 %:
#   49 ft 5 in = 15.0622 m  [WEB https://en.wikipedia.org/wiki/General_Dynamics_F-16_Fighting_Falcon]
#   49 ft 4 in = 15.0368 m  [WEB https://www.milavia.net/aircraft/f-16/f-16_specs.htm]
#   49.5 ft    = 15.0876 m  [TO Blatt 1] — dieselbe Zahl auf 0.1 ft quantisiert.
# Der Massstabsbalken des Risses entscheidet zugunsten von 49 ft 5 in (0.03 %).
kLength = 49.0 * FT + 5.0 * M_PER_IN   # 15.06220 m  Pitotspitze -> Seitenleitwerks-Hinterkante
kToQuantum = 0.1 * FT                  # 0.03048 m — Aufloesung und damit Toleranzboden aller [TO]

# ================================================================ amtliche Aussenmasse [TO Blatt 1]
kSpan = 31.0 * FT              #  9.4488 m  ueber die Fluegelspitzen-Startschienen, ohne FK
kSpanMissiles = 32.8 * FT      #  9.9974 m  ueber die Flossen der Spitzen-FK
kTailSpan = 18.3 * FT          #  5.5778 m  Hoehenleitwerksspannweite
kHeightTO = 16.7 * FT          #  5.0902 m  auf dem Fahrwerk (Kontrollwert, s. kGroundZ)

# ================================================================ Bezugstragwerk [NASA Tab.I]
kSpanRef = 9.144               # m  Trapezspannweite (BL 180 je Seite)
kWingArea = 27.87              # m2
kMac = 3.45                    # m
kMacFraction = 0.35            # Bezugsschwerpunkt

# Das Trapez ist durch die drei amtlichen Zahlen VOLLSTAENDIG bestimmt (Hinterkante ungepfeilt):
#   c_r + c_t = 2S/b ;  c_r^2 + c_r c_t + c_t^2 = 1.5 * MAC * (c_r+c_t)
_sum = 2.0 * kWingArea / kSpanRef
_prod = _sum ** 2 - 1.5 * kMac * _sum
_disc = math.sqrt(_sum ** 2 - 4.0 * _prod)
kWingRootChord = (_sum + _disc) / 2.0        # 4.96537 m
kWingTipChord = (_sum - _disc) / 2.0         # 1.13043 m
kWingTaper = kWingTipChord / kWingRootChord  # 0.22766
kSweepLE = math.degrees(math.atan((kWingRootChord - kWingTipChord) / (kSpanRef / 2.0)))
#   = 39.986 deg gegen "LEADING EDGE SWEEP 40 DEGREES" [TO] — 0.03 %. Die Pfeilung ist damit die
#   PROBE darauf, dass Flaeche/Spannweite/MAC zueinander passen, keine Eingabe.
# Gegenprobe im Grundriss [KH, gemessen]: Wurzeltiefe auf die Mittellinie verlaengert = 815.3 px
#   = 4.9591 m gegen 4.96537 m aus [NASA] — 0.13 %.
kMacSpanY = (kSpanRef / 6.0) * (1.0 + 2.0 * kWingTaper) / (1.0 + kWingTaper)   # 1.80663 m
kTcRoot = 0.04                 # NACA 64A-204, 4 % Dicke  [WEB https://en.wikipedia.org/wiki/General_Dynamics_F-16_Fighting_Falcon]
kAirfoilXt = 0.40              # Dickenruecklage der 64A-Reihe [WEB ebenda]

kProbeLen = _kh(80.0 * _kPxRound2)         # 0.48662 m Pitotmast [KH: 80 px in Seiten- UND Grundriss]

# Stationen der acht Spantrisse [KH, gemessen]. Gegenprobe Runde 3: die Schnittmarken В Г Д Е Ж З
# liegen im Seitenriss bei x = 2670 / 2875 / 3096 / 3306 / 3462 / 3728 px; mit kPx ergibt jede
# einzeln die Radomspitze bei x = 2037.7 / 2038.4 / 2036.0 / 2036.2 / 2033.1 / 2035.7 px.
# Streuung 5 px = 30 mm — die Stationen sind untereinander auf 0.2 % konsistent.
kSectionStations = {k: _kh(v) for k, v in
                    (("A", 1.240), ("B", 2.281), ("V", 3.846), ("G", 5.089),
                     ("D", 6.447), ("E", 7.724), ("Zh", 8.692), ("Z", 10.294))}

# ================================================================ Laengsstationen [KH, gemessen]
kWingTeS = _kh(10.615)         # = 10.600 m  Fluegelhinterkante, ueber die Spannweite ungepfeilt
#   BELEGT IST HIER DIE STREUUNG, NICHT DIE ZAHL (Runde-4-Befund 10). Vier Messungen derselben
#   Kante in drei Ansichten desselben Blattes ergeben, jeweils auf die Radomspitze der eigenen
#   Ansicht bezogen:  10.455 / 10.554 / 10.639 / 10.639 m  (Kritiker-Messung, Runde 4).
#   Spanne 0.184 m = 1.74 %, Standardabweichung +-0.087 m = +-0.87 %. Die 0.5-%-Toleranz des
#   Baums ist FEINER als die Quelle: der Riss kann diese Kante nicht entscheiden.
#   Der Wert bleibt bei 10.600 m — er liegt im Band und traegt den ganzen Laengsaufbau des
#   Tragwerks (kWingLeRootS, kS0, alle Pylonen). Ihn auf eine der vier Einzelmessungen zu
#   ziehen hiesse, Rauschen fuer Signal zu halten. Was Runde 3 falsch machte, war nicht die
#   Zahl, sondern der Beleg "[KH Grundriss]", der eine Praezision behauptet, die es nicht gibt.
#   Eine Aufloesung braucht eine andere Quelle (Werksriss / FS-Stationsliste), nicht mehr Pixel.
kWingLeRootS = kWingTeS - kWingRootChord           # theoretische Wurzelvorderkante
kWingRootY = 0.66              # m  Halbspannweite der Fluegelwurzelrippe an der Rumpfflanke
#   [KH Grundriss, gemessen: die Hinterkantengerade x=677 px laeuft bis Reihe 920, also
#    |920-811.1| px * kPx = 0.662 m. Runde 2 hatte hier unbelegte 0.74.]
kStrakeTipS = _kh(6.87)        # m  hier laeuft die Strake-Kante in die Fluegelvorderkante
kStrakeTipY = _kh(1.564)       # m  Halbspannweite dieses Schnittpunkts
kHtTeS = _kh(14.168)           # m  Hoehenleitwerks-Hinterkante (ungepfeilt)
kHtSweepLE = 41.44             # deg [KH Grundriss, gemessen: Vorderkante 212 px auf 240 px
#                                Halbspannweite -> atan(0.8833). Runde 2 hatte 39.9 aus einer
#                                Zweipunktmessung; die Regression ueber neun Reihen ist genauer.
#                                Probe: Tiefe bei y=2.622 m rechnerisch 0.955 m, gemessen 0.991 m.]
kHtSemi = kTailSpan / 2.0                          # 2.7889 m
kHtRootChord = kHtTeS - _kh(10.897)                # Vorderkante auf y=0 verlaengert
kHtTipChord = kHtRootChord - kHtSemi * math.tan(math.radians(kHtSweepLE))
kHtPlaneZ = -0.122             # m  Ebene des Hoehenleitwerks unter der Fluegelsehnenebene
#   [KH Seitenriss, gemessen: Wasserlinie bei Reihe 1316.2 px — unabhaengig bestaetigt durch den
#    Kanzelscheitel (+1.151 m -> Reihe 1128, gemessen 1128) und den Rumpfboden bei Spant Е-Е
#    (-1.133 m -> Reihe 1502, gemessen 1502); die HLW-Mittellinie liegt 20 px tiefer.
#    Runde 2 hatte die Zahl unbelegt.]
kFinLeZ0S = _kh(9.669)         # m  Seitenleitwerks-Vorderkante bei z=0
kFinLeSlope = 1.130            # ds/dz der Vorderkante -> 48.5 deg Pfeilung  [KH]
kFinTeZ0S = _kh(12.946)        # m  Hinterkante bei z=0
kFinTipZ = _kh(3.271)          # m  Flossenspitze ueber der Sehnenebene
kFinTeS = kLength - kProbeLen  # 14.5756 m  hinterster Punkt des Flugzeugs
#   Gegenprobe [KH]: der Seitenriss endet bei x=4432 px, mit der Radomspitze bei 2036 px sind das
#   14.574 m — 1.6 mm Abweichung.
kFinTeSlope = (kFinTeS - kFinTeZ0S) / kFinTipZ     # Spitze und Gesamtlaenge fallen exakt zusammen
kNozzleExitS = _kh(14.55)      # m  Duesenaustritt
kVentralS0, kVentralS1 = _kh(10.16), _kh(11.42)    # m  Bauchflossen [KH]
kVentralTipZ = _kh(-1.395)     # m  tiefster Punkt der Bauchflosse [KH]
kVentralCant = 15.0            # deg  Neigung der Bauchflosse aus der Senkrechten nach aussen
#   [KH Frontansicht, gemessen: 0.155 m Querversatz auf 0.578 m Tiefe -> atan = 15.0 deg.
#    Runde 2 hatte den Wert im Baeckerskript ohne Herkunft stehen.]

# Nullpunkt des Modells. Physisch der Punkt 0.35 MAC in der Sehnenebene; er wird ab Runde 3 als
# der VRP aus [XML] GEFUEHRT, weil JSBSim genau diesen Punkt fuer Sichtmodelle publiziert und weil
# die Fahrwerkskontakte relativ zu ihm angegeben sind (s. unten).
kS0 = (kWingLeRootS + kMacSpanY * math.tan(math.radians(kSweepLE))
       + kMacFraction * kMac)                      # 8.3610 m hinter der Radomspitze

# ================================================================ Rumpf: Spantrissen
# (s, a = halbe Rumpfbreite OHNE Strake-Schelf, z_deck = Ruecken, z_bot = Rumpfboden)   [KH]
_FUSE = [
    # s      a      z_deck  z_bot
    (0.000, 0.018, -0.290, -0.310),   # Radomspitze
    (0.150, 0.110, -0.150, -0.370),
    (0.300, 0.164, -0.049, -0.402),
    (0.490, 0.231,  0.020, -0.426),
    (0.730, 0.305,  0.030, -0.451),
    (0.980, 0.365,  0.116, -0.469),
    (1.240, 0.439,  0.219, -0.475),   # Spant А-А
    (1.520, 0.481,  0.292, -0.470),
    (1.830, 0.545,  0.365, -0.463),
    (2.130, 0.560,  0.432, -0.439),
    (2.281, 0.603,  0.470, -0.428),   # Spant Б-Б
    (2.440, 0.603,  0.506, -0.420),   # Windschutzfuss: hier beginnt die Kanzel
    (2.740, 0.652,  0.545, -0.396),
    (3.050, 0.700,  0.585, -0.378),
    (3.350, 0.761,  0.615, -0.365),
    (3.846, 0.731,  0.640, -0.323),   # Spant В-В
    (4.150, 0.847,  0.655, -0.290),   # Einlauflippe (Kanal haengt darunter)
    (4.600, 0.800,  0.668, -0.255),
    (5.089, 0.760,  0.680, -0.225),   # Spant Г-Г
    (5.600, 0.755,  0.692, -0.430),
    (6.000, 0.750,  0.702, -0.760),
    (6.447, 0.750,  0.710, -0.962),   # Spant Д-Д
    (7.000, 0.750,  0.707, -1.005),
    (7.724, 0.752,  0.707, -1.133),   # Spant Е-Е
    (8.100, 0.830,  0.707, -1.170),
    (8.692, 1.150,  0.707, -1.103),   # Spant Ж-Ж: breitester Rumpf
    (9.200, 1.140,  0.740, -0.955),
    (9.600, 1.120,  0.760, -0.902),
    (10.294, 1.110, 0.770, -0.889),   # Spant З-З
    (11.000, 1.105, 0.760, -0.834),
    (11.800, 1.105, 0.720, -0.804),
    (12.600, 1.100, 0.640, -0.755),
    (13.400, 1.070, 0.545, -0.585),
    (14.100, 0.980, 0.420, -0.487),
    (14.450, 0.640, 0.180, -0.300),   # Bootsheck zum Duesenring
]
kFuseStations = [(_kh(s), a, zt, zb) for s, a, zt, zb in _FUSE]

# Querschnittsform (|x/a|^n + |z/b|^n = 1), aus den Spantrissen kleinstquadratisch angepasst.
kFuseShapeTop = [(_kh(s), n) for s, n in
                 ((0.0, 2.10), (1.24, 2.08), (2.28, 2.24), (3.85, 1.86),
                  (6.45, 1.95), (9.00, 2.30), (12.0, 2.55), (15.0, 2.40))]
kFuseShapeBot = [(_kh(s), n) for s, n in
                 ((0.0, 2.10), (1.24, 1.86), (2.28, 2.06), (3.85, 1.56),
                  (6.45, 2.30), (9.00, 2.65), (12.0, 2.75), (15.0, 2.40))]
kFuseChineZ = [(_kh(s), z) for s, z in
               ((0.0, -0.06), (1.24, -0.061), (2.28, -0.037), (3.85, 0.018),
                (5.09, -0.018), (6.45, 0.018), (7.72, -0.030), (8.69, 0.049), (15.0, 0.0))]
kFuseChineSharp = [(_kh(s), v) for s, v in
                   ((0.0, 0.0), (1.6, 0.0), (2.6, 0.35), (3.85, 0.75), (5.09, 0.95),
                    (8.69, 0.95), (10.0, 0.55), (12.0, 0.25), (15.0, 0.0))]

# Strake: duennes Schelf IN der Sehnenebene, Aussenkante aus dem Grundriss [KH, gemessen].
kStrakeEdge = [(_kh(s), _kh(y)) for s, y in
               ((2.60, 0.640), (3.05, 0.700), (3.50, 0.775), (3.90, 0.822), (4.14, 0.847),
                (4.45, 0.883), (4.75, 0.956), (5.06, 1.036), (5.36, 1.127), (5.66, 1.218),
                (5.85, 1.330), (6.10, 1.395), (6.45, 1.400), (6.70, 1.450), (6.87, 1.564))]
kStrakeHalfThick = [(_kh(s), t) for s, t in
                    ((2.60, 0.012), (4.00, 0.030), (5.50, 0.055), (6.87, 0.110))]  # [SET]

# ================================================================ Einlauf — NSI, Block 52
# Die Maulmasse der Runde 2 (1.400 x 0.560 m) stammten aus der Frontansicht und der Untersicht des
# Risses, und BEIDE sind dort ausdruecklich als "F-16C Block 50" beschriftet — also MCID.
# Fuer den geflogenen Block 52 gilt der NSI. Herleitung, Schritt fuer Schritt:
#   1. MCID-Maul amtlich: 4 ft 9 1/2 in breit, 1 ft 9 in hoch = 1.4605 x 0.5334 m.
#      [WEB https://groups.google.com/g/rec.models.scale/c/J157JRKY_LU]
#      Eigene Messung an der Block-50-Frontansicht [KH]: 233 px * kPx = 1.417 m breit (-3.0 %),
#      92 px = 0.560 m hoch (+5.0 %). Die amtliche Zahl gilt, die Messung ist ihre Bestaetigung.
#   2. Die Aenderung NSI->MCID war eine VERBREITERUNG bei gleicher Hoehe: "the intake was
#      enlarged / widened at the opening".
#      [WEB https://www.usaf-sig.org/index.php/references/reference/114-research-material/82-f-16-viper-faq-stuff-you-wanted-to-know-about-the-f-16cd]
#      [WEB https://glomilstrat.blogspot.com/2017/07/the-two-types-of-intakes-on-f-16.html]
#   3. Auslegungs-Luftmassenstrom: NSI 254 lb/s, MCID 270 lb/s.
#      [WEB https://groups.google.com/g/rec.models.scale/c/J157JRKY_LU]
#      Bei gleichem Flugzustand und gleichem Fangstromverhaeltnis ist die Fangflaeche dem
#      korrigierten Massenstrom proportional -> A_NSI/A_MCID = 254/270 = 0.94074.
#   4. Hoehe konstant (Schritt 2) -> die Breite traegt die ganze Flaechenaenderung.
kInletMcidW = 57.5 * M_PER_IN  # 1.46050 m  [WEB, Kontrollwert]
kInletFlowRatio = 254.0 / 270.0                    # 0.94074
kInletOuterW = kInletMcidW * kInletFlowRatio       # 1.37398 m  aeussere Maulbreite NSI
kInletOuterH = 21.0 * M_PER_IN                     # 0.53340 m  aeussere Maulhoehe, beide Varianten
# Eckenschaerfe: der MCID "has a slightly sharper corner at each side and therefore smiles a little
# more"; der NSI hat "a more rounded intake opening".  [WEB ebenda / glomilstrat]
kInletCornerN = 2.80           # Superellipsen-Exponent des NSI-Mauls (MCID war 3.4)  [SET]
# Lichte Weite: der Kanal wird rund; sein Durchmesser ist quellenmaessig getrennt bekannt.
kInletDuctD = 46.5 * M_PER_IN  # 1.18110 m NSI (MCID 50 in)  [WEB rec.models.scale]
kInletInnerW = kInletOuterW * 0.87   # [SET] seitliche Lippendicke, Verhaeltnis aus dem Riss
kInletInnerH = kInletOuterH * 0.686  # [SET] ebenda
# Lippenstation und -unterkante stammen aus dem BLOCK-52-Seitenriss und sind damit bereits NSI:
# die Bauchlinie springt zwischen 4.08 und 4.14 von -0.469 auf -0.895.                 [KH]
kInletLipS = _kh(4.110)        # m
kInletLipZ = _kh(-0.900)       # m  Unterlippe
kInletDuctDepth = 2.60         # m  sichtbare Kanaltiefe bis zur Kruemmung  [SET]
kDiverterGap = _kh(0.044)      # m  Grenzschichtspalt, Spant Г-Г: -0.225 / -0.269  [KH Block 52]
kInletFairingBot = [(_kh(s), _kh(z)) for s, z in
                    ((4.11, -0.900), (5.09, -0.980), (5.60, -1.005), (6.00, -1.005),
                     (6.45, -0.980))]              # [KH Seitenriss Block 52]

# ================================================================ Duese — F100-PW-229, Block 52
# Die -229 traegt WIEDER aeussere Klappen, in Kohlefaser und deshalb schwarz statt metallisch;
# der Satz sitzt genau auf Block 42 und 52.
#   [WEB https://www.usaf-sig.org/index.php/references/reference/114-research-material/82-f-16-viper-faq-stuff-you-wanted-to-know-about-the-f-16cd]
# Sichtbar ist damit ein nahezu ZYLINDRISCHER Mantel mit gerade abgeschnittener Hinterkante — nicht
# die konisch verjuengte, offene Blattkrone der F110. Runde 2 hatte die F110-Form gebaut.
kNozzleShroudR = _kh(0.530)    # m  Mantelradius [KH Heckansicht: 87.5 px * kPx = 0.532 m]
kNozzleExitR = _kh(0.330)      # m  Austritt in Militaerstellung  [SET]
kNozzleStraight = 0.88         # Anteil des Mantels, der zylindrisch bleibt  [SET, P&W-Form]
kNozzlePetals = 15             # aeussere Klappen  [SET] — die Heckansicht des Risses gibt bei
#                                ihrer Scanaufloesung keine eindeutige Winkelordnung her
#                                (Fourier-Analyse liefert 9/13/17 nebeneinander).
# Runde-4-Befund 8: im Baeckerskript standen drei nackte Zahlen (s0=14.05, zc=-0.03, r=0.620).
# Sie sind ersetzt: der Mantel beginnt, wo die Bremsklappen enden (kNozzleShroudS0, ganz unten
# definiert, weil kSpeedbrakeS erst spaeter kommt), sein vorderer Radius und seine Achshoehe
# kommen aus dem Rumpfquerschnitt an dieser Station (build_f16.nozzle, aus fuse_section).

# ================================================================ Kanzel (einsitzig) [KH]
kCanopyFrontS = _kh(2.44)      # m Windschutzfuss
kCanopyBowS = _kh(3.20)        # m Windschutz/Haube-Trennrahmen
kCanopyApexS = _kh(4.08)       # m Scheitel  [KH Seitenriss: hoechste Reihe 1128 bei s=3.92..4.28]
kCanopyApexZ = _kh(1.151)      # m Scheitelhoehe
kCanopyRearS = _kh(6.70)       # m Ende der Kanzelschale
#   [KH Seitenriss, Runde 3 nachgemessen: das Dachprofil faellt vom Scheitel (Reihe 1128) monoton
#    und laeuft ab s=7.0 flach auf der Ruecken-Reihe 1200 aus; das Knie liegt bei s=6.4..7.0.]
kCanopyHalfW = _kh(0.395)      # m [KH Spant В-В]

# WAS SICH BEWEGT UND WAS NICHT (Runde-4-Befund 4). Runde 3 haengte die ganze Schale von 2.44 bis
# 6.70 an EINEN Knoten und nahm damit 0.76 m Festbau mit: den Windschutz und seinen Rahmen.
# Die Fahrwerks-Seitenansicht zeigt die Haube OFFEN, und darin steht der Windschutz still:
#   gemessen im Riss, Radomspitze 2040 px -> die stehenbleibende Struktur reicht von
#   s = 2.43 bis s = 3.06..3.10 und traegt den Trennrahmen; das ist genau kCanopyFrontS ..
#   kCanopyBowS (3.196, Abweichung 0.10 m = die Strichbreite des Rahmens).
# Nach hinten endet die BEWEGLICHE Haube an ihrem Rahmen. Die Suche danach lief zweimal:
#   1. Versuch: eine senkrechte Struktur bei s = 4.60..4.90 — das war der SCHLEUDERSITZ, durch
#      das Glas gesehen. Ein Rahmen und ein Sitz sehen in einer Strichzeichnung gleich aus, wenn
#      man nur EIN Fenster absucht.
#   2. Korrektur: Spalte fuer Spalte den GANZEN Kanzelbereich (s = 3.2 .. 6.9) auf senkrechte
#      Fuellung zwischen Dach und Bruestung abgesucht. Es gibt genau vier Gruppen:
#         s = 3.89..4.21  (43 Spalten)  HUD
#         s = 4.56..4.90  (51 Spalten)  Schleudersitz
#         s = 5.87..5.89  ( 4 Spalten)  SCHMALER Rahmen  <- die Hinterkante der Haube
#         s = 6.20 / 6.31 ( 4 Spalten)  Blechstoesse der Verkleidung
#      Ein Rahmen ist SCHMAL, ein Sitz ist BREIT — das unterscheidet sie.
#   Gegenprobe am Foto (F-16 mit geschlossener Haube, 3/4-Seitenansicht): die Hinterkante der
#   Transparenz liegt rund 1.2 m hinter dem Helm; perspektivisch ist das eine Untergrenze, und
#   der Augenpunkt sitzt bei s = 4.39. 5.88 - 4.39 = 1.49 m.
kCanopyGlassRearS = _kh(5.88)      # 5.8719 m  Hinterkante der beweglichen Haube
kCanopyRearS = _kh(6.70)       # m Ende der KANZELVERKLEIDUNG (fest, hinter dem Haubenrahmen)
#   [KH Seitenriss, Dachprofil in 0.1-m-Schritten gemessen: Scheitel +1.145 m bei s=3.90..4.20
#    (Modell 1.1494 bei 4.074 — 4 mm / 0.02 m), danach monoton fallend auf +0.762 m bei s=6.70,
#    wo es in die Ruecken-Linie laeuft. Die Verkleidung setzt das Haubenprofil ohne Absatz fort.]
kCanopyHingeS = kCanopyGlassRearS + 0.08           # 4.9628 m  [SET-Abstand, Station gemessen]
kCanopyHingeZ = _kh(0.700)     # m Schwellenhoehe dort [KH Seitenriss]
kCanopyOpenDeg = 30.0          # deg  voller Oeffnungswinkel
#   HERGELEITET, nicht gesetzt: Hough-Transformation ueber die untere Haubenkante der
#   Offen-Ansicht (Fenster x 2500..2790, y 255..430, 290 Spalten, Konzentrationsmass sum(h^2))
#   liefert 27.90 +-0.15 deg unter der Waagerechten, mit drei parallelen Linien bei c = -1022
#   (Aussenkante), -1044 und -1149. Die GESCHLOSSENE Bruestung steigt nach hinten um
#   atan(0.110/3.50) = 1.80 deg (Rumpfrueckenlinie zwischen s=3.2 und s=6.7).
#   Drehung = 27.90 + 1.80 = 29.7 deg, gerundet 30.0.
#   WAS DIE OFFEN-ANSICHT NICHT HERGIBT, ehrlich benannt: die gezeichnete Haubenkante ist
#   2.586 m lang (Lauf x = 2470..2791 px entlang der 27.9-deg-Geraden), der Abstand Trennrahmen
#   -> Drehpunkt betraegt 2.76 m — 6 % Unterschied, das traegt die Zeichnung. Die VORDERKANTE
#   liegt dort aber bei s = 2.58, waehrend eine reine Drehung sie auf s = 3.57 bringt. Ein
#   Meter Rest. Zwei Erklaerungen sind moeglich (die Haube faehrt beim Entriegeln zusaetzlich
#   nach vorn-oben aus; oder die Beiskizze ist nicht massstaeblich), und der Riss entscheidet
#   nicht zwischen ihnen. Ein zweiter Freiheitsgrad ohne Quelle wird NICHT gebaut. Was die
#   Ansicht dagegen eindeutig zeigt und was hier zaehlt: der WINDSCHUTZ steht still, mitsamt
#   seinem Trennrahmen und dem darauf sitzenden Rueckspiegel.

# Cockpitoeffnung: die Haut MUSS hier ein Loch haben, sonst oeffnet die Haube auf geschlossenes
# Blech (Runde-3-Befund 4). Die Oeffnung laeuft vom Windschutzfuss bis zum Kanzelende.
# Die Kanzelschale (2.44 .. 6.70) ist LAENGER als die Oeffnung: hinter dem Sitz deckt sie
# Struktur ab. Die Oeffnung wird deshalb aus dem Augenpunkt hergeleitet, nicht aus der Schale:
#   vorn  = Windschutz/Haube-Trennrahmen: der WINDSCHUTZ ist feste Struktur, unter ihm sitzt
#           die Instrumententafel, kein Loch. Die Haut oeffnet erst unter der beweglichen Haube.
#   hinten= Augenpunkt + 0.65 m (Lehnenneigung 30 deg auf 0.80 m Lehnenhoehe = 0.40 m Versatz,
#           dazu 0.25 m Kopfstuetze und Rahmen)
kCockpitOpenS0 = kCanopyBowS + 0.05
kCockpitOpenHalfW = 0.92       # Anteil von kCanopyHalfW, den die Oeffnung breit ist  [SET]
kCockpitFloorDrop = 0.93       # m Wannenboden unter dem Augenpunkt  [SET]

# Augenpunkt und Pilotenmasse stehen im geflogenen Modell und verankern Sitz, Tafel und HUD:
#   [XML] EYEPOINT (-336.2, 0, +29.5) in ; Pilot-Punktmasse (-336.2, 0, 0) in ; VRP (-180, 0, 0) in
kEyeAheadVrp = (336.2 - 180.0) * M_PER_IN          # 3.96748 m vor dem Nullpunkt
kEyeZ = 29.5 * M_PER_IN                            # 0.74930 m ueber der Sehnenebene
kEyeS = kS0 - kEyeAheadVrp                         # Station des Augenpunkts
kSeatBackAngle = 30.0          # deg  ACES-II-Lehne, nach hinten geneigt
#   [WEB https://www.f-16.net/f-16_versions.html — "30-degree tilt-back seat"]
kSeatPanZ = kEyeZ - 0.80       # m  sitzende Augenhoehe ueber der Sitzflaeche  [SET]
kSeatBackH = 0.80              # m  Lehnenhoehe  [SET]
kCockpitOpenS1 = min(kEyeS + kSeatBackH * math.sin(math.radians(kSeatBackAngle)) + 0.25,
                     kCanopyGlassRearS - 0.05)   # nie hinter den Haubenrahmen

# ================================================================ Fahrwerk
#
# DIE REGISTRIERUNGSREGEL, EINMAL UND FUER ALLE (Runde-4-Befund 1).
#   Runde 3 hatte hier "[XML] ist massgeblich" geschrieben und die Regel dann selektiv angewandt:
#   bei NOSE_LG galt das XML, bei TOP_VS hiess dasselbe XML "ein Aufschlagpunkt, keine Formangabe".
#   Das ist kein Kriterium, sondern eine Ausrede. Die Regel lautet ab jetzt:
#
#       [XML] bestimmt, was die Simulation RECHNET.  [KH]/[TO] bestimmen, wie das Netz AUSSIEHT.
#       Wo sie sich widersprechen, folgt das Netz dem Riss, und der Widerspruch wird im
#       Sidecar als benannter Delta gefuehrt.
#
#   Sie verletzt Prinzip 1 nicht: Prinzip 1 schuetzt das FLUGMODELL (mods/f16/src/aircraft/), und
#   kein Dreieck dieses Netzes geht in eine Kraft ein. Der einzige sichtbare Preis ist, dass ein
#   gezeichnetes Rad nicht exakt auf dem gerechneten Kontaktpunkt steht; bei der statischen
#   Bodenlage (Nickwinkel ~0) ist der Hoehenfehler 0.503 m * tan(0) = 0, bei 10 deg Rotation
#   0.503 * tan(10 deg) = 89 mm, und da ist das Bugrad ohnehin in der Luft. Der Preis der
#   Gegenentscheidung waere ein dauerhaft um 0.50 m falsch stehendes Bugbein in jedem Bild.
#
# MESSUNG AM RISS. Der Fahrwerksriss ist die Seitenansicht MIT ausgefahrenem Fahrwerk (Blatt
# rechts oben, dieselbe Zeichnung, die auch die offene Haube zeigt).
#   Registrierung: Radomspitze bei x = 2040 px. Zwei unabhaengige Belege:
#     (a) Kreuzkorrelation der Spalten-Tintenprofile beider Seitenansichten ueber 2540 Spalten,
#         Maximum bei +4 px gegen die saubere Ansicht, deren Spitze aus sechs Schnittmarken bei
#         2036 +-2.5 px steht  ->  2040 +-3 px  (+-18 mm).
#     (b) hinterster Zeichnungspunkt bei x = 4437..4440 px gegen 2040 + kFinTeS/kPx = 4436.4 px.
#   Reifenmitten aus den Aussenkanten der Reifenkreise (Zeilenlaeufe, nicht geschaetzt):
#     Bugrad   links 2791..2793 / rechts 2872..2874  ->  Mitte 2832.5 px
#     Hauptrad links 3434..3436 / rechts 3545..3547  ->  Mitte 3490.5 px, Boden bei Zeile 866.5,
#              Mittenzeile 810.5 = 866.5 - 55.5 (in sich konsistent)
#   Die Umrechnung px -> s macht _khg (ganz oben, mit seinen Belegen).
kMainGearS = _khg(3490.5)      # 8.82294 m  [KH Fahrwerksriss]
kNoseGearS = _khg(2832.5)      # 4.82054 m  [KH Fahrwerksriss]
kWheelBase = kMainGearS - kNoseGearS                # 4.00241 m
#   Gegenprobe [TO] 13.1 ft = 3.99288 m -> 9.5 mm = 0.24 %, innerhalb kToQuantum. ZWEI Quellen.
#   [XML] NOSE_LG x=-299.6 in / MLG x=-158.6 in ergaebe 3.58140 m, also -10.5 %; der Fehler sitzt
#   VOLLSTAENDIG im Bugbein (das XML setzt es 0.503 m zu weit hinten, das Hauptbein trifft den
#   Riss auf 82 mm). Nach der Regel oben gilt fuers Netz der Riss.
kGearNoseAheadVrp = kS0 - kNoseGearS               # 3.54046 m  (Netz; [XML] 3.03784 m)
kGearMainBehindVrp = kMainGearS - kS0              # 0.46194 m  (Netz; [XML] 0.54356 m)
kWheelTrack = 2.0 * 48.0 * M_PER_IN                # 2.43840 m  [XML] LEFT/RIGHT_MLG y=+-48 in
#   Und hier bestaetigt der Riss das XML statt es zu widerlegen: in der Frontansicht liegen die
#   Aussenkanten der Hauptreifen bei x = 567.5 / 602.5 und 968.75 / 1003.75 px, Mitten 585.0 und
#   986.25 -> 401.25 px = 2.4407 m, also 0.09 % vom XML. [TO] 7.8 ft = 2.3774 m liegt 2.6 %
#   daneben und ist mit kToQuantum = 30.5 mm NICHT vereinbar; die Karte irrt hier, nicht das XML.
#   Das ist die Probe darauf, dass die Regel oben kein Rosinenpicken ist: sie entscheidet den
#   Radstand GEGEN das XML und die Spur FUER das XML, weil der Riss es so misst.
kGroundZ = -71.6 * M_PER_IN                        # -1.81864 m  Aufstandsebene = z der MLG-Kontakte
#   Beide Raeder stehen auf DERSELBEN Ebene (Runde-4-Befund 11). Die 0.4 in Unterschied zwischen
#   NOSE_LG (z=-72.0) und MLG (z=-71.6) im XML sind die statische Federvorspannung des Bugbeins,
#   keine Formangabe: JSBSim rechnet aus <contact> die UNBELASTETE Radaufstandshoehe, und ein
#   ungleicher Wert erzeugt genau den Standnickwinkel, den das Modell haben soll. Ein Netz, das
#   sie nachbaut, stellt das Bugrad 10.2 mm unter die Piste.
# Probe: Flossenspitze kFinTipZ ueber kGroundZ = 5.0851 m gegen [TO] 16.7 ft = 5.0902 m — 5.1 mm,
# also 0.10 % und deutlich innerhalb kToQuantum. Die T.O.-Hoehe und der XML-Boden sind DIESELBE
# Ebene. (Der STRUCTURE-Kontakt TOP_VS liegt bei +123.2 in = 3.129 m und damit 0.142 m unter der
# Flossenspitze des Risses; nach der Regel oben gilt fuers Netz der Riss, und TOP_VS bleibt
# unbenutzt — dieselbe Entscheidung wie beim Radstand, aus demselben Grund.)
kTireNose = 0.4699             # m Durchmesser 18x5.5-8   [WEB https://www.f-16.net/f-16_versions.html]
kTireMain = 0.6477             # m Durchmesser 25.5x8.0-14
kTireNoseW = 0.140             # m Breite 5.5 in
kTireMainW = 0.203             # m Breite 8.0 in

# Kinematik (Runde-3-Befund 3): EIN Scharnier reicht nicht.
#   Bugfahrwerk faehrt NACH HINTEN ein — "the gear retracts aft into the wheel well, rotating
#   about its trunnion pins"; der Schacht liegt hinter dem Einlauf.
#   [WEB https://www.baseops.net/wp-content/uploads/2015/08/Section2.pdf]
#   Hauptfahrwerk faehrt NACH VORN ein UND legt das Rad flach: "the main gear wheel can lie flat
#   against the fuselage when the gear is retracted, accomplished by the linkage between the bottom
#   of the strut and the drag link."  [WEB https://www.f-16.net/forum/viewtopic.php?f=23&t=12675]
#   Daraus zwei Knoten je Hauptbein: gear.main.X (Bein, Querachse) und gear.main.X.knuckle
#   (Rad um die Beinlaengsachse). Die Betraege sind im Baeckerskript nachgerechnet, nicht geraten.
#   Der BETRAG des Bugeinzugs steht nicht mehr hier: er ist die Loesung einer Forderung und wird
#   in build_f16.nose_bay() gerechnet (tiefster Radpunkt 20 mm ueber der Verkleidungsunterkante).
#   Runde 3 hatte -92 deg gesetzt; das Rad landete damit 0.6 m hinter seiner Klappe im Einlauf.
kGearMainSweep = 84.0          # deg  Einzug nach VORN    [SET, geprueft: Rad im Bauch]
kGearMainRoll = 90.0           # deg  Rad flach in den Bauch  [WEB f-16.net, s.o.]

# ================================================================ Block-50/52-Kennzeichen
kGunPortS = _kh(6.25)          # m [KH Seitenriss: Muendungsverkleidung 6.1..6.4]
kGunPortY = _kh(0.62)          # m Abstand von der Mittellinie [KH Grundriss]
kGunPortZ = _kh(0.30)          # m [KH Seitenriss]
kAlq213S = _kh(5.55)           # m [SET, an der gezeichneten Verkleidung]
kHtsS = _kh(5.10)              # m HTS-Aufnahme (AN/ASQ-213) rechts  [KH Beschriftung]
kHtsY = _kh(0.58)
kHtsZ = _kh(-0.62)
kBladeDorsalS = _kh(9.38)      # m UHF-Blattantenne [KH, gemessen]
kBladeDorsalH = _kh(0.205)     # m Hoehe ueber dem Ruecken
kBladeVentralS = (_kh(2.15), _kh(3.35))            # m IFF-Blaetter [KH Untersicht]

# ================================================================ Heckrumpf [TO Blatt 3]
# Blatt 3 beschriftet Fanghaken, Streuwerfer, JFS-Einlauf und -Auslass. Seine Zeichnung ist
# laengs NICHT massstaeblich (die Kanzel misst dort nur 1.6 m), deshalb wird der HECKBEREICH
# lokal zwischen zwei dort eindeutigen Marken kalibriert:
#     Hauptradmitte  x = 2000 px  <->  s = kMainGearS  (Runde 3 hatte hier 9.35 m stehen; die
#                                      Radmitte ist jetzt am Riss gemessen und liegt bei 8.823 m,
#                                      die Heckstationen ruecken dadurch um bis zu 0.15 m vor)
#     Flossen-HK     x = 3021 px  <->  s = kFinTeS
# -> 0.00563 m/px im Heck.  Alle vier Stationen unten sind so gelesen; die roten Marken der Karte
# wurden mit einer Zusammenhangsanalyse (numpy) gefunden, nicht abgeschaetzt.
def _to3(x):
    return kMainGearS + (x - 2000.0) * (kFinTeS - kMainGearS) / 1021.0


kJfsInletS = (_to3(2012), _to3(2042))              # 9.41 .. 9.57 m, linke untere Flanke
kJfsExhaustS = (_to3(2132), _to3(2144))            # 10.03 .. 10.09 m, linke untere Flanke
kDispenserS = (_to3(2480), _to3(2580))             # 11.81 .. 12.32 m, Flanke unter der HLW-Wurzel
kHookS = (_to3(2482), _to3(2740))                  # 11.82 .. 13.14 m, Bauchmittellinie
# Streuwerfer-Zaehlung: Blatt 3 markiert LINKS drei Felder, RECHTS eines. Block 25 trug "just the
# 2 bottom fuselage chaff/flare buckets", ab FY87 Block 30/32 kamen "2 extra buckets" dazu.
#   [WEB https://www.usaf-sig.org/index.php/references/reference/114-research-material/82-f-16-viper-faq-stuff-you-wanted-to-know-about-the-f-16cd]
kDispenserCount = (3, 1)       # (links, rechts)  [TO Blatt 3, gezaehlt]
# UARRSI: Betankungsklappe auf dem Ruecken hinter der Kanzel.
kUarrsiS = (_kh(6.95), _kh(7.55))                  # [KH Grundriss: Klappe hinter der Haube]
kUarrsiHalfW = _kh(0.21)

# ================================================================ Ruderflaechen — [KH Grundriss]
# Runde-3-Befund 8: diese Zahlen standen ohne Herkunft im Baeckerskript. Sie sind jetzt
# MASSSTABSFREI als Tiefenanteile aus dem Grundriss gelesen (Vorderkante, Hinterkante und
# Klappenlinie in derselben Bildreihe), damit kein Kalibrierfehler in sie eingeht.
#
# Vorderkantenklappe: ihre Hinterkante ist eine GERADE mit anderer Pfeilung als die Fluegel-VK,
# der Tiefenanteil waechst deshalb nach aussen. Zwei gemessene Stuetzstellen:
#   Reihe 1200 (y=2.3656 m): VK 1172 px, Klappenlinie 1090 px, HK 677 px -> 82/495 = 0.1657
#   Reihe 1520 (y=4.3122 m): VK  911 px, Klappenlinie  864 px, HK 677 px -> 47/234 = 0.2009
kLefHinge = ((2.3656, 0.1657), (4.3122, 0.2009))   # (Halbspannweite m, Tiefenanteil ab VK)
kLefY = (1.60, 0.985)          # Wurzel in m [KH: die Klappe beginnt an der Strake-Wurzel
#                                y=1.564 m], Spitze als Anteil der Halbspannweite  [SET]
# Flaperon: konstanter Tiefenanteil, weil VK und HK beide gerade sind. Der Riss zeichnet ZWEI
# Spaltlinien; das Scharnier liegt auf ihrer Mitte.
#   Reihe 1200: 760 px -> 0.1677 und 769 px -> 0.1859 der Resttiefe
#   Reihe 1400: 733 px -> 0.1672 und 741 px -> 0.1910
kFlaperonChord = 0.1775        # Resttiefe -> Scharnier bei 0.8225 c
kFlaperonY = (1.09, 3.70)      # m Halbspannweite [KH: die Spaltlinie existiert von Reihe 1000
#                                (y=1.149) bis Reihe 1400 (y=3.582); Enden auf die letzte
#                                Reihe OHNE Linie halbiert]
kHtHingeChord = 0.62           # Anteil der HLW-Wurzeltiefe vor der Hinterkante  [SET]
#   Diesen Drehpunkt FAEHRT die Simulation, er ist damit die wichtigste ungestuetzte Zahl im
#   Modell. Der Riss zeichnet die Drehachse nicht; sie bleibt [SET], bis eine Quelle sie hergibt.
kSpeedbrakeS = (_kh(13.05), _kh(14.05))            # m [KH Seitenriss: die vier Blaetter]
kSpeedbrakeInner = 0.20        # m Innenkante von der Mittellinie  [KH Heckansicht]
kSpeedbrakeOuter = 0.98        # m Aussenkante  [KH Heckansicht]

# ================================================================ abgeleitete Stationen
kNozzleShroudS0 = kSpeedbrakeS[1]                  # 14.0306 m
#   DEFINITION statt Eingabe: der Duesenmantel beginnt dort, wo die vier Bremsklappenblaetter
#   enden — davor ist Rumpf, dahinter Mantel. Beide Kanten sind im Riss dieselbe Linie.
#   (Runde 3 hatte hier 14.05 ohne Herkunft; die Definition trifft das auf 19 mm.)
kInletFairingEndS = kInletFairingBot[-1][0]        # 6.4411 m
#   DEFINITION statt Eingabe: die Einlaufverkleidung endet, wo ihre gemessene Unterkante
#   (kInletFairingBot) endet und in die Rumpfbauchlinie laeuft — im Riss faellt beides auf
#   0.018 m zusammen (-0.980 gegen -0.962 bei s=6.447). (Runde 3 hatte hier 6.60 ohne Herkunft.)

# ================================================================ AIM-9M an der Fluegelspitze
# Runde 3 hatte "2.85 m x 127 mm x 0.63 m" mit der af.mil-Fact-Sheet belegt; die Fact-Sheet nennt
# aber die BAUREIHE (9 ft 11 in). Die variantenrichtigen Zahlen stehen in der Varianten-Tabelle:
#   [WEB https://en.wikipedia.org/wiki/AIM-9_Sidewinder — "All-aspect variants": AIM-9L/M
#    Laenge 2.89 m, Spannweite 0.64 m, Masse 86 kg; Zellendurchmesser 5 in = 127.0 mm]
kAim9Len = 2.89                # m
kAim9BodyD = 5.0 * M_PER_IN    # 0.12700 m
kAim9FinSpan = 0.64            # m  Heckfluegel, Spitze zu Spitze
kAim9CanardSpan = 0.48         # m  vordere Steuerflaechen  [SET] — die Quelle bemasst nur die
#                                Heckfluegel; 0.75 davon ist am Werksfoto abgeschaetzt, nicht
#                                gemessen, und traegt deshalb keine Toleranzaussage.
kAim9RollDeg = 45.0            # deg  Kreuz in X-Stellung, NICHT in Plus-Stellung
#   HERGELEITET aus der Aufhaengung: die LAU-129 traegt den FK ueber zwei Schuhe auf seinem
#   RUECKEN. In Plus-Stellung zeigt dort eine Flosse senkrecht nach oben und der Schuh haette
#   keinen Platz; in X-Stellung liegt der Ruecken frei zwischen den beiden oberen Flossen.
#   FOLGE, offen benannt (Runde-4-Befund 7): die groesste Breite ueber die Spitzen-FK wird damit
#       2 * (kSpan/2 - 0.075 + kAim9FinSpan/2 * cos 45 deg) = 9.7514 m
#   gegen [TO] 32.8 ft = 9.9974 m, also -2.46 %. Die [TO]-Zahl braucht Plus-Stellung (9.939 m)
#   UND die FK-Achse auf der Schienen-Aussenflaeche (9.989 m) — beides widerspricht der
#   Aufhaengung. Der Riss kann nicht entscheiden: seine Frontansicht ist am linken Blattrand
#   abgeschnitten, die Fluegelspitze fehlt. Das Netz baut die Mechanik nach und TRAEGT die
#   Abweichung; sie steht als benannter Delta im Sidecar. Runde 3 hatte 9.9974 als Referenz
#   publiziert und nie mit der eigenen Geometrie (9.728 m) verrechnet — das war der Fehler,
#   nicht der Flossenwinkel.

# ================================================================ Ausschlagsgrenzen [NASA Tab.I]
kLimits = {
    "ctl.aileron":     (-21.5, 21.5),
    "ctl.elevon":      (-25.0, 25.0),
    "ctl.elevon.diff":  (-5.375, 5.375),
    "ctl.rudder":      (-30.0, 30.0),
    "ctl.lef":         (-2.0, 25.0),     # untere Grenze [SET]; 25 deg abwaerts [NASA]
    "ctl.speedbrake":   (0.0, 60.0),
}
