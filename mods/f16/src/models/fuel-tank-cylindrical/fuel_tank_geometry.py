#!/usr/bin/env python3
"""fuel-tank-cylindrical — die VERMESSUNG. Nur Zahlen und ihre Herkunft, keine Blender-Abhaengigkeit.

    ALLES METRISCH, DEZIMALSYSTEM. Eignerentscheid 2026-08-05, projektweit.
    Jede Tabelle aus API 650 wird aus ihrer SI-SPALTE gelesen, nie aus der US-Spalte,
    nie umgerechnet. Wer hier eine Zollzahl findet, hat einen Defekt gefunden.

WARUM DIESE ZEILE GANZ OBEN STEHT. API 650 fuehrt ZWEI Regelsaetze, und sie sind nicht deckungs-
gleich: die Tabelle zu 5.6.1.1 nennt fuer D < 15 m "5 mm" UND "3/16 in", und 3/16 in sind 4.7625 mm.
Das ist keine Umrechnung, das sind zwei Zahlen. Die Norm sagt es selbst (Kasten vor 5.4):
    "When 6 mm (1/4 in.) thick material is specified, 0.236 in. thick material may be used in the
     US Customary rule set ... Similarly when 5 mm (3/16 in.) thick material is specified, 4.8 mm
     thick material may be used in the SI rule set with Purchaser approval."
Runde 2 baute die Schale in Zoll und den Rest in SI. Die Folge war kein Rundungsfehler, sondern ein
BAUTEIL, das es nicht gibt: in Zoll fordert 5.9.7 einen Zwischen-Windring, in SI nicht (s. unten).

WAS DAS IST. Ein oberirdischer, senkrechter, geschweisster Lagertank mit getragenem Kegeldach nach
API Std 650 — die Bauform, die auf Flugplaetzen und Militaerstuetzpunkten als Treibstofflager steht.
doc/asset-inventory.md #1: das EINZIGE Objekt, das alle vier Titel platzieren.

QUELLEN
  [API]   API Standard 650, 11. Auflage (2007), oeffentlich als Incorporated-by-Reference-Kopie:
          [WEB https://law.resource.org/pub/us/cfr/ibr/002/api.650.2007.pdf]
          Jede [API]-Zahl nennt ihre Klausel- oder Tabellennummer und ist aus dieser Datei GELESEN.
          Bilder werden GERENDERT und abgelesen (pdftoppm -r 170), nicht aus dem Textstrom gefischt.
  [SIZE]  Normgroessentabelle fuer API-Tanks (Nenninhalt in bbl, Durchmesser x Hoehe in ft).
          [WEB https://www.piping-designer.com/index.php/disciplines/mechanical/stationary-equipment/88-tank/1527-api-tank-size]
          Die einzige Stelle, an der Fuss und Barrel vorkommen — es ist der KATALOG, aus dem der
          Tank bestellt ist, nicht seine Bemessung. Durchmesser und Hoehe werden EINMAL nach Meter
          gewandelt, danach ist Schluss damit.
  [OSHA]  29 CFR 1910.25, Standard stairs. [WEB https://www.osha.gov/laws-regs/regulations/standardnumber/1910/1910.25]
          API 650 5.8.10 a verweist ausdruecklich auf OSHA 29 CFR 1910 Subpart D.
  [SET]   von mir gesetzt, weil keine Quelle es hergibt. Jede solche Zahl steht in DEFECTS.md.

KOORDINATEN (Blender): +X rechts, +Y vorwaerts, +Z oben, 1 Einheit = 1 m.
  z = 0 ist das UMGEBENDE GELAENDE, nicht die Tankunterkante — der Tank steht auf einem
  Ringfundament, das aus dem Boden ragt. Ein Platzierer setzt den Nullpunkt auf das Terrain.
  Die Tankachse ist die z-Achse.
"""

import math

TAU = 2.0 * math.pi
_FT = 0.3048                             # NUR fuer die Katalogzeile [SIZE], sonst nirgends
_BBL = 42.0 * 231.0 * 0.0254 ** 3        # 0.158987294928 m^3, exakt (petroleum barrel)


# ================================================================ Hauptmasse

# [SIZE] Zeile "10,310 bbl | 48'-0" | 32'-0"" — der Katalogeintrag, aus dem der Tank bestellt ist.
kCapacityBbl = 10310.0
kDiameter = 48.0 * _FT                  # 14.6304 m
kShellHeight = 32.0 * _FT               # 9.7536 m
kRadius = kDiameter / 2.0               # 7.3152 m

