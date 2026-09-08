#!/usr/bin/env python3
"""Check Doxygen diagnostics and prove that every public header was parsed."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]


def quoted(path):
    return '"' + str(path).replace('\\', '\\\\').replace('"', '\\"') + '"'


def analyse(root, report, tool='doxygen', timeout=120):
    report.mkdir(parents=True, exist_ok=True)
    began = time.monotonic()
    summary = {'complete': False, 'expected': [], 'parsed': [], 'diagnostics': 0,
               'returncode': None, 'error': ''}
    status = 2
    try:
        headers = sorted(path.resolve() for path in (root / 'include').rglob('*')
                         if path.is_file() and path.suffix in ('.h', '.hpp'))
        summary['expected'] = list(map(str, headers))
        if not headers:
            raise ValueError('public header coverage is empty')
        with tempfile.TemporaryDirectory(prefix='doxygen-', dir=report) as output:
            warnings = Path(output) / 'warnings.log'
            config = (root / 'doc/Doxyfile').read_text() + '\n' + '\n'.join([
                'INPUT = ' + ' '.join(map(quoted, headers)),
                'USE_MDFILE_AS_MAINPAGE =',
                'OUTPUT_DIRECTORY = ' + quoted(output),
                'GENERATE_HTML = NO', 'GENERATE_XML = YES', 'XML_PROGRAMLISTING = NO',
                'WARN_AS_ERROR = NO', 'WARN_LOGFILE = ' + quoted(warnings),
            ]) + '\n'
            (report / 'doxygen.config').write_text(config)
            process = subprocess.run([tool, '-'], input=config, cwd=root, text=True,
                                     capture_output=True, timeout=timeout, check=False)
            summary['returncode'] = process.returncode
            (report / 'doxygen.stdout').write_text(process.stdout)
            (report / 'doxygen.stderr').write_text(process.stderr)
            if warnings.is_file():
                diagnostics = warnings.read_text()
                (report / 'doxygen.log').write_text(diagnostics)
                summary['diagnostics'] = len(re.findall(
                    r'^.*?:\d+: (?:warning|error):', diagnostics, re.MULTILINE))
            else:
                raise ValueError('Doxygen did not produce its diagnostic log')
            if process.returncode:
                raise ValueError(f'Doxygen exited with status {process.returncode}')
            index = ET.parse(Path(output) / 'xml/index.xml')
            parsed = set()
            for compound in index.findall('compound'):
                if compound.get('kind') != 'file':
                    continue
                document = ET.parse(Path(output) / 'xml' / f'{compound.get("refid")}.xml')
                location = document.find('compounddef/location')
                if location is None or not location.get('file'):
                    raise ValueError('Doxygen file record has no source location')
                parsed.add((root / location.get('file')).resolve())
            summary['parsed'] = sorted(map(str, parsed))
            if parsed != set(headers):
                raise ValueError(f'header coverage differs: missing={sorted(set(headers) - parsed)}; '
                                 f'unexpected={sorted(parsed - set(headers))}')
            summary['complete'] = True
            status = 1 if diagnostics.strip() or process.stderr.strip() else 0
    except (OSError, ValueError, ET.ParseError, subprocess.SubprocessError) as error:
        summary['error'] = str(error)
    summary['seconds'] = round(time.monotonic() - began, 3)
    (report / 'doxygen.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'documentation: {len(summary["parsed"])}/{len(summary["expected"])} public headers; '
          f'{summary["diagnostics"]} diagnostics; '
          f'{"complete" if summary["complete"] else "incomplete"}; {summary["seconds"]}s')
    if summary['error']:
        print(f'documentation: {summary["error"]}')
    return status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--tool', default='doxygen')
    parser.add_argument('--timeout', type=float, default=120)
    args = parser.parse_args()
    return analyse(args.root.resolve(), args.report.resolve(), args.tool, args.timeout)


if __name__ == '__main__':
    raise SystemExit(main())
