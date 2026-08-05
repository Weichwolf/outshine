#!/usr/bin/env python3
"""fuel-tank-cylindrical — die VERMESSUNG. Nur Zahlen und ihre Herkunft, keine Blender-Abhaengigkeit.

WAS DAS IST. Ein oberirdischer, senkrechter, geschweisster Lagertank mit getragenem Kegeldach nach
API Std 650 — die Bauform, die auf Flugplaetzen und Militaerstuetzpunkten als Treibstofflager steht.
doc/asset-inventory.md #1: das EINZIGE Objekt, das alle vier Titel platzieren (F22 `FUELTANK`,
Comanche Typ 3, Armored Fist Betankungsdepot, Delta Force `oil tank` 3008).

WARUM 48 ft x 32 ft. Weder gewaehlt noch geraten: es ist eine Zeile der Normgroessentabelle, die
US-Tankbauer seit Jahrzehnten fuehren, und die Zeile ist in sich pruefbar (s. kCapacityBbl).
Militaerischer Massenspeicher wird in Barrel bemessen; 10 000 bbl ist die gelaeufigste Groesse.

QUELLEN
  [API]   API Standard 650, 11. Auflage (2007), oeffentlich als Incorporated-by-Reference-Kopie:
          [WEB https://law.resource.org/pub/us/cfr/ibr/002/api.650.2007.pdf]
          Jede [API]-Zahl nennt ihre Klausel- oder Tabellennummer und ist aus dieser Datei GELESEN,
          nicht erinnert.
  [SIZE]  Normgroessentabelle fuer API-Tanks (Nenninhalt in bbl, Durchmesser x Hoehe in ft).
          [WEB https://www.piping-designer.com/index.php/disciplines/mechanical/stationary-equipment/88-tank/1527-api-tank-size]
          Die Tabelle ist geometrisch selbstkonsistent: pi/4 * D^2 * H der Zeile 10 310 bbl ergibt
          10 313.5 bbl (+0.034 %). Das ist der Grund, ihr zu glauben — nicht ihre Herkunft.
  [OSHA]  29 CFR 1910.25, Standard stairs. [WEB https://www.osha.gov/laws-regs/regulations/standardnumber/1910/1910.25]
          API 650 5.8.10 a verweist ausdruecklich auf OSHA 29 CFR 1910 Subpart D.
  [SET]   von mir gesetzt, weil keine Quelle es hergibt. Jede solche Zahl steht zusaetzlich in
          DEFECTS.md.

KOORDINATEN (Blender): +X rechts, +Y vorwaerts, +Z oben, 1 Einheit = 1 m.
  z = 0 ist das UMGEBENDE GELAENDE, nicht die Tankunterkante — der Tank steht auf einem Ringfundament,
  das aus dem Boden ragt. Ein Platzierer setzt den Nullpunkt auf das Terrain und ist fertig.
  Die Tankachse ist die z-Achse.
"""

import math

TAU = 2.0 * math.pi
IN = 0.0254                             # exakt, internationaler Zoll
FT = 12.0 * IN                          # 0.3048 m, exakt
GAL_US = 231.0 * IN ** 3                # 231 in^3, exakt (US liquid gallon)
BBL = 42.0 * GAL_US                     # 0.158987294928 m^3, exakt (petroleum barrel)


# ================================================================ Hauptmasse

# [SIZE] Zeile "10,310 bbl | 48'-0" | 32'-0"".
kCapacityBbl = 10310.0
kDiameter = 48.0 * FT                   # 14.6304 m
kShellHeight = 32.0 * FT                # 9.7536 m
kRadius = kDiameter / 2.0               # 7.3152 m

# [DERIVED] Probe auf die Tabelle: das Zylindervolumen der Zeile gegen ihren Nenninhalt.
kVolume = math.pi / 4.0 * kDiameter ** 2 * kShellHeight          # 1639.71 m^3
kCapacityCheckBbl = kVolume / BBL                                # 10 313.5 bbl
kCapacityErrRel = kCapacityCheckBbl / kCapacityBbl - 1.0         # +3.37e-4