# [DERIVED] Probe auf die Tabelle: das Zylindervolumen der Zeile gegen ihren Nenninhalt.
kVolume = math.pi / 4.0 * kDiameter ** 2 * kShellHeight          # 1639.71 m^3
kCapacityCheckBbl = kVolume / _BBL                               # 10 313.5 bbl
kCapacityErrRel = kCapacityCheckBbl / kCapacityBbl - 1.0         # +3.37e-4

# [DERIVED] Zahl der Schuesse. [API 5.6.1.2] fordert Schalenbleche von mindestens 1800 mm
# Nennbreite. Die Normhoehen der Tabelle [SIZE] sind 16 / 24 / 32 / 40 / 48 ft — alle Vielfache von
# 8 ft = 2438.4 mm, der kleinsten Walzbreite ueber dieser Grenze, die jede von ihnen ohne Rest
# teilt. Vier Schuesse zu 2.4384 m.
kCourses = 4
kCourseHeight = kShellHeight / kCourses          # 2.4384 m

# [API 5.6.1.1, Tabelle, SI-SPALTE] Nenndurchmesser < 15 m -> Nennblechdicke 5 mm.
# [API 5.6.1.1, FUSSNOTE 4] "For diameters less than 15 m (50 ft) but greater than 3.2 m (10.5 ft),
#   the nominal thickness of the LOWEST shell course shall not be less than 6 mm (1/4 in.)."
# D = 14.6304 m liegt in genau diesem Band, also ist Schuss 1 dicker als die uebrigen.
kShellThk = 0.005                                # Schuesse 2..4
kShellThkBottom = 0.006                          # Schuss 1 [Fussnote 4]
kCourseThk = (kShellThkBottom,) + (kShellThk,) * (kCourses - 1)

# [API 5.6.1.1 Note 1] "the nominal tank diameter shall be the CENTERLINE diameter of the bottom
# shell-course plates". Der Nennradius liegt in der Mitte von Schuss 1. Innen buendig gestossen
# (die 1-Meter-Methode und der Fuellstand beziehen sich auf die Innenwand), also springt die
# AUSSENwand am ersten Rundstoss um 1 mm nach innen.
kShellInnerR = kRadius - kShellThkBottom / 2.0   # 7.3122 m


def shell_outer(course):
    return kShellInnerR + kCourseThk[course]


kShellOuterMax = shell_outer(0)                  # 7.3182 m — dickster Schuss

# [DERIVED] Probe, dass die Mindestdicke massgeblich ist und nicht die Festigkeit.
# 1-Meter-Methode [API 5.6.3.2, SI]:  td = 4.9 D (H - 0.3) G / Sd  [mm, m, m, -, MPa]
# Sd = 160 MPa fuer A 36M [API Tabelle 5-2a, Zeile "A36M": Sy 250, Su 400, Sd 160, St 171].
kAllowStressMPa = 160.0


def _governing_sg(course):
    """Ab welcher Dichte reicht die Mindestdicke des Schusses nicht mehr? H = Hoehe von der
    Unterkante DIESES Schusses bis zur Schalenoberkante [API 5.6.3.2]."""
    h = kShellHeight - course * kCourseHeight
    return kCourseThk[course] * 1000.0 * kAllowStressMPa / (4.9 * kDiameter * (h - 0.3))


kGoverningSg = min(_governing_sg(c) for c in range(kCourses))     # 1.4166, Schuss 1
# Jedes Erdoelprodukt liegt darunter (Jet A-1 ~0.80, Diesel ~0.84), sogar Wasser (1.00), also
# regiert die Mindestdicke fuer JEDEN Inhalt dieses Tanks. Das Netz braucht keine Produktdichte.

# [SET] Senkrechte Stossnaehte. [API 5.1.5.2 b] fordert sie versetzt ("Vertical joints in adjacent
# shell courses shall not be aligned, but shall be offset from each other a minimum distance of
# 5t"), also EXISTIEREN sie; die Norm legt die Blechlaenge nicht fest. Sechs Bleche je Schuss
# ergeben 7.66 m Bogenlaenge, im Bereich handelsueblicher Walzlaengen. Versatz um ein halbes Blech
# zwischen benachbarten Schuessen: 3.83 m gegen die geforderten 5t = 30 mm.
kPlatesPerCourse = 6
kSeamOffsetFrac = 0.5


# ================================================================ Dach

# [API 5.10.4.1] "The slope of the roof shall be 1:16 or greater if specified by the Purchaser."
# Getragenes Kegeldach, flachster zulaessiger Fall = die Regelausfuehrung.
kRoofSlope = 1.0 / 16.0
kRoofRise = kRadius * kRoofSlope                 # 0.45720 m
# [API 5.10.2.2] "Roof plates shall have a nominal thickness of not less than 5 mm (3/16 in.)."
kRoofThk = 0.005

