# Entertainer Offline In-Game Test

The offline launcher starts the regular x64 gameplay client in its dormant
single-player ground-scene mode. It does not require or connect to a login,
cluster, or game server.

```powershell
.\scripts\Start-EntertainerOfflineClient.ps1
```

The default sandbox is `E:\SWG\SWGSource\Staging\EntertainerOfflineClient`.
Large `.tre` and `.toc` files are same-volume hard links and loose asset
directories are junctions, so the sandbox does not duplicate the client data.
Its configuration, executable, logs, profiles, screenshots, and MIDI bank are
independent of the normal gameplay client.

Once the world loads, enter `/startMidiToMusic` in chat. The 24 configured
performance keys and the selected MIDI controller play the locally configured
instrument. The player starts with a Fizz equipped, using instrument profile 6.
`[` and `]` shift octaves; `Escape` exits performance mode.

The last two toolbar slots contain native Start Midi to Music and Stop Midi to
Music buttons. The same draggable commands are available from the Command
Browser's Other tab. While active, the character uses the held-instrument
performance loop and each note-on triggers an original flourish action and
music-note particle effect.

Select another instrument for a future launch with:

```powershell
.\scripts\Start-EntertainerOfflineClient.ps1 -InstrumentId 11 `
    -InstrumentTemplate "object/tangible/instrument/shared_kloo_horn.iff" -NoBuild
```

The local-only authorization bypass is guarded by `Game::getSinglePlayer()`.
Normal connected clients continue to require server-side instrument, skill,
song, and performance validation.
