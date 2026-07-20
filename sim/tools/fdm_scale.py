#!/usr/bin/env python3
"""Froude (dynamic-similarity) FDM scaler: vanilla JSBSim reference model -> model-scale JSBSim model.

Physics — geometric length scale n = L_model / L_full. Under Froude-number similitude (V/sqrt(g*L)
constant, gravity-dominated free flight; Reynolds deliberately NOT matched — impossible without a
fluid change, standard NASA-Langley free-flight practice):

    length (span, chord, arms, all x/y/z locations) .... n
    area (wing/htail/vtail)  ........................... n^2
    mass (emptywt, pointmasses, tank contents) ......... n^3
    inertia (ixx..iyz) ................................. n^5
    gear spring_coeff  (force/length) .................. n^2
    gear damping_coeff (force/velocity) ................ n^2.5
    thrust / force  .................................... n^3
    engine power (force*velocity) ...................... n^3.5

The aerodynamics (<aerodynamics>) and control laws (<flight_control>/<system>) are DIMENSIONLESS
and stay VERBATIM — that is the whole point: the validated aero is preserved, only the airframe's
dimensional scale changes. The engine is re-emitted as a model powerplant (electric motor + prop, or
a scaled turbine) sized by the same laws.

Usage: fdm_scale.py <model-key> [out_root]   (model-keys defined in MODELS below)
       fdm_scale.py all [out_root]
"""
import os, re, sys, shutil

SRC_ROOT = os.path.join(os.path.dirname(__file__), "..", "jsbsim", "aircraft")
OUT_ROOT = os.path.join(os.path.dirname(__file__), "..", "aircraft", "models")

# --- target model definitions: vanilla source + geometric scale + emitted powerplant -------------
MODELS = {
    "c172": dict(src="c172p", n=0.20, engine=dict(
        kind="electric", power_W=("froude_pow", 119300.0),   # IO-320 160 HP -> n^3.5
        prop_in=("froude_len", 75.0), prop_ixx=("froude_i", 1.67))),
    "f16": dict(src="f16", n=1.0/6.0, engine=dict(
        kind="turbine", milthrust_lbf=("froude_f", 17800.0), maxthrust_lbf=("froude_f", 29000.0))),
    # motor-glider slot: vanilla Schweizer SGS 2-33 sailplane (2-seat trainer, consistent geometry,
    # validated aero) scaled 1:5 -> 3.1 m / 2.2 kg, plus an emitted self-launch electric motor. Chosen
    # over the SGS 1-26 for its higher wing loading (less floaty in NAV). The vanilla `minisgs` was
    # dropped: its wingarea (42 ft^2) is ~9x its geometric area -> ~1/3 cruise speed, too floaty for NAV.
    "sgs233": dict(src="sgs233", n=0.2, engine=dict(
        kind="electric", power_W=("fixed", 350.0),
        prop_in=("fixed", 13.0), prop_ixx=("fixed", 2.5e-4))),
}

# non-dimensional 2-blade fixed-pitch prop curve (validated JSBSim prop_generic2f, Ct/Cp vs advance
# ratio) — physically valid at ANY diameter, so it is the reusable model-prop aero.
PROP_CT = """0.0\t0.0936\n      0.1\t0.0927\n      0.2\t0.0918\n      0.3\t0.0909\n      0.4\t0.0828
      0.5\t0.0738\n      0.6\t0.063\n      0.7\t0.0495\n      0.8\t0.036\n      0.9\t0.0198
      1.0\t0.0045\n      1.04\t-0.0009"""
PROP_CP = """0.0\t0.0594\n      0.1\t0.0585\n      0.2\t0.0576\n      0.3\t0.0558\n      0.4\t0.054
      0.5\t0.0522\n      0.6\t0.0495\n      0.7\t0.0432\n      0.8\t0.0342\n      0.9\t0.0225
      1.0\t0.009\n      1.04\t0"""


def fnum(x):
    s = f"{x:.6g}"
    return s


def resolve(spec, n):
    kind, base = spec
    return {
        "fixed": base,
        "froude_len": base * n,
        "froude_i": base * n**5,
        "froude_f": base * n**3,
        "froude_pow": base * n**3.5,
    }[kind]