# [API 5.1.5.9 e, Tabelle, SI-SPALTE] Mindest-Kopfwinkel nach Durchmesser:
#   D <= 11 m -> 50x50x5 | 11 m < D <= 18 m -> 50x50x6 | D > 18 m -> 75x75x10  (mm)
# D = 14.6304 m faellt in das mittlere Band.
kTopAngleLeg = 0.050
kTopAngleThk = 0.006


# ================================================================ Kein Zwischen-Windring

# [API 5.2.1 k] "The design wind speed (V) shall be 190 km/hr (120 mph), the 3-sec gust design wind
# speed determined from ASCE 7, Figure 6-1, or the 3-sec gust design wind speed specified by the
# Purchaser". Erster Fall — kein Standort, kein Besteller, also der Normwert der Norm selbst.
# DIESE ZEILE TRAEGT DAS GANZE BAUTEIL. Sie ist der Grund, warum der Windring fehlt, und sie stand
# bis Runde 3 nirgends: der Term (190/V)^2 wurde weggelassen, was fuer V = 190 km/h richtig ist,
# aber die ANNAHME verschwieg, an der es haengt.
kWindSpeedKmh = 190.0

# [API 5.9.7.1, SI] Groesste unverstaerkte Schalenhoehe:
#       H1 = 9.47 t sqrt((t/D)^3) (190/V)^2
#   t in MILLIMETERN, D in METERN, V in km/h, H1 in METERN — die Klausel mischt die Einheiten.
#   t ist die Dicke des DUENNSTEN Schusses [Formelzeichenliste ebenda], hier der oberste.
kWindH1 = (9.47 * (kShellThk * 1000.0) * ((kShellThk * 1000.0) / kDiameter) ** 1.5
           * (190.0 / kWindSpeedKmh) ** 2)                                   # 9.45999 m

# [API 5.9.7.2 a] Transformierte Schale: jeder Schuss auf die Dicke des DUENNSTEN umgerechnet,
#   W_tr = W (t_uniform / t_actual)^2.5
kCourseTransposed = [kCourseHeight * (kShellThk / t) ** 2.5 for t in kCourseThk]
kShellTransposed = sum(kCourseTransposed)        # 8.860995 m

# [API 5.9.7.2 b] "Add the transposed widths of the courses. The sum of the transposed widths of
# the courses will give the height of the transformed shell. If the height of the transformed shell
# is greater than the maximum height H1, an intermediate wind girder is required."
#   8.860995 m < 9.45999 m  ->  KEINER.
# (Der Satz steht in 5.9.7.2 b. 5.9.7.3 regelt erst die LAGE eines geforderten Ringes.)
kWindGirderNeeded = kShellTransposed > kWindH1
# [DERIVED] Wie duenn ist das? H1 faellt mit V^-2, also kehrt der Ring zurueck bei
#   V > 190 sqrt(H1(190) / W_tr) = 196.32 km/h — 3.32 % ueber dem Normwert.
# Die Marge ist klein, die Annahme steht jetzt aber da, und sie ist die der Norm.
kWindSpeedCrit = kWindSpeedKmh * math.sqrt(kWindH1 / kShellTransposed)        # 196.32 km/h
kWindSpeedMargin = kWindSpeedCrit / kWindSpeedKmh - 1.0                      # +3.32 %
# Runde 2 baute einen Ring. Sie hatte in Zoll gerechnet, dort ist die transformierte Schale
# 27.897 ft gegen H1 27.466 ft und der Ring PFLICHT. Das Bauteil existierte also nur als Folge der
# Einheitenmischung. Mit ihm fallen: die Profilwahl aus Tabelle 5-20, die Lagerechnung nach
# 5.9.7.3.1/5.9.7.3.2, das Ausweichen vor der Treppe und die Durchdringung Stufe x Ring.
#
# KEIN assert HIER. Ein assert auf kWindGirderNeeded prueft eine Rechnung gegen sich selbst und
# kann nur fehlschlagen, wenn jemand eine Zahl darueber aendert — er prueft nicht, was GEBAUT
# wurde. Die Aussage "dieser Tank braucht keinen Zwischenring" wird in build_fuel_tank.check_norms
# aus den WANDDICKEN DES GEBAUTEN NETZES nachgerechnet und dort gegen die Koerperliste geprueft.


# ================================================================ Treppe

