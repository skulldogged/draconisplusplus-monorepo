"""Write a Meson stamp only after the external build command succeeds."""
from pathlib import Path
import subprocess
import sys

result = subprocess.run(sys.argv[2:])
if result.returncode == 0:
    Path(sys.argv[1]).touch()
sys.exit(result.returncode)
