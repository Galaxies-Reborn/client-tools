# Client/Tools Repo
This repository contains the source for the SWG Client as well as the tools that support certain aspects of development.

## Build Instructions
The legacy Win32 projects were originally configured for **Visual Studio 2013**. The regular gameplay client and Qt-based God client now have modern `Release|x64` builds. See the [x64 gameplay client guide](docs/x64-gameplay-client.md) and [God client guide](docs/god-client.md) for prerequisites, commands, outputs, and current runtime limitations.

The project has 3 configurations for building the applications:
* **Release** which is the version intended for public dissemination and gameplay. You may recognize this as the `_r` in the client name `SwgClient_r.exe`. 
* **Optimized** which is similar to the release client but has additional options and displays in-game for testing and is ideal for Quality Assurance or Support related activities. For example, this configuration allows for additional options like targeting static world objects, printing object information in the user interface, and releasing the camera from player attachment for custom views.
* **Debug** which is a development client that has extra features for testing and extensive logging and reporting. This build isn't particularly useful for any present application.

At present, only the `Release` version of the projects will build, but we're working on cleaning up the remainder of the configurations. As a temporary solution to accessing the features of the optimized version, you can set the constant in `production.h` to `0` regardless of the configuration. 

To build the client, find the `SwgClient` project in solution explorer and right click then select `Build`. Note that other projects may have similar names (like `ClientGame`) but these are shared across multiple tools. The actual game client you need for playing the game is the `SwgClient` project.

### x64 Quick Start

Check the build environment, or install missing Microsoft prerequisites from an elevated PowerShell window:

```powershell
.\scripts\Test-X64BuildPrerequisites.ps1
.\scripts\Setup-X64BuildPrerequisites.ps1 -Install
```