# [API Tabelle 5-18] Anforderungen an Treppen:
#   2. Mindestbreite der Treppe 710 mm.
#   3. Groesster Winkel zur Waagerechten 50 Grad.
#   4. Mindestbreite der Stufe 200 mm. 2R + r nicht unter 610 mm und nicht ueber 660 mm.
#      Steigungen ueber die ganze Hoehe gleich.
#   6. Hoehe des Handlaufs, senkrecht von der Stufenvorderkante, 760 bis 860 mm.
#   7. Groesster Abstand der Gelaenderpfosten, laengs der Neigung gemessen, 2400 mm.
#   9. Handlauf auf beiden Seiten gerader Treppen; bei RUNDEN Treppen auf beiden Seiten nur dann,
#      wenn der Abstand zwischen Tankschale und Wange 200 mm uebersteigt.
#  10. Umlaufende Treppen sind vollstaendig auf der Schale abzustuetzen, die Wangenenden frei vom
#      Boden. Die Treppe reicht vom Tankfuss bis zu einem Podest an der Dachkante.
kStairWidth = 0.710
kStairMaxAngle = math.radians(50.0)
kStairTreadMin = 0.200
kStairSum2Rr = 0.610                             # untere Bandgrenze [T.5-18 Pkt.4]
kStairSum2RrMax = 0.660                          # obere Bandgrenze
kStairPostMaxSpacing = 2.400

# [OSHA 1910.25(c)] Gegenprobe: Neigung 30..50 Grad, Steigung hoechstens 9.5 in, Auftritt
# mindestens 9.5 in. Die einzigen Zollzahlen im Modell, weil sie in einer US-Vorschrift stehen —
# EINMAL gewandelt, danach als Meter gefuehrt.
kOshaRiseMax = 0.2413
kOshaRunMin = 0.2413
kOshaMinAngle = math.radians(30.0)

# [API Tabelle 5-19a, SI] Rise/Run/Angle-Zeilen fuer 2R + r = 610 mm, Steigung R in mm.
kStairRiseRows = (135, 140, 145, 150, 155, 165, 170, 180, 185, 190, 195, 205, 210, 215, 220, 225)


# ================================================================ Fundament

# [API B.4.2.2] "The ringwall shall not be less than 300 mm (12 in.) thick. The centerline diameter
# of the ringwall should equal the nominal diameter of the tank."
kRingwallWidth = 0.300
kRingwallCLDia = kDiameter
# [API Bild B-1, Aufriss] Mass "0.3 m (1 ft)" zwischen der Boeschung des umgebenden Gelaendes und
# der Oberkante des Ringes — gerendert und abgelesen.
kRingwallRise = 0.300
# [API B.4.2.2, letzter Satz] "As a minimum, the bottom of the ringwall, if founded on soil, shall
# be located 0.6 m (2 ft) below the lowest adjacent finish grade."
kRingwallBottom = -0.600
# [API B.4.2.2 / Bild B-1] Zwischen Ringmauer und Blech liegt eine Sandbettung 75 mm min.
kSandMin = 0.075

# [API 5.4.1] "All bottom plates shall have a corroded thickness of not less than 6 mm."
kBottomPlateThk = 0.006
# [API 5.4.2] "at least a 50 mm (2 in.) width will project outside the shell".
kBottomPlateProj = 0.050

# [DERIVED] Die Schale steht AUF dem Bodenblech, das Bodenblech auf der Ringmauerkrone.
kTankBaseZ = kRingwallRise + kBottomPlateThk     # 0.306 m


# ================================================================ Stutzen und Mannloecher

# [API Tabelle 5-5a, SI] Schalenmannloch DN 600: Deckelplatte Dc = 832 mm, Lochkreis Db = 768 mm.
kShellManholeDia = 0.600
kShellManholeCover = 0.832
kShellManholeBolt = 0.768
kShellManholeBolts = 20                          # [SET] per Analogie, s. DEFECTS.md
# [SET] Hoehe der Mannlochachse ueber dem Tankboden. API 650 legt sie nicht fest.
kShellManholeZ = 0.900
# [SET] Durchmesser des Verstaerkungsblechs; Tabelle 5-5a gibt ihn fuer Mannloecher nicht an.
kShellManholeReinf = 0.930

# [API Tabelle 5-13a, SI] Dachmannloch DN 500: Hals 500, Deckel 660, Lochkreis 597, 16 Schrauben,
# Verstaerkungsblech aussen 1050.
kRoofManholeDia = 0.500
kRoofManholeCover = 0.660
kRoofManholeBolt = 0.597
kRoofManholeReinf = 1.050
kRoofManholeBolts = 16
kRoofManholeR = 0.55 * kRadius                   # [SET] Lage auf dem Dach

