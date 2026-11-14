#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
#
# sis.sh -- the pdk's package.yml runs it after the components: the phecompat SIS for PHOREPOC_ARCH
# ($PHOREPOC_BUILD/sis/phecompat-<arch>.sis). It carries the profile importer (run once at install), the shipped
# profiles (profiles/*.ini -> \private\e7016e02\profiles\) and the posting-surface library, and nothing the image owns.
#
# Needs the components built first:  phe component build [-arch arm] phecompat_import phepostsurf
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd)
ARCH=${PHOREPOC_ARCH:-arm}
case "$ARCH" in arm) AFL=--arm;; x32) AFL=;; *) echo "ARCH must be arm or x32 (got '$ARCH')"; exit 1;; esac
: "${PHOREPOC_PDK:?set PHOREPOC_PDK}" "${PHOREPOC_BUILD:?set PHOREPOC_BUILD}"
LIC=
for f in "$HERE/LICENSES/Apache-2.0.txt" "$HERE/NOTICE"; do
  b=$(basename "$f"); case $b in *.txt) ;; *) b=$b.txt;; esac
  LIC="$LIC --file $f=!:/resource/phecompat/$b"
done
# --run-on-install: the importer runs when the install finishes (makesis FR,RI), so the profiles apply without a restart
# shellcheck disable=SC2086
python3 "$PHOREPOC_PDK/tools/make_sis.py" phecompat_import phepostsurf $AFL --unsigned \
  --uid 0xE7016E01 --version "${PHOREPOC_PACKAGE_VERSION:-0.1.0}" --name "phecompat" --vendor Phorepoc \
  --dir "$HERE/profiles=!:/private/e7016e02/profiles" $LIC \
  --run-on-install '!:\sys\bin\phecompatimport.exe' \
  --out "$PHOREPOC_BUILD/sis/phecompat-$ARCH.sis"