# [DERIVED] Zahl der Schuesse. [API 5.6.1.2] fordert Schalenbleche von mindestens 1800 mm (72 in)
# Nennbreite. Die Normhoehen der Tabelle [SIZE] sind 16 / 24 / 32 / 40 / 48 ft — ALLE Vielfache von
# 8 ft. 8 ft = 2438 mm ist damit die kleinste Walzbreite ueber der Mindestbreite, die jede dieser
# Hoehen ohne Rest teilt; die Tabelle ist auf ihr gebaut. 32 ft / 8 ft = 4 Schuesse.
kCourses = 4
kCourseHeight = kShellHeight / kCourses          # 2.4384 m = 8 ft

# [API 5.6.1.1, Tabelle] Nenndurchmesser < 50 ft -> Nennblechdicke 3/16 in.
# [API 5.6.1.1, FUSSNOTE 4] "For diameters less than 15 m (50 ft) but greater than 3.2 m (10.5 ft),
#   the nominal thickness of the LOWEST SHELL COURSE shall not be less than 6 mm (1/4 in.)."
# D = 14.6304 m liegt in genau diesem Band. Runde 1 hat die Fussnote uebersehen und die Schale
# durchgehend 3/16 in gebaut — der schwerste Sachfehler der Runde, weil daran die ganze
# Windring-Kette haengt (die Schale ist nicht gleichdick, s. kWindGirderZ).
#
# 6 mm ODER 1/4 in? Note 3 derselben Tabelle sagt: "When specified by the Purchaser, plate with a
# nominal thickness of 6 millimeters may be SUBSTITUTED for 1/4-inch plate." 6 mm ist also die
# metrische ERSATZgroesse, 1/4 in die Grundgroesse. Dieser Tank ist durchgehend US-Zoll gerechnet
# (48 x 32 ft aus einer bbl/ft-Tabelle, Sd in psi, 3/16 in Schale), also 1/4 in.
# Der Unterschied ist nicht folgenlos: er verschiebt den Windring um 90 mm (s. kWindGirderZ).
kShellThk = 3.0 / 16.0 * IN                      # 0.0047625 m — Schuesse 2..4
kShellThkBottom = 1.0 / 4.0 * IN                 # 0.006350 m — Schuss 1 [Fussnote 4]
kCourseThk = (kShellThkBottom,) + (kShellThk,) * (kCourses - 1)

# [DERIVED] Probe, dass die Mindestdicke wirklich massgeblich ist und nicht die Festigkeit.
# 1-Fuss-Methode [API 5.6.3.2, US-Einheiten]:  td = 2.6 D (H-1) G / Sd  [in, ft, ft, -, psi]
# Sd = 23 200 psi fuer A 36 [API Tabelle 5-2b, Zeile "A36": Sy 36 000, Su 58 000, Sd 23 200].
# Nach G aufgeloest ergibt sich je Schuss die Dichte, ab der die Festigkeit uebernimmt:
kAllowStressPsi = 23200.0


def _governing_sg(course):
    """Ab welcher Dichte reicht die Mindestdicke des Schusses nicht mehr? H = Hoehe von der
    Unterkante DIESES Schusses bis zur Schalenoberkante [API 5.6.3.2]."""
    h_ft = 32.0 - course * 8.0
    t_in = kCourseThk[course] / IN
    return t_in * kAllowStressPsi / (2.6 * 48.0 * (h_ft - 1.0))


kGoverningSg = min(_governing_sg(c) for c in range(kCourses))     # 1.4996, Schuss 1
# Jedes Erdoelprodukt liegt darunter (Jet A-1 ~0.80, Diesel ~0.84, sogar Wasser 1.00), also
# regiert die Mindestdicke fuer JEDEN Inhalt dieses Tanks. Damit braucht das Netz keine
# Produktdichte als Quelle — ein Loch weniger.


# ================================================================ Schale: Radien je Schuss

