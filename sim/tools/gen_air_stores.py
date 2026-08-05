#!/usr/bin/env python3
"""gen_air_stores — the SEVEN air-to-air rounds the catalogue aircraft carry, from ONE recipe.

The recipe is the tree's own slender-body set, unchanged: the AIM-120 deck's NON-DIMENSIONAL
aerodynamics (it describes a finned cylindrical body, which all of them are), sized per row from the
published diameter, mass and terminal Mach. It is the identical procedure that produced the six
surface-to-air rounds of doc/modules/ground/catalogue.md; the ONE difference is the air-launched one,
restored here: dV is a DELTA OVER A LAUNCH SPEED and not the whole terminal speed, because these
rounds leave a rail at 250 m/s with full aerodynamic authority.

    tools/gen_air_stores.py [--out <mod>/src/aircraft] [--check] [--cpp]

--cpp prints the core/FBStore.h rows the same numbers imply, so the catalogue and the deck cannot say
two different things about one round.
"""

import argparse
import math
import os
import shutil
import subprocess
import sys
import tempfile

import fb_mod as mod

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

KG_LB = 2.2046226
M_FT = 3.2808399
M2_FT2 = 10.7639104
N_LBF = 0.2248089
ISP = 235.0                # the tree's shared solid-motor specific impulse [SET]
VE = ISP * 9.80665         # 2305 m/s
LAUNCH_MS = 250.0          # [SET] the separation speed every air-launched deck in this tree assumes
A11 = 295.07               # ICAO speed of sound at 11 km, the altitude every terminal Mach is quoted at


class Round:
    def __init__(self, key, name, seeker, mass_kg, dia_m, len_m, term_mach, warhead_kg, rng_m,
                 fov_deg, gimbal_deg, boost_frac, boost_s, sustain_s, min_ms, arming_s, source):
        self.__dict__.update(locals())
        del self.__dict__["self"]
        self.s_m2 = math.pi * dia_m * dia_m / 4.0
        self.dv = term_mach * A11 - LAUNCH_MS
        self.mp_kg = mass_kg * (1.0 - 1.0 / math.exp(self.dv / VE))
        self.burnout_kg = mass_kg - self.mp_kg
        self.impulse = self.mp_kg * VE
        self.boost_n = self.impulse * boost_frac / boost_s
        self.sustain_n = (self.impulse * (1.0 - boost_frac) / sustain_s) if sustain_s > 0 else 0.0
        # FuzeRadiusM [DERIVED] from the AIM-120's [SET] 10 m at 20.5 kg, at equal fragment areal
        # density r ~ sqrt(m) — the identical relation core/FBStore.h derives the R-27R's 13.8 m by.
        self.fuze_m = 10.0 * math.sqrt(warhead_kg / 20.5)


