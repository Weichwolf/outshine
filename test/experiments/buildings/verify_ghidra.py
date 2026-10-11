"""Reject stale Ghidra evidence even when the headless launcher reports success."""

import argparse
import json
from pathlib import Path
import tempfile
from unittest.mock import patch

from esri_ghidra import inspect, read_analysis


class SuccessfulLauncher:
    returncode = 0

    def __init__(self, *args, **kwargs):
        pass

    def wait(self, timeout):
        return 0


def missing_export():
    with tempfile.TemporaryDirectory(prefix='outshine-buildings-ghidra-check-') as temporary:
        directory = Path(temporary)
        core = directory/'fake-core.so'
        core.write_bytes(b'headless success without script output')
        output = directory/'existing'
        output.mkdir()
        previous = output/'ghidra-core.json'
        previous.write_text('{"old_reference":true}\n')
        with patch('esri_ghidra.subprocess.Popen', SuccessfulLauncher), patch('esri_ghidra.sized_targets', return_value={}):
            try:
                inspect(core, output)
            except RuntimeError as error:
                if 'fresh script export' not in str(error):
                    raise
            else:
                raise AssertionError('Headless success reused an old export')
        if previous.read_text() != '{"old_reference":true}\n':
            raise AssertionError('Failed inspection changed previous reference evidence')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, default=Path('build/experiments/buildings/esri-ghidra'))
    args = parser.parse_args()
    result = read_analysis(args.reference, 'existing reference')
    missing_export()
    print(json.dumps(dict(status='pass', fresh_primitives=len(result['selected_functions']), stale_export_rejected=True)))


if __name__ == '__main__':
    main()