# [API 5.6.1.1 Note 1] "the nominal tank diameter shall be the CENTERLINE diameter of the bottom
# shell-course plates". Der Nennradius liegt also in der Mitte von Schuss 1. Die Schuesse werden
# innen buendig gestossen (die 1-Fuss-Methode und der Fuellstand beziehen sich auf die Innenwand),
# also springt die AUSSENwand am ersten Rundstoss um 1.59 mm nach innen.
kShellInnerR = kRadius - kShellThkBottom / 2.0


def shell_outer(course):
    return kShellInnerR + kCourseThk[course]


kShellOuterMax = shell_outer(0)                  # 7.318375 m — dickster Schuss


# ================================================================ Dach

# [API 5.10.4.1] "The slope of the roof shall be 1:16 or greater if specified by the Purchaser."
# Getragenes Kegeldach, flachster zulaessiger Fall = die Regelausfuehrung.
kRoofSlope = 1.0 / 16.0
kRoofRise = kRadius * kRoofSlope                 # 0.45720 m
# [API 5.10.5, Anmerkung] Nenndicke der Dachbleche nicht unter 4.8 mm (3/16 in).
kRoofThk = 3.0 / 16.0 * IN

# [API 5.1.5.9 e, Tabelle] Mindest-Kopfwinkel nach Durchmesser:
#   D <= 11 m -> 50x50x5 | 11 m < D <= 18 m -> 50x50x6 | D > 18 m -> 75x75x10  (mm)
# D = 14.6304 m faellt in das mittlere Band.
kTopAngleLeg = 0.050
kTopAngleThk = 0.006


# ================================================================ Zwischen-Windring

# [API 5.9.7.1, SI] Groesste unverstaerkte Schalenhoehe:  H1 = 9.47 t (t/D)^1.5.
# Achtung auf die gemischten Einheiten der Klausel: t in MILLIMETERN, D in METERN, H1 in METERN.
# Gegenprobe mit der US-Fassung H1 = 600 000 t (t/D)^1.5 [in, ft, ft]: 27.47 ft = 8.372 m.
_t_mm = kShellThk * 1000.0
kWindH1 = 9.47 * _t_mm * (_t_mm / kDiameter) ** 1.5               # 8.376 m
# t ist die Dicke des OBERSTEN Schusses [API 5.9.7.1] — die aendert Fussnote 4 nicht, also bleibt
# H1 gegenueber Runde 1 gleich. Verglichen wird H1 aber mit der TRANSFORMIERTEN Hoehe [5.9.7.3],
# nicht mit der wirklichen.

# [API 5.9.7.2] TRANSFORMIERTE SCHALE. Weil Schuss 1 dicker ist als der Rest (Fussnote 4), ist die
# Schale NICHT gleichdick und die Mitte der wirklichen Schale ist nicht die Mitte der
# massgeblichen. Jeder Schuss wird auf die Dicke des DUENNSTEN umgerechnet:
#     W_tr = W * (t_uniform / t_actual)^2.5
# Runde 1 schrieb "alle Schuesse gleich dick, also ist die transformierte Schale die wirkliche" —
# das war die Folge des uebersehenen Fussnotenbandes, nicht ein eigener Fehler.
def _transposed():
    return [kCourseHeight * (kShellThk / t) ** 2.5 for t in kCourseThk]


kCourseTransposed = _transposed()                # [1.18783, 2.4384, 2.4384, 2.4384]
kShellTransposed = sum(kCourseTransposed)        # 8.50303 m
kWindGirderNeeded = kShellTransposed > kWindH1   # 8.50303 > 8.37633 -> ja, knapp


def _girder_z():
    """[API 5.9.7.3.1] Ring auf halbe Hoehe der TRANSFORMIERTEN Schale, dann zurueck auf den
    wirklichen Schuss an gleicher relativer Stelle, mit derselben Dickenbeziehung."""
    half = kShellTransposed / 2.0
    acc = 0.0
    for c, wtr in enumerate(kCourseTransposed):
        if half <= acc + wtr or c == kCourses - 1:
            off = (half - acc) * (kCourseThk[c] / kShellThk) ** 2.5
            return c, c * kCourseHeight + off
        acc += wtr