# fmt: off
ROUNDS = [
    Round("k13", "K-13 (R-13M)", "Infrared", 90.0, 0.127, 2.83, 2.5, 7.4, 3500.0, 5.0, 40.0,
          1.0, 5.0, 0.0, 200.0, 1.0,
          "[T4] Wikipedia K-13: 90 kg, 127 mm, 7.4 kg warhead, Mach 2.5, 0.97-3.5 km. THAT RANGE IS "
          "SHORT against commonly quoted R-13M figures and no second source was read (catalogue D9); "
          "it is carried AS READ and marked. REAR ASPECT ONLY — expressed by the GimbalHalf 40 deg "
          "plus the infrared aspect law the seeker slot already runs, not by a rule."),
    Round("r60", "R-60", "Infrared", 44.0, 0.120, 2.09, 2.47, 3.0, 8000.0, 5.0, 45.0,
          1.0, 3.0, 0.0, 200.0, 0.8,
          "[T4] Wikipedia R-60: 44 kg, 120 mm, 3 kg warhead, Mach 2.47, 8 km, altitude limit 20 km."),
    Round("r24r", "R-24R", "SemiActiveRadar", 222.0, 0.223, 4.60, 3.0, 25.0, 35000.0, 10.0, 50.0,
          0.6, 3.0, 5.0, 250.0, 1.5,
          "[T4] Wikipedia R-23/R-24: 222 kg, 223 mm, 25 kg expanding-rod warhead, Mach 3, 35 km, "
          "'comparable to the AIM-7 Sparrow'. The R model is SARH and therefore BINDS THE SHOOTER TO "
          "IMPACT (FBSeekerHandoverS -1); the T model is the same body with an infrared head and is "
          "not a separate deck."),
    Round("r40r", "R-40R", "SemiActiveRadar", 475.0, 0.310, 5.98, 4.5, 38.0, 80000.0, 10.0, 50.0,
          0.55, 4.0, 8.0, 300.0, 2.0,
          "[T4] Wikipedia R-40: 475 kg, 310 mm, 38-100 kg blast-fragmentation (the LOWER bound is "
          "taken and stated), Mach 2.2-4.5, 50-80 km, 15 g launch overload. The biggest air-to-air "
          "round in the tree by a factor of two."),
    Round("aim7", "AIM-7F/M Sparrow", "SemiActiveRadar", 231.0, 0.203, 3.66, 4.0, 40.0, 70000.0,
          10.0, 50.0, 0.6, 3.0, 6.0, 250.0, 1.5,
          "[T4] Wikipedia AIM-7 Sparrow: 510 lb = 231 kg, 8 in = 203 mm, 88 lb = 40 kg warhead, "
          "Mach 4, 70 km. THE PAIRING THIS ROUND EXISTS FOR: an F-15C with Sparrows fights the "
          "MiG-29's bound fight and the same aircraft with AMRAAMs fights the F-16's."),
    Round("s530f", "Super 530F", "SemiActiveRadar", 245.0, 0.263, 3.54, 4.5, 30.0, 25000.0,
          10.0, 50.0, 0.65, 2.5, 4.0, 250.0, 1.5,
          "[T4] Wikipedia Super 530: 245 kg, 263 mm, 30 kg HE-frag, Mach 4.5, 25 km."),
    Round("magic1", "R550 Magic 1", "Infrared", 89.0, 0.157, 2.75, 3.0, 12.7, 10000.0, 5.0, 55.0,
          1.0, 4.0, 0.0, 200.0, 1.0,
          "[T4] Wikipedia R550 Magic: 89 kg, 157 mm, 12.7 kg pre-fragmented, Mach 3, 10 km."),
]
# fmt: on

