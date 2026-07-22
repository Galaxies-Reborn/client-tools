# Publish 14.1 nonstandard profession graphs

The Skills mediator now recognizes the complete 54-root Publish 14.1
profession matrix while keeping **All Professions** restricted to the 33
ordinary searchable professions. Shipwright, the three pilot affiliations,
Force-sensitive disciplines, Force/Jedi disciplines, FRS ranks, Jedi title,
Padawan, and the light/dark Journeyman and Master families appear only in
**My Character** when the server says the character owns them.

The 19-row families render through the authentic four-by-four asset. Jedi
title uses the native one-by-four asset, and the ten FRS rank boxes use the
native pyramid asset. Every visible box is backed by the authoritative skill
name; root and novice ownership therefore refresh through the same production
skill delta used by ordinary professions.

Protocol 154 adds targeted `ShowMyProfessions` and `SelectMyProfession`
validation actions. They activate the retail Skills mediator, change the real
tab/selection model, and never synthesize system-wide input. The existing
`ShowAllProfessions` query still reports exactly 33 rows.

The Release x64 build produced:

- `SwgClient_r.exe`: 32,634,368 bytes
- SHA-256: `FA9F993DBBCED36BA2E0DD59719430DA8E2DB3F304F044DFAB3CB52B62DE2245`

Authenticated station 91001 validation rendered Shipwright, Imperial Navy
Pilot, Combat Prowess, Jedi Padawan, Dark Force Rank, and Jedi Title through
their real client templates. The paired server fixture restored the exact
character state after every capture.