def scale_tag(text, tag, factor):
    """Multiply the numeric content of every <tag ...> V </tag> in text by factor (unit attr kept)."""
    pat = re.compile(rf"(<{tag}(?:\s[^>]*)?>)\s*(-?[\d.]+(?:[eE][-+]?\d+)?)\s*(</{tag}>)")
    return pat.sub(lambda m: f"{m.group(1)} {fnum(float(m.group(2))*factor)} {m.group(3)}", text)


def scale_xyz(text, n):
    """Scale every <x>/<y>/<z> location component by n (locations only occur in the sections we pass)."""
    for t in ("x", "y", "z"):
        text = scale_tag(text, t, n)
    return text


def section(text, tag):
    m = re.search(rf"<{tag}[ >].*?</{tag}>", text, re.S)
    return m.span() if m else None


def replace_section(text, tag, new_body):
    sp = section(text, tag)
    return text[:sp[0]] + new_body + text[sp[1]:] if sp else text


def scale_metrics(body, n):
    for t in ("wingarea", "htailarea", "vtailarea"):
        body = scale_tag(body, t, n**2)
    for t in ("wingspan", "chord", "htailarm", "vtailarm"):
        body = scale_tag(body, t, n)
    return scale_xyz(body, n)


def scale_mass(body, n):
    for t in ("ixx", "iyy", "izz", "ixy", "ixz", "iyz"):
        body = scale_tag(body, t, n**5)
    for t in ("emptywt", "weight"):
        body = scale_tag(body, t, n**3)
    return scale_xyz(body, n)


def scale_ground(body, n):
    body = scale_tag(body, "spring_coeff", n**2)
    body = scale_tag(body, "damping_coeff", n**2.5)
    return scale_xyz(body, n)


def emit_electric(key, eng, n):
    P = resolve(eng["power_W"], n)
    D = resolve(eng["prop_in"], n)
    I = resolve(eng["prop_ixx"], n)
    motor = f"""<?xml version="1.0"?>
<!-- {key} model electric motor. Froude-scaled shaft power = source_max * n^3.5 (n={n:.4g}) = {fnum(P)} W.
     FGElectric delivers <power>*throttle to the prop; the prop finds its own equilibrium RPM. -->
<electric_engine name="{key}_motor">
  <power unit="WATTS"> {fnum(P)} </power>
</electric_engine>
"""
    prop = f"""<?xml version="1.0"?>
<!-- {key} model propeller. Non-dimensional Ct/Cp(J) from validated JSBSim prop_generic2f (any diameter).
     diameter = source * n = {fnum(D)} in ; ixx = source * n^5 = {fnum(I)} slug*ft^2. -->
<propeller name="{key}_prop">
  <ixx> {fnum(I)} </ixx>
  <diameter unit="IN"> {fnum(D)} </diameter>
  <numblades> 2 </numblades>
  <minpitch> 23 </minpitch>
  <maxpitch> 23 </maxpitch>
  <gearratio> 1.0 </gearratio>
  <table name="C_THRUST" type="internal">
    <tableData>
      {PROP_CT}
    </tableData>
  </table>
  <table name="C_POWER" type="internal">
    <tableData>
      {PROP_CP}
    </tableData>
  </table>
</propeller>
"""
    files = {f"{key}_motor.xml": motor, f"{key}_prop.xml": prop}
    propulsion = f"""<propulsion>
    <engine file="{key}_motor">
      <thruster file="{key}_prop">
        <location unit="IN"> <x> 0.0 </x> <y> 0.0 </y> <z> 0.0 </z> </location>
        <sense> 1 </sense>
        <p_factor> 1.0 </p_factor>
      </thruster>
    </engine>
  </propulsion>"""
    return files, propulsion


