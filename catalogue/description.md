Some S60 5th edition apps were made for a landscape-native screen and a resistive touch panel. On a portrait-native
phone they open sideways, or ignore a short tap. phecompat keeps a small profile per app (by its secure id): what
rotation the app is told for each screen mode, and how long a press has to last. The window server applies the profile
to that app only; nothing else changes, and the app's own files are never touched.

The package carries the profiles it ships (profiles/*.ini), an importer that loads them when the install finishes, and
`PostingSurfaceFactory.dll`, the display posting library that games from the N-Gage runtime need.

Shipped profiles: The Big Roll in Paradise.

Needs a Phorepoc image with the profile support in its window server.