kWindGirderCourse, kWindGirderZIdeal = _girder_z()   # Schuss 3, 5.50208 m ueber Tankboden
# Endgueltige Lage steht am Dateiende: die Treppe muss ueber den Ring hinweg, ohne ihn zu
# durchdringen (s. kWindGirderZ).
# [API 5.9.7.5] verbietet die Anbindung 150 mm um eine Rundnaht. Naechste Naht 4.8768 m,
# Abstand 625 mm — die Klausel greift hier NICHT. (In Runde 1 tat sie es, weil der Ring damals
# faelschlich genau auf der Naht lag.)
# [API 5.9.7.4] Zweiter Ring erst, wenn die halbe transformierte Hoehe H1 uebersteigt:
# 4.2515 < 8.3763 -> nein.
kWindGirderCount = 1

# Z_req steht am Dateiende, weil es von der endgueltigen Ringlage abhaengt.

# [API Tabelle 5-20a, Block "One Angle: Figure 5-24, Detail c", Spalte Schalendicke 5 mm]
#     65 x 65 x 6 -> 28.09 | 65 x 65 x 8 -> 34.63 | 100 x 75 x 7 -> 60.59 | 102 x 75 x 8 -> 66.97
# Kleinstes Profil ueber Z_req = 53.53 cm^3 ist 100 x 75 x 7 (60.59). Runde 1 hatte 100 x 100 x 8
# GESETZT — ein Profil, das in der Tabelle gar nicht vorkommt.
# ROBUSTHEIT: mit der metrischen Ersatzdicke 6 mm fuer Schuss 1 waere Z_req = 54.67 cm^3 und das
# Profil dasselbe. Die Zoll/Millimeter-Frage entscheidet die LAGE (90 mm), nicht das PROFIL.
kWindGirderLegH = 0.100                          # langer Schenkel, WAAGERECHT
kWindGirderLegV = 0.075                          # kurzer Schenkel, senkrecht nach UNTEN
kWindGirderThk = 0.007
kWindGirderZavailCm3 = 60.59
# [API Tabelle 5-20a, Fussnote] "The section moduli for Details c and d are based on the LONGER LEG
# being located HORIZONTALLY (perpendicular to the shell)." Bild 5-24 Detail c (gerendert und
# abgelesen, nicht extrahiert): der waagerechte Schenkel sitzt mit seinem Ende an der Schale, der
# freie Schenkel haengt am AEUSSEREN Ende nach unten. Runde 1 baute die Ferse an der Schale und den
# freien Schenkel nach oben — spiegelverkehrt, und damit traegt die Form das Tabellenmoment nicht.

# [API 5.9.7.7] "An opening for a stairway in an intermediate stiffener is unnecessary when the
# intermediate stiffener extends no more than 150 mm (6 in.) from the outside of the shell and the
# nominal stairway width is at least 710 mm (28 in.)." 100 mm <= 150 mm und 710 mm >= 710 mm —
# die Treppe laeuft also ueber den Ring hinweg, ohne Durchbruch. Sie darf ihn aber nicht
# DURCHDRINGEN; dafuer sorgt kWindGirderZ ueber die Treppenteilung (s. build_fuel_tank.stair).
kWindGirderNeedsStairOpening = False


# ================================================================ Treppe

# [API Tabelle 5-18] Anforderungen an Treppen:
#   2. Mindestbreite der Treppe 710 mm (28 in).
#   3. Groesster Winkel zur Waagerechten 50 Grad.
#   4. Mindestbreite der Stufe 200 mm (8 in). 2R + r nicht unter 610 mm und nicht ueber 660 mm.
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
kStairSum2RrMax = 0.660                          # obere Bandgrenze [T.5-18 Pkt.4]
kStairRailH = 0.860                              # oberes Ende des Bandes 760..860 [SET innerhalb]
# [T.5-18 Pkt.6] "The top railing shall join the platform handrail WITHOUT OFFSET". Der
# Treppenhandlauf steht 860 mm ueber der Stufenvorderkante, das Podestgelaender 1070 mm ueber dem
# Podestboden [T.5-17 Pkt.4] — sie koennen am Uebergang nicht dieselbe Hoehe haben. Runde 1 liess
# dort einen 210-mm-Absatz stehen. Der Handlauf rampt jetzt ueber die letzten Stufen hoch.
kStairRailRampSteps = 3                          # [SET], s. DEFECTS.md
kStairPostMaxSpacing = 2.400

