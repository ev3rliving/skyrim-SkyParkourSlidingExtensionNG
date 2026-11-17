"""Compatibility checker alias; accepts --build-directory."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('verify_compatibility.py')), run_name='__main__')