DECK_TPL = """<?xml version="1.0"?>
<?xml-stylesheet type="text/xsl" href="http://jsbsim.sourceforge.net/JSBSim.xsl"?>
<!--
  FlightBox — {name}, a FlightBox-OWN JSBSim model, GENERATED by tools/gen_air_stores.py.
  DO NOT EDIT BY HAND: the next run of the tool overwrites this file.

  ONE RECIPE, SEVEN ROUNDS. The aerodynamic set below is the AIM-120 deck's slender-body set,
  unchanged and deliberately so: it is NON-DIMENSIONAL and describes a finned cylindrical body, which
  all of them are. Only what depends on THIS round's own proportions is computed per row.

  PROVENANCE: {source}
  Everything else is [DERIVED] by the formula stated where it is used, or [SET] with its reason.

  GEOMETRY, non-dimensionalised against the BODY cross-section:
    S = pi*d^2/4 = pi*{dia:.4f}^2/4 = {s_m2:.6f} m^2 = {s_ft2:.6f} ft^2   [DERIVED]
    chord := d = {chord_ft:.4f} ft [DERIVED], the reference length of the pitch/yaw moments
    wingspan := 2.6*d = {span_ft:.4f} ft [SET, the AIM-9 deck's fin-span-to-diameter proportion]
    length {len_m:.3f} m = {len_in:.0f} in

  MASS. Launch {mass_kg:.1f} kg = {mass_lb:.1f} lb; propellant {mp_kg:.1f} kg = {mp_lb:.1f} lb
  [DERIVED, rocket equation over a DELTA above the {launch:.0f} m/s separation speed — this round
  leaves a rail with full aerodynamic authority, which is the one place the air-launched recipe
  differs from the surface one]. <emptywt> carries {empty_lb:.1f} lb and JSBSim adds the grain.
    Iyy = Izz: 0.84*m*L^2/12 = {iyy:.2f} slug*ft^2 [DERIVED, the same 0.84 the AIM-120 deck argues]
    Ixx = m*r^2/2 = {ixx:.4f} slug*ft^2 [DERIVED]
    CG at mid-body, station {cg_in:.0f} in [SET].

  CONTROL. Cm_de = -CN_de * arm = -1.2 * ({fin_in:.0f} - {cg_in:.0f}) in / {chord_in:.2f} in
  = {cmde:.2f} [DERIVED]; Cm_alpha = {cma:.2f} [DERIVED] so the trim relation
  alpha = -Cm_de*de/Cm_alpha lands on the same ~0.41 rad full-fin design point every other round in
  this tree uses.

  THE HONEST CONSEQUENCE, stated rather than tuned away: at equal dynamic pressure this deck pulls the
  same class of g the AIM-120 does. No public aero deck exists for any of these rounds, and inflating
  CN to reach a quoted g would be a number posing as physics.
-->
<fdm_config name="{name}" version="2.0" release="ALPHA"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xsi:noNamespaceSchemaLocation="http://jsbsim.sourceforge.net/JSBSim.xsd">

  <fileheader>
    <author> FlightBox (tools/gen_air_stores.py) </author>
    <filecreationdate> 2026-07-28 </filecreationdate>
    <version> 1.0 </version>
    <description> {name} — FlightBox's own model </description>
    <note>
      Built from public geometry and mass figures plus the slender-body aerodynamic set this tree's
      AIM-120 model derives and documents; it is not derived from, and contains nothing out of, any
      restricted source. Per-number provenance in the banner above.
    </note>
  </fileheader>

  <metrics>
    <wingarea  unit="FT2"> {s_ft2:.6f} </wingarea>
    <wingspan  unit="FT">  {span_ft:.4f} </wingspan>
    <chord     unit="FT">  {chord_ft:.4f} </chord>
    <htailarea unit="FT2"> 0 </htailarea>
    <htailarm  unit="FT">  0 </htailarm>
    <vtailarea unit="FT2"> 0 </vtailarea>
    <vtailarm  unit="FT">  0 </vtailarm>
    <location name="AERORP" unit="IN"> <x> {cg_in:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
    <location name="VRP" unit="IN"> <x> {cg_in:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
  </metrics>

  <mass_balance>
    <ixx unit="SLUG*FT2"> {ixx:.4f} </ixx>
    <iyy unit="SLUG*FT2"> {iyy:.2f} </iyy>
    <izz unit="SLUG*FT2"> {iyy:.2f} </izz>
    <emptywt unit="LBS"> {empty_lb:.1f} </emptywt>
    <location name="CG" unit="IN"> <x> {cg_in:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
  </mass_balance>

  <!-- Present so the model is complete, not so they fire: a weapon in flight is given no ground to
       collide with by its owner (units/FBSimUnit's kWeaponNoGroundElevM). -->
  <ground_reactions>
    <contact type="STRUCTURE" name="NOSE_CONTACT">
      <location unit="IN"> <x> 0 </x> <y> 0 </y> <z> 0 </z> </location>
      <static_friction> 0 </static_friction>
      <dynamic_friction> 0 </dynamic_friction>
      <rolling_friction> 0 </rolling_friction>
      <spring_coeff unit="LBS/FT"> 10000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 20000 </damping_coeff>
      <max_steer unit="DEG"> 0.0 </max_steer>
      <brake_group> NONE </brake_group>
      <retractable> 0 </retractable>
    </contact>
    <contact type="STRUCTURE" name="TAIL_CONTACT">
      <location unit="IN"> <x> {len_in:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
      <static_friction> 0 </static_friction>
      <dynamic_friction> 0 </dynamic_friction>
      <rolling_friction> 0 </rolling_friction>
      <spring_coeff unit="LBS/FT"> 10000 </spring_coeff>
      <damping_coeff unit="LBS/FT/SEC"> 20000 </damping_coeff>
      <max_steer unit="DEG"> 0.0 </max_steer>
      <brake_group> NONE </brake_group>
      <retractable> 0 </retractable>
    </contact>
  </ground_reactions>

  <!-- The grain sits AT the CG so the burn changes mass without walking the centre of gravity. -->
  <propulsion>
    <engine file="{key}-motor">
      <feed> 0 </feed>
      <thruster file="{key}-motor_nozzle">
        <location unit="IN"> <x> {len_in:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
        <orient unit="DEG"> <roll> 0 </roll> <pitch> 0 </pitch> <yaw> 0 </yaw> </orient>
      </thruster>
    </engine>

    <tank type="FUEL">
      <location unit="IN"> <x> {cg_in:.0f} </x> <y> 0 </y> <z> 0 </z> </location>
      <radius unit="IN"> {radius_in:.2f} </radius>
      <grain_config type="CYLINDRICAL">
        <length unit="IN"> {grain_in:.1f} </length>
        <bore_diameter unit="IN"> {bore_in:.2f} </bore_diameter>
      </grain_config>
      <capacity unit="LBS"> {mp_lb:.1f} </capacity>
      <contents unit="LBS"> {mp_lb:.1f} </contents>
    </tank>
  </propulsion>

  <!-- The three normalised control channels every FlightBox airframe exposes (fdm/FBFdm.h), through a
       first-order actuator (lag c1 = 60 1/s, ~17 ms) into a +-25 deg fin deflection. -->
  <flight_control name="FCS: {name} fins">
    <channel name="Pitch">
      <lag_filter name="fcs/elevator-actuator">
        <input> fcs/elevator-cmd-norm </input>
        <c1> 60.0 </c1>
        <clipto> <min> -1.0 </min> <max> 1.0 </max> </clipto>
      </lag_filter>
      <aerosurface_scale name="fcs/elevator-scale">
        <input> fcs/elevator-actuator </input>
        <range> <min> -0.4363 </min> <max> 0.4363 </max> </range>
        <output> fcs/elevator-pos-rad </output>
      </aerosurface_scale>
    </channel>
    <channel name="Yaw">
      <lag_filter name="fcs/rudder-actuator">
        <input> fcs/rudder-cmd-norm </input>
        <c1> 60.0 </c1>
        <clipto> <min> -1.0 </min> <max> 1.0 </max> </clipto>
      </lag_filter>
      <aerosurface_scale name="fcs/rudder-scale">
        <input> fcs/rudder-actuator </input>
        <range> <min> -0.4363 </min> <max> 0.4363 </max> </range>
        <output> fcs/rudder-pos-rad </output>
      </aerosurface_scale>
    </channel>
    <channel name="Roll">
      <lag_filter name="fcs/aileron-actuator">
        <input> fcs/aileron-cmd-norm </input>
        <c1> 60.0 </c1>
        <clipto> <min> -1.0 </min> <max> 1.0 </max> </clipto>
      </lag_filter>
      <aerosurface_scale name="fcs/aileron-scale">
        <input> fcs/aileron-actuator </input>
        <range> <min> -0.4363 </min> <max> 0.4363 </max> </range>
        <output> fcs/left-aileron-pos-rad </output>
      </aerosurface_scale>
    </channel>
  </flight_control>

  <aerodynamics>
    <axis name="AXIAL">
      <function name="aero/coefficient/CA">
        <description> Axial force (zero lift) </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <table>
            <independentVar> velocities/mach </independentVar>
            <tableData>
              0.0000  0.400
              0.6000  0.410
              0.8000  0.430
              0.9000  0.470
              0.9500  0.550
              1.0500  0.820
              1.2000  0.840
              1.5000  0.780
              2.0000  0.680
              2.5000  0.600
              3.0000  0.550
              4.0000  0.470
              5.0000  0.440
            </tableData>
          </table>
        </product>
      </function>
      <function name="aero/coefficient/CA_alpha">
        <description> Axial force due to angle of attack </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <value> 0.9 </value>
          <property> aero/alpha-rad </property>
          <property> aero/alpha-rad </property>
        </product>
      </function>
    </axis>

    <axis name="NORMAL">
      <function name="aero/coefficient/CN_alpha">
        <description> Normal force due to angle of attack </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> aero/alpha-rad </property>
          <table>
            <independentVar> velocities/mach </independentVar>
            <tableData>
              0.0000   9.00
              0.8000   9.20
              1.0500  11.00
              1.2000  11.50
              1.5000  10.50
              2.0000   9.50
              2.5000   8.80
              3.0000   8.20
              4.0000   7.40
              5.0000   7.00
            </tableData>
          </table>
        </product>
      </function>
      <function name="aero/coefficient/CN_alpha2">
        <description> Normal force, crossflow (nonlinear in alpha) </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <value> 10.0 </value>
          <property> aero/alpha-rad </property>
          <abs> <property> aero/alpha-rad </property> </abs>
        </product>
      </function>
      <function name="aero/coefficient/CN_de">
        <description> Normal force due to pitch fin deflection </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <value> 1.2 </value>
          <property> fcs/elevator-pos-rad </property>
        </product>
      </function>
    </axis>

    <axis name="SIDE">
      <function name="aero/coefficient/CY_beta">
        <description> Side force due to sideslip </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> aero/beta-rad </property>
          <value> -1.0 </value>
          <table>
            <independentVar> velocities/mach </independentVar>
            <tableData>
              0.0000   9.00
              0.8000   9.20
              1.0500  11.00
              1.2000  11.50
              1.5000  10.50
              2.0000   9.50
              2.5000   8.80
              3.0000   8.20
              4.0000   7.40
              5.0000   7.00
            </tableData>
          </table>
        </product>
      </function>
      <function name="aero/coefficient/CY_beta2">
        <description> Side force, crossflow (nonlinear in beta) </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <value> -10.0 </value>
          <property> aero/beta-rad </property>
          <abs> <property> aero/beta-rad </property> </abs>
        </product>
      </function>
      <function name="aero/coefficient/CY_dr">
        <description> Side force due to yaw fin deflection </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <value> -1.2 </value>
          <property> fcs/rudder-pos-rad </property>
        </product>
      </function>
    </axis>

    <axis name="PITCH">
      <function name="aero/coefficient/Cm_alpha">
        <description> Pitch moment due to angle of attack </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> metrics/cbarw-ft </property>
          <property> aero/alpha-rad </property>
          <value> {cma:.2f} </value>
        </product>
      </function>
      <function name="aero/coefficient/Cm_q">
        <description> Pitch moment due to pitch rate </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> metrics/cbarw-ft </property>
          <property> aero/ci2vel </property>
          <property> velocities/q-aero-rad_sec </property>
          <value> -500.0 </value>
        </product>
      </function>
      <function name="aero/coefficient/Cm_de">
        <description> Pitch moment due to pitch fin deflection </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> metrics/cbarw-ft </property>
          <property> fcs/elevator-pos-rad </property>
          <value> {cmde:.2f} </value>
        </product>
      </function>
    </axis>

    <axis name="YAW">
      <function name="aero/coefficient/Cn_beta">
        <description> Yaw moment due to sideslip </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> metrics/cbarw-ft </property>
          <property> aero/beta-rad </property>
          <value> {cnb:.2f} </value>
        </product>
      </function>
      <function name="aero/coefficient/Cn_r">
        <description> Yaw moment due to yaw rate </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> metrics/cbarw-ft </property>
          <property> aero/ci2vel </property>
          <property> velocities/r-aero-rad_sec </property>
          <value> -500.0 </value>
        </product>
      </function>
      <function name="aero/coefficient/Cn_dr">
        <description> Yaw moment due to yaw fin deflection </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> metrics/cbarw-ft </property>
          <property> fcs/rudder-pos-rad </property>
          <value> {cndr:.2f} </value>
        </product>
      </function>
    </axis>

    <axis name="ROLL">
      <function name="aero/coefficient/Cl_p">
        <description> Roll moment due to roll rate </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> metrics/bw-ft </property>
          <property> aero/bi2vel </property>
          <property> velocities/p-aero-rad_sec </property>
          <value> -12.0 </value>
        </product>
      </function>
      <function name="aero/coefficient/Cl_da">
        <description> Roll moment due to roll fin deflection </description>
        <product>
          <property> aero/qbar-psf </property>
          <property> metrics/Sw-sqft </property>
          <property> metrics/bw-ft </property>
          <property> fcs/left-aileron-pos-rad </property>
          <value> 0.05 </value>
        </product>
      </function>
    </axis>
  </aerodynamics>

</fdm_config>
"""

