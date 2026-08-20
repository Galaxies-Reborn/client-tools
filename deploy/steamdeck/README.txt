GALAXIES REBORN - STEAM DECK RUNTIME
=====================================

This folder is a ready-to-run x64 Direct3D 11 client payload. It is configured
for the Steam Deck's 1280x800 display, SDL gamepad input, JUCE audio, and a
60 FPS limit. No account name, password, avatar name, or autologin setting is
included.

INSTALL ON STEAM DECK
---------------------

1. Copy this entire folder to the Steam Deck. A location such as
   /home/deck/Games/Galaxies-Reborn is easy to find later.

2. Enter Desktop Mode and open Steam.

3. Choose Games > Add a Non-Steam Game to My Library. Select Browse, change
   the file type to All Files if necessary, and choose SwgClient_r.exe from
   this folder.

4. Open the new shortcut's Properties. Under Compatibility, enable
   "Force the use of a specific Steam Play compatibility tool" and select a
   current Proton release. Proton Experimental is a useful first choice when
   the current stable Proton release has a game-specific problem.

5. Confirm that the shortcut's Start In directory is this folder. Launch the
   shortcut from Gaming Mode.

The client uses Direct3D 11. Proton normally translates it through DXVK; do not
force the OpenGL WineD3D path unless troubleshooting. The app-local Microsoft
Visual C++ runtime DLLs shipped beside the executable avoid a separate runtime
installer inside the Proton prefix.

CONTROLS
--------

SDL exposes the Steam Deck controls as a mapped gamepad. Use the in-game
Options > Controls screen to bind the sticks, triggers, buttons, and D-pad.
Steam Input can additionally map a trackpad to the mouse and provide keyboard
keys needed by the original PC interface. If a controller is attached after
launch, use the game's Find Controllers action or restart the client.

CONFIGURATION
-------------

options.cfg owns the 1280x800 borderless-windowed Direct3D 11 defaults.
user.cfg owns credential-free user overrides and the 60 FPS cap. You can lower
frameRateLimit to 40.0 for battery life or set it to 0.0 to disable the engine
limit. Do not store a password in either file.

login.cfg is copied from the selected client asset source and therefore keeps
that installation's login-server address. Configure that source file before
staging and enter credentials through the normal login UI. If the packaged
server address is wrong, correct the asset-source file and generate a new clean
package. Manually editing the staged copy invalidates its file hash and the
aggregate payload hash in steamdeck-runtime-manifest.json.

VERIFYING THE PAYLOAD
---------------------

steamdeck-runtime-manifest.json records the source commit, asset source,
runtime profile, file counts, and a SHA-256 hash for every payload file. It
also contains payloadSha256, a deterministic digest of the sorted file-hash
list.

The licenses directory contains the notices for the third-party runtime and
statically linked JUCE audio code. Redistribution requires compliance with
those terms, including selection of either JUCE's AGPL or commercial path and
the GNU libiconv LGPL source/relinking obligations.

If the client exits immediately, first confirm that the whole folder was
copied, Compatibility is forced to Proton, and Steam's Start In directory is
the folder containing SwgClient_r.exe. Proton logs can be enabled temporarily
with this launch option:

    PROTON_LOG=1 %command%

Remove that option after troubleshooting.