# [API Tabelle 5-6a, SI, Zeile NPS 6] Rohr-Aussendurchmesser 168.3 mm, Wand 10.97 mm,
# Verstaerkungsblech Do = 400 mm, kleinster Abstand Schale->Flanschspiegel J = 200 mm,
# Abstand Tankboden -> Stutzenachse (Regelausfuehrung) W = 306 mm.
kNozzleOD = 0.1683
kNozzleWall = 0.01097
kNozzleReinfDia = 0.400
kNozzleProj = 0.200
kNozzleZ = 0.306
# [SET] Flanschmasse, s. DEFECTS.md.
kNozzleFlangeDia = 0.280
kNozzleFlangeThk = 0.025

# [SET] Dachentlueftung. Die Bemessung nach API Std 2000 (5.8.5.2) wurde nicht durchgefuehrt.
kVentDia = 0.200
kVentHeight = 0.450
kVentCapDia = 0.320
kVentR = 0.30 * kRadius


# ================================================================ Podest an der Dachkante

# [API 5.8.10 c] "Unless declined on the Data Sheet, a roof edge landing or gauger's platform shall
# be provided at the top of all tanks." Pflicht, kein Zierat.
# [API Tabelle 5-17] Podeste und Laufstege:
#   2. Mindestbreite des Laufstegs 610 mm, NACH ABZUG ALLER VORSPRUENGE.
#   4. Hoehe des obersten Gelaenders ueber dem Boden 1070 mm (Fussnote: von OSHA gefordert).
#   5. Mindesthoehe der Fussleiste 75 mm.
#   7. Der Mittelholm liegt etwa auf halber Hoehe.
#   8. Groesster Pfostenabstand 2400 mm.
kPlatformClear = 0.610                           # LICHTE Breite, nicht die Blechbreite
kPlatformRailH = 1.070
kPlatformToeH = 0.075
kPlatformToeThk = 0.008                          # [SET]
kPlatformPostMax = 2.400
kPlatformArc = 2.400                             # [SET] Bogenlaenge der Verbreiterung

# MESSKONVENTION DES GELAENDERS. [API T.5-17 Pkt.4] sagt "height of the top railing above the
# floor" und verweist in der Fussnote auf OSHA; [OSHA 1910.29(b)(1)] misst die OBERKANTE des
# Holms ("top edge height"). Also liegt die ROHRACHSE einen Rohrradius tiefer. Runde 2 legte die
# Achse auf 1070 und baute damit 1091 mm Oberkante.
kRailMeasure = "Oberkante des Holms [OSHA 1910.29(b)(1)]"



# ================================================================ Bauteilmasse ohne Norm

# [SET] Was kein Regelwerk festlegt (DEFECTS.md). API 650 5.8.10 b nennt PIP STF05501/05520/05521
# als Regeldetails; diese Blaetter wurden nicht beschafft.
kStairTreadThk = 0.030                           # Gitterroststufe
kStairNosing = 0.025                             # Ueberstand der Vorderkante
kStringerH = 0.200                               # Wangenhoehe
kStringerThk = 0.010
kRailTubeDia = 0.042                             # Handlaufrohr
kPostDia = 0.048
kBeadH, kBeadW = 0.003, 0.014                    # Schweissraupe der Stossnaehte
kStairRailH = 0.860                              # oberes Ende des Bandes 760..860 [T.5-18 Pkt.6]
kStairRailRampSteps = 3                          # Uebergang auf Podesthoehe
# [SET] Umfangslagen. Keine Quelle schreibt sie vor.
kStairPhi0 = math.radians(130.0)
kManholePhi = math.radians(300.0)
kNozzlePhi = math.radians(345.0)
kRoofManholePhi = math.radians(30.0)
kVentPhi = math.radians(70.0)

# ================================================================ Dachrandgelaender