# [OSHA 1910.25(c)] Gegenprobe: Neigung 30..50 Grad, Steigung hoechstens 241 mm (9.5 in),
# Auftritt mindestens 241 mm (9.5 in). Die API-Zahlen unten muessen BEIDE Regelwerke erfuellen.
kOshaRiseMax = 9.5 * IN
kOshaRunMin = 9.5 * IN
kOshaMinAngle = math.radians(30.0)               # 1910.25(c)(1) "between 30 to 50 degrees"


# ================================================================ Fundament

# [API B.4.2.2] "The ringwall shall not be less than 300 mm (12 in.) thick. The centerline diameter
# of the ringwall should equal the nominal diameter of the tank."
kRingwallWidth = 0.300
kRingwallCLDia = kDiameter
# [API Bild B-1, Aufriss] Mass "0.3 m (1 ft)" zwischen der Boeschung des umgebenden Gelaendes und
# der Oberkante des Ringes. Runde 1 hatte das als [SET] gefuehrt mit der Begruendung, die
# Textextraktion koenne den Pfeil keiner Kante zuordnen — das war der falsche Weg zum Bild:
# eine Rastergrafik wird GERENDERT und abgelesen (pdftoppm -r 170, Seite 188), nicht extrahiert.
kRingwallRise = 0.300
# [API B.4.2.2, letzter Satz] "As a minimum, the bottom of the ringwall, if founded on soil, shall
# be located 0.6 m (2 ft) below the lowest adjacent finish grade." Runde 1 setzte -0.15 m; auf
# geneigtem DEM-Gelaende tritt der Ring dann bergab aus dem Boden.
kRingwallBottom = -0.600
# [API B.4.2.2 / Bild B-1] Zwischen Ringmauer und Blech liegt eine Sandbettung 75 mm (3 in) min.
kSandMin = 75.0 / 1000.0

# [API 5.4.2] "Bottom plates of sufficient size shall be ordered so that, when trimmed, at least a
# 50 mm (2 in.) width will project outside the shell." Runde 1 setzte 25 mm und fuehrte die
# Klausel als ungelesen in DEFECTS. Sie ist Text, nicht Bild — sie war nur nie aufgeschlagen.
kBottomPlateProj = 0.050
kBottomPlateThk = 0.006                          # [SET], s. DEFECTS.md

# [DERIVED] Die Schale steht AUF dem Bodenblech, das Bodenblech auf der Ringmauerkrone.
kTankBaseZ = kRingwallRise + kBottomPlateThk


# ================================================================ Stutzen und Mannloecher

# [API Tabelle 5-5a] Schalenmannloch, Nenngroesse 600 mm: Deckelplatte Dc = 832 mm,
# Lochkreis Db = 768 mm.
kShellManholeDia = 0.600
kShellManholeCover = 0.832
kShellManholeBolts = 20                          # [API Tabelle 5-13a analog; s. DEFECTS.md #6]
# [SET] Hoehe der Mannlochachse ueber dem Tankboden. API 650 legt sie nicht fest.
kShellManholeZ = 0.914                           # 3 ft, gelaeufige Werkspraxis

# [API Tabelle 5-13a] Dachmannloch, Nenngroesse 500 mm: Hals 500, Deckel 660, Lochkreis 597,
# 16 Schrauben, Verstaerkungsblech aussen 1050.
kRoofManholeDia = 0.500
kRoofManholeCover = 0.660
kRoofManholeReinf = 1.050
kRoofManholeBolts = 16
# [SET] Lage auf dem Dach. API 650 5.8.4 schreibt keine vor.
kRoofManholeR = 0.55 * kRadius

