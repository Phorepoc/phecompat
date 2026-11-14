# phecompat

Per-app compatibility profiles for Phorepoc: lets S60 5th edition apps that assumed a landscape-native screen run as they
were made, without changing the apps. A small hook in core (the window server reports a per-app rotation, taken from a
profile) plus what this repository installs: the profiles, a settings page and stand-in libraries.

Status: skeleton. Nothing here builds yet.

- `profiles/` one `.ini` per app, keyed by its secure id.
- `os/src/phecompat/` the settings app and the profile importer.
- `os/src/phepostsurf/` the Display Posting API stand-in (`PostingSurfaceFactory.dll`).
- `package/` the SIS recipes.
- `catalogue/` the Catalogue text (`summary.txt`, `description.md`).

Licence: Apache-2.0 for everything in this repository (see LICENSE).
