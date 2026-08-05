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
kShellThk = 3.0 / 16.0 * IN                      # 0.0047625 m

# [DERIVED] Probe, dass die Mindestdicke wirklich massgeblich ist und nicht die Festigkeit.
# 1-Fuss-Methode [API 5.6.3.2, US-Einheiten]:  td = 2.6 D (H-1) G / Sd  [in, ft, ft, -, psi]
# Sd = 23 200 psi fuer A 36 [API Tabelle 5-2b, Zeile "A36": Sy 36 000, Su 58 000, Sd 23 200].
# Nach G aufgeloest bei td = 3/16 in ergibt sich die Dichte, ab der die Festigkeit uebernimmt:
kAllowStressPsi = 23200.0
kGoverningSg = (3.0 / 16.0) * kAllowStressPsi / (2.6 * 48.0 * (32.0 - 1.0))   # 1.1244
# Jedes Erdoelprodukt liegt darunter (Jet A-1 ~0.80, Diesel ~0.84, sogar Wasser 1.00), also
# regiert die Mindestdicke fuer JEDEN Inhalt dieses Tanks. Damit braucht das Netz keine
# Produktdichte als Quelle — ein Loch weniger.


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
kWindGirderNeeded = kShellHeight > kWindH1       # 9.7536 m > 8.376 m -> ja

# [API 5.9.7.3.1] "For equal stability above and below the intermediate wind girder, the girder
# should be located at the mid-height of the transformed shell." Alle vier Schuesse haben dieselbe
# Dicke, also ist die transformierte Schale die wirkliche und die Mitte ist die Mitte.
# [API 5.9.7.5] "Intermediate wind girders shall not be attached to the shell within 150 mm (6 in.)
# of a horizontal joint ... the girder shall preferably be located 150 mm below the joint."
# Die Schalenmitte 4.8768 m IST der Rundnaht zwischen Schuss 2 und 3 — der Ring rutscht also
# 150 mm tiefer. Das ist keine Wahl, das ist die Klausel.
kWindGirderZ = kShellHeight / 2.0 - 0.150        # 4.7268 m ueber Tankboden
# [API 5.9.7.4] Zweiter Ring erst, wenn die halbe Schalenhoehe H1 uebersteigt: 4.8768 < 8.376 -> nein.
kWindGirderCount = 1

# [API 5.9.7.6, SI] Erforderliches Widerstandsmoment:  Z = D^2 H1 (V/190)^2 / 17  [cm^3, m, m, km/h]
# mit H1 = Abstand Ring -> Kopfwinkel und V = 190 km/h Regelwindgeschwindigkeit [API 5.2.1 k].
kWindGirderZreqCm3 = kDiameter ** 2 * (kShellHeight - kWindGirderZ) / 17.0        # 63.3 cm^3
# [SET] Die Schenkellaenge. Aus Z_req folgt ueber [API Tabelle 5-20] ein konkretes Profil; diese
# Auswahl wurde in dieser Runde NICHT durchgefuehrt (DEFECTS.md #4). 100 mm ist die naechste
# handelsuebliche Groesse ueber dem Kopfwinkel und traegt sichtbar das richtige Verhaeltnis.
kWindGirderLeg = 0.100
kWindGirderThk = 0.008                           # [SET], mit derselben Begruendung


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
kStairSum2Rr = 0.610                             # untere Grenze des Bandes = steilste Ausfuehrung
kStairRailH = 0.860                              # oberes Ende des Bandes 760..860 [SET innerhalb]
kStairPostMaxSpacing = 2.400

# [OSHA 1910.25(c)] Gegenprobe: Neigung 30..50 Grad, Steigung hoechstens 241 mm (9.5 in),
# Auftritt mindestens 241 mm (9.5 in). Die API-Zahlen unten muessen BEIDE Regelwerke erfuellen.
kOshaRiseMax = 9.5 * IN
kOshaRunMin = 9.5 * IN


# ================================================================ Fundament

# [API B.4.2.2] "The ringwall shall not be less than 300 mm (12 in.) thick. The centerline diameter
# of the ringwall should equal the nominal diameter of the tank."
kRingwallWidth = 0.300
kRingwallCLDia = kDiameter
# [SET] Hoehe ueber dem umgebenden Gelaende. [API Bild B-1] traegt an dieser Stelle ein Mass
# 0.3 m (1 ft), aber die Textextraktion kann den Pfeil keiner Kante zuordnen (DEFECTS.md #6).
kRingwallRise = 0.300
# [DERIVED] Der Tankboden liegt auf der Ringmauer, also ist die Schalenunterkante die Ringoberkante.
kTankBaseZ = kRingwallRise
# [API B.4.2.2 / Bild B-1] Zwischen Ringmauer und Blech liegt eine Sandbettung 75 mm (3 in) min.
kSandMin = 75.0 / 1000.0


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


def _stair_steps():
    """Zahl der Steigungen: die kleinste, bei der ALLE vier Bedingungen halten.

    R = kPlatformZ / N (gleiche Steigungen, [API T.5-18 Pkt.4]), r = 610 mm - 2R (dasselbe),
    dann muss gelten:  r >= 200 mm [API],  R <= 241 mm und r >= 241 mm [OSHA 1910.25(c)],
    atan(R/r) <= 50 Grad [API].  Die OSHA-Auftrittsgrenze ist die scharfe: r >= 241.3 mm
    erzwingt R <= 184.35 mm und damit N >= 54.54, also N >= 55.
    """
    for n in range(2, 400):
        r_rise = kPlatformZ / n
        r_run = kStairSum2Rr - 2.0 * r_rise
        if r_run < max(kStairTreadMin, kOshaRunMin):
            continue
        if r_rise > kOshaRiseMax:
            continue
        if math.atan2(r_rise, r_run) > kStairMaxAngle:
            continue
        return n, r_rise, r_run
    raise RuntimeError("keine Treppenteilung erfuellt API 650 T.5-18 und OSHA 1910.25 zugleich")


kStairSteps, kStairRise, kStairRun = _stair_steps()      # 55, 182.79 mm, 244.42 mm
kStairAngle = math.atan2(kStairRise, kStairRun)          # 36.79 Grad

# [DERIVED] Die Treppe liegt auf der Schale. Innenkante = Schalenaussenflaeche.
kStairInnerR = kRadius + kShellThk / 2.0
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


# ================================================================ LOD

# Umfangsteilung je Stufe. Die Korrektur des Radius steht im Bauskript (ring_radius): eine
# regelmaessige n-Ecke bekommt den Umkreisradius, bei dem ihr UMFANG dem des Kreises gleicht.
# Nach Cauchy ist die ueber alle Richtungen gemittelte Schattenbreite eines konvexen Koerpers
# gleich Umfang/pi — die Korrektur macht also die mittlere Silhouettenbreite ueber alle vier
# Stufen EXAKT gleich. Genau das ist die Bedingung "kein Springen beim Umschalten".
kLodSegments = (96, 48, 24, 12)

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