def emit_turbine(key, eng, n, src_dir):
    mil = resolve(eng["milthrust_lbf"], n)
    mx = resolve(eng["maxthrust_lbf"], n)
    eng_dir = os.path.join(SRC_ROOT, "..", "engine")   # JSBSim ships engines in a shared dir
    turb = open(os.path.join(eng_dir, "F100-PW-229.xml")).read()
    turb = scale_tag(turb, "milthrust", n**3)
    turb = scale_tag(turb, "maxthrust", n**3)
    turb = turb.replace('name="F100"', f'name="{key}_turbine"')
    direct = open(os.path.join(eng_dir, "direct.xml")).read()
    files = {f"{key}_turbine.xml": turb, "direct.xml": direct}
    tank = f"""    <tank type="FUEL">
      <location unit="IN"> <x> 0.0 </x> <y> 0.0 </y> <z> 0.0 </z> </location>
      <capacity unit="LBS"> {fnum(3486.0*n**3)} </capacity>
      <contents unit="LBS"> {fnum(1500.0*n**3)} </contents>
    </tank>"""
    propulsion = f"""<propulsion>
    <engine file="{key}_turbine">
      <feed>0</feed>
      <thruster file="direct">
        <location unit="IN"> <x> 0.0 </x> <y> 0.0 </y> <z> 0.0 </z> </location>
      </thruster>
    </engine>
{tank}
  </propulsion>"""
    # note: keep milthrust/maxthrust comment for traceability
    print(f"    turbine {key}: milthrust {fnum(mil)} lbf, maxthrust {fnum(mx)} lbf")
    return files, propulsion


def build(key, out_root):
    cfg = MODELS[key]
    n = cfg["n"]
    src_dir = os.path.join(SRC_ROOT, cfg["src"])
    out_dir = os.path.join(out_root, key)
    xml = open(os.path.join(src_dir, f"{cfg['src']}.xml")).read()

    # --- drop ground-handling system includes (pushback/hook): irrelevant to autonomous flight and
    #     they pull external Systems files. The flight-critical FLCS is inline <flight_control>, kept. ---
    for gs in ("pushback", "hook"):
        xml = re.sub(rf'<system file="{gs}"\s*/>', "", xml)
        xml = re.sub(rf'<system file="{gs}">.*?</system>', "", xml, flags=re.S)

    # --- scale the airframe sections; aerodynamics + flight_control stay verbatim ---
    for tag, fn in (("metrics", scale_metrics), ("mass_balance", scale_mass),
                    ("ground_reactions", scale_ground)):
        sp = section(xml, tag)
        if sp:
            xml = xml[:sp[0]] + fn(xml[sp[0]:sp[1]], n) + xml[sp[1]:]

    # --- emit the model powerplant, replace <propulsion> ---
    eng = cfg["engine"]
    if eng["kind"] == "electric":
        efiles, propulsion = emit_electric(key, eng, n)
    else:
        efiles, propulsion = emit_turbine(key, eng, n, src_dir)
    if section(xml, "propulsion"):
        xml = replace_section(xml, "propulsion", propulsion)
    else:  # vanilla glider has an empty <propulsion/></propulsion>; insert after mass_balance
        xml = re.sub(r"<propulsion>\s*</propulsion>", propulsion, xml, count=1)

    xml = xml.replace(f'fdm_config name="{re.search(r"fdm_config name=.([^\"]+)", xml).group(1)}"',
                      f'fdm_config name="{key}"', 1)
    header = f"<!-- GENERATED by tools/fdm_scale.py from vanilla JSBSim '{cfg['src']}' at Froude scale " \
             f"n={n:.4g}. Aerodynamics + flight_control are verbatim; airframe & engine are scaled. " \
             f"Do not edit by hand — edit the scaler. -->\n"
    xml = xml.replace("<fdm_config", header + "<fdm_config", 1)

    # --- write out: fresh model dir (aero preserved, geometry/engine scaled) ---
    os.makedirs(os.path.join(out_dir, "engine"), exist_ok=True)
    with open(os.path.join(out_dir, f"{key}.xml"), "w") as f:
        f.write(xml)
    for fn_, body in efiles.items():
        with open(os.path.join(out_dir, "engine", fn_), "w") as f:
            f.write(body)
    # copy reset (Systems intentionally dropped: ground-handling includes were stripped above)
    for extra in ("reset00.xml",):
        s = os.path.join(src_dir, extra)
        d = os.path.join(out_dir, extra)
        if os.path.isdir(s):
            shutil.rmtree(d, ignore_errors=True); shutil.copytree(s, d)
        elif os.path.isfile(s):
            if os.path.exists(d):
                os.chmod(d, 0o644); os.remove(d)   # source tree may be chmod a-w (read-only submodule)
            shutil.copy(s, d)
    print(f"  {key}: {cfg['src']} x n={n:.4g} -> {out_dir}")


def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(2)
    out_root = sys.argv[2] if len(sys.argv) > 2 else OUT_ROOT
    keys = list(MODELS) if sys.argv[1] == "all" else [sys.argv[1]]
    for k in keys:
        build(k, out_root)


if __name__ == "__main__":
    main()
