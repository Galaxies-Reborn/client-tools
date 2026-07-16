# Entertainer Audio Test

`EntertainerAudioTest.exe` exercises the same JUCE/WASAPI backend, procedural
instrument patches, MIDI input callback, and Standard MIDI File parser used by
the x64 gameplay client. It does not load the game, connect to a server, open a
network socket, or require client assets.

The 14 patches use distinct synthesis models for strings, reeds, brass, flute,
horn, plucked strings, percussion, metallic boxes, organ, bells, and flanged
voices. They are not General MIDI samples and are intended as a tunable first
pass at recognizable Star Wars Galaxies instrument families.

Build and launch it from PowerShell:

```powershell
.\scripts\Run-EntertainerAudioTest.ps1
```

Pass a MIDI file to start playback immediately:

```powershell
.\scripts\Run-EntertainerAudioTest.ps1 -MidiFile C:\music\sequence.mid
```

The harness opens the default Windows audio output and the first available MIDI
input. Press `F2` to rescan and cycle through connected MIDI inputs.

## Controls

- Notes: `Z S X D C V G B H N J M , L . ; / Q 2 W 3 E R 5`
- `[` / `]`: shift the octave
- `Page Up` / `Page Down`: select one of the 14 entertainer patches
- `F2`: rescan and select the next MIDI controller
- `F4`: choose and play a `.mid` or `.midi` file
- `F8`: pause or resume MIDI-file playback
- `F9`: stop MIDI-file playback
- `Space`: all notes off
- `Escape`: exit

Keyboard notes respond only while the harness console is focused. MIDI input
continues while the harness is running. MIDI files use the same format, size,
duration, event-count, and event-density validation as the gameplay client.

This harness validates local synthesis and input behavior only. Flourish command
authorization, animation, observer relay, and multiplayer synchronization still
require a server-backed gameplay test.
