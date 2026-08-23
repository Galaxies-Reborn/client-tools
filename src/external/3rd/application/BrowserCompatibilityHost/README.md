# Browser compatibility host

`BrowserCompatibilityHost.exe` is a Win32-only, windowless broker for the final
legacy Mozilla/XUL runtime. The Release x64 game links `libMozillaProxy.cpp` and
communicates with this process through unnamed, explicitly allowlisted inherited
handles. Browser pixels return to the in-game `IBlitter`; this component never
opens an OS browser or parents a window across process boundaries.

The complete broker and legacy Mozilla runtime must be copied to the writable,
isolated directory relative to the directory containing `SwgClient_r.exe`:

`runtime\mozilla-broker`

That directory must contain `BrowserCompatibilityHost.exe`, the contents of
`libMozilla/include/private/bin/release`, and `deps/win32/bin/msvcr71.dll` (a
direct dependency of the legacy XUL/NSPR DLLs). Keep the runtime resource
directories (`chrome`, `components`, `greprefs`, and `res`) intact. The broker deliberately
refuses `_client` reference paths, a differently named directory, or a runtime
path that is not its own executable directory.

The host accepts only `http`, `https`, `about:blank`, and legacy schemeless host
navigation. Rendering is normalized to BGRA8 and published through a
double-buffered, versioned, pointer-free shared-memory protocol.