# WARUM ES EXISTIERT — und warum das keine Geschmacksfrage ist. [API 5.8.10 a] erklaert
# "OSHA 29 CFR 1910, Subpart D" fuer Podeste, Laufstege und Treppen ausdruecklich mitverbindlich.
# [OSHA 1910.28(b)(1)(i)]: "each employee on a walking-working surface with an unprotected side or
# edge that is 4 feet (1.2 m) or more above a lower level is protected from falling" — zulaessig
# u. a. "guardrail systems". Das Dach IST eine begehbare Flaeche (5.8.10 c setzt den Peilerzugang
# voraus, 5.8.5 die Dachmannloecher), und seine Kante liegt 10.07 m ueber dem Gelaende. Also
# Gelaender, ueber den ganzen Umfang, unterbrochen nur am Podest.
# Runde 3 baute nur den 2.4-m-Bogen des Podests. Der Kritiker hat das Fehlende beziffert: XOR gegen
# dieselbe Geometrie MIT Ring 1.293 % / 1.441 % / 0.968 % bei 11 / 43 / 62 m — groesser als jede
# LOD-Stufe ausser L2->L3. Das ist ein Fehler gegen die WIRKLICHKEIT, den die 2-%-Schranke gar
# nicht deckt: die ist ein Selbstkonsistenz-Budget zwischen zwei eigenen Naeherungen.
kRoofRimR = kShellInnerR + kShellThk + kTopAngleLeg          # [DERIVED] 7.3672 m Traufenradius
# [API T.5-17 Pkt.4] 1070 mm, gemessen wie oben als Oberkante; [OSHA 1910.29(b)(1)] laesst
# 42 in +- 3 in = 1067 +- 76 mm, die API-Zahl liegt darin und ist die engere.
kRoofRailH = kPlatformRailH
# [API T.5-17 Pkt.8] groesster Pfostenabstand 2400 mm. Der Ring laesst den Podestbogen aus.
kRoofRailArc = TAU * kRoofRimR - kPlatformArc                # [DERIVED] 43.8897 m
kRoofRailPosts = int(math.ceil(kRoofRailArc / kPlatformPostMax)) + 1          # 20
kRoofRailSpacing = kRoofRailArc / (kRoofRailPosts - 1)                        # 2.3100 m
# [SET] Die Pfostenachse liegt um einen Pfostenradius INNERHALB der Traufe, damit die Pfosten
# ganz auf dem Dachblech stehen und ihre Aussenflaeche mit der Dachkante fluchtet. Keine Quelle
# legt die Anschlussart fest (PIP STF05521 nicht beschafft, s. DEFECTS.md).
kRoofRailInset = kPostDia / 2.0
# [SET] KEINE Fussleiste am Dachrand. [API T.5-17 Pkt.5] und [OSHA 1910.29(k)] gelten dem Laufsteg
# bzw. dem Schutz vor herabfallenden Teilen ueber Arbeitsplaetzen [OSHA 1910.28(c)]; unter dem
# Dachrand steht niemand, und ein Dachblech ist kein Laufsteg mit Boden. Offen in DEFECTS.md.
kRoofRailToeboard = False


# ================================================================ LOD

# Umfangsteilung je Stufe. Die Radienkorrektur steht in ring_radius.
#
# WARUM NICHT MEHR NUR DER UMFANG (Runde-3-Befund). Bis Runde 2 bekam jede n-Ecke den Radius, bei
# dem ihr UMFANG dem des Kreises gleicht — mit Cauchy begruendet: die ueber alle Richtungen
# gemittelte Schattenbreite eines konvexen Koerpers ist Umfang/pi, also ist die mittlere
# Silhouettenbreite dann exakt gleich. Das ist richtig und es war zu wenig. Das Tor misst die
# XOR-FLAECHE, und in der Draufsicht ist die Flaeche kein Umfang: eine umfangsgleiche 16-Ecke hat
# 1.2884 % zu WENIG Flaeche.
# Die Korrektur gleicht deshalb BEIDE Fehler aus, statt einen auf null zu zwingen und den anderen
# laufen zu lassen: gesucht ist k mit
#       (Flaeche/Kreisflaeche - 1)  =  -(Umfang/Kreisumfang - 1)
# also  a0 k^2 + p0 k - 2 = 0   mit  a0 = n sin(2pi/n) / (2pi),  p0 = n sin(pi/n) / pi.
# Bei n = 20 bleiben dann +-0.2759 % statt 0 % Breite und -0.8238 % Flaeche.
#
# WAS AN DIESER STELLE NICHT HERGELEITET IST, und in Runde 3 als Herleitung ausgegeben wurde:
# der Faktor 2. Runde 3 schrieb, die 2.58 % gemessene XOR seien "der geschlossen vorhergesagte
# Wert 2 x 1.29 %". Nachgerechnet (Polarintegral 1/2 |r1^2 - r2^2| dphi, 4e6 Stuetzstellen) ist
# keiner der beiden in Frage kommenden Werte 2.577 %:
#       XOR(Kreis, 16-Eck umfangsgleich)          1.5151 %
#       XOR(24-Eck, 16-Eck, beide umfangsgleich)  1.3030 %
# Das Flaechendefizit stimmt, die Verdopplung ist erfunden. Die AUSGEGLICHENE Korrektur bleibt —
# aber ohne die Behauptung, sie sei XOR-optimal; gemessen ist sie es nicht (DEFECTS.md):
#       n = 20 gegen den Kreis:  ausgeglichen 0.7032 %  flaechengleich 0.6363 %  Optimum 0.6207 %
kLodSegments = (96, 48, 24, 20)

