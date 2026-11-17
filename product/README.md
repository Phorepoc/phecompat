# Per-app product version

`<sid>.txt` answers `SysVersionInfo::GetVersionInfo(TProductVersion)` for that one app (core's sysversioninfo reads
`\resource\phecompat\product\<sid>.txt` on any drive before `Z:\resource\versions\product.txt`; every other
process sees the image's answer). Same `Tag=value` lines as product.txt; `recipes/sis.sh` stores them as UTF-16.

- `2002a096.txt` -- The Big Roll in Paradise. Its N-Gage runtime (ngi3runtime 0.9) looks the `Product` value up in the
  device list it installs (`ngi3devcap_0_9.xml`) and quits at start when nothing matches. `S60_Emulator` is the
  runtime's own entry for the SDK emulator (Config2: the posting-surface back buffer, which this package provides).