# [API Tabelle 5-6a, Zeile NPS 6] Schalenstutzen: Rohr-Aussendurchmesser 168.3 mm, Wand 10.97 mm,
# Verstaerkungsblech-Durchmesser Do = 400 mm, kleinster Abstand Schale->Flanschspiegel J = 200 mm,
# Abstand Tankboden -> Stutzenachse (Regelausfuehrung) W = 306 mm.
kNozzleOD = 0.1683
kNozzleWall = 0.01097
kNozzleReinfDia = 0.400
kNozzleProj = 0.200
kNozzleZ = 0.306
# [SET] Flanschaussendurchmesser. ASME B16.5 Klasse 150 NPS 6 waere 279.4 mm; eine Kopie von
# B16.5 wurde in dieser Runde nicht geholt (DEFECTS.md #6).
kNozzleFlangeDia = 0.2794
kNozzleFlangeThk = 0.0254

# [SET] Dachentlueftung. Die Bemessung nach API Std 2000 (5.8.5.2) wurde nicht durchgefuehrt
# (DEFECTS.md #5); Groesse und Form sind eine gelaeufige Freiatmer-Haube.
kVentDia = 0.200
kVentHeight = 0.450
kVentCapDia = 0.320
kVentR = 0.30 * kRadius


# ================================================================ Podest an der Dachkante

# [API 5.8.10 c] "Unless declined on the Data Sheet, a roof edge landing or gauger's platform shall
# be provided at the top of all tanks." Also Pflicht, nicht Zierat.
# [API Tabelle 5-17] Podeste und Laufstege:
#   2. Mindestbreite des Laufstegs 610 mm nach Abzug aller Vorspruenge.
#   4. Hoehe des obersten Gelaenders ueber dem Boden 1070 mm (Fussnote: von OSHA gefordert).
#   5. Mindesthoehe der Fussleiste 75 mm.
#   7. Der Mittelholm liegt etwa auf halber Hoehe.
#   8. Groesster Pfostenabstand 2400 mm.
kPlatformWidth = 0.610
kPlatformRailH = 1.070
kPlatformToeH = 0.075
kPlatformPostMax = 2.400
# [SET] Bogenlaenge des Podests. API 650 legt sie nicht fest.
kPlatformArc = 2.400


# ================================================================ Abgeleitete Treppengeometrie

# Die Treppe steigt vom Gelaende (z = 0) bis auf die Podestebene. Podestebene = Oberkante Schale.
kPlatformZ = kTankBaseZ + kShellHeight           # 10.0536 m ueber Gelaende


# [API Tabelle 5-19a] Rise/Run/Angle-Zeilen fuer 2R + r = 610 mm, Steigung R in mm.
# Runde 1 setzte 2R + r fest auf die UNTERE Bandgrenze 610 und rechnete R = H/N frei — das Ergebnis
# 182.79 / 244.41 mm ist keine Zeile dieser Tabelle. Die Tabelle ist zwar eine Hilfstafel und keine
# abschliessende Liste (bindend ist T.5-18 Pkt.4), aber eine gedruckte Zeile ist eine bessere
# Herkunft als ein an die Bandgrenze geklemmter Bruch.
kStairRiseRows = (135, 140, 145, 150, 155, 165, 170, 180, 185, 190, 195, 205, 210, 215, 220, 225)


def _stair_steps():
    """Steilste Zeile der Tabelle 5-19a, die auch OSHA besteht — daraus die Zahl der Steigungen.

    Zwei Schritte, beide belegt:
      1. Aus den Zeilen mit 2R + r = 610 (die steilste Spalte der Tabelle) diejenige mit der
         GROESSTEN Steigung waehlen, die OSHA 1910.25(c) haelt: r = 610 - 2R >= 241.3 mm erzwingt
         R <= 184.35 mm, also R = 180 mm, r = 250 mm, Winkel 35 deg 45 min — eine echte Zeile.
      2. N = aufgerundet(Steighoehe / R), damit die Steigungen gleich sind [T.5-18 Pkt.4], und R
         auf Steighoehe / N zurueckrechnen. Die Anpassung ist 0.47 mm.
    """
    rows = [R / 1000.0 for R in kStairRiseRows
            if (kStairSum2Rr - 2.0 * R / 1000.0) >= max(kStairTreadMin, kOshaRunMin)
            and R / 1000.0 <= kOshaRiseMax]
    n = int(math.ceil(kPlatformZ / max(rows)))
    r_rise = kPlatformZ / n
    r_run = kStairSum2Rr - 2.0 * r_rise
    assert kStairSum2Rr <= 2.0 * r_rise + r_run <= kStairSum2RrMax
    assert r_run >= max(kStairTreadMin, kOshaRunMin) and r_rise <= kOshaRiseMax
    assert kOshaMinAngle <= math.atan2(r_rise, r_run) <= kStairMaxAngle
    return n, r_rise, r_run