Installer URLs, pinned DirectX hashes, and the offline-cache workflow are documented in [the x64 prerequisite profile](deps/build-prerequisites/README.md).

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build-X64Client.ps1
```

The script builds the gameplay client and its D3D9 raster DLLs, then verifies that every runtime artifact is x64. Pass `-StagePath` to deploy those artifacts into an existing client data directory with automatic backups.

## Shared Files
Please note that certain projects and files are prepended with `shared` which means they are files that are used in both the game engine ([the `src` repository](https://github.com/swg-source/src)) and the client. There are many enums, for instance, that must match between the client and server or there may be crashes, errors, unintended functionality or some combination thereof. ***If you make changes to any of these shared files, you must make the changes both in the src and in client-tools.***

## Deprecated Components
Some specific features have been removed or disabled from the client as they are either no longer needed or outside the scope of the development work of SWG Source. Those removals include:
* The In-Game Web Browser (which uses libmozilla) and any UI elements or commands to activate it
* The Trading Card Game and any UI elements or commands to activate it
* The Customer Service "Help" Context Menu and the Bug Reporting Form, and any UI elements or commands to activate it
* Any references to Perforce, a version control solution that is no longer used.

## Documentation
We're currently working to compile more guides and developer documentation, but what is available can be found on our [SWG Source Wiki](https://github.com/swg-source/swg-main/wiki).

## History
This repository and code has undergone extensive renovations and refactoring since its release in 2013 and some history isn't included in GitHub. If you're looking for how these files were originally received from SOE without modification, see the [whitengold repository](https://github.com/swg-source/whitengold).

## Branches
* **master** - The primary development and release branch.
* **stdlib** - Current "work in progress" for building on Visual Studio 2015, which includes it's own, complete STL implementation.
* **wolfssl** - A test branch where Darth was trying to implement DTLS SSL to make the connection secure. Feel free to finish this implementation.

## Contributing and More Information
Contributions and improvements are welcome and encouraged, please submit a pull request. If you have any questions or are looking for more information and haven't already joined us in Discord, you can join [here](https://discord.gg/Va8e6n8). Please note that any changes to the client-tools that requires a rebuild of the SwgClient, will also mean a newly compiled client binary must be added to the [client-assets repository](https://github.com/swg-source/client-assets) so it can be shipped to end users.

The Publish 14.1 Skills mediator supports the complete 54-root profession
matrix without exposing the 21 specialized roots in the ordinary 33-row
catalog. See [the nonstandard profession graph guide](docs/precu-nonstandard-profession-graphs.md).

The background-input bridge also exposes the milestone-243
`QueueEmboldenPets` action. It admits the real nonqueued `emboldenpets`
command with no explicit target so the server-owned active-pet transaction can
be proved through an authenticated client session.

Milestone 244 adds `QueueHealMind` (bridge command 231). It submits the real
nonqueued `healMind` command with an explicit target OID; the server retains
all Combat Medic ownership, PvP, range, line-of-sight, power, wound, and
battle-fatigue authority.

Milestone 245 advances the bridge to protocol 156 and adds `QueueBerserk1`
(command 232). It submits the authentic nonqueued `berserk1` command without a
target; the server retains Brawler ownership, weapon, chance, adjusted HAM,
state-duration, persistence, and expiration authority.

Milestone 246 advances the bridge to protocol 157 and adds `QueueBerserk2`
(command 233). It submits the authentic Brawler-master tier without a target;
the server retains the +20 modifier, chance, adjusted HAM, durable 40-second
state, and expiration authority.

Milestone 247 advances the bridge to protocol 159 and adds the isolated
`TargetSquadCounterpart` identity mapping plus `QueueFormup` (commands 234 and
235). Two separately rooted x64 clients prove real invite/join/disband and
submit the retained nonqueued `formup`; group, cost, member eligibility,
state clearing, and PvP-help policy remain server-authoritative.

Milestone 248 advances the bridge to protocol 160 and adds `QueueRetreat`
(command 236). It submits the authentic nonqueued `retreat` command without a
target; the server retains Squad Leader Support III ownership, group and member
eligibility, adjusted Action/Mind cost, 1.822 speed/acceleration application,
30-second cooldown and expiry, and PvP-help authority.

Milestone 249 advances the bridge to protocol 161 and adds
`QueueBoostMorale` (command 237). It submits the authentic nonqueued
`boostmorale` command without a target; the server retains Squad Leader
Defense IV ownership, group eligibility, adjusted three-pool HAM cost,
nine-attribute wound conservation, redistribution, and PvP-help authority.

Milestone 250 advances the bridge to protocol 162 and adds `QueueSteadyAim`
(command 238). It submits the authentic nonqueued Squad Leader command while
the server owns group authority, adjusted HAM, ranged filtering, and the
five-minute `private_aim` modifier.

Milestone 251 advances the bridge to protocol 164 and adds targeted
`QueueApplyPoison` and `QueueApplyDisease` actions (commands 239 and 240).
They submit the authentic nonqueued Combat Medic commands with an explicit
target OID; the server remains authoritative for ownership, organic/PvP/LOS
admission, medicine selection, range, Mind cost, recovery, DOT resistance,
area targeting, combat effects, XP, and charge consumption.

Milestone 252 advances the bridge to protocol 165 and adds `QueueAreaTrack`
and `SelectAreaTrackType` (commands 241 and 242). The queue action submits the
authentic nonqueued Ranger command. The selector accepts only the exact
`skl_use:scan_type_d` production page, chooses one real row, and dispatches
the mediator's ordinary selection and OK callbacks; the server remains
authoritative for skill tiers, cooldown, movement/combat aborts, scan range,
candidate filters, direction, distance, and result construction.

Milestone 255 advances the bridge to protocol 176 and adds
`QueueActionShot1` and `ActionShot1WeaponStatus` (commands 256 and 257). The
status route proves a concrete CDEF carbine against mask `0x00000002`; the
queue route submits the production `actionShot1` command while the server
remains authoritative for adjusted HAM, Action-only damage and bleeding,
posture defense, posture transition, and recovery.

Milestone 256 advances the bridge to protocol 177 and adds
`QueueActionShot2` and `ActionShot2WeaponStatus` (commands 258 and 259). The
same identity-bound CDEF carbine proves mask `0x00000002`; the server remains
authoritative for the 15-degree cone, adjusted HAM, Action-only damage and
bleeding, `sapblast` prose, posture transition, and recovery.

Milestone 257 advances the bridge to protocol 178 and closes the two remaining
Marksman-novice attacks with `QueueOverChargeShot1`,
`OverChargeShot1WeaponStatus`, `QueuePointBlankSingle1`, and
`PointBlankSingle1WeaponStatus` (commands 260 through 263). The reversible
CDEF rifle proves aggregate-ranged mask `0x08000000`; the server remains
authoritative for exact HAM costs, RANDOM pool selection, the 12-meter
point-blank boundary, direct damage, combat prose, and generated animation.

Milestone 258 advances the bridge to protocol 179 and restores the two
Marksman support attacks with `QueueThreatenShot`, `ThreatenShotWeaponStatus`,
`QueueWarningShot`, and `WarningShotWeaponStatus` (commands 264 through 267).
The reversible CDEF rifle proves aggregate-ranged mask `0x08000000`; the
server remains authoritative for exact HAM costs, RANDOM pool selection,
direct damage, `threatenshot`/`warningshot` prose, and their distinct
generated ranged/intensity animation semantics.

Milestone 259 advances the bridge to protocol 180 and adds `QueueAim` and
`AimWeaponStatus` at commands 268/269. The reversible CDEF rifle proves the
aggregate-ranged mask before the native client queues the production command;
the server remains authoritative for the 12/0/0 adjusted HAM cost, five-second
`STATE_AIMING`/`private_aim` lifecycle, no-refresh repeat rule, expiry, and
consumption by the next successful combat action.

Milestone 260 advances the bridge to protocol 181 and adds
`QueueSuppressionFire1` and `SuppressionFire1WeaponStatus` at commands
270/271. The reversible CDEF rifle proves the aggregate-ranged mask before
the native client queues production `suppressionFire1`; the server remains
authoritative for exact HAM costs, Health damage, combat prose, exact
posture-down playback, state application, defense, and timed recovery.

Milestone 261 advances the bridge to protocol 182 and adds queue and
generated-weapon status routes for `rollShot`, `diveShot`, and `kipUpShot` at
commands 272-277. The reversible fixture exposes the retained Pistol II
grants, and authenticated clients exercise the complete
upright-to-crouched-to-prone-to-upright acrobatic posture lifecycle.

Milestone 262 advances the bridge to protocol 183 and adds `QueueTakeCover`
and `TakeCoverWeaponStatus` at commands 278/279. The identity-bound fixture
exposes the retained Marksman Rifle II ownership while the native client
queues production `takeCover`; the server remains authoritative for the
Quickness-adjusted Action debit, combat chance, dizzy-fall branch, cover
state, persistence evidence, and reversible cleanup.

Milestone 263 advances the bridge to protocol 184 and adds
`QueueFullAutoSingle1` and `FullAutoSingle1WeaponStatus` at commands 280/281.
The identity-bound CDEF carbine path proves the retained Marksman Carbine II
grant while the authenticated client queues production `fullAutoSingle1`;
the server remains authoritative for exact costs, random-pool damage,
full-auto prose and animation, three independent defended timed-state
attempts, persistence evidence, and ownership-safe cleanup.

## Additional Dependencies
Most of the development tools use the [Qt framework](https://www.qt.io/) to render their user interface. You may wish to install the [Qt VS Tools for Visual Studio](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools-19123) to ease development. 

## Known Issues
* For the debug build, and possibly the optimized versions, you will get linker errors about libmozilla, and in release, possibly Vivox - if you alter the project settings you can disable this from killing the output of an exe, as libmozilla is only needed for the ingame browser.
* Other linker errors sometimes throw, you have to work on these case by case. Please pull request any changes you make.
* cmd.exe issues sometimes occur as SOE originally had the build setup copying files to a proper game bin directory. You can just remove these from projects that complain about them, just copy the output files manually.
* Plenty of warnings and sometimes even errors regarding deprecated libs happen. Fixes for these are case by case.
