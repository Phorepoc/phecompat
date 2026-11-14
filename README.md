# phecompat

Per-app compatibility profiles for [Phorepoc OS](https://github.com/Phorepoc): lets S60 5th edition apps that were made for a
landscape-native panel and a resistive touch screen run as they were made, without changing the apps.

How it works: core's window server answers each app's screen-mode queries from a small profile, kept in a Central Repository
file (`0xE7016E00`, one key per app: its secure id). A profile can shift the rotation the app is told for each mode
(so an app that takes "the first mode whose rotation is 0" as landscape picks our landscape mode) and make a press last
a minimum time. Apps without a profile are not touched.

This repository installs the profiles:

- `profiles/*.ini` one file per app (`sid`, `rotation`, `touchhold_ms`); a profile is added by adding a file.
- `os/src/phecompat/` `phecompatimport.exe`, run when the package has been installed: it writes the profiles into the repository.
- `os/src/phepostsurf/` `PostingSurfaceFactory.dll`, the display posting library the N-Gage runtime needs.
- `recipes/sis.sh` the package (`phecompat-arm.sis`, `phecompat-x32.sis`), built by the pdk's `package.yml` (`release.yml`).
- `catalogue/` the text Catalogue shows.

UIDs: `0xE7016E01` the package, `0xE7016E02` the importer, `0xE7016E03` the posting library.

Licence: Apache-2.0 (LICENSE, LICENSES/Apache-2.0.txt).
