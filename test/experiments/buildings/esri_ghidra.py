"""Inspect selected external Esri geometry primitives with bounded Ghidra headless work."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import tempfile
import time

from evidence import revision


CORE = Path('/home/cosmo/Downloads/esri_ce_sdk-3.4.12206/bin/libcom.esri.prt.core.so')
HEADLESS = Path('/snap/bin/ghidra.analyzeHeadless')
SCRIPT = r'''
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.framework.Application;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import com.google.gson.GsonBuilder;
import java.nio.file.*;
import java.util.*;
import java.util.regex.Pattern;

public class InspectBuildingCore extends GhidraScript {
    public void run() throws Exception {
        Path output = Path.of(getScriptArgs()[0]);
        Files.createDirectories(output);
        Map<String,Object> report = new LinkedHashMap<>();
        report.put("ghidra_version", Application.getApplicationVersion());
        report.put("program", currentProgram.getName());
        report.put("language", currentProgram.getLanguageID().toString());
        report.put("image_base", currentProgram.getImageBase().toString());
        Pattern relevant = Pattern.compile("(?i)(offset|skeleton|convex|split|subdiv|boolean|extrud|interior|room|floor|stair|roof|clip|triangul)");
        List<Map<String,Object>> names = new ArrayList<>(), selected = new ArrayList<>();
        FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        int total = 0;
        while (functions.hasNext() && !monitor.isCancelled()) {
            Function function = functions.next();
            if (function.isExternal()) continue;
            total++;
            String name = function.getName(true);
            String address = function.getEntryPoint().toString();
            if (relevant.matcher(name).find()) names.add(Map.of("name",name,"address",address,"thunk",function.isThunk()));
            if (!name.contains("PolygonOps") || name.contains(".cold") || !(name.contains("computeOffset") || name.contains("computeSkeleton") || name.contains("convexify"))) continue;
            disassemble(function.getEntryPoint());
            CreateFunctionCmd.fixupFunctionBody(currentProgram,function,monitor);
            Map<String,Object> item = new LinkedHashMap<>();
            item.put("name", name);
            item.put("address", address);
            item.put("body_bytes", function.getBody().getNumAddresses());
            Map<String,Integer> calls = new TreeMap<>();
            InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(),true);
            int count = 0;
            while (instructions.hasNext()) {
                Instruction instruction = instructions.next();
                count++;
                for (Reference ref: currentProgram.getReferenceManager().getReferencesFrom(instruction.getAddress())) {
                    if (!ref.getReferenceType().isCall()) continue;
                    Function callee = currentProgram.getFunctionManager().getFunctionAt(ref.getToAddress());
                    String target = ref.getToAddress().toString()+" "+(callee==null?"unresolved":callee.getName(true));
                    calls.merge(target,1,Integer::sum);
                }
            }
            item.put("instructions",count);
            item.put("direct_call_sites",calls);
            DecompileResults result = decompiler.decompileFunction(function,25,monitor);
            item.put("decompile_completed",result.decompileCompleted());
            item.put("decompile_message",result.getErrorMessage());
            if (result.decompileCompleted() && result.getDecompiledFunction()!=null) {
                String file = "reference-"+address+".c";
                Files.writeString(output.resolve(file),result.getDecompiledFunction().getC());
                item.put("external_reference_file",file);
            }
            selected.add(item);
        }
        decompiler.dispose();
        report.put("functions_in_database",total);
        report.put("relevant_named_functions",names);
        report.put("selected_functions",selected);
        report.put("scope","Selected external geometry primitives; symbol matches do not prove a complete interior layout generator.");
        Files.writeString(output.resolve("ghidra-core.json"),new GsonBuilder().setPrettyPrinting().create().toJson(report)+"\n");
        println("Building core inspection: "+selected.size()+" selected functions; "+total+" database functions.");
    }
}
'''


def inspect(core, output):
    if not core.is_file() or not HEADLESS.is_file():
        raise FileNotFoundError('Expected external SDK core and ghidra.analyzeHeadless Snap')
    output.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(core.read_bytes()).hexdigest()
    log_path = Path(tempfile.gettempdir())/('outshine-buildings-ghidra-'+digest[:12]+'.log')
    snap_work = Path.home()/'snap/ghidra/common/outshine-building-analysis'
    snap_work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='outshine-buildings-ghidra-', dir=snap_work) as temporary:
        directory = Path(temporary)
        reference = directory/'reference'
        (directory/'InspectBuildingCore.java').write_text(SCRIPT)
        command = [str(HEADLESS), str(directory), 'EsriBuildingCore', '-import', str(core),
                   '-scriptPath', str(directory), '-postScript', 'InspectBuildingCore.java', str(reference),
                   '-noanalysis', '-max-cpu', '2', '-deleteProject']
        began = time.perf_counter()
        with log_path.open('w') as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                process.wait(timeout=360)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                raise
        elapsed = time.perf_counter()-began
        if process.returncode == 0:
            for path in reference.glob('*'):
                if path.suffix in ('.json', '.c'):
                    shutil.copy2(path, output/path.name)
    if process.returncode:
        raise RuntimeError(f'Ghidra exited {process.returncode}; inspect {log_path}')
    result_path = output/'ghidra-core.json'
    result = json.loads(result_path.read_text())
    if len(result['selected_functions']) != 3 or any(row['instructions'] < 5 or not row['decompile_completed']
                                                   for row in result['selected_functions']):
        raise ValueError(f'Expected three disassembled primitives; inspect {log_path}')
    result.update(**revision(), binary_path=str(core), binary_sha256=digest, sdk_version=[3, 4, 12206],
                  headless_exit_code=process.returncode, elapsed_seconds=elapsed, log_path=str(log_path),
                  analysis_mode='Selected function disassembly/body recovery; global auto-analysis disabled',
                  process_limit_seconds=360, decompile_limit_seconds_per_function=25,
                  decompiler_types_verified=False,
                  reference_script_sha256=hashlib.sha256(SCRIPT.encode()).hexdigest())
    for row in result['selected_functions']:
        row['signature'] = subprocess.check_output(['c++filt', row['name']], text=True).strip()
        row['direct_calls'] = [dict(address=key.split(' ', 1)[0], symbol=key.split(' ', 1)[1], sites=count,
                                   signature=subprocess.check_output(['c++filt', key.split(' ', 1)[1]], text=True).strip())
                               for key, count in row['direct_call_sites'].items()]
        if 'external_reference_file' in row:
            row['external_reference_sha256'] = hashlib.sha256((output/row['external_reference_file']).read_bytes()).hexdigest()
    result_path.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, default=CORE)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/esri-ghidra'))
    args = parser.parse_args()
    result = inspect(args.core, args.output)
    print(json.dumps(dict(status='pass', ghidra=result['ghidra_version'], functions=result['functions_in_database'],
                          selected=len(result['selected_functions']), seconds=result['elapsed_seconds'])), flush=True)


if __name__ == '__main__':
    main()
