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
TARGETS = {
    '_ZN11CoreToolbox10PolygonOps13computeOffsetEfPKfm': 'polygon offset wrapper',
    '_ZN11CoreToolbox10PolygonOps15computeSkeletonEPKfm': 'polygon straight skeleton',
    '_ZN11CoreToolbox10PolygonOps9convexifyEPfm': 'polygon convex decomposition',
    '_ZN11CoreToolbox9CGALTools13computeOffsetEfPKfmRSt6vectorIfSaIfEE': 'actual offset construction',
    '_ZN2GC10SSplitNode12prepareSplitER9Processor': 'split size preparation',
    '_ZN2GC10SSplitNode5splitEi': 'split allocation',
    '_ZN12_GLOBAL__N_18splitXYZER9ProcessorN4util8MeshBase4AXISEbRN2GC10SSplitNodeE': 'axis mesh subdivision',
    '_ZN9Processor7extrudeEd': 'face extrusion',
    '_ZN12_GLOBAL__N_117computeInnerRectsERKSt6vectorISt10shared_ptrIN4util14Polygon2DUtils18Polygon2dWithHolesEESaIS5_EERKS0_ISt4pairIddESaISB_EERS0_INS2_9Polygon2dESaISG_EENS_23InnerRectangleAlgorithmENS_23InnerRectangleAlignModeE.constprop.0': 'inner rectangle dispatch with holes',
    '_ZN4util16LargestRectangle20findLargestRectangleEb': 'largest rectangle search',
    '_ZN4util27LargestRectangleDanielsFast22leafOrthogonallyConvexERKNS_18Trapezoidalization4PolyE': 'orthogonally convex rectangle contacts',
    '_ZNK4util27LargestRectangleDanielsFast17continueRecursionERKNS_18Trapezoidalization4PolyEb': 'rectangle recursion bound',
    '_ZN9Processor4compEN2GC12CompSelector4EnumENS0_14ScopeAlignment4EnumERKSt10shared_ptrINS0_9SplitNodeEE': 'component selection and aligned scope',
    '_ZN9Processor1iERKSt10shared_ptrINSt7__cxx1112basic_stringIwSt11char_traitsIwESaIwEEEEN2GC14UpAxisSelector4EnumENSA_10InsertMode4EnumENSA_11CleanupMode4EnumE': 'asset insertion dispatch',
    '_ZN4util4Mesh16transformInPlaceERKNS_6MatrixIdEE': 'component mesh transformation',
    '_ZN20EncodePreparatorImpl12addInstancesEPN3prt5CacheERKN4util18GeometryAssetProxyESt10shared_ptrIN4prtx3URIEERKNS3_7Vector3IdEERKNS3_6MatrixIdEERKS7_INS8_8MaterialEEjRKNSt7__cxx1112basic_stringIwSt11char_traitsIwESaIwEEEPKNS0_12AttributeMapEjSU_RKS7_INS8_7ReportsEE': 'prepared asset instance output',
}
SCRIPT = r'''
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.framework.Application;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.Address;
import ghidra.program.model.pcode.PcodeBlockBasic;
import com.google.gson.GsonBuilder;
import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import java.nio.file.*;
import java.util.*;
import java.util.regex.Pattern;

public class InspectBuildingCore extends GhidraScript {
    public void run() throws Exception {
        Path output = Path.of(getScriptArgs()[0]);
        Files.createDirectories(output);
        Map<String,Map<String,Object>> targets = new Gson().fromJson(Files.readString(Path.of(getScriptArgs()[1])),
            new TypeToken<Map<String,Map<String,Object>>>(){}.getType());
        Map<String,Object> report = new LinkedHashMap<>();
        report.put("schema_version",2);
        report.put("ghidra_version", Application.getApplicationVersion());
        report.put("program", currentProgram.getName());
        report.put("language", currentProgram.getLanguageID().toString());
        report.put("image_base", currentProgram.getImageBase().toString());
        Pattern relevant = Pattern.compile("(?i)(offset|skeleton|convex|split|subdiv|boolean|extrud|interior|room|floor|stair|roof|clip|triangul|rectangle|trapezoid|insert|instance|transformInPlace|Processor4comp)");
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
            if (!targets.containsKey(name)) continue;
            Map<String,Object> targetMeta = targets.get(name);
            long symbolBytes = ((Number)targetMeta.get("elf_symbol_bytes")).longValue();
            Address last = function.getEntryPoint().add(symbolBytes-1);
            AddressSet scope = new AddressSet(function.getEntryPoint(),last);
            disassemble(function.getEntryPoint());
            CreateFunctionCmd.fixupFunctionBody(currentProgram,function,monitor);
            Map<String,Object> item = new LinkedHashMap<>();
            item.put("name", name);
            item.put("purpose", targetMeta.get("purpose"));
            item.put("elf_symbol_bytes", symbolBytes);
            item.put("address", address);
            item.put("body_bytes", function.getBody().getNumAddresses());
            DecompileResults result = decompiler.decompileFunction(function,25,monitor);
            AddressSet recovered = new AddressSet(function.getBody()).intersect(scope);
            int outside = 0;
            if (result.getHighFunction()!=null) {
                for (PcodeBlockBasic block: result.getHighFunction().getBasicBlocks()) {
                    Address start = block.getStart(), end = block.getStop();
                    if (start.compareTo(function.getEntryPoint())<0 || end.compareTo(last)>0) outside++;
                    if (start.compareTo(function.getEntryPoint())<0) start = function.getEntryPoint();
                    if (end.compareTo(last)>0) end = last;
                    if (start.compareTo(end)<=0) {
                        recovered.addRange(start,end);
                        disassemble(start);
                    }
                }
            }
            item.put("control_flow_blocks_outside_symbol", outside);
            item.put("recovered_control_flow_bytes",recovered.getNumAddresses());
            Map<String,Integer> calls = new TreeMap<>();
            InstructionIterator instructions = currentProgram.getListing().getInstructions(recovered,true);
            int count = 0, indirect = 0;
            while (instructions.hasNext()) {
                Instruction instruction = instructions.next();
                count++;
                if (instruction.getFlowType().isCall() && instruction.getFlows().length==0) indirect++;
                for (Reference ref: currentProgram.getReferenceManager().getReferencesFrom(instruction.getAddress())) {
                    if (!ref.getReferenceType().isCall()) continue;
                    Function callee = currentProgram.getFunctionManager().getFunctionAt(ref.getToAddress());
                    String target = ref.getToAddress().toString()+" "+(callee==null?"unresolved":callee.getName(true));
                    calls.merge(target,1,Integer::sum);
                }
            }
            item.put("instructions",count);
            item.put("indirect_call_sites",indirect);
            item.put("direct_call_sites",calls);
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


def sized_targets(core):
    symbols = subprocess.check_output(['nm', '-S', '--defined-only', str(core)], text=True)
    targets = {}
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[3] in TARGETS:
            targets[fields[3]] = dict(purpose=TARGETS[fields[3]], elf_symbol_bytes=int(fields[1], 16))
    if set(targets) != set(TARGETS):
        raise ValueError('Selected reference functions require their ELF symbol boundaries')
    return targets


def read_analysis(reference, log_path):
    path = reference/'ghidra-core.json'
    if not path.is_file():
        raise RuntimeError(f'Headless returned without a fresh script export; inspect {log_path}')
    result = json.loads(path.read_text())
    selected = result.get('selected_functions', [])
    if (result.get('schema_version') != 2 or {row['name'] for row in selected} != set(TARGETS)
            or any(row.get('instructions', 0) < 5 or not row.get('decompile_completed')
                   or not 0 < row.get('recovered_control_flow_bytes', 0) <= row.get('elf_symbol_bytes', 0)
                   or not (reference/row.get('external_reference_file', '')).is_file() for row in selected)):
        raise ValueError(f'Expected fresh bounded exports for all {len(TARGETS)} primitives; inspect {log_path}')
    return result


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
        (directory/'targets.json').write_text(json.dumps(sized_targets(core)))
        command = [str(HEADLESS), str(directory), 'EsriBuildingCore', '-import', str(core),
                   '-scriptPath', str(directory), '-postScript', 'InspectBuildingCore.java', str(reference),
                   str(directory/'targets.json'),
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
        if process.returncode:
            raise RuntimeError(f'Ghidra exited {process.returncode}; inspect {log_path}')
        result = read_analysis(reference, log_path)
        for path in reference.glob('*'):
            if path.suffix in ('.json', '.c'):
                shutil.copy2(path, output/path.name)
    result_path = output/'ghidra-core.json'
    result.update(**revision(), binary_path=str(core), binary_sha256=digest, sdk_version=[3, 4, 12206],
                  headless_exit_code=process.returncode, elapsed_seconds=elapsed, log_path=str(log_path),
                  analysis_mode='Selected function disassembly/body recovery; global auto-analysis disabled',
                  process_limit_seconds=360, decompile_limit_seconds_per_function=25,
                  decompiler_types_verified=False,
                  symbol_body_coverage_verified=False,
                  call_scope='Disassembled control-flow blocks clamped to ELF symbol boundaries; indirect callees remain unresolved',
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
