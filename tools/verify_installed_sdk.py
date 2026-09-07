"""Compile and run independent consumers against an already installed SDK."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

prefix = Path(sys.argv[1]).resolve()
env = os.environ.copy()
env['PKG_CONFIG_PATH'] = str(prefix / 'lib/pkgconfig') + os.pathsep + env.get('PKG_CONFIG_PATH', '')
for variable, location in [('PATH', 'bin'), ('LD_LIBRARY_PATH', 'lib'), ('DYLD_LIBRARY_PATH', 'lib')]:
    env[variable] = str(prefix / location) + os.pathsep + env.get(variable, '')
source = Path(__file__).resolve().parent.parent / 'tests/installed-sdk'
setup_options = []
# The C++ SDK exposes STL types, so consumers must match its debug ABI and CRT.
# Read the installed metadata instead of assuming the producer's build defaults.
for variable, option in [('sdk_buildtype', '--buildtype='), ('sdk_vscrt', '-Db_vscrt=')]:
    value = subprocess.check_output(
        ['pkg-config', f'--variable={variable}', 'draconis'], env=env, text=True
    ).strip()
    if value:
        setup_options.append(option + value)
with tempfile.TemporaryDirectory(prefix='draconis-sdk-') as directory:
    for arguments in [['setup', directory, str(source), *setup_options], ['compile', '-C', directory], ['test', '-C', directory, '--no-rebuild', '--print-errorlogs']]:
        subprocess.run([sys.executable, '-m', 'mesonbuild.mesonmain', *arguments], env=env, check=True)