kStairSteps, kStairRise, kStairRun = _stair_steps()      # 56, 179.53 mm, 250.94 mm
kStairAngle = math.atan2(kStairRise, kStairRun)          # 35.58 Grad

# [DERIVED] Die Treppe liegt auf der Schale. Innenkante = Schalenaussenflaeche.
kStairInnerR = kShellOuterMax
kStairOuterR = kStairInnerR + kStairWidth
# Der Auftritt r wird an der MITTE der Stufenbreite als Bogen gemessen — dort geht ein Mensch.
kStairMidR = 0.5 * (kStairInnerR + kStairOuterR)
kStairDPhi = kStairRun / kStairMidR                      # Bogenwinkel je Stufe
kStairWrap = kStairSteps * kStairDPhi                    # 1.7938 rad = 102.8 Grad

# [API T.5-18 Pkt.9] Abstand Schale <-> innere Wange ist null, also KEIN innerer Handlauf.
kStairRailInner = False
# [SET] Umfangslage des Treppenfusses. Keine Quelle schreibt sie vor; gewaehlt, damit Treppe und
# Podest in Seiten- UND Frontansicht sichtbar sind und keinen Stutzen kreuzen (DEFECTS.md #8).
kStairPhi0 = math.radians(130.0)
# [DERIVED] Pfosten: Neigungslaenge / 2400 mm aufgerundet, plus einer.
kStairSlopeLen = kStairSteps * math.hypot(kStairRise, kStairRun)
kStairPosts = int(math.ceil(kStairSlopeLen / kStairPostMaxSpacing)) + 1

# [SET] Bauteilmasse der Treppe, die kein Regelwerk festlegt (DEFECTS.md #6):
kStairTreadThk = 0.030                           # Gitterroststufe
kStairNosing = 0.025                             # Ueberstand der Vorderkante
kStringerH = 0.200                               # Wangenhoehe
kStringerThk = 0.010
kRailTubeDia = 0.042                             # Handlaufrohr, 1 1/4 in Rohr
kPostDia = 0.048


# ================================================================ Endgueltige Ringlage

def _girder_z_clear():
    """[API 5.9.7.3.2] "Other locations for the girder may be used, provided the height of
    unstiffened shell on the transformed shell does not exceed H1."

    WARUM UEBERHAUPT VERSCHIEBEN. [API 5.9.7.7] erlaubt der Treppe, ohne Durchbruch ueber einen
    Ring zu laufen, der nicht weiter als 150 mm aus der Schale steht (100 mm) — sie darf ihn dann
    aber nicht DURCHDRINGEN. An der Sollstelle 5.50208 m schneidet die Unterseite der Stufe 31 den
    Ring. Der Ring rutscht deshalb so weit ab, dass seine Oberkante 20 mm unter der Unterkante der
    naechsttieferen Stufe liegt. Runde 1 hatte diese Durchdringung (Stufe 27 durch den Ring,
    58.78 cm3) ueberhaupt nicht bemerkt, weil nichts sie gemessen hat.
    """
    clear = 0.020
    ideal_abs = kTankBaseZ + kWindGirderZIdeal
    i = math.floor(ideal_abs / kStairRise)        # Stufe, deren Oberkante unter dem Ring liegt
    while i > 0:
        tread_bot = i * kStairRise - kStairTreadThk
        z = tread_bot - clear - kTankBaseZ
        if z + kWindGirderLegH * 0.0 <= ideal_abs:   # nur nach unten ausweichen
            return z
        i -= 1
    raise RuntimeError("keine Ringlage frei von der Treppe")


