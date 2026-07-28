# Das JSBSim-Flugmodell — F-16 und die Waffen

**Quelle dieser Datei ist KEIN Handbuch, sondern der Modellbaum selbst:**

| Datei | Zeilen | Rolle | Lizenz / Herkunft |
|---|---|---|---|
| `sim/assets/aircraft/f16/f16.xml` | 1941 | Zelle: Geometrie, Masse, Bodenkontakte, Antrieb, FLCS, Aerodynamik | GPL, Erik Hofman, Rev. 1.95, erstellt 2001-12-28 |
| `sim/assets/aircraft/f16/engine/F100-PW-229.xml` | 86 | Triebwerk (`<turbine_engine>`) | Aero-Matic v0.8 generiert |
| `sim/assets/aircraft/f16/engine/direct.xml` | 6 | Thruster (`<direct>`) — Schub = Triebwerksschub, keine Propellergeometrie | — |
| `sim/assets/aircraft/f16/Systems/hook.xml` | 75 | Fanghaken als `<system>` | — |
| `sim/assets/aircraft/f16/Systems/pushback.xml` | 41 | Schlepper als `<system>` | — |
| `sim/assets/aircraft/f16/reset00.xml` | 16 | IC „auf der Bahn" (von FlightBox NICHT benutzt) | — |
| `sim/assets/aircraft/mk82/mk82.xml` | 331 | Mk-82, ungelenkte Bombe | vendored, `release="BETA"` |
| `sim/assets/aircraft/aim120/aim120.xml` | 498 | AIM-120, **FlightBox-eigen** | FlightBox, `release="ALPHA"` |
| `sim/assets/aircraft/aim120/engine/WPU-6.xml` | 62 | Feststoffmotor (`<rocket_engine>`) | FlightBox |
| `sim/assets/aircraft/aim120/engine/WPU-6_nozzle.xml` | 12 | Düse (`<nozzle>`) | FlightBox |

Sekundär, weil die halbe Semantik nicht im XML steht, sondern in der Engine, die es liest:
`sim/vendor/jsbsim/src/models/**` (gepinntes Submodul, read-only).

**Warum diese Datei existiert.** `doc/modules/f16/*.md` beschreibt den ECHTEN Jet aus den Handbüchern
(Design-Ziele). `doc/*.md` beschreibt UNSEREN Code. CLAUDE.mds Prinzip 5 stützt die ganze
Fidelity-Aussage aber auf ein Drittes — *„Referenz ist das MODELL selbst"* —, und was in diesem Modell
steht, stand bis hierher nirgends. Diese Datei ist die Bestandsaufnahme: was das Modell TUT, nicht was
der Jet KANN.

**Herkunfts-Tags je Zahl** (durchgängig, keine Angabe ohne Tag):

| Tag | Bedeutung |
|---|---|
| `[XML]` | im Modell abgelesen; Fundstelle = Datei + Element |
| `[JSB]` | Semantik aus dem gepinnten JSBSim-Quelltext; Fundstelle = Datei:Zeile |
| `[ABL]` | hier abgeleitet, Formel steht daneben |
| `[MESS]` | gemessen, Kommando/Harness steht daneben |
| `[DOC]` | aus `doc/modules/f16/` bzw. `doc/` mit deren eigenem Zitat-Tag |

## Spec

**Das gepinnte Modell, wie es geflogen wird, plus die deklarierten Deltas** — mehr verlangt Prinzip 5
nicht, und weniger genügt ihm nicht. Die Delta-Liste und ihr Prüftor führt
[`sim/assets/MODEL-DELTAS.md`](../../../sim/assets/MODEL-DELTAS.md) (`make -C sim verify-models`).
Alles Übrige in `doc/modules/f16/` beschreibt den ECHTEN Jet (Design-Ziele); diese Datei beschreibt, was unser
Flugzeug tatsächlich TUT.

## State

**Diese Datei IST der Zustand.** Sie beschreibt kein Ziel, sondern eine Bestandsaufnahme: §1–§10 sind
das geflogene Modell — Herkunftskette und Delta, Geometrie, Masse, Bodenkontakte, Antrieb, Aerodynamik,
die FLCS als XML, die gemessene Einhüllende, die akzeptierten Modell-Eigenschaften und die beiden
Waffenmodelle. Die für die übrige Ablage wichtigsten zwei Abschnitte:

- **§7.11 — die Abweichungstabelle Modell vs. echte FLCS.** Die Brücke zwischen dieser Datei und
  [`flight-controls-flcs.md`](flight-controls-flcs.md): Design-Ziel `[DOC]` gegen Ist `[XML]`, Zeile
  für Zeile. Wer eine Zahl aus dem Handbuch nachbauen will, liest zuerst hier nach, ob das Modell sie
  überhaupt hat.
- **§9 — die zwölf akzeptierten Modell-Eigenschaften.** Ausdrücklich Wahrheit, kein Defekt
  (Prinzip 5); sie dürfen nicht „repariert" werden.

Wie der Code diese Zahlen benutzt, steht auf der anderen Seite:
[`../flightbox/sim/fdm.md`](../../fdm.md) (Adapter, Ladeablauf, Kanäle),
[`module.md`](module.md) (die Hooks des Moduls) und
[`../flightbox/aircraft/stores.md`](../stores.md) (Modellwurzeln + Delta-Regel).

### 1. Herkunftskette und Modell-Delta

#### 1.1 NASA TP-1538 → JSBSim → FlightBox

Die Quellenlage von TP-1538 ist in `aerodynamics-performance.md` §„Reference aerodynamic dataset"
vollständig aufgeschrieben (Nguyen, Ogburn, Gilbert, Kibler, Brown & Deal, *Simulator Study of
Stall/Post-Stall Characteristics of a Fighter Airplane With Relaxed Longitudinal Static Stability*,
NASA TP-1538, Dez. 1979, NTRS 19800005879) und wird hier **nicht wiederholt**. `f16.xml`s
`<fileheader>` nennt sie selbst als Referenz `refID="NASA TP-1538"` neben Richard Murrays
Caltech-Modellseite, dem Dash-1 (nur Widerstands-/Gewichtsdaten), zwei NASA-Bodeneffekt-Reports
(H-1999, H-2177) und „Observers Aircraft" `[XML]`.

**Was von TP-1538 tatsächlich im Modell gelandet ist — und was nicht:**

| TP-1538 | Im Modell | Beleg |
|---|---|---|
| AoA-Bereich **−20°…+90°** | **−10°…+45°**, 12 Stützstellen à 5° | `[XML]` jede α-Tabelle: −0,175…0,785 rad |
| β-Bereich **−30°…+30°** | **−30°…+30°**, 13 bzw. 7 Stützstellen | `[XML]` Clb/Cnb (13 Spalten à 5°), Clda/Cldr/Cnda/Cndr (7 Spalten à 10°) |
| statische Kräfte/Momente | **ja**, als `Cx(α, δh)` bzw. `Cx(α, β)`-Flächen | `[XML]` CLDh, CDDh, CmDh, Clb, Cnb, Clda, Cldr, Cnda, Cndr |
| dynamische (forced-oscillation) Dämpfungsderivative | **ja**, als `Cx_rate(α)`-Kurven | `[XML]` Clp, Clr, Cmq, Cnp, Cnr, CYp, CYr, CLq, CDq |
| Nachstall / Deep-Stall-Bereich 45°…90° | **NEIN** | `[ABL]` s.u. |
| Kompressibilität | im Modell ERGÄNZT (nicht aus TP-1538, das ist Niedriggeschwindigkeit) | `[XML]` die neun `*_M(Mach)`-Funktionen + CDmach |

**Der Nachstall-Befund ist der wichtigste dieser Tabelle.** JSBSim extrapoliert Tabellen NICHT, es
klemmt auf die Randstützstelle `[JSB]` — der Modellautor schreibt das selbst in einen Kommentar
(„note: JSBSim doesn't extrapolate, so no need to specify beyond 30 degrees", `f16.xml` Pitch-Kanal
`[XML]`). Jenseits α = 45° liefert also jede Aerotabelle konstant ihren 45°-Wert. Der in
`aerodynamics-performance.md` beschriebene Deep-Stall-Hangup bei 50–60° AoA ist damit in DIESEM Modell
**nicht repräsentiert**: dort stünde eine nicht-monotone `Cm(α)`-Fläche, hier steht eine Konstante.
Was der Modellautor stattdessen gebaut hat, ist eine FLCS-seitige Krücke: die Bremsklappe fährt
selbsttätig aus, wenn α ≥ 53° UND v ≤ 18 ft/s (§7.7).

#### 1.2 Modell-Delta gegenüber dem gepinnten Submodul

`sim/assets/aircraft/f16/` ist **byte-identisch** zu `sim/vendor/jsbsim/aircraft/f16/` mit genau einer
Ergänzung: das Verzeichnis `engine/` mit `F100-PW-229.xml` und `direct.xml`, die im Submodul unter
`sim/vendor/jsbsim/engine/` liegen und dort ebenfalls byte-identisch sind `[MESS]`
(`diff -rq sim/vendor/jsbsim/aircraft/f16 sim/assets/aircraft/f16` → nur `Only in …: engine`;
`diff` beider Engine-Dateien → leer). `mk82/` ist ebenfalls byte-identisch zum Submodul `[MESS]`.
`aim120/` hat kein Gegenstück im Submodul — es ist FlightBox-eigen, weil das gepinnte JSBSim keine
AMRAAM kennt und read-only ist (CLAUDE.md Prinzip 1).

**`fcs/fbw-override` ist KEINE FlightBox-Erfindung.** Die Property wird in `f16.xml` Zeile 314 vom
Modell selbst deklariert und in den Zeilen 414 und 625 ausgewertet `[XML]` — sie steht so im
Vanilla-JSBSim. FlightBox setzt sie nur (`fdm/FBFdm.cpp:148`, Default AUS).

---

### 2. Geometrie — `f16.xml → <metrics>`

| Element | Wert | Bemerkung |
|---|---|---|
| `wingarea` | **300 ft²** (27,87 m²) | `[XML]`; Bezugsfläche JEDER Aerofunktion (`metrics/Sw-sqft`) |
| `wingspan` | **30,0 ft** (9,14 m) | `[XML]`; Bezugslänge der Roll-/Giermomente und von `aero/bi2vel` = b/2V `[JSB]` |
| `chord` | **11,32 ft** (3,45 m) | `[XML]` MAC; Bezugslänge des Nickmoments und von `aero/ci2vel` = c̄/2V `[JSB]` |
| `htailarea` / `htailarm` | 63,7 ft² / 16,46 ft | `[XML]` — nur Buchhaltung, keine Aerofunktion liest sie |
| `vtailarea` / `vtailarm` | 54,75 ft² / **0 ft** | `[XML]` — der Hebelarm ist 0, also erst recht ungenutzt |
| `AERORP` | x −189,5 / y 0 / z +3,9 in | `[XML]` Angriffspunkt aller Aerokräfte |
| `EYEPOINT` | x −336,2 / y 0 / z +29,5 in | `[XML]`; identisch zur Pilotenmasse (§3) und Bezug von `n-pilot-*` |
| `VRP` | x −180 / y 0 / z 0 in | `[XML]` visueller Bezugspunkt |

**Achsen des Strukturrahmens:** x wächst nach ACHTERN (Radom −486,6 in ist der vorderste Punkt, der
Fanghaken +100,669 in der hinterste), y nach steuerbord, z nach oben `[ABL]` aus den Kontaktpunkt-
Koordinaten (§4). `modules/f16/FBF16Damage` rechnet mit derselben Ableitung `[DOC]`.

**Der Aero-Bezugspunkt liegt 3,5 in ACHTERN des CG** (−189,5 gegen −193,0) `[ABL]` = 2,6 % MAC.
JSBSim versetzt die am AERORP angreifenden Kräfte selbst ins CG und erzeugt daraus das
Zusatzmoment `[JSB]`; eine CG-Korrektur der Momentenkoeffizienten (wie sie Stevens & Lewis für einen
von 0,35 c̄ abweichenden Schwerpunkt vorsehen) macht das Modell NICHT.

---