# Aufloesung des Zielbildes: 720p, 60 Grad horizontales Blickfeld [doc/render/visual-target.md §1].
kPixelAngle = math.radians(60.0) / 1280.0        # 8.1812e-4 rad/px


def ring_radius(r, n):
    """Umkreisradius der n-Ecke, deren Flaechen- und Umfangsfehler gleich gross und gegenlaeufig
    sind (s. Kommentar oben)."""
    a0 = n * math.sin(TAU / n) / TAU
    p0 = n * math.sin(math.pi / n) / math.pi
    return r * (-p0 + math.sqrt(p0 * p0 + 8.0 * a0)) / (2.0 * a0)


def ring_balance(n):
    """Der verbleibende, ausgeglichene Fehler je Richtung — Flaeche negativ, Umfang positiv."""
    a0 = n * math.sin(TAU / n) / TAU
    p0 = n * math.sin(math.pi / n) / math.pi
    k = ring_radius(1.0, n)
    return k * k * a0 - 1.0, k * p0 - 1.0


def ring_error(r, n):
    """Groesste radiale Abweichung der n-Ecke vom Kreis — das Mass, das ein Pixel fuellt oder nicht."""
    rn = ring_radius(r, n)
    return max(rn - r, r - rn * math.cos(math.pi / n))


def lod_range(n):
    """Entfernung, ab der die Abweichung der n-Ecke unter ein Pixel faellt."""
    return ring_error(kRadius, n) / kPixelAngle


def poly_radius(r, n, phi):
    """Radialer Abstand der n-Ecke (Umfangskorrektur eingerechnet) in Richtung phi.

    revolve() skaliert jeden Profilradius auf den Umkreis der umfangsgleichen n-Ecke. Ein Anbau,
    der stur beim wahren Kreisradius sitzt, haengt bei grobem n vor der Wand oder steckt darin.
    Die Ecken der n-Ecke liegen bei Vielfachen von 2 pi / n (phi0 = 0 in revolve).
    """
    a = TAU / n
    return ring_radius(r, n) * math.cos(math.pi / n) / math.cos((phi % a) - a / 2.0)


# ================================================================ Abgeleitete Treppengeometrie

kPlatformZ = kTankBaseZ + kShellHeight           # 10.0596 m ueber Gelaende

# [API T.5-18 Pkt.10] "Circumferential stairways shall be completely supported on the shell of the
# tank, and the ends of the stringers shall be clear of the ground. Stairways shall extend FROM THE
# BOTTOM OF THE TANK up to a roof edge landing or gauger's platform."
#
# WARUM DIE TREPPE NICHT MEHR AM GELAENDE ANFAENGT — und warum das kein Geschmack ist. Bis Runde 3
# stieg sie von z = 0 auf. Die unterste Stufe lag damit bei 179.6 mm, also UNTER der Krone der
# Ringmauer (300 mm), die 147 mm weiter aussen steht als die Schale. Das Bauskript wich ihr aus,
# indem es die Stufe innen beschnitt — 553 mm breit gegen die 710 mm aus [T.5-18 Pkt.2]. Runde 3
# hat das nie bemerkt, weil nichts die GEBAUTE Stufe gemessen hat; check_norms misst sie und meldet
# es beim ersten Lauf. Die Norm loest den Fall selbst: die Treppe beginnt am TANKBODEN. Damit liegt
# die unterste Stufe 153 mm ueber der Ringmauerkrone, jede Stufe hat die volle Breite, und der
# Wangenfuss bleibt frei vom Boden, wie derselbe Punkt es verlangt.
kStairBaseZ = kTankBaseZ                         # 0.306 m
kStairClimb = kPlatformZ - kStairBaseZ           # 9.7536 m = Schalenhoehe


def _stair_steps():
    """Steilste Zeile der Tabelle 5-19a, die auch OSHA besteht — daraus die Zahl der Steigungen.

      1. Aus den Zeilen mit 2R + r = 610 (der steilsten Spalte der Tabelle) diejenige mit der
         GROESSTEN Steigung waehlen, die OSHA 1910.25(c) haelt: r = 610 - 2R >= 241.3 mm erzwingt
         R <= 184.35 mm, also R = 180 mm, r = 250 mm, Winkel 35 deg 45 min — eine echte Zeile.
      2. N = aufgerundet(Steighoehe / R), damit die Steigungen gleich sind [T.5-18 Pkt.4], und R
         auf Steighoehe / N zurueckrechnen.

    KEIN assert HIER. Die drei Bandpruefungen, die bis Runde 3 an dieser Stelle standen, lasen
    dieselben drei Zahlen, die zwei Zeilen darueber aus denselben Konstanten entstanden waren —
    sie konnten nur fehlschlagen, wenn jemand eine Tabellenzeile aendert, nie wenn das Netz falsch
    ist. Gemessen wird jetzt an den GEBAUTEN Stufen (build_fuel_tank.check_norms): Steigung aus dem
    Hoehenabstand der Trittflaechen, Auftritt aus dem Bogen zwischen den Vorderkanten, Winkel und
    2R + r daraus. Das ist dieselbe Aussage ueber das Ding statt ueber die Rechnung.
    """
    rows = [R / 1000.0 for R in kStairRiseRows
            if (kStairSum2Rr - 2.0 * R / 1000.0) >= max(kStairTreadMin, kOshaRunMin)
            and R / 1000.0 <= kOshaRiseMax]
    n = int(math.ceil(kStairClimb / max(rows)))
    r_rise = kStairClimb / n
    return n, r_rise, kStairSum2Rr - 2.0 * r_rise


