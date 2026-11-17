#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: AGPL-3.0-only
#
# sis.sh -- the pdk's package.yml runs it after the components: the phecompat SIS for PHOREPOC_ARCH
# ($PHOREPOC_BUILD/sis/phecompat-<arch>.sis). It carries the profile importer (run once at install), the shipped
# profiles (profiles/*.ini -> \private\e7016e02\profiles\), the per-app product versions
# (product/<sid>.txt -> \resource\phecompat\product\) and the posting-surface library, and nothing the image owns.
#
# Needs the components built first:  phe component build [-arch arm] phecompat_import phepostsurf
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd)
ARCH=${PHOREPOC_ARCH:-arm}
case "$ARCH" in arm) AFL=--arm;; x32) AFL=;; *) echo "ARCH must be arm or x32 (got '$ARCH')"; exit 1;; esac
: "${PHOREPOC_PDK:?set PHOREPOC_PDK}" "${PHOREPOC_BUILD:?set PHOREPOC_BUILD}"
LIC=
for f in "$HERE/LICENSE" "$HERE/NOTICE"; do
  b=$(basename "$f"); case $b in *.txt) ;; *) b=$b.txt;; esac
  LIC="$LIC --file $f=!:/resource/phecompat/$b"
done
# The per-app product versions, stored as UTF-16 with CRLF lines like the image's product.txt (core's sysversioninfo reads them)
PROD=$PHOREPOC_BUILD/sis/phecompat-product
rm -rf "$PROD"; mkdir -p "$PROD"
for f in "$HERE"/product/*.txt; do
  python3 - "$f" "$PROD/$(basename "$f")" <<'PY'
import sys
text = open(sys.argv[1], encoding="utf-8").read().replace("\n", "\r\n")
open(sys.argv[2], "wb").write(text.encode("utf-16"))
PY
done
# --run-on-install: the importer runs when the install finishes (makesis FR,RA), so the profiles apply without a restart
# shellcheck disable=SC2086
python3 "$PHOREPOC_PDK/tools/make_sis.py" phecompat_import phepostsurf $AFL --unsigned \
  --uid 0xE7016E01 --version "${PHOREPOC_PACKAGE_VERSION:-0.1.0}" --name "phecompat" --vendor Phorepoc \
  --dir "$HERE/profiles=!:/private/e7016e02/profiles" --dir "$PROD=!:/resource/phecompat/product" $LIC \
  --run-on-install '!:/sys/bin/phecompatimport.exe' \
  --out "$PHOREPOC_BUILD/sis/phecompat-$ARCH.sis"