MOTOR_TPL = """<?xml version="1.0"?>
<!-- FlightBox — {name} solid rocket motor, GENERATED by tools/gen_air_stores.py.

     propellant   m_p = m0*(1 - 1/exp(dV/ve)) = {mass_kg:.1f}*(1 - 1/exp({dv:.0f}/{ve:.0f}))
                      = {mp_kg:.1f} kg = {mp_lb:.1f} lb
                      [DERIVED, the tree's shared Isp {isp:.0f} s -> ve = {ve:.0f} m/s]
     dV is a DELTA over the {launch:.0f} m/s separation speed: an air-launched round starts with the
     shooter's own energy, which is the one place this recipe differs from the surface-to-air one.
     total impulse = {mp_kg:.1f} * {ve:.0f} = {impulse:.0f} N*s
     boost   {boost_n:.0f} N ({boost_lbf:.0f} lbf) x {boost_s:.1f} s{sustain_note}
     JSBSim's contract: the table is indexed by PROPELLANT BURNED (lb) and yields VACUUM thrust (lbf);
     the engine consumes wdot = thrust/Isp itself, so burn TIME is an outcome and never stated here.
     THE BURN SPLIT IS [SET] within the class bound of 2-15 s that core/FBStore.h's R-27R row already
     declares; no public source gives one for any round in this file.
-->
<rocket_engine name="{name} motor (FlightBox)">

  <isp> {isp:.0f} </isp>

  <!-- Ignition transient, [SET] 60 ms, as every other FlightBox motor. -->
  <builduptime> 0.06 </builduptime>

  <thrust_table name="propulsion/thrust_prop_remain" type="internal">
    <tableData><!--
      propellant burned (lb)   vacuum thrust (lbf) -->
{table}
    </tableData>
  </thrust_table>

</rocket_engine>
"""

