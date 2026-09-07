"""Run cache-writing tests without touching the developer's real cache/config."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="draconis-test-") as directory:
    env = os.environ.copy()
    for name in ("HOME", "USERPROFILE", "LOCALAPPDATA", "APPDATA", "XDG_CACHE_HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "TEMP", "TMP", "TMPDIR"):
        path = Path(directory) / name
        path.mkdir()
        env[name] = str(path)
    sys.exit(subprocess.run(sys.argv[1:], env=env).returncode)
