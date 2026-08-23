# SWG TCG Reborn

This branch restores the final-live Trading Card Game as an embedded SWG feature. The player enters through the existing ground-HUD TCG button, the TCG surface remains inside the SWG mediator, input returns through the existing TCG window/control API, and TCG navigation opens the SWG in-game browser.

There is no supported WPF launcher, standalone-client handoff, operating-system browser, or other user-facing fallback. The x64 game uses contained Win32 compatibility processes only because the final-live SWGTCG and Mozilla binaries are 32-bit. Those processes provide pixels, input, and callbacks to the in-game UI; they do not replace it.

## Source and workspace boundaries

The work began from fresh checkouts. The recorded branch baselines are:

| Repository | Working branch | Upstream base | Recorded base commit |
| --- | --- | --- | --- |
| Galaxies-Reborn/client-tools | x64-dx11-tcg-reborn | origin/x64-dx11-integration | 36044003e655fb20a749dce711be826775c8a13a |
| Galaxies-Reborn/swg-main | x64-dx11-tcg-reborn | origin/x64-dx11-vanilla | 7e41a7514262ca4c6012ae77bad1e5a1f06cceec |

The historical client removal is commit d9f5d36180989ea1c202eb37c827cf3e84374a3c (remove TCG functions in Swgclient). Commit 82c76fe8599b4652cf8de72bd361631e221f83ef later reduced button-bar row constants after the Help/Service and TCG rows were disabled.

Two roots have deliberately different roles:

| Path | Role |
| --- | --- |
| E:\SWG\SWG-TCG-Restore-WhitenGold\_client | Protected final-live compiled reference. Never build, test, or write here. |
| E:\SWG\SWG-TCG-Restore-WhitenGold\_whitengold_client | Writable build artifacts, intermediates, staged runtimes, dev-kit checkout, logs, and test evidence. |

The build and staging guards reject output outside the writable root and reject reparse-point traversal. Staging hashes the protected TCG payload before and after copying it and verifies the writable copy. The current expected final-live SWGTCG.dll SHA-256 is 139399A654D1FA8780A03F31249920F75A31C9C8D1E4199B12EF204F6458ABB9.

## In-game entry point

The final-live assets already contain the authoritative ground-HUD path:

~~~text
buttonTcg
  -> CMD_uiTcg
  -> /ui action tcg
  -> SwgCuiHudAction
  -> SwgCuiTcgManager
  -> SwgCuiTcgWindow / SwgCuiTcgControl
~~~

The restoration re-enables the tcg action identifier, HUD registration, manager, mediator, and button-bar binding. It does not add a second launch in SwgCuiButtonBar::OnButtonPressed: UIButton already dispatches its CmdName, and a second handler would launch twice.

The TCG control remains responsible for painting the current surface and forwarding focus, mouse, wheel, and keyboard input. On x64 it keeps the fixed-layout retail TCG at its native surface size, scales the complete surface into the SWG control, and inverse-maps mouse coordinates. Resizing the retail window itself is not equivalent: a controlled 1010x694 probe proved that the legacy Qt UI reflows and clips fixed panels instead of uniformly scaling them. TCG navigation callbacks are delivered on the game thread to SwgCuiWebBrowserManager, which creates the existing in-game browser page and passes the full URL or POST request to it. Logs redact URL credentials, query strings, fragments, and POST bodies.

## Runtime architecture

### Win32 embedded baseline

The Win32 game loads TradingCardGame\SWGTCG.dll in process through the original libEverQuestTCG adapter. SwgCuiTcgManager supplies the desktop window, callbacks, login fields, and realm, launches the DLL, and activates the in-game TCG mediator. The DLL owns its legacy implementation; SWG owns the visible mediator and input path.

The Win32 release browser initializes the legacy Mozilla runtime from the writable runtime's mozilla directory. A TCG navigate callback therefore remains an in-game browser transition, not a shell launch.

### x64 game to Win32 TCG host

The final-live SWGTCG.dll is an x86 PE and its ABI contains 32-bit handle-sized fields. It must never be loaded into SwgClient_r.exe when the game is x64.

For x64, libEverQuestTCG_x64.cpp preserves the public libEverQuestTCG and Window API used by SwgCuiTcgManager, SwgCuiTcgWindow, and SwgCuiTcgControl. Internally it:

