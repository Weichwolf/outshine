#!/usr/bin/env python3
"""Audit the Make shader inventory against actual SPIR-V and SDL descriptor conventions.

This proves package coverage and the supported SDL binding profile, not renderer
graphics selection, host struct layout, interface linkage or backend execution.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
STAGES = {'vert', 'frag', 'comp'}
RESOURCE_KINDS = ('textures', 'images', 'ssbos', 'ubos')
METADATA = {'entryPoints', 'types', 'inputs', 'outputs'}
COMPUTE_FIELDS = ('samplers', 'readonly_textures', 'readwrite_textures', 'readonly_buffers',
                  'readwrite_buffers', 'uniform_buffers', 'group_x', 'group_y', 'group_z')


def validate_reflection(document, stage):
    if not isinstance(document, dict):
        raise ValueError('reflection is not an object')
    entries = document.get('entryPoints', [])
    if len(entries) != 1 or entries[0].get('name') != 'main' or entries[0].get('mode') != stage:
        raise ValueError(f'expected exactly main in stage {stage}')
    if stage == 'comp':
        entry = entries[0]
        group = entry.get('workgroup_size', [])
        specialized = entry.get('workgroup_size_is_spec_constant_id', [])
        if (len(group) != 3 or any(type(size) is not int or size <= 0 for size in group) or
                len(specialized) != 3 or any(specialized)):
            raise ValueError('compute workgroup must have three fixed positive dimensions')
    for name in document.keys() - METADATA - set(RESOURCE_KINDS):
        if document[name]:
            raise ValueError(f'unsupported reflection category: {name}')
    sets = {}
    resources = []
    for kind in RESOURCE_KINDS:
        for resource in document.get(kind, []):
            if resource.get('array'):
                raise ValueError('descriptor arrays need an explicit runtime binding contract')
            binding = resource.get('binding')
            descriptor_set = resource.get('set')
            if (type(binding) is not int or binding < 0 or
                    type(descriptor_set) is not int or descriptor_set < 0):
                raise ValueError('resource lacks a nonnegative set and binding')
            readonly = resource.get('readonly', False)
            rank = RESOURCE_KINDS.index(kind)
            if kind == 'ubos':
                required = 2 if stage == 'comp' else 1 if stage == 'vert' else 3
            elif stage == 'comp':
                required = 0 if kind == 'textures' or readonly else 1
            else:
                required = 0 if stage == 'vert' else 2
                if kind in ('images', 'ssbos') and not readonly:
                    raise ValueError('graphics storage resources must be readonly')
            if descriptor_set != required:
                raise ValueError(f'{kind} binding {binding}: set {descriptor_set}, SDL requires {required}')
            sets.setdefault(descriptor_set, []).append((binding, rank))
            resources.append({'kind': kind, 'set': descriptor_set, 'binding': binding})
    for descriptor_set, bindings in sets.items():
        ordered = sorted(bindings)
        if [binding for binding, _ in ordered] != list(range(len(ordered))):
            raise ValueError(f'set {descriptor_set}: bindings must be unique and consecutive from zero')
        ranks = [rank for _, rank in ordered]
        if ranks != sorted(ranks):
            raise ValueError(f'set {descriptor_set}: SDL requires samplers, storage textures, then buffers')
    return resources


def inventory(root, manifest):
    names = manifest.read_text().splitlines()
    if not names or any(not name for name in names):
        raise ValueError('empty shader inventory or empty inventory row')
    if len(names) != len(set(names)):
        raise ValueError('duplicate shader inventory entry')
    directory = root / 'build/shaders'
    for name in names:
        path = Path(name)
        if (path.is_absolute() or '..' in path.parts or path.parent != Path('build/shaders') or
                path.suffix != '.spv' or path.stem.rsplit('.', 1)[-1] not in STAGES):
            raise ValueError(f'invalid shader inventory path: {name}')
        if not (root / path).resolve().is_relative_to(directory.resolve()):
            raise ValueError(f'shader artifact escapes package: {name}')
    stages = {Path(name).stem.rsplit('.', 1)[-1] for name in names}
    if stages != STAGES:
        raise ValueError('shader package must declare vertex, fragment and compute stages')
    actual = {str(path.relative_to(root)) for path in directory.rglob('*.spv')}
    unexpected = sorted(actual - set(names))
    return sorted(names), [f'undeclared shader artifact: {name}' for name in unexpected]


def inspect_artifact(root, name, tool, timeout, report):
    started = time.monotonic()
    stage = Path(name).stem.rsplit('.', 1)[-1]
    record = {'artifact': name, 'stage': stage, 'reflected': False, 'valid': False}
    try:
        data = (root / name).read_bytes()
        record['sha256'] = hashlib.sha256(data).hexdigest()
        if len(data) < 20 or len(data) % 4 or data[:4] != b'\x03\x02\x23\x07':
            raise ValueError('not a complete SPIR-V word stream with header')
        result = subprocess.run([tool, str(root / name), '--reflect'],
                                capture_output=True, text=True, timeout=timeout)
        record['returncode'] = result.returncode
        (report / (Path(name).name + '.stderr')).write_text(result.stderr)
        if result.returncode:
            raise ValueError(f'reflection tool exited {result.returncode}: {result.stderr.strip()[:500]}')
        (report / (Path(name).name + '.json')).write_text(result.stdout)
        document = json.loads(result.stdout)
        record['reflected'] = True
        record['resources'] = validate_reflection(document, stage)
        if stage == 'comp':
            record['workgroup'] = document['entryPoints'][0]['workgroup_size']
        record['valid'] = True
    except (OSError, ValueError, TypeError, KeyError, AttributeError, subprocess.TimeoutExpired) as error:
        record['error'] = str(error)
    record['elapsed_seconds'] = time.monotonic() - started
    return record


def check_compute_contracts(path, names, records):
    result = {'source': str(path), 'declared': 0, 'checked': 0, 'valid': 0, 'errors': []}
    try:
        text = path.read_text()
        result['sha256'] = hashlib.sha256(text.encode()).hexdigest()
        entries = json.loads(text)
        if not isinstance(entries, list) or not entries:
            raise ValueError('compute catalog must be a nonempty array')
        result['declared'] = len(entries)
        contracts = {}
        for entry in entries:
            if not isinstance(entry, dict) or set(entry) != {'artifact', 'shape'}:
                raise ValueError('invalid compute catalog entry')
            name = entry['artifact']
            shape = entry['shape']
            if not isinstance(name, str) or name in contracts or not name.endswith('.comp.spv'):
                raise ValueError('invalid or duplicate compute catalog artifact')
            if (not isinstance(shape, dict) or set(shape) != set(COMPUTE_FIELDS) or
                    any(type(value) is not int or value < 0 or value > 0xffffffff
                        for value in shape.values())):
                raise ValueError('compute shape must declare every uint32 field')
            contracts[name] = shape
        built = {name for name in names if name.endswith('.comp.spv')}
        result['errors'].extend(f'uncatalogued compute artifact: {name}' for name in sorted(built - contracts.keys()))
        result['errors'].extend(f'compute catalog requests unbuilt artifact: {name}' for name in sorted(contracts.keys() - built))
        by_name = {record['artifact']: record for record in records}
        for name in sorted(built & contracts.keys()):
            record = by_name.get(name)
            if not record or not record['valid']:
                result['errors'].append(f'compute contract has no valid reflection: {name}')
                continue
            resources = record['resources']
            actual = {
                'samplers': sum(row['kind'] == 'textures' for row in resources),
                'readonly_textures': sum(row['kind'] == 'images' and row['set'] == 0 for row in resources),
                'readwrite_textures': sum(row['kind'] == 'images' and row['set'] == 1 for row in resources),
                'readonly_buffers': sum(row['kind'] == 'ssbos' and row['set'] == 0 for row in resources),
                'readwrite_buffers': sum(row['kind'] == 'ssbos' and row['set'] == 1 for row in resources),
                'uniform_buffers': sum(row['kind'] == 'ubos' for row in resources),
                'group_x': record['workgroup'][0], 'group_y': record['workgroup'][1],
                'group_z': record['workgroup'][2]}
            mismatches = [field for field in COMPUTE_FIELDS if actual[field] != contracts[name][field]]
            result['checked'] += 1
            if mismatches:
                result['errors'].extend(f'{name}: compute shape.{field} declares {contracts[name][field]}, '
                                        f'reflection requires {actual[field]}' for field in mismatches)
            else:
                result['valid'] += 1
    except (OSError, ValueError, TypeError) as error:
        result['errors'].append(str(error))
    return result


def audit(root, manifest, contracts, tool, report, timeout=30, jobs=4):
    started = time.monotonic()
    report.mkdir(parents=True, exist_ok=True)
    names, records, errors = [], [], []
    tool_hash = None
    resolved_tool = shutil.which(tool)
    try:
        names, errors = inventory(root, manifest)
        if not resolved_tool:
            raise ValueError(f'reflection tool unavailable: {tool}')
        tool_hash = hashlib.sha256(Path(resolved_tool).read_bytes()).hexdigest()
        with ThreadPoolExecutor(max_workers=jobs) as workers:
            records = list(workers.map(
                lambda name: inspect_artifact(root, name, resolved_tool, timeout, report), names))
    except (OSError, ValueError) as error:
        errors.append(str(error))
    compute = check_compute_contracts(contracts, names, records)
    errors.extend(compute['errors'])
    valid = sum(record['valid'] for record in records)
    summary = {'tool': resolved_tool, 'tool_sha256': tool_hash,
               'inventory': str(manifest), 'artifacts': records, 'compute_contracts': compute,
               'errors': errors, 'checked': len(records), 'valid': valid,
               'reflected': sum(record['reflected'] for record in records),
               'elapsed_seconds': time.monotonic() - started}
    (report / 'shader-audit.json').write_text(json.dumps(summary, indent=2) + '\n')
    for error in errors:
        print(f'shaders: {error}', file=sys.stderr)
    for record in records:
        if not record['valid']:
            print(f"shaders: {record['artifact']}: {record['error']}", file=sys.stderr)
    counts = ', '.join(f"{stage}={sum(record['stage'] == stage for record in records)}"
                       for stage in sorted(STAGES))
    print(f'shaders: {valid}/{len(records)} artifacts valid ({counts}); '
          f"{summary['elapsed_seconds']:.3f} s; report {report / 'shader-audit.json'}")
    print(f"shaders: {compute['valid']}/{compute['declared']} compute contracts match reflection")
    print('shaders: graphics selector/Shape coverage, host layouts, bound resource identities and backend execution remain separate checks')
    return 0 if records and valid == len(records) and not errors else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--compute-contracts', type=Path)
    parser.add_argument('--tool', default='spirv-cross')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0 or args.jobs <= 0:
        parser.error('timeout and jobs must be positive finite values')
    root = args.root.resolve()
    return audit(root, args.manifest or root / 'build/shader-artifacts.txt',
                 args.compute_contracts or root / 'build/compute-shaders.json', args.tool,
                 args.report or Path(tempfile.mkdtemp(prefix='outshine-shaders-')),
                 args.timeout, args.jobs)


if __name__ == '__main__':
    sys.exit(main())
