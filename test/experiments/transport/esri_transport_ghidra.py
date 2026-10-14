"""Reuse Buildings' headless protocol, with strict Transport symbol scopes/exports."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import uuid

from esri_transport_catalog import BINARY_SHA256, CORE, TARGETS, catalog


ROOT = Path(__file__).resolve().parents[3]
HEADLESS = Path("/snap/bin/ghidra.analyzeHeadless")


def script_source():
    directory = ROOT / "test/experiments/buildings"
    sys.path.insert(0, str(directory))
    try:
        spec = importlib.util.spec_from_file_location("transport_buildings_ghidra_protocol", directory / "esri_ghidra.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    finally:
        sys.path.pop(0)
    text = module.SCRIPT.replace("InspectBuildingCore", "InspectTransportCore")
    changes = {
        'report.put("schema_version",2);': 'report.put("schema_version",3); report.put("run_id",getScriptArgs()[2]); report.put("binary_sha256",getScriptArgs()[3]);',
        'import ghidra.app.cmd.function.CreateFunctionCmd;': 'import ghidra.app.cmd.disassemble.DisassembleCommand;',
        'disassemble(function.getEntryPoint());\n            CreateFunctionCmd.fixupFunctionBody(currentProgram,function,monitor);':
            '''Address expected = currentProgram.getImageBase().add(((Number)targetMeta.get("elf_address")).longValue());
            if (!expected.equals(function.getEntryPoint())) throw new IllegalStateException("ELF address mapping mismatch: "+name);
            DisassembleCommand bounded = new DisassembleCommand(function.getEntryPoint(),scope,true);
            bounded.enableCodeAnalysis(false);
            if (!bounded.applyTo(currentProgram,monitor)) throw new IllegalStateException("Bounded disassembly failed: "+name);
            function.setBody(scope);
            List<Map<String,Object>> boundaryTransfers = new ArrayList<>();
            InstructionIterator scoped = currentProgram.getListing().getInstructions(scope,true);
            while (scoped.hasNext()) {
                Instruction instruction = scoped.next();
                if (!instruction.getFlowType().isJump()) continue;
                for (Address destination: instruction.getFlows()) {
                    if (scope.contains(destination)) continue;
                    boundaryTransfers.add(Map.of("site",instruction.getAddress().toString(),"destination",destination.toString(),
                        "original_mnemonic",instruction.getMnemonicString(),"conditional",instruction.getFlowType().isConditional(),
                        "model","CALL_RETURN boundary hypothesis, external path not analyzed"));
                    instruction.setFlowOverride(FlowOverride.CALL_RETURN);
                }
            }''',
        'disassemble(start);': 'new DisassembleCommand(start,scope,true).applyTo(currentProgram,monitor);',
        'int count = 0, indirect = 0;': 'int count = 0, indirect = 0; long decodedBytes = 0;',
        'count++;': '''count++;
                if (!scope.contains(instruction.getAddress()) || !scope.contains(instruction.getMaxAddress())) throw new IllegalStateException("Instruction outside ELF symbol");
                decodedBytes += instruction.getLength();''',
        'item.put("instructions",count);': 'item.put("instructions",count); item.put("decoded_instruction_bytes",decodedBytes);',
        'item.put("purpose", targetMeta.get("purpose"));': 'item.put("purpose", targetMeta.get("purpose")); item.put("boundary_transfer_models",boundaryTransfers);',
        'Selected external geometry primitives; symbol matches do not prove a complete interior layout generator.':
            'Selected external transport geometry operations; no complete OSM road generator proof.',
    }
    for old, new in changes.items():
        if text.count(old) != 1:
            raise ValueError("Buildings Ghidra protocol changed; inspect adaptation before execution")
        text = text.replace(old, new)
    return text


def read_analysis(reference, targets, run_id, binary_sha256):
    path = reference / "ghidra-core.json"
    if not path.is_file():
        raise RuntimeError("Headless returned without a fresh script export")
    result = json.loads(path.read_text())
    rows = result.get("selected_functions", [])
    if result.get("schema_version") != 3 or result.get("run_id") != run_id or result.get("binary_sha256") != binary_sha256:
        raise ValueError("stale or foreign headless export")
    if len(rows) != len(targets) or {row["name"] for row in rows} != set(targets):
        raise ValueError("incomplete selected symbol export")
    base = int(result["image_base"], 16)
    for row in rows:
        target = targets[row["name"]]
        file = reference / row.get("external_reference_file", "")
        if (int(row["address"], 16) != target["elf_address"] + base or row["elf_symbol_bytes"] != target["elf_symbol_bytes"]
                or row["body_bytes"] != target["elf_symbol_bytes"] or not row["decompile_completed"]
                or row.get("control_flow_blocks_outside_symbol", 1) or row.get("instructions", 0) < 5
                or not 0 < row.get("decoded_instruction_bytes", 0) <= target["elf_symbol_bytes"]
                or not file.is_file() or not file.stat().st_size):
            raise ValueError("incomplete or out-of-range selected function: " + row["name"])
        if row.get("external_reference_sha256") and hashlib.sha256(file.read_bytes()).hexdigest() != row["external_reference_sha256"]:
            raise ValueError("changed decompiler export: " + row["name"])
    return result


def inspect(core, output, *, process_limit_s=360, launcher=subprocess.Popen):
    inventory = catalog(core)
    if inventory["binary_sha256"] != BINARY_SHA256:
        raise ValueError("selected hypotheses require the version-pinned external SDK")
    source, run_id = script_source(), uuid.uuid4().hex
    targets = inventory["selected_symbols"]
    snap_work = Path.home() / "snap/ghidra/common/outshine-transport-analysis"
    snap_work.mkdir(parents=True, exist_ok=True)
    log_path = Path(tempfile.gettempdir()) / ("outshine-transport-ghidra-" + run_id + ".log")
    began = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="run-", dir=snap_work) as temporary:
        directory, reference = Path(temporary), Path(temporary) / "reference"
        (directory / "InspectTransportCore.java").write_text(source)
        (directory / "targets.json").write_text(json.dumps(targets))
        command = [str(HEADLESS), str(directory), "EsriTransportCore", "-import", str(core.resolve()),
                   "-scriptPath", str(directory), "-postScript", "InspectTransportCore.java", str(reference),
                   str(directory / "targets.json"), run_id, inventory["binary_sha256"], "-noanalysis", "-max-cpu", "2", "-deleteProject"]
        with log_path.open("w") as log:
            process = launcher(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                process.wait(timeout=process_limit_s)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                raise
        if process.returncode:
            raise RuntimeError(f"Headless exited {process.returncode}; inspect {log_path}")
        try:
            result = read_analysis(reference, targets, run_id, inventory["binary_sha256"])
        except (ValueError, RuntimeError) as error:
            if reference.is_dir():
                failed = output.parent / ("failed-" + run_id)
                shutil.copytree(reference, failed)
                (failed / "failure.json").write_text(json.dumps(dict(validated=False, error=str(error), log_path=str(log_path))) + "\n")
            raise
        if hashlib.sha256(core.read_bytes()).hexdigest() != inventory["binary_sha256"]:
            raise ValueError("SDK binary changed during inspection")
        result.update(catalog=inventory, elapsed_seconds=time.perf_counter() - began, log_path=str(log_path),
                      command=command, process_limit_seconds=process_limit_s, sdk_version_requested=[3, 4, 12206],
                      script_sha256=hashlib.sha256(source.encode()).hexdigest(), headless_exit_code=process.returncode,
                      decompiler_types_verified=False, all_reachable_symbol_bytes_verified=False,
                      call_scope="Instructions restricted to ELF ranges; external/indirect callees may remain unresolved",
                      commit=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                      experiment_tree_dirty=bool(subprocess.check_output(["git", "status", "--porcelain", "--", "test/experiments/transport"], cwd=ROOT, text=True)))
        output.mkdir(parents=True, exist_ok=True)
        for row in result["selected_functions"]:
            row.update(elf_address=targets[row["name"]]["elf_address"], signature=targets[row["name"]]["signature"])
            row["direct_calls"] = [dict(address=key.split(" ", 1)[0], symbol=key.split(" ", 1)[1], sites=count,
                                        signature=subprocess.check_output(["c++filt", key.split(" ", 1)[1]], text=True).strip())
                                   for key, count in row["direct_call_sites"].items()]
            for call in row["direct_calls"]:
                elf = int(call["address"], 16) - int(result["image_base"], 16)
                call["elf_address"] = elf
                if call["symbol"] == "unresolved" and elf in inventory["plt_imports"]:
                    call["symbol"] = inventory["plt_imports"][elf] + "@plt"
                    call["signature"] = subprocess.check_output(["c++filt", inventory["plt_imports"][elf]], text=True).strip()
            file = row["external_reference_file"]
            row["external_reference_sha256"] = hashlib.sha256((reference / file).read_bytes()).hexdigest()
            shutil.copy2(reference / file, output / file)
            assembly = output / ("assembly-" + hex(row["elf_address"])[2:] + ".txt")
            text = subprocess.check_output(["objdump", "-d", "--no-show-raw-insn", "--start-address=" + hex(row["elf_address"]),
                                            "--stop-address=" + hex(row["elf_address"] + row["elf_symbol_bytes"]), str(core)], text=True)
            assembly.write_text(text)
            row.update(assembly_reference_file=assembly.name, assembly_reference_sha256=hashlib.sha256(assembly.read_bytes()).hexdigest())
        (output / "ghidra-core.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=CORE)
    parser.add_argument("--output", type=Path, default=Path("build/experiments/transport/esri-reference/ghidra"))
    args = parser.parse_args()
    result = inspect(args.core, args.output)
    print(json.dumps(dict(status="pass", selected=len(result["selected_functions"]), seconds=result["elapsed_seconds"],
                          ghidra=result["ghidra_version"], log_path=result["log_path"])))