kWindGirderZ = _girder_z_clear()


def _transposed_pos(z):
    """Wirkliche Hoehe -> Hoehe auf der transformierten Schale."""
    acc, rest = 0.0, z
    for c in range(kCourses):
        if rest <= kCourseHeight or c == kCourses - 1:
            return acc + rest * (kShellThk / kCourseThk[c]) ** 2.5
        acc += kCourseTransposed[c]
        rest -= kCourseHeight


kWindGirderTrPos = _transposed_pos(kWindGirderZ)
# Beide unverstaerkten Abschnitte muessen unter H1 bleiben — das ist die Bedingung aus 5.9.7.3.2,
# und sie wird hier GEPRUEFT, nicht behauptet.
assert kWindGirderTrPos <= kWindH1, "Abschnitt unter dem Ring ueberschreitet H1"
assert kShellTransposed - kWindGirderTrPos <= kWindH1, "Abschnitt ueber dem Ring ueberschreitet H1"
kWindGirderSeamClear = min(abs(kWindGirderZ - c * kCourseHeight) for c in range(1, kCourses))
assert kWindGirderSeamClear >= 0.150, "[API 5.9.7.5] 150 mm um eine Rundnaht verletzt"

# [API 5.9.7.6, SI] Z = D^2 H1 (V/190)^2 / 17  [cm^3, m, m, km/h], V = 190 km/h [API 5.2.1 k].
kWindGirderZreqCm3 = kDiameter ** 2 * (kShellHeight - kWindGirderZ) / 17.0
assert kWindGirderZreqCm3 <= kWindGirderZavailCm3, "Profil 100x75x7 traegt Z_req nicht"


# ================================================================ LOD

# Umfangsteilung je Stufe. Die Korrektur des Radius steht im Bauskript (ring_radius): eine
# regelmaessige n-Ecke bekommt den Umkreisradius, bei dem ihr UMFANG dem des Kreises gleicht.
# Nach Cauchy ist die ueber alle Richtungen gemittelte Schattenbreite eines konvexen Koerpers
# gleich Umfang/pi — die Korrektur macht also die mittlere Silhouettenbreite ueber alle vier
# Stufen EXAKT gleich. Genau das ist die Bedingung "kein Springen beim Umschalten".
kLodSegments = (96, 48, 24, 16)

# Aufloesung des Zielbildes: 720p, 60 Grad horizontales Blickfeld [doc/render/visual-target.md §1].
kPixelAngle = math.radians(60.0) / 1280.0        # 8.1812e-4 rad/px


def ring_radius(r, n):
    """Umkreisradius der n-Ecke mit gleichem Umfang wie der Kreis vom Radius r."""
    return r * math.pi / (n * math.sin(math.pi / n))


def ring_error(r, n):
    """Groesste radiale Abweichung der n-Ecke vom Kreis — das Mass, das ein Pixel fuellt oder nicht."""
    rn = ring_radius(r, n)
    return max(rn - r, r - rn * math.cos(math.pi / n))


def lod_range(n):
    """Entfernung, ab der die Abweichung der n-Ecke unter ein Pixel faellt."""
    return ring_error(kRadius, n) / kPixelAngle


def poly_radius(r, n, phi):
    """Radialer Abstand der n-Ecke (Umfangskorrektur eingerechnet) in Richtung phi.

    WARUM DAS GEBRAUCHT WIRD. revolve() skaliert jeden Profilradius auf den Umkreis der
    umfangsgleichen n-Ecke. Ein Anbau, der stur beim wahren Kreisradius sitzt, haengt dann bei
    grobem n vor der Wand oder steckt darin: bei n=12 sind das +84 mm an den Ecken und -170 mm in
    den Kantenmitten. Runde 1 mass das nicht und liess das Stutzenblech bei L3 181 mm frei
    schweben. Die Ecken der n-Ecke liegen bei Vielfachen von 2 pi / n (phi0 = 0 in revolve).
    """
    a = TAU / n
    return ring_radius(r, n) * math.cos(math.pi / n) / math.cos((phi % a) - a / 2.0)