- creates a pointer-free, explicitly packed shared-memory mapping and inherited ready, stop, and callback events;
- creates a mandatory kill-on-close job;
- starts TcgCompatibilityHost.exe suspended, assigns it to the job, and resumes it only after containment succeeds;
- passes credentials only through a filtered private child environment, never on the command line;
- waits for the host to load the writable runtime's final-live SWGTCG.dll and publish a valid surface before declaring readiness;
- copies double-buffered BGRA surfaces into the x64-side Window objects;
- sends bounded input/window commands to the host;
- drains bounded callback records and invokes CUI-facing callbacks only from the game update thread.

The host is a Win32 child beside SwgClient_r.exe. It has no standalone player UI. It exits when the game releases it or its containment job closes. Credential, launch-identity, malformed-ring, nonterminated-string, capacity, and process-lifetime failures are fail-closed.

The bridge retains its window-resize command for API compatibility, but SwgCuiTcgControl does not send that command from an x64 game. The native 1772x1293 retail surface is resampled into the current in-game control with a cached nearest-neighbor map, and all mouse move, button, double-click, and wheel coordinates are mapped back to native surface space. Secondary TCG windows use the same native-to-control geometry transform, while their input is mapped back to each native child window. The original Win32 in-process resize behavior is unchanged.

### x64 game to Win32 Mozilla broker

The legacy Mozilla/XUL runtime is also Win32-only. The x64 libMozilla build therefore uses libMozillaProxy.cpp, while BrowserCompatibilityHost.exe loads the original implementation from:

~~~text
runtime\mozilla-broker
~~~

The broker is windowless. It publishes browser pixels through a separate packed shared-memory protocol and receives the same navigation, resize, mouse, keyboard, and history commands exposed by the existing libMozilla Window API. Browser pixels still render through the SWG in-game browser page. The game-side blitter validates both layouts and safely resamples a stale-size broker frame if an asynchronous resize crosses a render boundary.

BrowserCompatibilityHost is also launched suspended into a mandatory kill-on-close job. Only explicitly inherited handles are available to it, and its private environment strips every `SWGTCG_TEST_*` variable before process creation. The host independently rejects any such variable if the launcher filter ever regresses. The staging script copies and hashes the complete legacy Mozilla runtime plus msvcr71.dll into its private directory and rejects non-x86 or unallowlisted PE files there.

The complete x64 path is:

~~~text
x64 SwgClient
  -> in-game TCG mediator
  -> x64 libEverQuestTCG proxy
  -> contained Win32 TcgCompatibilityHost
  -> final-live Win32 SWGTCG.dll
  -> navigate callback returned to x64 game thread
  -> in-game browser page
  -> x64 libMozilla proxy
  -> contained Win32 BrowserCompatibilityHost
  -> legacy Mozilla/XUL
  -> browser surface returned to the in-game page
~~~

## Build and stage from fresh artifacts

Run commands from the fresh client-tools checkout:

~~~powershell
Set-Location 'E:\SWG\SWG-TCG-Restore-WhitenGold\client-tools'
.\scripts\Test-ClientBuildPrerequisites.ps1
~~~

Release/JUCE/DX11 Win32:

~~~powershell
.\scripts\Build-Client.ps1 -Architecture x86 -Renderer DX11 -Configuration Release -AudioBackend Juce
.\scripts\Stage-TcgClient.ps1 -Architecture x86 -Configuration Release -AudioBackend Juce -LocalDevKit
~~~

The default staged path is:

~~~text
E:\SWG\SWG-TCG-Restore-WhitenGold\_whitengold_client\runtime\win32-dx11-release-juce
~~~

Release/JUCE/DX11 x64:

~~~powershell
.\scripts\Build-Client.ps1 -Architecture x64 -Renderer DX11 -Configuration Release -AudioBackend Juce
.\scripts\Stage-TcgClient.ps1 -Architecture x64 -Configuration Release -AudioBackend Juce -LocalDevKit
~~~

The x64 build command also builds Win32 TcgCompatibilityHost.exe and BrowserCompatibilityHost.exe into the external artifact tree. The x64 stage command creates:

~~~text
E:\SWG\SWG-TCG-Restore-WhitenGold\_whitengold_client\runtime\x64-dx11-release-juce
~~~

It does not copy the final-live root wholesale. It copies an explicit non-PE asset allowlist, the exact protected TradingCardGame tree for the TCG host, the validated x64 game/renderer/dependencies, the exact Win32 TCG host, and the exact Win32 Mozilla broker payload. A final recursive PE audit rejects every image not covered by those architecture and hash rules. `-LocalDevKit` then adds a new writable-only `TradingCardGame\host.svr`, points the staged `login.cfg` at the loopback SWG login service, and records both hashes in `local-devkit-runtime.json`; it never rewrites the protected reference.

