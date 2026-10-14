"""Inventory external SDK text symbols; names identify candidates, not semantics."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


CORE = Path.home() / "Downloads/esri_ce_sdk-3.4.12206/bin/libcom.esri.prt.core.so"
BINARY_SHA256 = "850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45"
GROUPS = dict(offset=r"offset|Offset|buffer|Buffer", intersection=r"intersect|Intersect|clip|Clip|boolOp|Boolean",
              projection=r"project|Project|Plane|plane", triangulation=r"triangul|Triangul", extrusion=r"extrud|Extrud",
              geometry=r"MeshUtils|Polygon2DUtils|PolygonOps|Math::")
TARGETS = {
    "_ZN3prt7VersionC1Ev": "public Version object constructor: SDK and ABI provenance",
    "_ZN11CoreToolbox10PolygonOps13computeOffsetEfPKfm": "exported single-ring float XZ offset helper",
    "_ZN11CoreToolbox9CGALTools13computeOffsetEfPKfmRSt6vectorIfSaIfEE": "offset helper's local CGAL dispatch and component selection",
    "_ZN4util12intersection14edgesIntersectERKNS_7Vector3IdEES4_S4_S4_RS2_": "local edge intersection and numerical limits",
    "_ZN4util12intersection9getXPointERKNS_7Vector3IdEES4_S4_S4_iRS2_Rd": "line/ray intersection modes and distance output; integer semantics require oracle",
    "_ZN4util4Mesh35fillVertexPlaneDistVectorAndProjectERSt6vectorIdSaIdEERKNS_15GemsPlane2Plane8vect4strEd": "signed plane distances and vertex projection",
    "_ZN4util9MeshUtils26projectToBestMainAxisPlaneERKSt6vectorINS_7Vector3IdEESaIS3_EERKNS_4Mesh7PolygonERi.constprop.0": "best main-axis polygon projection",
    "_ZN4util12Triangulator11triangulateERKNS_4MeshEPN5boost9container12small_vectorIjLm4EvvEEb": "logical mesh triangulation dispatch",
    "_ZN4util12Triangulator23triangulateVerticesOnlyERKNS_4MeshENS0_4ModeE": "vertices-only triangulation entry",
    "_ZN4util12Triangulator23triangulateVerticesOnlyERKNS_4MeshEPSt6vectorINS_7Vector3IdEESaIS6_EENS0_4ModeE": "triangulation modes, projection and callbacks",
    "_ZN9Processor7extrudeEN2GC9AxisWorld4EnumEd": "world/normal extrusion dispatch; Processor object ABI unverified",
}


def catalog(core=CORE):
    raw = subprocess.check_output(["nm", "-S", "--defined-only", str(core)], text=True)
    rows = []
    for line in raw.splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[2].lower() == "t" and int(fields[1], 16):
            rows.append(dict(elf_address=int(fields[0], 16), elf_symbol_bytes=int(fields[1], 16),
                             symbol=fields[3], symbol_type=fields[2]))
    signatures = subprocess.check_output(["c++filt"], input="\n".join(r["symbol"] for r in rows) + "\n", text=True).splitlines()
    if len(signatures) != len(rows):
        raise ValueError("incomplete demangled symbol catalog")
    exported = {line.split()[-1] for line in subprocess.check_output(["nm", "-D", "--defined-only", str(core)], text=True).splitlines() if line.split()}
    plt = {}
    for line in subprocess.check_output(["objdump", "-d", "-j", ".plt", "--no-show-raw-insn", str(core)], text=True).splitlines():
        match = re.match(r"^([0-9a-f]+) <(.+)@plt>:$", line)
        if match:
            plt[int(match[1], 16)] = match[2]
    selected, relevant = {}, []
    for row, signature in zip(rows, signatures):
        groups = [name for name, pattern in GROUPS.items() if re.search(pattern, signature)]
        row.update(signature=signature, groups=groups, dynamic_export=row["symbol"] in exported,
                   elf_range_exclusive=[hex(row["elf_address"]), hex(row["elf_address"] + row["elf_symbol_bytes"])])
        if groups:
            relevant.append(row)
        if row["symbol"] in TARGETS:
            if row["symbol"] in selected:
                raise ValueError("ambiguous selected ELF symbol")
            selected[row["symbol"]] = dict(row, purpose=TARGETS[row["symbol"]])
    if set(selected) != set(TARGETS):
        raise ValueError("selected geometry operations missing from ELF")
    return dict(schema_version=1, sdk_version_requested=[3, 4, 12206], sdk_version_evidence="SDK folder label; validate through isolated public Version ABI",
                binary_path=str(core.resolve()), binary_sha256=hashlib.sha256(core.read_bytes()).hexdigest(),
                nm_command=["nm", "-S", "--defined-only"], demangler="c++filt", defined_text_symbols=len(rows),
                relevant_symbols=relevant, selected_symbols=selected, plt_imports=plt,
                scope="External operation candidates, including aliases/cold clones; no full OSM road generator proof")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=CORE)
    parser.add_argument("--output", type=Path, default=Path("build/experiments/transport/esri-reference/catalog.json"))
    args = parser.parse_args()
    result = catalog(args.core)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(dict(binary_sha256=result["binary_sha256"], symbols=result["defined_text_symbols"],
                          relevant=len(result["relevant_symbols"]), selected=len(result["selected_symbols"]))))