kStairSteps, kStairRise, kStairRun = _stair_steps()      # 55, 177.34 mm, 255.32 mm
kStairAngle = math.atan2(kStairRise, kStairRun)          # 34.78 Grad

# [DERIVED] Die Treppe liegt auf der Schale. Innenkante = Schalenaussenflaeche des dicksten
# Schusses; das Bauskript rueckt sie noch auf die WIRKLICH gebaute Polygonwand hinaus.
kStairInnerR = kShellOuterMax
# Der Auftritt r wird an der MITTE der Stufenbreite als Bogen gemessen — dort geht ein Mensch.
kStairMidR = kStairInnerR + kStairWidth / 2.0
kStairDPhi = kStairRun / kStairMidR

# WOHER DER AUSSENRADIUS KOMMT, und warum nicht einfach Innenradius + 710 mm. [API T.5-18 Pkt.2]
# fordert 710 mm BREITE. Die Innenkante jeder Stufe sitzt aber nicht auf dem Kreis, sondern auf der
# GEBAUTEN Wand: Polygonecke plus Schweissraupe plus die Sehne ueber den Stufenbogen. Bei einem
# festen Aussenradius bleiben davon 703.6 mm uebrig (gemessen, L0) — Runde 3 hat das nie gesehen,
# weil nichts die gebaute Stufe gemessen hat. Es ist derselbe Fehler, den das Podest in Runde 3
# beim lichten Mass hatte, eine Ebene tiefer.
# Der Aussenradius folgt deshalb der gebauten Innenkante — und zwar der von L0. Das ist keine
# Bequemlichkeit, sondern die Antwort auf die Frage, WAS hier eigentlich vermessen wird: L0 IST der
# Koerper, L1..L3 sind gemessen begrenzte Naeherungen davon, und eine Fertigungsnorm gilt dem
# Koerper. Wuerde der Aussenrand der GROEBSTEN Teilung folgen, diktierte das 20-Eck der Fernsicht
# die Breite einer Treppe (765 statt 715 mm) — eine Naeherung, die das Original umbaut.
# Der Aussenrand ist trotzdem auf allen vier Stufen DERSELBE, denn er traegt die Silhouette; nur
# die Innenkante wandert mit der Teilung, und die liegt an der Wand und ist nie zu sehen.
kLodSegRef = max(kLodSegments)                   # L0 — die Dichte, die den Koerper darstellt
kStairSpan = kStairDPhi + kStairNosing / kStairMidR      # Azimutbogen einer Stufe mit Nase


def _stair_inner_built():
    """Innenkante der Stufe auf der GEBAUTEN Wand: Polygonecke + Schweissraupe, ueber die Sehne des
    Stufenbogens gerechnet. Dieselbe Formel wie _shell_wall_max im Bauskript, an ihrem Maximum."""
    return (ring_radius(kShellOuterMax, kLodSegRef) + kBeadH) / math.cos(0.5 * kStairSpan)


kStairInnerRBuilt = _stair_inner_built()         # 7.3225 m
kStairOuterR = kStairInnerRBuilt + kStairWidth   # 8.0325 m
kStairWrap = kStairSteps * kStairDPhi                    # 1.82985 rad = 104.843 Grad

# [API T.5-18 Pkt.9] Abstand Schale <-> innere Wange ist null, also KEIN innerer Handlauf.
kStairRailInner = False
# [DERIVED] Pfosten: Neigungslaenge / 2400 mm aufgerundet, plus einer. Der oberste faellt weg —
# dort steht der erste Podestpfosten am selben Ort.
kStairSlopeLen = kStairSteps * math.hypot(kStairRise, kStairRun)
kStairPosts = int(math.ceil(kStairSlopeLen / kStairPostMaxSpacing)) + 1
