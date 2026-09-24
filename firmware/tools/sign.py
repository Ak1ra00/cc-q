#!/usr/bin/env python3
# Runs Coinkite's signit.py (kept unmodified) the way its setup.py entry point
# does: signit=signit:main. Usage is signit's own, e.g. `sign.py sign --help`.
import os, sys

here = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [here, os.path.join(here, '..', 'python')]      # signit, sigheader

from signit import main
main(prog_name='signit')