### 3. Masse und Trägheit — `f16.xml → <mass_balance>`

| Element | Wert | Bemerkung |
|---|---|---|
| `emptywt` | **17.400 lb** (7.893 kg) | `[XML]`; Handbuch-Leermasse ≈ 18.900 lb `[DOC aerodynamics-performance.md]` |
| `ixx` | 9.496 slug·ft² | `[XML]` Rollträgheit |
| `iyy` | 55.814 slug·ft² | `[XML]` Nickträgheit |
| `izz` | 63.100 slug·ft² | `[XML]` Gierträgheit |
| `ixz` | **−982** slug·ft² | `[XML]`; Attribut `negated_crossproduct_inertia="true"` → der Wert geht mit diesem Vorzeichen direkt in den Tensor `[JSB FGMassBalance.cpp:121-128]` |
| `ixy`, `iyz` | 0 | `[XML]` |
| `<pointmass name="Pilot">` | 230 lb bei (−336,2 / 0 / 0) in | `[XML]` — die EINZIGE deklarierte Punktmasse |

**Diese Punktmasse ist architektonisch relevant:** `fdm/FBFdm`s `AddStorePointMass` treibt genau die
`<pointmass>`-Mechanik, von der das Modell hier ein Exemplar zeigt — deshalb kommen Masse,
Schwerpunkt UND Trägheitsmomente einer Zuladung aus der Engine statt aus FlightBox-Arithmetik
`[DOC CLAUDE.md, fdm/]`.

**Tanks** (`<propulsion>`, hier einsortiert weil Massen):

| Tank | Ort (in) | Kapazität | Vorbelegung | Bemerkung |
|---|---|---|---|---|
| 0 | −174,4 / +65,0 / +5,0 | 3.486 lb | 1.500 lb | intern rechts |
| 1 | −174,4 / −65,0 / +5,0 | 3.486 lb | 1.500 lb | intern links |
| 2 | −174,4 / +65,0 / −15,0 | 2.991 lb | 0 lb | Außentank Station 4 |
| 3 | −174,4 / −65,0 / −15,0 | 2.991 lb | 0 lb | Außentank Station 6 |

`[XML]` — intern **6.972 lb**, extern **5.982 lb**, alle vier vom Triebwerk `<feed>`-verdrahtet.
Die Butt-Line ±65 in ist genau die Referenz, an der `modules/f16/FBF16Sms` seine Stationen 4/6
verankert `[DOC modules-f16.md]`.

**Alle vier Tanks haben Priorität 1 und werden GLEICHZEITIG leergesaugt** `[JSB
FGPropulsion::ConsumeFuel]` — der echte Jet zieht zuerst die Außentanks. Wer eine Betankungs- oder
Schwerpunktwanderungs-Prozedur modelliert, modelliert sie gegen dieses Verhalten, nicht gegen das
Handbuch.

---

### 4. Bodenkontakte — `f16.xml → <ground_reactions>`

#### 4.1 Fahrwerk (`type="BOGEY"`)

| Kontakt | Ort (in) | Feder (lb/ft) | Dämpfung (lb/ft/s) | µ stat/dyn/roll | max_steer | Bremsgruppe |
|---|---|---|---|---|---|---|
| `NOSE_LG` | −299,6 / 0 / −72,0 | **17.250** | **4.250** | 0,8 / 0,5 / 0,02 | **80°** | NOSE |
| `LEFT_MLG` | −158,6 / −48,0 / −71,6 | **37.500** | **7.500** | 0,8 / 0,5 / 0,02 | 0° | LEFT |
| `RIGHT_MLG` | −158,6 / +48,0 / −71,6 | **37.500** | **7.500** | 0,8 / 0,5 / 0,02 | 0° | RIGHT |

`[XML]`, alle drei `<retractable>1</retractable>`.

#### 4.2 Struktur (`type="STRUCTURE"`) — die Aufschlagpunkte

| Kontakt | Ort (in) | Feder | Dämpfung | µ (alle drei) |
|---|---|---|---|---|
| `LEFT_WT` / `RIGHT_WT` | −121,3 / ∓189,0 / 0 | 10.000 | 2.000 | 0,2 |
| `TOP_VS` | −27,2 / 0 / +123,2 | 10.000 | 2.000 | 0,2 |
| `LEFT_VFT` / `RIGHT_VFT` | −97,6 / ∓24,8 / −52,0 | **200** | 5.000 | 0,2 |
| `INTAKE` | −322,4 / 0 / −36,6 | 10.000 | 2.000 | 0,2 |
| `RADOME` | −486,6 / 0 / −8,7 | 10.000 | 2.000 | 0,2 |

`[XML]`. Die Flügelspitzen liegen bei ±189 in = ±15,75 ft, also **31,5 ft Spannweite** gegen die
30 ft der `<metrics>` `[ABL]` — die Kontaktgeometrie zählt die Startschienen mit, die Bezugs-
Spannweite nicht.

#### 4.3 Es gibt KEINE Bruchlast im Modell

JSBSims `<contact>` kennt kein Feld für eine Versagenslast — weder für eine Fahrwerksstrebe noch für
einen Strukturpunkt `[JSB FGLGear]`. Was `core/FBFlightMonitor` als K.O. wertet, leitet er deshalb
**aus dem gemessenen Federverhalten dieser Zahlen** ab, nicht aus einer deklarierten Grenze:
Aufsetzkraft (`GetMaxGearForceLbs`, die Spitze EINER Strebe) gegen das modelleigene Standgewicht,
Schranke `kHardLandingForceFactor = 3,0` `[DOC core.md]`. Ebenso ist `gear/gear-pos-norm` (der
5-Sekunden-Transit aus §7.6) die Wahrheitsquelle für „Bauchlandung" mit Schranke 0,5 `[DOC core.md]`.
Der Monitor ist damit modellgetrieben, aber die Schranken sind seine, nicht die des XML — und das ist
genau die Trennung, die Prinzip „Kein Cheaten" verlangt.

#### 4.4 `<external_reactions>`

| Kraft | Ort (in) | Richtung (Körper) | Wer treibt sie |
|---|---|---|---|
| `pushback` | −2,98 / 0 / −1,97 | +x | `Systems/pushback.xml` → `external_reactions/pushback/magnitude` `[XML]` |
| `hook` | +100,669 / 0 / −28,818 | −0,9995 / 0 / +0,01 | **NIEMAND** `[MESS]` |

Der Fanghaken ist deklariert, `Systems/hook.xml` rechnet eine Verzögerungskraft
(`hook-decel-multiplier`-Tabelle über Radgeschwindigkeit, ×`inertia/weight-lbs`) — und schreibt sie
**nirgendwo hin**. `grep -rn "external_reactions/" sim/assets/aircraft/f16/Systems/` findet genau
eine Zeile, die des Schleppers `[MESS]`. Der Haken ist im Modell also eine Animation ohne Physik.

FlightBox legt zur LAUFZEIT zwei weitere Kräfte an derselben Mechanik an — `fb-stores`
(Zuladungs-Widerstand) und `fb-damage` (Strukturschaden-Widerstand) —, ohne das XML zu patchen
`[DOC CLAUDE.md, fdm/]`.

---

### 5. Antrieb — `engine/F100-PW-229.xml`

#### 5.1 Skalare

| Element | Wert | Bedeutung |
|---|---|---|
| `milthrust` | **17.800 lbf** | `[XML]` Bezugsschub trocken (SL, M 0) |
| `maxthrust` | **29.000 lbf** | `[XML]` Bezugsschub mit Nachbrenner (SL, M 0) |
| `bypassratio` | 0,4 | `[XML]` — geht NUR in die Spool-Zeitkonstante ein `[JSB FGTurbine.h:330]` |
| `tsfc` / `atsfc` | 0,74 / **2,05** lb/(lbf·h) | `[XML]` Verbrauch trocken / mit AB |
| `idlen1` / `idlen2` | 40 / **53** % | `[XML]` Leerlaufdrehzahlen |
| `maxn1` / `maxn2` | 100 / 100 % | `[XML]` |
| `augmented` / `augmethod` | 1 / **2** | `[XML]` — Methode 2 = STUFENLOSER Nachbrenner (s.u.) |
| `injected` | 0 | `[XML]` keine Wassereinspritzung |

#### 5.2 Die drei Schubtabellen

Alle drei sind **Faktoren**, indiziert nach Mach (Zeile) × `atmosphere/density-altitude` in ft
(Spalte). Spaltenraster durchgängig −10.000 / 0 / 10.000 / 20.000 / 30.000 / 40.000 / 50.000 /
60.000 ft, letzte Spalte **überall 0,0** `[XML]`.

| Tabelle | Mach-Zeilen | Wertebereich | Wirkung |
|---|---|---|---|
| `IdleThrust` | 0,0…1,0 (6) | +0,1467 … **−0,2839** | `[XML]` Leerlaufschub, ×`milthrust`. Negative Werte = Windmilling-WIDERSTAND: bei M 1,0 auf Meereshöhe −0,2839 · 17.800 = **−5.053 lbf** `[ABL]` |
| `MilThrust` | 0,0…1,4 (8) | 1,5941 (M1,4/−10 kft) … 0 | `[XML]` Trockenschub. Bei M 0/SL genau 1,0000 |
| `AugThrust` | 0,0…**2,6** (14) | 2,2000 (M2,6/−10 kft) … 0 | `[XML]` Nachbrennerschub. Bei M 0/SL genau 1,0000 |

**Da JSBSim klemmt statt zu extrapolieren `[JSB]`:** oberhalb Mach 1,4 friert der TROCKENSCHUB auf
seinem M-1,4-Wert ein, oberhalb Mach 2,6 der Nachbrennerschub, und oberhalb 60.000 ft
Dichtehöhe ist der Schub **exakt null** — das Modell hat dort keine Gipfelhöhe, sondern eine Wand.

#### 5.3 Schub- und Drehzahlgesetz `[JSB FGTurbine::Run(), Zeilen 198-260]`

```
idlethrust = milthrust · IdleThrust(M, h)
milthrust' = (milthrust − idlethrust) · MilThrust(M, h)
N2        →  IdleN2 + ThrottlePos · (MaxN2 − IdleN2)      (Seek, s. 5.4)
N2norm     = (N2 − IdleN2) / (MaxN2 − IdleN2)
thrust     = idlethrust + milthrust' · N2norm²            ← QUADRATISCH in N2norm
AugMethod 2, AugmentCmd > 0:
thrust    += (maxthrust · AugThrust(M,h) − thrust) · min(AugmentCmd, 1)
```

