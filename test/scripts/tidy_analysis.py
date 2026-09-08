#!/usr/bin/env python3
"""Run and account for every source translation unit before judging diagnostics."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import subprocess
import time

DIAGNOSTIC = re.compile(r'^(.+?):\d+:\d+: (?:fatal )?(?:warning|error):', re.MULTILINE)


def source_units(root):
    return sorted(path.resolve() for path in (root / 'src').rglob('*.cpp'))


def database_units(root):
    records = json.loads((root / 'compile_commands.json').read_text())
    if not isinstance(records, list) or not records:
        raise ValueError('compile database is empty or not an array')
    units = []
    for entry in records:
        if not isinstance(entry, dict) or not entry.get('file') or not entry.get('directory'):
            raise ValueError('compile entry lacks file or directory')
        if not entry.get('command') and not entry.get('arguments'):
            raise ValueError(f"compile entry lacks command: {entry['file']}")
        directory = Path(entry['directory'])
        if not directory.is_absolute():
            directory = root / directory
        path = Path(entry['file'])
        units.append((path if path.is_absolute() else directory / path).resolve())
    if len(set(units)) != len(units):
        raise ValueError('duplicate compile entries: analysis configuration would be ambiguous')
    expected = source_units(root)
    missing = sorted(set(expected) - set(units))
    extra = sorted(set(units) - set(expected))
    if not expected or missing or extra:
        raise ValueError(f'source coverage differs: missing={list(map(str, missing))}; '
                         f'unexpected={list(map(str, extra))}')
    return expected


def analyse(root, report, tool, jobs, timeout):
    report.mkdir(parents=True, exist_ok=True)
    summary = {'complete': False, 'expected': [], 'units': [], 'error': ''}
    diagnostics = set()
    combined = []
    try:
        units = database_units(root)
        summary['expected'] = [str(path) for path in units]
        version = subprocess.run([tool, '--version'], capture_output=True, text=True,
                                 timeout=timeout, check=True)
        summary['tool'] = {'path': str(Path(tool).resolve()), 'version': version.stdout.strip()}

        def run(one):
            began = time.monotonic()
            result = {'file': str(one), 'returncode': None, 'error': ''}
            try:
                process = subprocess.run([tool, '-p', str(root), '-quiet', str(one)],
                                         cwd=root, capture_output=True, text=True, timeout=timeout)
                result['returncode'] = process.returncode
                output = process.stdout + process.stderr
            except (OSError, subprocess.TimeoutExpired) as failed:
                result['error'] = str(failed)
                output = str(failed) + '\n'
            result['elapsedSeconds'] = time.monotonic() - began
            result['log'] = one.relative_to(root).as_posix().replace('/', '_') + '.log'
            (report / result['log']).write_text(output)
            return result, output

        with ThreadPoolExecutor(max_workers=jobs) as workers:
            for result, output in workers.map(run, units):
                summary['units'].append(result)
                combined.append(output)
                for line in output.splitlines():
                    match = DIAGNOSTIC.match(line)
                    if match:
                        path = Path(match[1])
                        path = (path if path.is_absolute() else root / path).resolve()
                        if path.is_relative_to(root):
                            diagnostics.add((str(path) + line[len(match[1]):]).replace(' [', '\t['))
        summary['complete'] = (len(summary['units']) == len(units) and
                               all(row['returncode'] == 0 for row in summary['units']))
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as failed:
        summary['error'] = str(failed)
    summary['findings'] = len(diagnostics)
    (report / 'tidy.log').write_text(''.join(combined))
    (report / 'tidy.unique').write_text(''.join(line + '\n' for line in sorted(diagnostics)))
    (report / 'tidy.execution.json').write_text(json.dumps(summary, indent=2) + '\n')
    successful = sum(row['returncode'] == 0 for row in summary['units'])
    print(f"tidy: {successful}/{len(summary['expected'])} units completed successfully; "
          f"{len(diagnostics)} finding(s); execution {'complete' if summary['complete'] else 'INCOMPLETE'}")
    if summary['error']:
        print('tidy: ' + summary['error'])
    for row in summary['units']:
        if row['returncode'] != 0:
            print(f"tidy: failed {row['file']}: status={row['returncode']} {row['error']}; "
                  f"log={report / row['log']}")
    return 2 if not summary['complete'] else int(bool(diagnostics))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--tool', required=True)
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 1)
    parser.add_argument('--timeout', type=float, default=120)
    args = parser.parse_args()
    if args.jobs < 1 or not 0 < args.timeout < float('inf'):
        parser.error('jobs and finite timeout must be positive')
    return analyse(args.root.resolve(), args.report.resolve(), args.tool, args.jobs, args.timeout)


if __name__ == '__main__':
    raise SystemExit(main())