NOZZLE_TPL = """<?xml version="1.0"?>
<!-- FlightBox — the {name}'s nozzle. JSBSim subtracts ambient pressure * exit area from the vacuum
     thrust (FGNozzle::Calculate), which is what makes a solid motor weaker at sea level than at
     altitude. area {area:.4f} FT2 [DERIVED] = 53.5 % of this round's {s_ft2:.4f} ft^2 body
     cross-section, the same proportion the AIM-120 nozzle occupies. The exit diameter is [SET].
-->
<nozzle name="{name} nozzle (FlightBox)">
  <area unit="FT2"> {area:.4f} </area>
</nozzle>
"""


def emit(r):
    s_ft2 = r.s_m2 * M2_FT2
    chord_ft = r.dia_m * M_FT
    span_ft = 2.6 * chord_ft
    len_in = r.len_m * M_FT * 12.0
    cg_in = 0.5 * len_in
    fin_in = 0.93 * len_in
    chord_in = chord_ft * 12.0
    cmde = -1.2 * (fin_in - cg_in) / chord_in
    cma = cmde * 1.064   # the AIM-120 deck's own Cm_alpha/Cm_de proportion, which fixes the trim alpha
    mass_lb = r.mass_kg * KG_LB
    mp_lb = r.mp_kg * KG_LB
    ixx = 0.5 * r.burnout_kg * (0.5 * r.dia_m) ** 2 * 0.7375621
    iyy = 0.84 * r.burnout_kg * r.len_m ** 2 / 12.0 * 0.7375621
    rows = ["%11.2f %12.1f" % (0.0, r.boost_n * N_LBF),
            "%11.2f %12.1f" % (mp_lb * r.boost_frac, r.boost_n * N_LBF)]
    if r.sustain_n > 0.0:
        rows += ["%11.2f %12.1f" % (mp_lb * r.boost_frac + 0.2, r.sustain_n * N_LBF),
                 "%11.2f %12.1f" % (mp_lb - 0.2, r.sustain_n * N_LBF)]
    rows.append("%11.2f %12.1f" % (mp_lb, 0.0))
    deck = DECK_TPL.format(
        key=r.key, name=r.name, source=r.source, dia=r.dia_m, s_m2=r.s_m2, s_ft2=s_ft2,
        chord_ft=chord_ft, span_ft=span_ft, len_m=r.len_m, len_in=len_in, mass_kg=r.mass_kg,
        mass_lb=mass_lb, mp_kg=r.mp_kg, mp_lb=mp_lb, launch=LAUNCH_MS,
        empty_lb=r.burnout_kg * KG_LB, iyy=iyy, ixx=ixx, cg_in=cg_in, fin_in=fin_in,
        chord_in=chord_in, cmde=cmde, cma=cma, cnb=-cma, cndr=-cmde,
        radius_in=0.5 * r.dia_m * M_FT * 12.0 * 0.7, grain_in=0.35 * len_in,
        bore_in=0.28 * r.dia_m * M_FT * 12.0)
    motor = MOTOR_TPL.format(
        name=r.name, mass_kg=r.mass_kg, dv=r.dv, ve=VE, mp_kg=r.mp_kg, mp_lb=mp_lb, isp=ISP,
        launch=LAUNCH_MS, impulse=r.impulse, boost_n=r.boost_n, boost_lbf=r.boost_n * N_LBF,
        boost_s=r.boost_s,
        sustain_note=("\n     sustain {n:.0f} N ({l:.0f} lbf) x {s:.1f} s, {f:.0f} % of the impulse"
                      .format(n=r.sustain_n, l=r.sustain_n * N_LBF, s=r.sustain_s,
                              f=100.0 * (1.0 - r.boost_frac)) if r.sustain_n > 0 else
                      "\n     single-pulse motor: no sustainer, which is what an infrared dogfight "
                      "round has"),
        table="\n".join(rows))
    nozzle = NOZZLE_TPL.format(name=r.name, area=0.535 * s_ft2, s_ft2=s_ft2)
    return deck, motor, nozzle