Der Nachbrenner ist also **stufenlos** (kein Schnappen bei „Throttle > 99 %", das wäre AugMethod 1)
und mischt linear zwischen Trocken- und Vollschub.

#### 5.4 Spool-Dynamik `[JSB FGTurbine.h:326-340, FGTurbine::Seek()]`

Weder das Modell noch das Triebwerks-XML deklarieren Spool-Funktionen; JSBSim setzt Defaults ein:

```
delay      = faktor · 90 / (BPR + 3)          BPR = 0,4  →  faktor · 26,47 %/s
rate(N2)   = delay / (1 + 3·(1−n)³ + (1−ρ/ρ₀)),  n = min(1, N2norm + 0,1)
faktor:  N1 hoch 1,0 | N1 runter 2,4 | N2 hoch 1,0 | N2 runter 3,0
```

| Größe | Wert | Herkunft |
|---|---|---|
| N2-Beschleunigung bei Leerlauf, SL | **8,3 %/s** | `[ABL]` 26,47/(1+3·0,9³) |
| N2-Beschleunigung nahe MIL, SL | **26,5 %/s** | `[ABL]` |
| Leerlauf → MIL (N2 53→100 %), SL | **≈ 2,65 s** | `[ABL]` ∫dN2/rate über die obige Kurve |
| MIL → Leerlauf, SL | **≈ 0,88 s** | `[ABL]` dieselbe Integration mit faktor 3,0 |

**Herunterfahren ist ~3× schneller als Hochfahren** — eine Eigenschaft von JSBSims generischem
Turbinenmodell, nicht des F100. Beide Zeiten wachsen mit der Höhe (der `(1−ρ/ρ₀)`-Term).

Sonstige Kennwerte sind reine Algebra, keine Physik: `EGT = TAT + 363,1 + Throttle·357,1`,
`Öldruck = 0,62·N2`, `Leerlauf-Kraftstofffluss = 17800^0,2 · 107 = 758 pph` `[JSB]`.

#### 5.5 Die Throttle-Abbildung — der wichtigste Fallstrick

`f16.xml → <channel name="Throttle">` enthält genau eine Komponente:
`fcs/throttle-pos-norm = 2 · fcs/throttle-cmd-norm` `[XML]`. JSBSims Turbine liest die Position und
schneidet alles über 1,0 als Nachbrennerkommando ab `[JSB FGTurbine::Calculate]`:

| `fcs/throttle-cmd-norm` | `ThrottlePos` | Wirkung |
|---|---|---|
| 0,00 | 0,0 | Leerlauf |
| **0,50** | 1,0 | **MIL** (17.800 lbf SL/M0) |
| 0,75 | 1,5 | halber Nachbrenner |
| 1,00 | 2,0 | **max. AB** (29.000 lbf SL/M0) |

FlightBox schreibt `fcs/throttle-cmd-norm` direkt (`FBFdm::SetControls`), mit einem eigenen
Slew-Limit, weil ein Sprung die Drehzahl-ODE sprengt `[DOC fdm.md]`. Ein Regler, der „Throttle 0,8"
als „80 % Schub" liest, liegt also um den halben Nachbrenner daneben.

#### 5.6 Thruster

`engine/direct.xml`: `<direct name="Direct">`, leer — der Schub des Triebwerks IST der Schub am
Rumpf. Ort (0/0/0) in, Ausrichtung 0/0/0 `[XML]`, also durch den Strukturursprung und exakt entlang
der Rumpfachse: **kein Schubvektor-Versatz, kein Anstellwinkel der Düse**.

---

### 6. Aerodynamik — `f16.xml → <aerodynamics>`

#### 6.1 Aufbau

**41 `<function>`, 35 `<table>`** `[MESS]`, verteilt auf eine globale Funktion und sechs Achsen:

| Achse | Funktionen | Tabellen | JSBSim-Rahmen |
|---|---|---|---|
| (global) | 1 | 1 | — (`aero/function/kCLge`, von den Auftriebstermen als Faktor gelesen) |
| `DRAG` | 8 | 6 | Windachse, +x entgegen Flugrichtung `[JSB]` |
| `SIDE` | 6 | 3 | Windachse, +y |
| `LIFT` | 6 | 5 | Windachse, +z nach oben |
| `ROLL` | 8 | 8 | Körperachse |
| `PITCH` | 4 | 4 | Körperachse |
| `YAW` | 8 | 8 | Körperachse |

Jede Funktion ist ein `<product>`, das den **dimensionslosen Beiwert selbst mit dem Staudruck und der
Bezugsfläche multipliziert** — die Aerofunktionen liefern also KRÄFTE (lbf) bzw. MOMENTE (lbf·ft),
nicht Beiwerte. Muster:

```
Kraft   = qbar-psf · Sw-sqft · [Beiwert oder Tabelle] · [ggf. Stellgröße] · [ggf. kCLge]
Moment  = qbar-psf · Sw-sqft · (bw-ft | cbarw-ft) · […]
Rate    = … · (bi2vel | ci2vel) · (p|q|r)-aero-rad_sec      bi2vel = b/2V, ci2vel = c̄/2V [JSB]
```

#### 6.2 Stützstellenraster (durchgängig, gilt für ALLE Tabellen)

| Variable | Stützstellen (rad) | in Grad |
|---|---|---|
| `aero/alpha-rad` | −0,175 −0,087 0 0,087 0,175 0,262 0,349 0,436 0,524 0,611 0,698 0,785 | **−10 −5 0 5 10 15 20 25 30 35 40 45** |
| `aero/beta-rad` (13 Spalten) | −0,524 … +0,524 in 0,0873-Schritten | −30 … +30 in 5°-Schritten |
| `aero/beta-rad` (7 Spalten) | −0,524 −0,349 −0,175 0 0,175 0,349 0,524 | −30 −20 −10 0 10 20 30 |
| `fcs/elevator-pos-rad` (5 Spalten) | −0,436 −0,218 0 +0,218 +0,436 | −25 −12,5 0 +12,5 +25 |
| `velocities/mach` | tabellenspezifisch, s.u. | — |

Das ist exakt das Raster des kanonischen offenen F-16-Modells (Stevens & Lewis / AeroBench)
`[DOC flight-controls-flcs.md §Canonical model constants]`.

#### 6.3 Globale Funktion

| Name | Beschreibung | Unabhängige Var. | Verlauf |
|---|---|---|---|
| `aero/function/kCLge` | Auftriebsänderung durch Bodeneffekt | `aero/h_b-mac-ft` = (AGL − z_MAC)/Spannweite `[JSB FGAuxiliary.cpp:234]` | 13 Stützstellen 0,0…1,1; **1,229** am Boden → 1,000 ab h/b = 1,0 `[XML]` |

**+22,9 % Auftrieb im vollen Bodeneffekt**, monoton fallend bis eine Spannweite Höhe. Multipliziert
JEDEN Term der LIFT-Achse außer `CLq_Dsb`.

#### 6.4 Achse `DRAG`

| Funktion | Unabhängige Var. | Stellgröße | Kernzahlen |
|---|---|---|---|
| `CDDh` — Widerstand durch Höhenruderstellung | α × δh (2D, 12×5) | — | **Das ist der GRUNDWIDERSTAND**: CD(α=0, δh=0) = **0,0210**; CD(45°, 0°) = 1,478; CD(−10°, ±25°) = 0,217/0,230 `[XML]` |
| `CDmach` — Wellenwiderstand | Mach (4) | — | 0 bis M 0,81 → **+0,0230** bei M 1,10 → 0,0150 bei M 1,80 `[XML]` |
| `CDDlef` — Vorflügel | α (12) | `fcs/lef-pos-rad` | 0,003 (−10°) … 0,024 (45°), Minimum 0 bei α=0 `[XML]` |
| `CDDflaps` — Landeklappen | — | `fcs/flaperon-mix-rad` | konstant **0,0800** je rad `[XML]` — s. §7.9 |
| `CDgear` — Fahrwerk | — | `gear/gear-pos-norm` | konstant **0,0270** voll ausgefahren `[XML]` |
| `CDDsb` — Bremsklappe | α (12) | `fcs/speedbrake-pos-rad` | −0,0545 (−10°) … **+0,2324** (20°) … +0,1196 (45°) je rad `[XML]`; bei 60° = 1,047 rad also ΔCD ≈ 0,24 |
| `CDq` — Nickrate | α (12) | q·c̄/2V | −2,139 (−5°) … **+24,105** (45°) `[XML]` |
| `CDq_Dlef` — Nickrate × Vorflügel | α (12) | q·c̄/2V · δlef | 0,015…0,067 `[XML]` |

#### 6.5 Achse `SIDE`

| Funktion | Unabhängige Var. | Kernzahlen |
|---|---|---|
| `CYb` — Seitenkraft aus β | — (Konstante) | **−1,1460** je rad `[XML]` |
| `CYb_M` — Machkorrektur dazu | Mach (3) | 0 bei M 0,4 → +0,0573 bei M 1,2 → **−0,1719** bei M 1,6 `[XML]` |
| `CYDa` — aus Querruder | — | **−0,0226** je rad `[XML]` |
| `CYdr` — aus Seitenruder | — | **+0,0860** je rad `[XML]` |
| `CYp` — aus Rollrate | α (12) | −0,188 (0°) → +0,611 (30°) → **−0,227** (45°) `[XML]` |
| `CYr` — aus Gierrate | α (12) | +0,876 (0°) → +1,210 (35°) → **−1,040** (45°) `[XML]` |

Beide Ratenterme **wechseln jenseits 35° AoA das Vorzeichen** — das ist die Nachstall-Signatur, die
TP-1538 auch bei −10…45° noch hergibt.

#### 6.6 Achse `LIFT`

| Funktion | Unabhängige Var. | Stellgröße | Kernzahlen |
|---|---|---|---|
| `CLDh` | α × δh (2D, 12×5) | ×kCLge | **Der GRUNDAUFTRIEB.** CL(0°,0°) = 0,100; **Maximum CL = 1,900 bei α = 35°, δh = −25°**; CL(45°,0°) = 1,674; CL(−10°,0°) = −0,754 `[XML]` |
| `CLDlef` | α (12) | δlef × kCLge | −0,012 (−10°) … +0,028 (35–40°) je rad `[XML]` |
| `CLDflaps` | — | `flaperon-mix-rad` × kCLge | konstant **0,3500** je rad `[XML]` — s. §7.9 |
| `CLDsb` | α (12) | δsb × kCLge | +0,3684 (0°) … **−0,1836** (35°) je rad `[XML]` |
| `CLq` | α (12) | q·c̄/2V × kCLge | +8,71 (−10°) … **+31,40** (5°) … +25,82 (45°) `[XML]` |
| `CLq_Dsb` | α (12) | q·c̄/2V · δsb | −0,256 … +0,063 `[XML]`, **ohne** kCLge |

**`CL_max ≈ 1,9` und der Abfall danach** (35° → 40° → 45°: 1,900 / 1,898 / 1,753 bei δh=0
`[XML]`) sind die einzige Stall-Information, die das Modell trägt: eine flache Kuppe, kein Abriss.

#### 6.7 Achse `ROLL`

| Funktion | Unabhängige Var. | Kernzahlen |
|---|---|---|
| `Clb` — Rollmoment aus β | α × β (2D, 12×13) | Antisymmetrisch in β, Cl(α, β=0) ≡ 0. **Maximum |Cl| = 0,091 bei α = 30°, β = −30°** `[XML]` |
| `Clb_M` | Mach (6) | 0 (M 0,6) → **+0,1891** (M 0,8) → +0,0859 (M 1,6), ×β `[XML]` |
| `Clp` — Rolldämpfung | α (12) | **−0,443 (0°)** → −0,230 (30°) → **−0,100 (45°)** `[XML]` |
| `Clr` — aus Gierrate | α (12) | −0,126 (−10°) … +0,680 (30°) … **−0,330 (45°)** `[XML]` |
| `Clda` — aus Querruder | α × β (2D, 12×7) | **0,053 bei α=0** → 0,012…0,017 bei 45°: die Querruderwirkung fällt bei hohem α auf **ein Viertel** `[XML]` |
| `Clda_M` | Mach (3) | 0 (M 0,6) → **−0,0630** (M 1,2 und 1,6), ×α×δa `[XML]` |
| `Cldr` — aus Seitenruder | α × β (2D, 12×7) | ~0,014 bei kleinem α, → ~0,001…0,008 bei 45° `[XML]` |
| `Cldr_M` | Mach (2) | 0 (M 0,6) → −0,0201 (M 1,6) `[XML]` |

Die **Rolldämpfung viertelt sich zwischen 0° und 45° AoA** — der physikalische Grund, warum ein
Rollkommando bei hohem α im Modell so lange braucht.

#### 6.8 Achse `PITCH`

| Funktion | Unabhängige Var. | Kernzahlen |
|---|---|---|
| `CmDh` | α × δh (2D, 12×5) | **Das GRUNDNICKMOMENT.** Cm(0°, 0°) = **−0,009**; Cm(0°, −25°) = +0,186; Cm(0°, +25°) = −0,184. Bei α = 45°: +0,192 / +0,032 / −0,005 `[XML]` |
| `Cma_M` | Mach (6) | 0 (M 0,6) → +0,0974 (M 0,8) → **−0,9626 (M 1,0)** → −0,7907 (M 1,2), ×α `[XML]` |
| `CmDsb` | α (12) | −0,0036 (0°) → +0,0303 (5°) → **−0,0884 (45°)** je rad `[XML]` |
| `Cmq` — Nickdämpfung | α (12) | **−7,21 (−10°)** … −5,23 (0°) … −6,00 (45°) `[XML]` |

**Es gibt keine separate `Cm(α)`-Tabelle** — die statische Längsstabilität steckt vollständig in
`CmDh`s Spalte δh = 0. Der Verlauf dort ist −0,046 / −0,009 / −0,005 / −0,006 / +0,010 / +0,006 /
−0,001 / +0,014 / 0,000 / −0,013 / +0,032 (α = −10…45°) `[XML]`: um Null herumkriechend, mit
positiven Abschnitten — genau die **entspannte Längsstabilität**, die den geschlossenen Regelkreis
zwingend macht. Die Machkorrektur `Cma_M` kippt bei M 1,0 stark nach negativ: der klassische
Machtuck-Stabilitätssprung.

#### 6.9 Achse `YAW`

| Funktion | Unabhängige Var. | Kernzahlen |
|---|---|---|
| `Cnb` — Wetterfahne | α × β (2D, 12×13) | **Vorzeichenwechsel bei α ≈ 35°**: bei α ≤ 30° ist Cn(β>0) > 0 (stabil), ab α = 35° negativ (instabil) `[XML]` |
| `Cnb_M` | Mach (8) | 0 / −0,0688 / −0,0172 / +0,0229 / 0 / −0,0688 / +0,0057 / +0,0688 über M 0,6…1,6 `[XML]` |
| `Cnp` — aus Rollrate | α (12) | −0,052 (0°) → +0,024 (15°) → **−0,240 (40°)** `[XML]` |
| `Cnr` — Gierdämpfung | α (12) | −0,378 (0°) → **−1,020 (40°)** → −0,840 (45°) `[XML]` |
| `Cnda` — aus Querruder | α × β (2D, 12×7) | +0,010 bei α=0 → **negativ ab α ≈ 25°**: adverses Gieren kippt zu proversem `[XML]` |
| `Cnda_M` | Mach (6) | 0 … +0,0149 (M 0,8) … −0,0040 (M 1,6) `[XML]` |
| `Cndr` — aus Seitenruder | α × β (2D, 12×7) | **−0,045 bei α=0**, auf −0,016 bei 45° gefallen — Seitenruderwirkung drittelt sich `[XML]` |
| `Cndr_M` | Mach (3) | 0 (M 0,6) → +0,0401 (M 1,2) → +0,0716 (M 1,6) `[XML]` |

**Der `Cnb`-Vorzeichenwechsel bei 35° AoA ist die Departure-Grenze des Modells** — jenseits davon
verstärkt Schiebewinkel sich selbst. Er ist echte TP-1538-Physik, kein Artefakt.

#### 6.10 Was die Aerodynamik NICHT enthält

| Fehlt | Konsequenz |
|---|---|
| jede α-Information über 45° und unter −10° | Deep Stall nicht darstellbar (§1.1); JSBSim klemmt `[JSB]` |
| `Cmadot` / `Clbdot` (Abwind-Verzögerungsterme) | kein α̇-Term im Nickmoment (die Mk-82 hat einen, die F-16 nicht) `[XML]` |
| Machabhängigkeit der GRUND-Tabellen | CDDh/CLDh/CmDh/Clb/Cnb sind machunabhängig; Kompressibilität kommt nur über die neun `*_M`-Zusatzterme + CDmach `[XML]` |
| Reynolds-/Höhenabhängigkeit | keine |
| Außenlast-Aerodynamik | keine — FlightBox modelliert sie als CdA-Zusatzkraft über `<external_reactions>` `[DOC CLAUDE.md]` |
| Bodeneffekt auf Widerstand/Nickmoment | nur der Auftrieb (`kCLge`) ist bodeneffektiert `[XML]` |

---

### 7. Die FLCS als `<flight_control>`-XML

`f16.xml → <flight_control name="F-16 FC">`, Zeilen 311–981: **11 Kanäle, 58 Komponenten,
5 Verstärkungspläne** `[MESS]`. Vier Interface-Properties werden vorab deklariert:
`fcs/alpha-norm`, `fcs/hook-engage`, `fcs/canopy-engage`, `fcs/fbw-override` `[XML]`.

#### 7.1 Kanalübersicht

| # | Kanal | Zeilen | Komponenten | Ausgang in die Aerodynamik |
|---|---|---|---|---|
| 1 | Flaps (TEF) | 316–351 | 3 | **keiner** (s. §7.9) |
| 2 | Roll | 353–490 | 11 | `fcs/aileron-pos-rad` |
| 3 | Pitch | 492–671 | 13 | `fcs/elevator-pos-rad` |
| 4 | Yaw | 673–762 | 7 | `fcs/rudder-pos-rad` |
| 5 | Landing Gear | 764–802 | 3 | `gear/gear-pos-norm` (→ CDgear) |
| 6 | Leading Edge Flap | 804–856 | 4 | `fcs/lef-pos-rad` |
| 7 | Throttle | 858–866 | 1 | — (Triebwerk) |
| 8 | Speedbrake | 868–936 | 5 | `fcs/speedbrake-pos-rad` |
| 9 | Hook | 938–955 | 1 | — |
| 10 | Canopy | 957–981 | 2 | — |

#### 7.2 Roll-Kanal

```
velocities/p-aero-rad_sec ──×0,31821──► roll-rate-norm
fcs/aileron-cmd-norm ──┬──(−)──► roll-trim-error ──► PID(3,0 / 5e−4 / −1,25e−3) ──┐
                       └───────────────────────────────────────────────► summer ◄─┘
                                                    roll-rate-command (clip ±1)
                                                        │
              ┌─────────────────────────────────────────┴──────────────────┐
              ▼                                                            ▼
  aerosurface_scale ±0,375 rad ──► fcs/aileron-pos-rad ──► AERO            switch(fbw-override)
                                                                            │
                        kinematic 0,3 s ──► left-aileron-pos-norm ──► scheduled_gain(Mach)
                        ──► aileron-speed-compensated ──► ±0,375 rad ──► left/right-aileron-pos-rad
                        ──► dht-left/right-pos-rad (nur Animation)  +  flaperon-mix (§7.9)
```

| Größe | Wert | Bedeutung |
|---|---|---|
| Rollraten-Normierung | **0,31821** | `[XML]`; Vollausschlag entspricht p = 1/0,31821 = **3,1426 rad/s = 180,1 °/s** `[ABL]` |
| PID kp/ki/kd | 3,00000 / 0,00050 / **−0,00125** | `[XML]` — kd ist NEGATIV |
| PID-Freigabe | `vc-kts ≥ 20` | `[XML]` |
| Querruderausschlag | ±0,375 rad = **±21,49°** | `[XML]` |
| Aktuatorzeit (kinematic) | 0,3 s von −1 nach +1 | `[XML]` — **NICHT im Aeropfad**, s.u. |
| Machplan `aileron-speed-compensated` | 1,00 bei M 0 → **0,15 bei M 1,0** | `[XML]` |

**Zwei Befunde, die für FlightBox zählen:**

1. **Die Aerodynamik liest `fcs/aileron-pos-rad`, und das kommt DIREKT aus `roll-rate-command`** —
   vor dem FBW-Override-Schalter, vor der Kinematik, vor dem Machplan `[XML]`. Der Rollaktuator hat
   im Kraftpfad also **weder Ratenbegrenzung noch Machkompensation**; beide wirken nur auf die
   Flächenpositionen für die Animation und den Flaperon-Mixer.
2. Damit **überbrückt `fcs/fbw-override` den Roll-PID NICHT** (§7.10).

#### 7.3 Pitch-Kanal

```
accelerations/n-pilot-z-norm ──┬─► g-load-corrected ──×0,020──► g-load-norm ──┐
   cos(pitch)·cos(roll) ───(−)─┘                                              │
velocities/q-aero-rad_sec ──×6,2──► pitch-rate-norm ─────────────────────────┤
fcs/elevator-cmd-norm + pitch-trim-cmd-norm ─► clip[−1, +0,44] ─► elevator-cmd-limiter
   └─► scheduled_gain(α) ─► elevator-scheduler ────────────────────────────►─┤
                                                            pitch-trim-error ◄┘
                                          └─► PID(0,3 / 0,025 / 0) clip ±1 ─► g-load-pid
aero/alpha-rad ──×1,0472──► alpha-limiter-norm ──┐
elevator-scheduler + alpha-limiter-norm + g-load-pid ─► pitch-scheduler (clip ±1)
   └─► switch(fbw-override) ─► kinematic 0,3 s ─► elevator-pos-norm
   └─► aerosurface_scale ±0,436 rad ─► fcs/elevator-pos-rad ─► AERO
```

| Größe | Wert | Bedeutung |
|---|---|---|
| Kommandogrenzen | **clip [−1, +0,44]** | `[XML]`; Kommentar: „G limit of 9 G positive and 4 G negative (44,44 % of 9 G)". −1 = voll ziehen (JSBSim: +Höhenruder = Nase runter) |
| α-Autoritätsplan | 1,0 bei α = 0 → 0,11 bei ±28,6° → **0,0 bei ±30°** | `[XML]` `fcs/elevator-scheduler` |
| α-Limiter | ×1,0472 auf α in rad, ADDITIV nach Nase-unten | `[XML]`; erreicht 1,0 erst bei α = 54,7° `[ABL]` |
| Nickraten-Normierung | **6,2** | `[XML]`; Vollausschlag entspricht q = 1/6,2 = 0,1613 rad/s = **9,24 °/s** `[ABL]` |
| g-Normierung | **0,020** | `[XML]`; Vollausschlag entspricht **50 g** `[ABL]` |
| PID kp/ki/kd | 0,3000 / 0,0250 / 0 | `[XML]`, Freigabe `vc-kts ≥ 5` |
| Höhenruderausschlag | ±0,436 rad = **±24,98°** | `[XML]` |
| Aktuatorzeit | 0,3 s von −1 nach +1 = **167 °/s** | `[ABL]` |
| DHT-Mischung | `dht-left = −elev − ail`, `dht-right = +elev + ail`, clip ±0,436 | `[XML]` — **wird von keiner Aerofunktion gelesen** |

**Das Verhältnis 9,24 °/s Nickrate zu 50 g ist der Kern der Nick-Charakteristik:** die Ratenschleife
dominiert die g-Schleife um mehr als das Dreißigfache. Der Kanal ist praktisch ein
**Nickraten-Kommando mit g-Beimischung**, nicht das Nz-Kommando des echten Jets — und genau daraus
folgt die gemessene Einhüllende (§8).

**Vorzeichen-Befund zur g-Rückführung.** `accelerations/n-pilot-z-norm` ist in JSBSim die spezifische
Kraft in Körper-z am Augpunkt, ohne Schwerkraft und **ohne Negierung**
`[JSB FGAuxiliary.cpp:219-225, FGAccelerations.cpp:189]` — sie ist im Geradeausflug **−1**, während
`accelerations/Nz` (das, was FlightBox liest) **+1** ist. Nachgemessen: `Nz = +1,000`,
`n-pilot-z-norm = −1,000`, `n-pilot-z-correction = +1,000`, `g-load-corrected = −1,482` im
getrimmten Horizontalflug `[MESS]` (JSBSim-CLI-Sonde auf diesem Modell, 350 KCAS/10.000 ft). Die
„Schwerkraftkorrektur" des Modells zieht `cos·cos` ab, statt es zu addieren, und **verdoppelt damit
den Offset, den ihr Kommentar beseitigen will**. Wirkungslos bleibt das nur, weil der Term mit 0,020
gewichtet ist: der Fehler ist ±0,02 im Signal.

#### 7.4 Yaw-Kanal

| Größe | Wert | Bedeutung |
|---|---|---|
| Gierraten-Plan über `velocities/vg-fps` | 80 → 0 \| 100 → 15 \| **150 → 100** | `[XML]`; ab 150 ft/s (89 kt) Bodengeschwindigkeit entspricht Vollausschlag r = 1/100 rad/s = **0,57 °/s** `[ABL]` |
| Quer-g-Normierung | 0,25 | `[XML]`; Vollausschlag = 4 g seitlich `[ABL]` |
| PID kp/ki/kd | 0,105500 / 0,000010 / 0,00005 | `[XML]`, Freigabe `vc-kts ≥ 10` |
| Seitenruderausschlag | ±0,524 rad = **±30,02°** | `[XML]` |
| Aktuatorzeit | 0,4 s von −1 nach +1 = **150 °/s** | `[ABL]` |

Die Gierratenrückführung ist damit **extrem hart**: der Kanal duldet oberhalb 89 kt praktisch keine
Gierrate. Ein ARI (Aileron-Rudder-Interconnect) existiert nicht — es gibt keinen Pfad vom
Rollkommando ins Seitenruder `[XML]`.

`fcs/rudder-pos-norm` wird **zweimal geschrieben** (vom PID `fcs/yaw-load-pid` und von der Kinematik
`fcs/rudder-position`) `[XML]`; die Kinematik läuft später im Kanal und gewinnt `[JSB]`.

#### 7.5 Leading Edge Flap — der α-/Mach-Plan

`<switch name="fcs/lef-pos-rad">`, Tests in dieser Reihenfolge (erster Treffer gewinnt `[JSB
FGSwitch]`):

| # | Bedingung | δLEF |
|---|---|---|
| 1 | `gear-wow == 1` UND `gear-pos-norm > 0` | **−2°** (−0,0349 rad) |
| 2 | `gear-pos-norm == 0` UND `α > 15°` | **+25°** (0,436 rad) |
| 3 | `gear-wow == 0` UND `α > 5°` | **+15°** (0,262 rad) |
| 4 | `mach > 0,9` | **−2°** |
| — | sonst | 0° |

`[XML]`. Normierung ×2,293578 (= 1/0,436, also 25° ≙ 1,0 `[ABL]`), Kinematik 3 s, Skalierung
`fcs/lef-pos-deg` auf ±25°. Der Plan ist also eine **Dreistufen-Treppe**, kein stetiger Plan wie
beim echten Jet.

#### 7.6 Landing Gear

| Größe | Wert |
|---|---|
| `fcs/gear-wow` | 1, wenn **beide** Hauptfahrwerke (`gear/unit[1]` und `[2]`) WOW melden `[XML]` |
| Transitzeit | **5,0 s** von 0 nach 1 `[XML]` |
| Bugradsteuerung `fcs/scheduled-steer-pos-deg` | über `vg-fps`: 10 → **80°** \| 50 → 15° \| 150 → **2°** `[XML]` |

Der Lenkplan ist genau der, den `procedures-takeoff-taxi.md` als „NWS-Gain-vs-Groundspeed-Prinzip"
beschreibt `[DOC]` — hier mit konkreten Zahlen.

#### 7.7 Speedbrake

| Größe | Wert |
|---|---|
| Auto-Auslösung | `α ≥ 53°` UND `v ≤ 18 ft/s` → volle Bremsklappe `[XML]` |
| Pilotenauslösung | `fcs/speedbrake-cmd-norm == 1` (binär, kein Zwischenwert) `[XML]` |
| Fahrwerksplan | Fahrwerk ein: ×1,0 → **60°**; Fahrwerk aus: ×0,71667 → **43,0°** `[XML]` |
| Fahrzeit | 0 → 60° in **1,0 s** `[XML]` |

Die 60°/43°-Paarung stimmt exakt mit `hotas.md`s ED-Zahlen überein `[DOC]` — einer der wenigen
Punkte, an denen Modell und Handbuch sich direkt decken.

Der Auto-Trigger ist die einzige Deep-Stall-Vorkehrung des Modells (§1.1); der Kommentar nennt sie
so („to prevent deep stall … just enough pitch down moment"). Bei α ≥ 53° stehen die Aerotabellen
allerdings längst auf ihrem 45°-Klemmwert.

#### 7.8 Trailing Edge Flap (Kanal „Flaps")

| Bedingung | δTEF |
|---|---|
| `vc-kts < 250` | **+20°** (0,349 rad) |
| `mach > 0,9` | **−2°** (−0,0349 rad) |
| sonst | 0° |

`[XML]`, Normierung ×2,864789 (= 1/0,349 `[ABL]`), Kinematik 3 s. **Ohne Fahrwerksbezug** — die
Klappen fahren allein auf Fahrtmesser.

#### 7.9 Der Flaperon-Mixer — behoben per Modell-Delta D1

> **Status: BEHOBEN.** Der hier beschriebene Vorzeichenfehler ist seit `sim/assets/MODEL-DELTAS.md` **D1**
> nicht mehr Teil des geflogenen Modells. Der Abschnitt bleibt als BEFUND stehen — er ist der Beleg des
> Deltas —, aber alles unter „Der Befund" beschreibt den UPSTREAM-Stand (`sim/vendor/jsbsim`), nicht das,
> was FlightBox fliegt. Was heute fliegt, steht unter „Nach dem Delta". Die Folgen für §8 und §9 sind dort
> nachgemessen.

##### Der Befund (Upstream-Stand)

```
left-flaperon-norm  = −tef-control − aileron-speed-compensated
right-flaperon-norm = +tef-control − aileron-speed-compensated
flaperon-summer     = left + right = −2 · aileron-speed-compensated      ← TEF KÜRZT SICH WEG
flaperon-mix-rad    = 1,4324 · flaperon-summer = −2,8648 · ail_sc
```
`[XML]` Zeilen 444–470, `[ABL]` die Zusammenfassung.

`fcs/flaperon-mix-rad` ist die Stellgröße von **`CLDflaps` (0,35/rad) und `CDDflaps` (0,08/rad)**
`[XML]` — den einzigen beiden Verbrauchern. Daraus folgen zwei Dinge:

1. **Die Landeklappen haben im Modell KEINE aerodynamische Wirkung.** `fcs/tef-control` erreicht
   keine Aerofunktion; sein einziger Pfad kürzt sich in der Summe heraus `[MESS]`
   (`grep -n "tef-control\|flaperon" f16.xml` → 8 Treffer, alle im Mixer).
2. **Jedes Rollkommando erzeugt stattdessen einen großen SYMMETRISCHEN Auftriebs- und
   Widerstandssprung.** Gemessen an diesem Modell (getrimmt, 350 KCAS, 10.000 ft, Querruderkommando
   +0,5 als Sprung) `[MESS]`:

   | t (s) | `aileron-cmd` | `flaperon-mix-rad` | `Nz` | Bank | `fbz-aero` (lbf) |
   |---|---|---|---|---|---|
   | 1,92 | 0,0 | −0,000 | **+1,000** | −0,1° | −20.630 |
   | 2,11 | 0,5 | **−1,106** | **−0,880** | +1,5° | +20.763 |
   | 2,30 | 0,5 | −1,250 | −0,931 | +11,1° | +17.982 |

   Rechnerisch: ΔCL = 0,35 · (−1,106) = −0,387; ΔL = −0,387 · 377 psf · 300 ft² = **−43.800 lbf**
   `[ABL]` — die Messung zeigt −41.400 lbf Auftriebsverlust. Der Widerstandsterm läuft mit:
   ΔCD = 0,08 · (−1,106) = −0,0885 → **−10.000 lbf, also NEGATIVER Widerstand** `[ABL]`.

   Und weil das Vorzeichen an `ail_sc` hängt, ist der Effekt **asymmetrisch**: Rollen nach rechts
   entlastet und schiebt an, Rollen nach links belastet und bremst.

Das ist die folgenreichste Einzelheit dieser Datei.

##### Nach dem Delta (der geflogene Stand)

```
left-flaperon-norm  = +tef-control + aileron-speed-compensated
right-flaperon-norm = +tef-control − aileron-speed-compensated
flaperon-summer     = left + right = +2 · tef-control            ← der QUERRUDER-Anteil kürzt sich weg
flaperon-mix-rad    = 0,1745329 · flaperon-summer = 0,349 · tef  = der kommandierte Klappenwinkel in rad
```

Herleitung, Beleg und der exakte Diff: `sim/assets/MODEL-DELTAS.md`, Eintrag **D1**. Nachgemessen am
geflogenen Modell, gleiches Profil wie oben `[MESS]`:

| Größe | vorher | nachher |
|---|---|---|
| `flaperon-mix-rad` bei reinem Rollkommando ±0,5 | −1,28 / +1,29 | **0,0000** |
| `Nz`-Spitze im Rollansatz (Roll rechts / links) | −1,54 g / +3,46 g | **+0,97 g / +0,97 g** |
| `fbx-aero` im Rollansatz (rechts) | **+6.420 lbf** (Schub!) | **−5.267 lbf** (Widerstand) |
| `flaperon-mix-rad` bei voll ausgefahrenem TEF | −0,0002 rad | **0,3490 rad** (= 20°) |
| ΔCL / ΔCD der Landeklappen | ≈ 0 | **0,122 / 0,028** |
| Auftriebszuwachs der Klappen bei 202 KCAS | −3 lbf | **+5.059 lbf** |
| Rollrate 400 KCAS rechts / links | +187,8 / −132,3 °/s (Asym. **55,5**) | +156,4 / −156,6 °/s (Asym. **0,2**) |
| 11°-AoA-Trimmgeschwindigkeit, Fahrwerk aus, 40 % Sprit | 165 KCAS | **154 KCAS** |

NICHT gefallen ist die **Reiseflug-Restasymmetrie**: Median |φ| auf eingeschwungenen Routenbeinen
0,186° → 0,185° über 60.900 Proben `[MESS]`. Sie hängt nicht am Mixer — bei |φ| < 2° ist `ail_sc` so
klein, dass der frühere Symmetrieanteil selbst klein war; die 0,18° sind der stationäre Rest des
Roll-PID (§7.2), nicht des Mixers.

#### 7.10 Was `fcs/fbw-override` wirklich überbrückt

| Kanal | Schalter vorhanden | Ersetzt | Erreicht die Aerodynamik? |
|---|---|---|---|
| Pitch | ja (`fcs/pitch-scheduler-switch`, Z. 625) | `pitch-scheduler` → `elevator-cmd-limiter` | **JA** — der Aeropfad läuft durch den Schalter |
| Roll | ja (`fcs/roll-rate-command-switch`, Z. 414) | `roll-rate-command` → `aileron-cmd-norm` | **NEIN** — `fcs/aileron-pos-rad` wird vor dem Schalter abgegriffen |
| Yaw | **nein** | — | — |

`[XML]` + `[ABL]`. In der Übersetzung: **`fbw-override=1` schaltet die Nickschleife ab, lässt aber
Roll-PID und Gierschleife voll aktiv.** Der Rollpfad hinter dem Schalter bewegt nur Flächenpositionen
(Animation, DHT-Mischung, Flaperon-Mixer aus §7.9) — dessen Wirkung ist also real, nur eben nicht
die Rollmomenten-Wirkung. FlightBox setzt den Schalter im Default NICHT (`FBFdmSpawn::FbwOverride =
false` `[DOC fdm.md]`), fliegt also die modelleigene FLCS; die obige Tabelle gilt für jeden, der es
anders vorhat.

#### 7.11 Modell-FLCS gegen echte FLCS — die Fidelity-Tabelle

Design-Ziele aus `flight-controls-flcs.md` (Chuck Part 15 / ED / AIAA-Literatur), Ist-Zustand aus
`f16.xml`. **Diese Tabelle ist der eigentliche Zweck dieses Kapitels.**

| Merkmal | Echte FLCS `[DOC]` | Modell `[XML]` | Bewertung |
|---|---|---|---|
| Nick-Antwortart | **Nz-Kommando**, nahe der Grenze in ein α-Kommando übergeblendet | **Nickraten-Kommando** (Normierung 9,24 °/s) mit 2 %-g-Beimischung (50 g Vollausschlag) | **Grundlegende Abweichung.** Erklärt die gemessenen 5,6 g statt 9 g (§8.1) |
| g-Bereich | −3 … +9 g (CAT I) | Kommando-Clip [−1; +0,44] = das VERHÄLTNIS 9:4 ist da, der ABSOLUTWERT nicht | Struktur richtig, Betrag nicht |
| α-Limiter | harte Grenze **25°**, g-Plan +7,3 g bei 20°, +1 g bei 25° | weicher Autoritätsplan: 100 % bei 0°, 11 % bei 28,6°, 0 % bei 30°, plus additiver Nase-unten-Term (1,0 erst bei 54,7°) | Anders geformt; effektiv begrenzt das Modell bei **α ≈ 13°** im Vollausschlagszug (§8.1) |
| Roll-Antwortart | **stabilitätsachsen-Rollrate Ps** | körperfeste Rollrate p (`p-aero-rad_sec`) | Abweichung: kein Achsentransfer |
| Roll-Vollausschlagsrate | 308 °/s (Handbuchgrenze des Jets, unbeladen) | **180 °/s Kommando**, gemessen 186 °/s (§8.2) | Modell rollt ~60 % der Jet-Rate |
| ARI (Querruder→Seitenruder) | vorhanden, mit Cutouts bei > 60 kt Radgeschwindigkeit und α > 35° | **fehlt vollständig** | Fehlt |
| Anti-Spin / Departure-Logik | Gierratenbegrenzer ab 35° α übernimmt Roll+Gier, Anti-Spin-Ruder unter −5° α / < 170 kt | **fehlt vollständig** | Fehlt |
| CAT I / CAT III | zwei Konfigurationssätze, CAT III limitiert α auf 15,5–15,8° und Rollrate −40 % | **kein Konfigurationsbegriff** | Fehlt |
| MPO (Manual Pitch Override) | vorhanden (Deep-Stall-Rettung) | **fehlt** (die Bremsklappen-Automatik §7.7 ist der Ersatz) | Anders gelöst |
| DBU (Digital Backup) | vorhanden | fehlt | Fehlt |
| Gain-Scheduling | auf Luftdaten (q̄ / Mach / Höhe) in allen Achsen | **fünf** Pläne, davon nur einer auf Mach (Querruder-Kompensation), zwei auf Bodengeschwindigkeit, einer auf α, einer auf Fahrwerksstellung | Rudimentär; die PIDs selbst sind **ungeplant** |
| Fahrwerks-Nickgesetz („Note 2") | Nickraten-Kommando auf 10° α im Landeanflug | **kein fahrwerksabhängiges Nickgesetz** | Fehlt |
| Gun-Kompensation | vorhanden | fehlt | Fehlt |
| Flächengrenzen | Höhenruder ±25°, Flaperon ±21,5°, Seitenruder ±30°, LEF 0…25° | **±24,98° / ±21,49° / ±30,02° / 0…25°** | **Deckungsgleich** |
| Flächenraten | 60 / 80 / 120 °/s (Stevens-Lewis-Aktuatormodell) | Nick 167 °/s, Gier 150 °/s, **Roll: unbegrenzt im Kraftpfad** (§7.2) | Zu schnell bzw. fehlend |
| Sensorsatz | Kreisel p/q/r, Nz (15 ft vor CG) + Ny, redundante α-Sonden, Luftdaten | `p/q/r-aero-rad_sec`, `n-pilot-z/y-norm` (am EYEPOINT, 143,2 in = 11,9 ft vor CG `[ABL]`), `aero/alpha-rad`, `vc-kts`, `mach`, `vg-fps` | Bemerkenswert nah, inkl. der vorverlegten Beschleunigungsmesser |
| Rechentakt | ~64 Hz (AFTI-Beleg, mittlere Konfidenz) | JSBSim-Rate; FlightBox fährt 100 Hz Substeps `[DOC CLAUDE.md]` | Erfüllt |
| Redundanz/Voting | vierkanalig, ±20 %-Stroke-Voting | keine | Fehlt (auch nicht sinnvoll simulierbar) |
| Seitenruder-Antwortart | Schiebewinkel-/Quer-g-Regelung | Quer-g (0,25) + **sehr harte** Gierraten-Rückführung (0,57 °/s Vollausschlag über 89 kt) | Struktur ähnlich, Verstärkung viel höher |

**Fazit für die Fidelity-Bewertung:** das Modell ist eine **Ratenkommando-Stabilisierung mit drei
PIDs**, nicht die Kommando-Augmentierung des echten Jets. Die STELLGRÖSSEN (Ausschläge, LEF-/
Bremsklappenpläne, Bugradlenkung) sind auffällig genau; die REGELGESETZE sind eine Näherung, und alle
Schutzfunktionen (CAT-Umschaltung, Anti-Spin, ARI, MPO, DBU) fehlen. Wer Prinzip 5 zitiert, zitiert
diese Zeile: bewertet wird, ob FlightBox DIESE FLCS treu kommandiert — nicht, ob DIESE FLCS die echte
ist.

---

### 8. Die gemessene Einhüllende

#### 8.1 Corner-Speed — `make -C sim test-corner`

Methode (`sim/src/clients/FBTestCornerSpeed.cpp`): Luftstart getrimmt bei **5.000 m**, Rollen auf **85°
Schräglage**, dann Vollausschlag `fcs/elevator-cmd-norm = −1` **durch die modelleigene FLCS**,
Mittelung über **4 s**; Corner = die langsamste Eintrittsgeschwindigkeit innerhalb 3 % der besten
Rate. Lauf am geflogenen Modell **nach Delta D1**, 23 Punkte `[MESS]`:

| KCAS | 260 | 300 | 340 | 360 | **380** | 400 | 440 | 500 | 560 | 620 |
|---|---|---|---|---|---|---|---|---|---|---|
| Drehrate °/s | 13,30 | 14,24 | 15,20 | 15,70 | **16,18** | 16,37 | 13,93 | 11,51 | 11,95 | 12,77 |
| nz (Mittel) | 3,13 | 3,81 | 4,59 | 5,01 | **5,44** | 5,77 | 5,60 | 5,38 | 6,24 | 7,32 |
| nz (Spitze) | 3,71 | 4,42 | 5,20 | 5,66 | **6,13** | 6,52 | 6,10 | 5,71 | 6,71 | 7,92 |
| α ° | 12,89 | 12,75 | 12,79 | 12,81 | **12,83** | 12,44 | 9,53 | 6,78 | 6,57 | 6,61 |

```
RESULT result=CORNER_FOUND cornerCasKt=380 cornerTurnRateDegS=16,1805
       cornerNz=5,43589 cornerAlphaDeg=12,834 bestTurnRateDegS=16,3652 points=23
```

Derselbe Lauf am Upstream-Stand ergab `cornerCasKt=380 cornerTurnRateDegS=16,2214 cornerNz=5,6251
cornerAlphaDeg=12,959 bestTurnRateDegS=16,2214` `[MESS]`. **Die Corner-GESCHWINDIGKEIT ist unverändert
380 KCAS**; das g an ihr fällt von 5,63 auf 5,44, weil der Rollansatz in die Schräglage keinen
Auftriebssprung mehr mitbringt. Der SCHEITEL der Kurve wandert dabei von 380 auf 400 KCAS
(bestTurnRateDegS 16,37) — 380 bleibt Corner, weil es das langsamste Band innerhalb 3 % des Besten ist.
Das Kriterium des Harness (Corner gefunden, Kurve monoton-dann-flach) hält unverändert, Exit 0; es
musste nicht nachgezogen werden.

**Wie das zu den Tabellen passt.** Das Modell hätte den Auftrieb: `CLDh` gibt bei α = 30° und
δh = −25° ein CL von 1,804, bei α = 13° nur ≈ 0,66 `[XML]`. Die Grenze ist also **nicht
aerodynamisch, sondern regelungstechnisch** — der Nickkanal (§7.3) fordert bei Vollausschlag eine
Nickrate von 9,24 °/s, und bei 380 KCAS/5.000 m stellt sich das Gleichgewicht bei α ≈ 13° und 5,6 g
ein. Genau deshalb wächst nz von 380 nach 620 KCAS weiter (bis 7,54), obwohl die Drehrate fällt: der
Kanal regelt eine RATE, und bei höherem Speed kostet dieselbe Rate mehr g.

Die 380 KCAS liegen dennoch **innerhalb** des in `aerodynamics-performance.md` publizierten
Corner-PLATEAUS von 330–440 KCAS `[DOC]` — die stärkste verfügbare Gegenprobe für eine Zahl, die
kein Handbuch tabelliert.

#### 8.2 Maximale Rollrate

Direkt am geflogenen Modell gemessen (JSBSim-CLI-Sonde, getrimmter Horizontalflug bei 10.000 ft,
`fcs/aileron-cmd-norm = ±1,0` als Sprung, Spitzenwert über 4 s), **nach Delta D1** und in BEIDE
Richtungen — die Richtungsspalte ist der eigentliche Messwert geworden `[MESS]`:

| Eintritt (KCAS) | 250 | 300 | 350 | 400 | 450 | 500 | 550 | **600** |
|---|---|---|---|---|---|---|---|---|
| Spitzen-p rechts (°/s) | 107,0 | 118,8 | 130,3 | 156,4 | 166,3 | 168,5 | 173,8 | **181,8** |
| Spitzen-p links (°/s) | −106,8 | −118,9 | −130,6 | −156,6 | −166,4 | −168,4 | −173,7 | **−181,7** |
| Asymmetrie (°/s) | 0,2 | 0,1 | 0,2 | 0,2 | 0,1 | 0,1 | 0,2 | 0,1 |
| Mach dabei | 0,52 | 0,61 | 0,69 | 0,77 | 0,84 | 0,91 | 0,98 | 1,05 |

Zum Vergleich derselbe Sweep am Upstream-Stand `[MESS]`: rechts 98,3 / 126,5 / 176,7 / 187,8 / 186,8 /
185,0 / 183,8 / 186,2, links −105,4 / −116,0 / −129,0 / −132,3 / −136,8 / −148,1 / −160,2 / −174,4 —
**bis zu 55,5 °/s Unterschied je nach Rollrichtung**, weil der Auftriebssprung aus §7.9 den Anstellwinkel
und damit `Clda` mitzog. Genau diese Asymmetrie ist verschwunden (≤ 0,2 °/s über den ganzen Sweep).

**Die Kommando-Normierung bleibt der Deckel**: 1/0,31821 rad/s = 180,06 °/s `[ABL]`, den der PID
(kp = 3,0) leicht überschwingt; erreicht wird er jetzt aber erst bei ~600 KCAS statt scheinbar schon bei
350–400. CLAUDE.mds „Rollrate ~190 °/s" ist damit als die alte, artefaktgetragene Rechts-Rollrate zu
lesen — der geflogene Wert ist **~182 °/s im Kommando-Sättigungsbereich, ~156 °/s bei 400 KCAS**. Unter
diesem Bereich ist die Querruderautorität (`Clda`, §6.7) die Grenze.

#### 8.3 Weitere belegte Messungen

Alle aus `doc/pilot.md` bzw. `doc/modules/f16/module.md`, dort mit ihren Läufen belegt `[DOC]`:

| Größe | Wert | Herkunft |
|---|---|---|
| Corner-Drehrate für den ω-Ansatz | 16,18 °/s gemessen gegen 15,3 °/s aus `g·√(n²−1)/V` (n = 5,4) | `pilot-ai.md` §„ω = er kurvt wie ich" |
| `BfmMinSpeedKt` | 300 KCAS (dort ~13 % unter dem Peak) | `modules-f16.md` §3.3 |
| Bremsverzögerung `BfmBrakeMs2` | 2,4 m/s²; nachgemessen über D1 hinweg 2,531 → 2,527 m/s² (Median, 163 Proben 325–400 KCAS bei 4.000 m) — die Klappen fahren erst unter 250 KCAS, das Band sieht sie nie | `pilot-ai.md` |
| Anfluggeschwindigkeit bei 11° AoA | 154 KCAS (Fahrwerk aus, 40 % Sprit) — vor D1 165 KCAS | §7.9 `[MESS]` |
| Landerollstrecke ab Aufsetzen, Payerne RWY23 | 785–928 m (D1 + Bremsfahrplan), vorher 1.341–1.597 m | `journal.md` |
| LOC-K.O.-Schwelle des Physik-Monitors | 150 °/s Rollrate | `pilot-ai.md` §5.7 |
| Kosten eines F-16-Steps | ~95–100 µs, phasenunabhängig (±7 %) | CLAUDE.md „Etappe 4" |

---

### 9. Akzeptierte Modell-Eigenschaften (Prinzip 5)

Was hier steht, ist **Wahrheit, nicht Fehlerliste**. `sim/vendor` ist read-only; diese Eigenschaften
sind zu KENNEN, nicht zu reparieren.

| # | Eigenschaft | Beleg | Praktische Folge |
|---|---|---|---|
| 1 | Vollausschlag kauft am Corner **5,4 g**, nicht 9 g | §8.1 `[MESS]` | Jeder g-Ansatz im Piloten muss die 5,4 benutzen; `BfmMaxG = 9,0` ist nur die Strukturgrenze `[DOC]` |
| 2 | Beste Drehrate **~16,4 °/s**, nicht ~20+ | §8.1 `[MESS]` | Die Kurvenzeit-Konstante der BFM-Phase leitet sich hieraus ab |
| 3 | Rollrate sättigt am **Kommando** bei ~182 °/s, und zwar erst um 600 KCAS | §8.2 `[MESS]` | Der Rollratenregler in `FBPilot` regelt gegen dieses Plateau; bei Kampfgeschwindigkeit (400 KCAS) sind es ~156 °/s |
| 4 | α wird im Zug faktisch bei **~13°** begrenzt, nicht bei 25° | §8.1 `[MESS]` | Das Modell kann seinen eigenen `CL_max` nie erreichen |
| 5 | ~~Rollen erzeugt Auftriebs-/Widerstandssprünge~~ | §7.9, **behoben per Delta D1** | Entfällt: `flaperon-mix-rad` bleibt bei reinem Rollkommando 0,0000, Rollrichtungs-Asymmetrie ≤ 0,2 °/s |
| 6 | ~~Landeklappen wirken aerodynamisch nicht~~ | §7.9, **behoben per Delta D1** | Entfällt: 20° TEF geben ΔCL 0,122 / ΔCD 0,028; die Anfluggeschwindigkeit bei 11° AoA fällt 165 → 154 KCAS |
| 7 | **Kein Deep Stall** — Tabellen enden bei α = 45° | §1.1 `[ABL]` | Ein „Deep-Stall-Test" prüft nichts; der Physik-Monitor entscheidet vorher |
| 8 | **Fanghaken ohne Kraft** | §4.4 `[MESS]` | Kein Fanghaken-Szenario möglich |
| 9 | **Kein ARI, keine Anti-Spin-Logik, kein CAT III** | §7.11 `[XML]` | Departure-Verhalten ist reine Aerodynamik + drei PIDs |
| 10 | **Schub null über 60.000 ft Dichtehöhe** (Klemmen, kein Abfall) | §5.2 `[ABL]` | Die Gipfelhöhe ist eine Kante |
| 11 | **Spool-down 3× schneller als Spool-up** | §5.4 `[ABL]` | Energie-Management-Regler dürfen sich nicht auf symmetrische Schubantwort verlassen |
| 12 | `fbw-override` überbrückt **nur Nick** | §7.10 `[ABL]` | Ein „direkter Stick" ist im Roll- und Gierkanal keiner |

---

### 10. Die Waffenmodelle

#### 10.1 Mk-82 — `sim/assets/aircraft/mk82/mk82.xml` (vendored, `release="BETA"`)

**Der Vorbehalt steht im Modell selbst.** `<fileheader><note>` `[XML]`, wörtlich:

> „If this model has been validated at all, it would be only to the extent that it seems to 'fly
> right' … **Or, it could be a gross approximation, with the only similarity to an actual object
> being the name.** Thus, this model is meant for educational and entertainment purposes only."

Das ist keine Formalie: **die Abwurfgenauigkeit von FlightBox wird gegen genau dieses Modell
gemessen** (`stores DELIVERY`, `core/FBBallistics` — der Feuerleitrechner integriert die
Store-Tabelle, die Bombe fliegt diese Aerodynamik, und die Differenz IST die gemessene Größe
`[DOC CLAUDE.md]`). Der gemessene Fehler ist damit der Fehler gegen eine erklärtermaßen ungeprüfte
Referenz — was ihn als RELATIVES Maß (Verfahren gegen Verfahren, CCIP gegen CCRP) gültig lässt und
als ABSOLUTES nicht.

| Block | Inhalt |
|---|---|
| `<metrics>` | wingarea **2,54 ft²**, wingspan 0,9 ft, chord 0,9 ft; AERORP 33,08 in, VRP 31 in `[XML]` |
| `<mass_balance>` | emptywt **500 lb**; Ixx 0,66, Iyy = Izz **633** slug·ft²; CG bei 31 in `[XML]` |
| `<ground_reactions>` | 2 STRUCTURE-Punkte (Nase 0 in, Heck 66 in), Feder 10.000, **Dämpfung 200.000** lb/ft/s `[XML]` |
| `<propulsion>` | **leer** |
| `<flight_control>` | **leer** — keine Steuerflächen, keine Kanäle |
| `<aerodynamics>` | 8 Funktionen, 6 Tabellen, alle über α × Mach (13 Machspalten 0,2…1,6) |

Aerodynamik im Detail `[XML]`:

| Achse | Funktion | Form | Kernzahlen |
|---|---|---|---|
| DRAG | `CDmin` | Tabelle(Mach) | **0,140** (M 0,2) → 0,149 (M 0,9) → **0,418 (M 1,4)** → 0,275 (M 1,6) |
| DRAG | `CDi` | Tabelle(α × Mach) × `aero/cl-squared` | 0 bei α=0 bis 10,7 bei α = 89° |
| SIDE | `CYb` | Tabelle(α × Mach) × β | −4,572 (α=0, M 0,2) … −7,8 (α = 50°, M 1,4) |
| LIFT | `CLwbh` | Tabelle(α × Mach) × α | 0,892 (α = 10°, M 0,2) bis 5,27 |
| ROLL | `Clbeta` | Tabelle(α × Mach) × β | +0,15 … +0,38 |
| PITCH | `Cmalpha` | Tabelle(α × Mach) × α | −1,372 (α=0, M 0,2) bis **+9,024 (M 1,6, α=0)** |
| PITCH | `Cmq` | Konstante | **−50,0** |
| PITCH | `Cmadot` | Konstante | **−3,46** |
| YAW | `Cnbeta` | Tabelle(α × Mach) × β | **exakt `Cmalpha` mit umgekehrtem Vorzeichen** |
| YAW | `Cnr` | Konstante | **−60,0** |

**α-Raster: 0 / 0,175 / 0,349 / 0,524 / 0,698 / 0,873 / 1,047 / 1,222 / 1,396 / 1,553 rad = 0…89°**
`[ABL]` — die Bombe ist über den vollen Anstellwinkelbereich tabelliert, die F-16 nicht.

Zwei Auffälligkeiten `[ABL]`: (a) `Cnbeta` ist eine vorzeichengedrehte Kopie von `Cmalpha` — für
einen rotationssymmetrischen Körper konsistent, aber eben eine Kopie; (b) `Cnbeta` multipliziert mit
`metrics/cbarw-ft` statt `bw-ft` — folgenlos, weil bei diesem Modell Spannweite = Flügeltiefe =
0,9 ft, aber eine Formfehler-Signatur. Die positiven `Cmalpha`-Werte bei M 1,6 (+9,024) sind
**statisch instabil** und der einzige Bereich, in dem das Modell sich selbst widerspricht.

`reset00.xml` (u 560,88 / w −190,0 ft/s, θ 90°, 2.010 m) und `reset01.xml` (u 400 / v 120 ft/s,
10.000 ft) liegen bei, werden von FlightBox aber nicht benutzt — FlightBox spawnt Stores über
`FBFdmSpawn::Ballistic` aus dem Trägerzustand `[DOC CLAUDE.md]`.

#### 10.2 AIM-120 — `sim/assets/aircraft/aim120/` (FlightBox-eigen, `release="ALPHA"`)

Dieses Modell ist als einziges **selbst durchdokumentiert**: jede Zahl trägt im XML ein Tag
(`[T-ED]` / `[T3]` / `[DERIVED]` / `[SET]`) und jede Ableitung ihre Formel. Die Zusammenfassung:

| Block | Wert | Tag im XML |
|---|---|---|
| `wingarea` | **0,2672 ft²** = π·d²/4 bei d = 7 in | `[DERIVED]` |
| `chord` | 0,5833 ft = d (Bezugslänge der Nick-/Giermomente) | `[DERIVED]` |
| `wingspan` | 1,726 ft = Finnenspannweite 526 mm (nur Rolldämpfung) | `[T3]` |
| `emptywt` | **220,0 lb** = 335 lb Startmasse − 115 lb Treibsatz | `[T3]` / `[DERIVED]` |
| `ixx` / `iyy` / `izz` | 0,44 / 105,0 / 105,0 slug·ft² | `[DERIVED]` / `[SET]` |
| CG + AERORP + VRP | alle bei Station **69 in** | `[SET]` |
| Kontakte | Nase 0 in, Heck 144 in, Feder 10.000, Dämpfung 20.000 | — (existieren, feuern nie) |

**Antrieb** `engine/WPU-6.xml`:

| Element | Wert | Tag |
|---|---|---|
| `isp` | **235 s** | `[SET]` — Lehrbuchwert HTPB, rückwärts aus EDs „Mach 4" hergeleitet |
| `builduptime` | 0,06 s (Zündtransient, sin-Rampe) | `[SET]` |
| Schubtabelle (indiziert nach VERBRANNTEM Treibsatz in lb, Vakuumschub in lbf) | 0,00 → 5.400 \| 68,80 → 5.400 \| 69,00 → 1.400 \| 114,70 → 1.400 \| 115,00 → 0 | `[SET]` Split |
| abgeleitete Phasen | Boost 5.400 lbf × 3,00 s, Sustain 1.400 lbf × 7,70 s, gesamt **26.980 lbf·s über 10,7 s** | `[DERIVED]` |
| Düse (`WPU-6_nozzle.xml`) | Fläche 0,143 ft² → −303 lbf auf Meereshöhe (5,6 % des Boost) | `[DERIVED]` |
| Tank | 115 lb, `grain_config CYLINDRICAL`, Länge 40 in, Bohrung 1,5 in, Radius 3,5 in, **am CG** | `[DERIVED]` / `[SET]` |

**Brennzeit ist ein ERGEBNIS, keine Angabe:** JSBSims `FGRocket` indiziert die Tabelle über den
verbrannten Treibsatz und zieht `wdot = thrust/Isp` selbst ab `[XML]`-Kommentar, `[JSB]`.
Gezündet wird bei Throttle == 1,0, danach brennt der Satz **unabhängig vom Throttle** bis zur
Erschöpfung — die Eigenschaft, mit der die Lenkung leben muss.

**Steuerung** (`<flight_control>`): drei identische Kanäle Pitch/Yaw/Roll, je
`lag_filter c1 = 60 1/s` (≈ 17 ms Zeitkonstante) → `aerosurface_scale ±0,4363 rad = ±25°` `[XML]`.
Ausgänge `fcs/elevator-pos-rad`, `fcs/rudder-pos-rad`, `fcs/left-aileron-pos-rad`.

**Aerodynamik** — Raketenkonvention, Achsen `AXIAL`/`NORMAL`/`SIDE` + `PITCH`/`YAW`/`ROLL`
(Widerstand entlang der Bahn ist KEIN Koeffizient, er entsteht als `CA·cos α + CN·sin α`):

| Achse | Funktion | Form | Kernzahlen |
|---|---|---|---|
| AXIAL | `CA` | Tabelle(Mach, 13 Punkte 0…5) | 0,400 (M 0) → **0,840 (M 1,2)** → 0,470 (M 4) → 0,440 (M 5) |
| AXIAL | `CA_alpha` | 0,9·α² | `[SET]` |
| NORMAL | `CN_alpha` | Tabelle(Mach, 10 Punkte) je rad | 9,00 (M 0) → **11,50 (M 1,2)** → 7,00 (M 5) |
| NORMAL | `CN_alpha2` | 10,0·α·|α| (Querströmung) | `[SET]` |
| NORMAL | `CN_de` | +1,2 je rad | Nicht-Minimalphasen-Verhalten des Heckruders |
| SIDE | `CY_beta` / `CY_beta2` / `CY_dr` | Spiegelbild der NORMAL-Achse (−1,0 × dieselbe Machtabelle; −10,0; −1,2) | kreuzförmig |
| PITCH | `Cm_alpha` / `Cm_q` / `Cm_de` | −12,0 / −500,0 / **−11,3** je rad | `Cm_de` = −1,2 × 9,43 Kaliber Hebel `[DERIVED]` |
| YAW | `Cn_beta` / `Cn_r` / `Cn_dr` | +12,0 / −500,0 / +11,3 | gespiegelt |
| ROLL | `Cl_p` / `Cl_da` | −12,0 / **+0,05** je rad | Skid-to-turn: Roll ist Lagehaltung, kein Manöverkanal |

Die drei Aussagen, gegen die dieses Set gebaut wurde, stehen als Rechnung im XML `[XML]`:
**(1) Trimm** bei Vollruder α = 11,3·0,4363/12,0 = 0,41 rad = 23,5°; **(2) Manöver** bei M 2/6 km
14,9 kN ≈ **13,8 g** (≈19 g bei 3 km, ≈25 g auf Meereshöhe); **(3) Verzögerung** nach Brennschluss
bei M 4/6 km **5,6 g**.

## Gaps

Beide Sorten stehen hier zusammen, weil sie bei dieser Datei zusammenfallen: was das MODELL nicht
abbildet, ist zugleich das, was FlightBox nicht simulieren kann. §12 ist bewusst dreigeteilt —
fehlend, widersprüchlich, unplausibel — plus das, was nicht untersucht wurde.

### 12. Offene Punkte

#### 12.1 Was das Modell nicht abbildet

| Fehlend | §  | Bemerkung |
|---|---|---|
| Nachstall α > 45° / Deep Stall | 1.1, 6.10 | Widerspricht der Erwartung, die `aerodynamics-performance.md` aus TP-1538 ableitet |
| ARI, Anti-Spin, CAT I/III, MPO, DBU, Gun-Kompensation | 7.11 | die gesamte Schutzschicht der echten FLCS |
| Fanghakenkraft | 4.4 | deklariert, nicht verdrahtet |
| Außenlast-Aerodynamik im Deck | 6.10 | FlightBox ersetzt sie extern |
| Bodeneffekt auf Widerstand und Nickmoment | 6.3 | nur Auftrieb |
| `Cmadot` (α̇-Term) | 6.10 | die Mk-82 hat einen, die F-16 nicht |
| Machabhängigkeit der Grundtabellen | 6.10 | nur additive `*_M`-Korrekturen |

#### 12.2 Wo das Modell sich selbst widerspricht

| Befund | § | Belegzeile |
|---|---|---|
| `n-pilot-z-norm` ist −1 im Horizontalflug; der „Schwerkraftkorrektur"-Summer zieht `cos·cos` ab statt es zu addieren und verdoppelt den Offset | 7.3 | `[MESS]` `g-load-corrected = −1,482` bei `Nz = +1,000` |
| ~~Der Flaperon-Mixer kürzt die Klappen weg und verdoppelt die Querruder~~ — **behoben per `MODEL-DELTAS.md` D1** | 7.9 | `[ABL]` `flaperon-summer = −2·ail_sc` (Upstream) |
| `fcs/rudder-pos-norm` hat zwei Schreiber im selben Kanal | 7.4 | `[XML]` PID-`<output>` und Kinematik-`<output>` |
| `fcs/dht-left/right-pos-rad` werden gerechnet und von nichts gelesen | 7.3 | `[MESS]` Grep |
| Spannweite in `<metrics>` (30 ft) gegen Flügelspitzen-Kontakte (31,5 ft) | 4.2 | `[ABL]` |
| Mk-82: `Cnbeta` benutzt `cbarw-ft` statt `bw-ft` | 10.1 | folgenlos, weil hier gleich groß |
| Mk-82: `Cmalpha` wird bei M 1,6 positiv (statisch instabil) | 10.1 | `[XML]` +9,024 bei α = 0 |

#### 12.3 Wo Zahlen unplausibel wirken

| Befund | § | Warum verdächtig |
|---|---|---|
| Gierraten-Normierung 100 (0,57 °/s Vollausschlag über 89 kt) | 7.4 | Zwei Größenordnungen härter als jede plausible Gierdämpfung; wirkt wie eine Einheitenverwechslung |
| Roll-PID `kd = −0,00125` (negativ) | 7.2 | Ein negativer D-Anteil verstärkt Ratenänderungen, statt sie zu dämpfen |
| Nickraten-Normierung 6,2 gegen g-Normierung 0,020 | 7.3 | Faktor > 30 zwischen den beiden Rückführungen — der Grund für die 5,6-g-Decke |
| Schub exakt 0 ab 60.000 ft Dichtehöhe | 5.2 | Eine Nullspalte als Tabellenrand ist eine Wand, kein Abfall |
| Spool-down 3× schneller als Spool-up | 5.4 | JSBSim-Default, physikalisch verkehrt herum |
| Trägheitsverhältnis Izz/Ixx = 6,6 | 3 | Plausibel für eine Deltaflügel-Zelle, aber nirgends im Modell belegt |
| `vtailarm = 0` bei `vtailarea = 54,75` | 2 | Offensichtlich unausgefüllt; folgenlos, weil ungelesen |

#### 12.4 Nicht untersucht

- Das Trimmverhalten (`FBFdmBoot::Spawn` → JSBSims `DoTrim`) über der Einhüllenden; die
  Corner-Messung zeigt oberhalb 580 KCAS „udot doesn't appear to be trimmable" `[MESS]` — die
  betroffenen Punkte fliegen ungetrimmt an.
- Ob und wie stark `Cma_M`s Machtuck-Sprung bei M 1,0 die geschlossene Nickschleife destabilisiert.
- ~~Die Wechselwirkung zwischen dem Flaperon-Artefakt (§7.9) und der Corner-Messung (§8.1)~~ —
  QUANTIFIZIERT und erledigt: der Auftriebssprung fiel in die Rolleinleitung und hob das gemessene g im
  Messfenster; nach Delta D1 fällt `cornerNz` 5,63 → 5,44 bei unveränderter Corner-Geschwindigkeit
  (§8.1).

## Knowledge

*§11 ist die übertragbare Vorlage: was eine solche Modellbeschreibung enthalten MUSS und woher jede
Angabe kommen muss. Sie ist die Bauanleitung für `doc/modules/mig29/flight-model.md` und steht deshalb hier,
nicht im Zustandsteil.*

### 11. Was eine solche Beschreibung braucht — die Übergabe-Checkliste

Für die nächste Zelle (MiG-29) ist diese Datei die Vorlage. Die Gliederung ist übertragbar; was je
Abschnitt zwingend woher kommen MUSS, steht hier:

| # | Abschnitt | Pflichtinhalt | Quelle MUSS sein |
|---|---|---|---|
| 0 | **Kopf** | Dateiliste mit Zeilenzahlen, Lizenz je Datei, Autor, Revision, `release=`-Stufe | **Modell-XML** (`<fileheader>`) + `wc -l` |
| 1.1 | **Herkunftskette** | Primärdatensatz benennen; Tabelle „was die Quelle abdeckt vs. was im Modell steht", Zeile für Zeile | **Handbuch/Paper** für die linke, **Modell-XML** für die rechte Spalte. Nie eine Spalte aus der anderen füllen |
| 1.2 | **Modell-Delta** | Byte-Diff gegen das gepinnte Submodul; jede Abweichung benannt | **Messung** (`diff -rq`) |
| 2 | **Geometrie** | jedes `<metrics>`-Element mit Wert und Verbraucher; Achsenkonvention aus den Koordinaten ABLEITEN, nicht annehmen | **Modell-XML** + `[ABL]` |
| 3 | **Masse/Trägheit** | alle Trägheiten inkl. Vorzeichenkonvention; jede `<pointmass>`; jeder Tank mit Ort/Kapazität/Vorbelegung; **Reihenfolge der Entleerung** | **Modell-XML** + **JSBSim-Quelltext** (Konvention, Feed-Logik) |
| 4 | **Bodenkontakte** | jede Strebe: Ort, Feder, Dämpfung, Reibung, Lenkwinkel, Bremsgruppe; jeder Strukturpunkt; **ausdrücklich: ob es eine Bruchlast gibt** (in JSBSim: nein) | **Modell-XML**; die Schranken des Physik-Monitors aus **`doc/core.md`**, nie erfinden |
| 4.4 | **External Reactions** | jede deklarierte Kraft + **wer sie treibt** (Grep-Beleg, wenn niemand) | **Messung** (Grep) |
| 5 | **Antrieb** | Skalare; jede Schubtabelle mit Achsen, Rasterbereich und **Klemmverhalten an den Rändern**; Spool-Gesetz; **die Throttle→Schub-Abbildung explizit** | **Modell-XML** + **JSBSim-Quelltext** (Spool-Defaults, AugMethod, Verbrauchsgesetz) |
| 6 | **Aerodynamik** | Zähl-Übersicht (Funktionen/Tabellen je Achse); **das Stützstellenraster EINMAL zentral**; je Funktion: unabhängige Variablen, Stellgröße, Kernzahlen an den Rändern und am Extremum; Vorzeichenwechsel benennen | **Modell-XML**, ausschließlich. Physikalische Deutung als `[ABL]` markieren |
| 6.x | **Was fehlt** | eigene Tabelle: welche Terme das Deck NICHT hat und was daraus folgt | `[ABL]` aus der Abwesenheit, mit Grep-Beleg |
| 7 | **Regelung als XML** | Kanalübersicht mit Zeilenbereichen; je Kanal ein Signalfluss-Diagramm; **jede Verstärkung als „Vollausschlag entspricht X"** umgerechnet; jeder Plan als Tabelle; Aktuatorgrenzen und -raten | **Modell-XML** + `[ABL]` für die Umrechnung |
| 7.x | **Override-Analyse** | für jeden Kanal: gibt es einen Bypass, was ersetzt er, **erreicht der Aeropfad ihn überhaupt** | `[ABL]`, per Property-Verfolgung Ausgang → Aerofunktion |
| **7.z** | **Abweichungstabelle Modell vs. echtes System** | eine Zeile je Merkmal: Design-Ziel `[DOC]` \| Ist `[XML]` \| Bewertung. **Die wichtigste Tabelle der Datei** | linke Spalte NUR aus dem Handbuch-Distillat, rechte NUR aus dem XML |
| 8 | **Gemessene Einhüllende** | Corner-Sweep, Rollratensweep, alles mit Methode, Datum, Rohausgabe; **und die Erklärung, welche Modellzahl die Grenze setzt** | **Messung** (benanntes Harness/Kommando), Erklärung `[ABL]` gegen §6/§7 |
| 9 | **Akzeptierte Eigenschaften** | nummerierte Liste; je Zeile Beleg + praktische Folge. Ausdrücklich als „Wahrheit, kein Defekt" gerahmt | Verweise auf §§ dieser Datei |
| 10 | **Waffenmodelle** | dasselbe Schema kompakt; **Selbstvorbehalt des Modells wörtlich zitieren**, wenn vorhanden | **Modell-XML** |
| 11 | **Checkliste** | dieser Abschnitt, fortgeschrieben | — |
| 12 | **Offene Punkte** | was fehlt, was widersprüchlich ist, was unplausibel wirkt — **getrennt nach diesen drei** | `[ABL]`, mit Belegzeile |

**Drei Regeln, die die Vorlage trägt:**

1. **Nie eine Zahl ohne Herkunfts-Tag.** Ein Modell-Wert und ein Handbuch-Wert sehen im Fließtext
   gleich aus und sind es nie.
2. **Die Semantik steht oft nicht im XML.** Klemmen statt Extrapolieren, Spool-Defaults,
   Throttle > 1, Tank-Priorität, Vorzeichen von `n-pilot-z-norm` — jedes davon ist ein
   `[JSB]`-Befund. Wer nur das XML liest, beschreibt das Modell falsch.
3. **Property-Verfolgung schlägt Lesen.** Ob eine FCS-Komponente wirkt, entscheidet sich daran, ob
   ihr Ausgang eine Aerofunktion erreicht — nicht daran, wie sie heißt. §7.9 und §7.10 sind beide auf
   diesem Weg gefunden worden.
