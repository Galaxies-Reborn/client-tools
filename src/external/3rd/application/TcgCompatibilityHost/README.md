# TCG compatibility host

`TcgCompatibilityHost.exe` is a deliberately isolated Win32 process for the final 32-bit
`SWGTCG.dll`. It loads the original DLL and calls its exact embedded-client ABI. With no
shared mapping in its inherited environment it retains the standalone diagnostic mode:
it pumps frames, inventories HWND IDs, and reports surface metadata without displaying a
standalone TCG UI. With a shared mapping it is the Win32 half of the x64 in-game bridge.

The host refuses any path containing an exact `_client` component and verifies that the
supplied directory is writable. Copy the final `TradingCardGame` payload into a runtime
under `_whitengold_client` before running it.

## Build

From a Visual Studio Developer PowerShell:

```powershell
msbuild .\TcgCompatibilityHost.vcxproj /m /p:Configuration=Release /p:Platform=Win32
```

Only `Win32` configurations exist, and the source has a compile-time x86 assertion. The
Release executable uses the static MSVC runtime. This repository's shared build routing
currently places it under
`_whitengold_client\build\tcg-reborn\artifacts\Win32\Release\Juce`; use the final MSBuild
output line as the authoritative location if that shared routing changes.

## Launch contract

```text
TcgCompatibilityHost.exe --tcg-dir <absolute-writable-TradingCardGame-path>
  [--parent-pid <pid>] [--timeout-ms <100..86400000>]
```

Credential-bearing values are never accepted on the command line and are never logged.
The parent may set these only in the child's inherited environment:

- `SWG_TCG_USERNAME`
- `SWG_TCG_SESSION_ID`
- `SWG_TCG_CHALLENGE`
- `SWG_TCG_CHARACTER`

Optional inherited settings are `SWG_TCG_CHALLENGE_FOUNDER`,
`SWG_TCG_START_TUTORIAL`, `SWG_TCG_REALM=stage`, and `SWG_TCG_LANGUAGE=fr|de`.
The child consumes and clears its private inherited copies before `Initialize` returns.

For cooperative shutdown, the parent can also inherit an event handle into the child and
put its numeric value in `SWG_TCG_STOP_HANDLE`. The host exits when that event is signaled,
the process named by `--parent-pid` exits, a console/process control signal arrives, or the
mandatory timeout expires in standalone mode. Bridge mode remains alive until a shutdown
command or a process/event stop condition; `--timeout-ms` is retained only for standalone
diagnostics.

## x64 bridge contract

Bridge mode is selected only when `SWG_TCG_MAPPING_HANDLE` is present. The parent must also
inherit `SWG_TCG_READY_HANDLE` and `SWG_TCG_STOP_HANDLE`, set the shared state to
`LifecycleStarting`, and provide a nonzero `parentProcessId` that matches `--parent-pid`
when that option is supplied. The host
maps exactly `TcgCompatibilityProtocol::SharedStateBytes`, validates the magic, version,
size, ring state, active-frame index, and live parent process, then publishes its PID.

After `Initialize`, the host keeps pumping real frames in `LifecycleStarting` until at least
one published window has a valid, nonzero surface. Only then does it change the lifecycle to
`LifecycleReady` and signal the ready event. Parent exit, stop requests, malformed transport,
or the configured timeout before that surface exists publish a bounded failure message with
`LifecycleFailed` and signal the same event. Normal shutdown publishes `LifecycleStopped`.

The command ring uses these `values[]` layouts:

- window state: `state`
- focus: `focused`
- location: `x, y`
- size: `width, height`
- mouse: `eventType, x, y, globalX, globalY, button, mouseState, keyboardState`
- wheel: `x, y, globalX, globalY, delta, mouseState, keyboardState`
- key: `eventType, key, keyboardState, nativeCode, virtualKey, nativeModifiers`

Close, music-completion, and shutdown carry no values. A command for a window that
disappeared after publication is harmlessly ignored; malformed types and values remain
fatal. Up to eight windows are published to the inactive frame with bounded metadata and
full packed BGRA surface bytes, followed by an atomic active-frame flip. `frameSequence` is
a seqlock: odd while the inactive frame is being written and even once the flip is complete.

Navigation, navigation-with-POST, sound/music volume, stop-all-sounds, and window-state
requests are placed in the callback ring. URL and POST data are explicitly length-bounded;
POST bodies are never written to logs. Sound buffers remain owned by the legacy process and
are not transported by this protocol.

Logging is limited to lifecycle state, TCG window/surface metadata, and navigation URLs.
URL user-info, query strings, and fragments are redacted, and POST bodies are never read or
logged in standalone mode. Bridge mode does not log navigation values at all.
