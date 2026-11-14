# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

r"""components_phecompat -- phecompat's components (merged into core's registry when this repository is a layer:
PHOREPOC_ADAPTATIONS=<board adaptations>:<path to phecompat>). Paths are relative to the merged OS tree.

  phecompat_import  phecompatimport.exe: reads the profiles the package installs (\\private\e7016e02\profiles\*.ini) and writes
                    them into core's profile repository (CenRep 0xE7016E00), which the window server watches
  phepostsurf       PostingSurfaceFactory.dll: the Display Posting API for games that post frames (the N-Gage runtime)

Build:  phe component build -arch arm phecompat_import phepostsurf        (x32: without -arch)
Package: recipes/sis.sh (the pdk's package.yml runs it): $PHOREPOC_BUILD/sis/phecompat-<arch>.sis
"""
import os

from paths import OS_ROOT

_SRC = os.path.join(OS_ROOT, "src")

COMPONENTS = {
    "phecompat_import": dict(mmp=os.path.join(_SRC, "phecompat", "phecompatimport.mmp"), rom="phecompatimport.exe", sis=True),
    "phepostsurf": dict(mmp=os.path.join(_SRC, "phepostsurf", "phepostsurf.mmp"), rom="PostingSurfaceFactory.dll", sis=True),
}
