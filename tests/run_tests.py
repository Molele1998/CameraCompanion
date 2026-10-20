"""Compile and run the real firmware's host suites; no hardware is accessed."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='g++')
parser.add_argument('--output', type=Path, default=ROOT / '.build/host')
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
includes = ['-I', str(ROOT / 'CameraCompanion'), '-I', str(ROOT / 'tests'),
            '-I', str(ROOT / 'tests/scene_fakes')]
for name in ['model', 'radio', 'ui', 'scene']:
    executable = output / ('test_' + name + ('.exe' if sys.platform == 'win32' else ''))
    command = [args.compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
               '-Wno-misleading-indentation', *includes]
    if name == 'scene':
        command.append('-DCAMERA_FONT_HOST=1')
    subprocess.run([*command, str(ROOT / 'tests' / ('test_' + name + '.cpp')),
                    '-o', str(executable)], check=True)
    subprocess.run([str(executable), *([str(output)] if name == 'scene' else [])], check=True)
subprocess.run([sys.executable, str(ROOT / 'tools/render_preview.py'),
                '--input', str(output)], check=True)
print('PASS: four host suites; synthetic data only, no physical device validation.')