def cpp_rows():
    out = []
    for r in ROUNDS:
        s_ft2 = r.s_m2 * M2_FT2
        out.append(
            "    /* %s. %s */\n"
            "    key=%s mass=%.1f lb dragArea=%.4f ft2 fuze=%.1f m warhead=%.1f kg\n"
            "    boost=%.0f N x %.1f s  sustain=%.0f N x %.1f s  launch=%.1f kg burnout=%.1f kg\n"
            "    refArea=%.6f m2 seeker=%s range=%.0f m"
            % (r.name, r.source[:60], r.key, r.mass_kg * KG_LB, 0.43 * s_ft2, r.fuze_m,
               r.warhead_kg, r.boost_n, r.boost_s, r.sustain_n, r.sustain_s, r.mass_kg,
               r.burnout_kg, r.s_m2, r.seeker, r.rng_m))
    return "\n".join(out)


def write_round(root, r):
    d = os.path.join(root, r.key)
    os.makedirs(os.path.join(d, "engine"), exist_ok=True)
    deck, motor, nozzle = emit(r)
    with open(os.path.join(d, "%s.xml" % r.key), "w") as f:
        f.write(deck)
    with open(os.path.join(d, "engine", "%s-motor.xml" % r.key), "w") as f:
        f.write(motor)
    with open(os.path.join(d, "engine", "%s-motor_nozzle.xml" % r.key), "w") as f:
        f.write(nozzle)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=mod.AIRCRAFT)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--cpp", action="store_true")
    a = ap.parse_args()
    if a.cpp:
        print(cpp_rows())
        return 0
    root = tempfile.mkdtemp(prefix="fbstores") if a.check else a.out
    for r in ROUNDS:
        write_round(root, r)
    if a.check:
        bad = 0
        for r in ROUNDS:
            p = subprocess.run(["diff", "-r", os.path.join(a.out, r.key), os.path.join(root, r.key)],
                               capture_output=True, text=True)
            if p.returncode != 0:
                print("gen_air_stores: %s differs:\n%s" % (r.key, p.stdout[:1500]))
                bad += 1
        shutil.rmtree(root)
        if bad:
            print("gen_air_stores: FAILED — %d round(s) are not what the recipe produces" % bad,
                  file=sys.stderr)
            return 1
        print("gen_air_stores: %d round(s) match the recipe byte for byte" % len(ROUNDS))
        return 0
    print("gen_air_stores: %d round(s) -> %s" % (len(ROUNDS), root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