Each stage target must be new or empty. If a prior runtime exists, archive or remove that writable runtime deliberately before staging again. Never point either command at _client.

The build wrapper deletes expected artifacts before invoking MSBuild, routes object/library/PDB/output files below _whitengold_client\build\tcg-reborn, rejects unresolved externals even when legacy /FORCE emits a binary, and verifies the expected PE machine for each result.

## Local TCG dev kit

The fresh dev-kit execution checkout belongs at:

~~~text
E:\SWG\SWG-TCG-Restore-WhitenGold\_whitengold_client\tcg-devkit\source
~~~

Prepare its client assets and start loopback services:

~~~powershell
Set-Location 'E:\SWG\SWG-TCG-Restore-WhitenGold\_whitengold_client\tcg-devkit\source'
.\scripts\start-dev.ps1 -PrepareClient -OpenRegistration -NoLaunch
~~~

The embedded SWG tests use the server side of this kit; they do not launch its standalone client or WPF launcher.

| Service | Endpoint |
| --- | --- |
| Auth and disposable test registration | http://127.0.0.1:16780 |
| TCG gateway | 127.0.0.1:16782 |
| TCG lobby | 127.0.0.1:16783 |
| Development admin | http://127.0.0.1:8088 |

Check auth health:

~~~powershell
Invoke-RestMethod 'http://127.0.0.1:16780/ping'
~~~

Stop the local services with the dev kit's maintained script:

~~~powershell
Set-Location 'E:\SWG\SWG-TCG-Restore-WhitenGold\_whitengold_client\tcg-devkit\source'
.\scripts\stop-local.ps1
~~~

Open registration and loopback plaintext services are development-only. They are not a production authentication design.

## Embedded test modes

Test-TcgEmbeddedInClient.ps1 is the supported harness. It rejects conflicting processes from the exact staged runtime, creates a disposable dev-kit account, supplies bounded one-shot credentials without logging them, verifies process ownership and module architecture, checks the local lobby/Home transition, writes redacted JSON below the writable runtime, and proves the protected TCG reference is unchanged. Unrelated SWG processes outside the requested fresh workspace are neither used nor stopped.

The harness rejects TCGRebornLauncher.exe or SWGTCGGame.exe when either is newly created beneath the tested SwgClient process tree. Unrelated processes that predate the test or live outside that tree are baselined and ignored.

### SplashDirect

SplashDirect is the default diagnostic trigger. It runs before an SWG scene or game workspace exists and calls the embedded adapter directly. It validates the DLL/compatibility-host surface and local dev-kit connection without requiring an SWG login or claiming the live HUD path.

~~~powershell
.\scripts\Test-TcgEmbeddedInClient.ps1 -Architecture Win32 -Trigger SplashDirect
.\scripts\Test-TcgEmbeddedInClient.ps1 -Architecture x64 -Trigger SplashDirect
~~~

Omitting -Trigger is equivalent to SplashDirect.

### GameAction

GameAction waits for a real game scene, game workspace, current ground button bar, registered HUD TCG action, and the main input-map queue to be the queue currently scanned by the game. The queue check prevents an automated press during a loading/free/debug-camera queue transition. It requires a drawable, enabled `buttonTcg` with its original `CMD_uiTcg` binding, invokes the real `UIButton::Press()` path, proves the ordered input-map, game-message-queue, UI-parser, and actual SwgCuiHudAction route, launches through SwgCuiTcgManager, activates the mediator, and verifies a rendered surface and local dev-kit Home state.

By default this lane is interactive: log in to SWG and enter a character within the selected timeout. For an isolated local SWG server, -AutoLoginLocalServer performs the same proof after entering a local avatar. That switch requires an explicitly supplied positive -LocalStationId; the harness has no shared default account and records the selected ID in its evidence. Use an unused ID when validating automatic account/avatar bootstrap, or a previously prepared ID for repeat TCG-only acceptance runs. The integration-only TCG credentials are consumed from the process environment during manager installation and kept in fixed zeroable storage while waiting. A bounded guarded state spans the real `UIButton::Press()` and queued InputMap command until SwgCuiHudAction consumes it; the manager's pending copy is wiped when that handler returns or the deadline expires. The adapter and compatibility host retain only the identity/session state required by the active legacy DLL, in zeroable storage, until TCG release or process exit. With the test section disabled, normal production action behavior is unchanged.

The local SWG auto-login password follows a separate test-only path: it is accepted only when the integration test is enabled and the configured login endpoint is loopback port 44453, consumed from `SWGTCG_TEST_SWG_LOGIN_PASSWORD`, removed from the environment immediately, and wiped from bounded storage during shutdown. It is never placed on the SwgClient command line or persisted in `warning.log`; the harness asserts both conditions. Compatibility launchers strip it before creating child processes, and both hosts fail closed if they receive it or another integration-test variable.

~~~powershell
.\scripts\Test-TcgEmbeddedInClient.ps1 -Architecture x64 -Trigger GameAction -TimeoutSeconds 180
.\scripts\Test-TcgEmbeddedInClient.ps1 -Architecture x64 -Trigger GameAction -TimeoutSeconds 180 -AutoLoginLocalServer -LocalStationId 700000123
~~~

### GameAction browser probe

The browser probe extends GameAction. After the real TCG surface is ready, it drives the known final-live EULA Accept and Home View More controls, requires the SWGTCG navigate callback correlated to the same nonce, and then requires the x64 in-game browser/broker path. It is valid only with x64 GameAction:

~~~powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Test-TcgEmbeddedInClient.ps1 -Architecture x64 -Trigger GameAction -GameActionBrowserProbe -TimeoutSeconds 180 -AutoLoginLocalServer -LocalStationId 700000123
~~~

Use a fresh local station ID for each clean acceptance run. A successful run requires a passed evidence JSON containing the correlated browser_probe object and redacted client markers.

## Evidence currently established

The integrated x64 acceptance run passed on 2026-08-23. Its evidence is:

~~~text
E:\SWG\SWG-TCG-Restore-WhitenGold\_whitengold_client\runtime\x64-dx11-release-juce\test-evidence\embedded_tcg_embedded_20260823211416_c8806dc3.json
~~~

That run proves the following as one correlated chain:

1. A fresh x64 SwgClient entered a real game scene and workspace, pressed the visible and enabled `buttonTcg`/`CMD_uiTcg` control, found the final-live `CMD_uiTcg -> /ui action tcg` mapping, queued it on the active game input map, observed it in the game message queue, dispatched it through the UI parser and actual SwgCuiHudAction handler, activated the TCG mediator, and launched successfully. The evidence preserves this current-process sequence in order and binds it to SHA-256 hashes of both the selected lines and the complete warning log.
2. Its contained Win32 TcgCompatibilityHost child owned the exact final-live SWGTCG.dll, rendered the native 1772x1293 BGRA surface, connected to 127.0.0.1:16783, and reached the dev-kit Home state.
3. Input passed through SwgCuiTcgControl, the retail DLL emitted the authentic View More navigation callback, and the x64 game created the existing in-game browser page.
4. The game-owned Win32 BrowserCompatibilityHost loaded the staged x86 xul.dll, completed exactly one expected loopback GET, remained at one request through a two-second quiescence audit, and returned a nonzero 614x408 browser frame to SWG. No external browser or standalone client was created.
5. Both compatibility children exited with SwgClient, no credentials or raw navigation data were placed on their command lines, and the protected 39-file TradingCardGame reference remained unchanged.

The earlier Win32 embedded baseline and standalone x64 bridge smoke remain useful component regressions. The negative resize diagnostic is archived below _whitengold_client\diagnostics\tcg-resize-negative-20260823T185210Z; its callback_count=0 and captured malformed 1010x694 surface justify retaining the native retail TCG layout in the x64 integration.

## Server boundary and remaining work

The swg-main x64-dx11-tcg-reborn branch currently remains at its requested x64-dx11-vanilla base. The local dev kit supplies the TCG auth/gateway/lobby services used for client development; it is not evidence that SWG game-server entitlement, redemption, inventory, or account-linking behavior is complete.

Normal in-game launch uses the SWG account/session contract expected by the final-live DLL. The integration harness substitutes a disposable dev-kit credential envelope only inside its gated one-shot test. A production deployment still needs an explicit, reviewed authentication and account/character binding contract between SWG and the TCG service.

Future server work should begin with an evidence-based gap analysis of entitlements, loot-card redemption, claim delivery, persistence, logout/account switching, token audience, replay prevention, and least-privilege service boundaries. Do not change packet formats or redemption semantics merely to make the client launch.
