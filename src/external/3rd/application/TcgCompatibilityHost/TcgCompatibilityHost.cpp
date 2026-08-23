#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "TcgCompatibilityProtocol.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#if !defined(_M_IX86)
#error TcgCompatibilityHost must be built for Win32/x86 so it can load the final 32-bit SWGTCG.dll.
#endif

namespace
{
    // These definitions intentionally mirror coreclient/include/coreclient.h and
    // libEverQuestTCG.cpp. The final SWGTCG.dll retains the callback table.
    typedef void (__stdcall *NavigateProc)(const char *);
    typedef void (__stdcall *NavigateWithPostDataProc)(const char *, const char *);
    typedef void (__stdcall *PlaySoundProc)(char *, unsigned, int);
    typedef void (__stdcall *PlayMusicProc)(char *, unsigned, int);
    typedef void (__stdcall *SetSoundVolumeProc)(float);
    typedef void (__stdcall *SetMusicVolumeProc)(float);
    typedef void (__stdcall *StopAllSoundsProc)();
    typedef void (__stdcall *SetWindowStateProc)(int);

    struct CallbackTable
    {
        NavigateProc navigateProc;
        NavigateWithPostDataProc navigateWithPostDataProc;
        PlaySoundProc playSoundProc;
        PlayMusicProc playMusicProc;
        SetSoundVolumeProc setSoundVolumeProc;
        SetMusicVolumeProc setMusicVolumeProc;
        StopAllSoundsProc stopAllSoundsProc;
        SetWindowStateProc setWindowStateProc;
    };

    static_assert(sizeof(void *) == 4, "SWGTCG.dll compatibility ABI requires 32-bit pointers");
    static_assert(sizeof(CallbackTable) == 8 * sizeof(void *), "SWGTCG callback ABI changed");
    static_assert(sizeof(TcgCompatibilityProtocol::WindowRecord) == 308u,
        "TCG bridge window-record layout changed");
    static_assert(sizeof(TcgCompatibilityProtocol::Frame) == 50334128u,
        "TCG bridge frame layout changed");
    static_assert(offsetof(TcgCompatibilityProtocol::SharedState, frames) == 647988u,
        "TCG bridge frame offset changed");
    static_assert(sizeof(TcgCompatibilityProtocol::SharedState) == 101316244u,
        "TCG bridge shared-state layout changed");

    typedef void (__cdecl *InitializeProc)(int, const char *[], HWND, CallbackTable *);
    typedef void (__cdecl *RunFrameProc)();
    typedef void (__cdecl *ShutdownProc)();
    typedef unsigned (__cdecl *GetWindowsProc)(unsigned *, unsigned);
    typedef unsigned (__cdecl *GetCaptureWindowProc)();
    typedef HCURSOR (__cdecl *GetCurrentCursorProc)();
    typedef unsigned (__cdecl *GetWindowSurfaceDataProc)(unsigned, void **, unsigned *, unsigned *, unsigned *);
    typedef unsigned (__cdecl *GetWindowRepaintRectsProc)(unsigned, RECT *, unsigned);
    typedef void (__cdecl *OnWindowStateChangedProc)(int);
    typedef void (__cdecl *OnMusicCompletionProc)();
    typedef void (__cdecl *OnMouseEventProc)(unsigned, int, int, int, int, int, int, int, int);
    typedef void (__cdecl *OnMouseWheelEventProc)(unsigned, int, int, int, int, int, int, int);
    typedef unsigned (__cdecl *OnKeyEventProc)(int, int, int, int, int, int);
    typedef void (__cdecl *OnFocusProc)(unsigned, int);

    struct LegacyApi
    {
        InitializeProc initialize = nullptr;
        RunFrameProc runFrame = nullptr;
        ShutdownProc shutdown = nullptr;
        GetWindowsProc getWindows = nullptr;
        GetCaptureWindowProc getCaptureWindow = nullptr;
        GetCurrentCursorProc getCurrentCursor = nullptr;
        GetWindowRepaintRectsProc getWindowRepaintRects = nullptr;
        GetWindowSurfaceDataProc getWindowSurfaceData = nullptr;
        OnMouseEventProc onMouseEvent = nullptr;
        OnMouseWheelEventProc onMouseWheelEvent = nullptr;
        OnKeyEventProc onKeyEvent = nullptr;
        OnFocusProc onFocus = nullptr;
        OnWindowStateChangedProc onWindowStateChanged = nullptr;
        OnMusicCompletionProc onMusicCompletion = nullptr;
    };

    HANDLE g_shutdownEvent = nullptr;
    ULONGLONG g_startTick = 0;
    SRWLOCK g_logLock = SRWLOCK_INIT;
    SRWLOCK g_callbackQueueLock = SRWLOCK_INIT;
    TcgCompatibilityProtocol::SharedState *g_sharedState = nullptr;
    HANDLE g_bridgeReadyEvent = nullptr;
    HANDLE g_bridgeCallbackEvent = nullptr;
    volatile LONG g_bridgeReadySignaled = 0;
    volatile LONG g_bridgeFatal = 0;

    class ExclusiveSrwLock
    {
    public:
        explicit ExclusiveSrwLock(SRWLOCK &lock)
        : m_lock(lock)
        {
            AcquireSRWLockExclusive(&m_lock);
        }

        ~ExclusiveSrwLock()
        {
            ReleaseSRWLockExclusive(&m_lock);
        }

        ExclusiveSrwLock(const ExclusiveSrwLock &) = delete;
        ExclusiveSrwLock &operator=(const ExclusiveSrwLock &) = delete;

    private:
        SRWLOCK &m_lock;
    };

    void logMessage(const char *category, const char *format, ...)
    {
        AcquireSRWLockExclusive(&g_logLock);
        const ULONGLONG elapsed = GetTickCount64() - g_startTick;
        std::fprintf(stdout, "[%10llu ms] %-10s ", static_cast<unsigned long long>(elapsed), category);

        va_list arguments;
        va_start(arguments, format);
        std::vfprintf(stdout, format, arguments);
        va_end(arguments);

        std::fputc('\n', stdout);
        std::fflush(stdout);
        ReleaseSRWLockExclusive(&g_logLock);
    }

    std::string wideToUtf8(const std::wstring &value)
    {
        if (value.empty())
            return std::string();

        const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0)
            throw std::runtime_error("Could not encode a path as UTF-8");

        std::string result(static_cast<size_t>(required), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            &result[0], required, nullptr, nullptr) != required)
        {
            throw std::runtime_error("Could not encode a path as UTF-8");
        }
        return result;
    }

    std::string wideToAnsi(const std::wstring &value)
    {
        if (value.empty())
            return std::string();

        BOOL usedDefault = FALSE;
        const int required = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value.data(),
            static_cast<int>(value.size()), nullptr, 0, nullptr, &usedDefault);
        if (required <= 0 || usedDefault)
            throw std::runtime_error("TCG directory is not representable in the active Windows code page");

        std::string result(static_cast<size_t>(required), '\0');
        usedDefault = FALSE;
        if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value.data(), static_cast<int>(value.size()),
            &result[0], required, nullptr, &usedDefault) != required || usedDefault)
        {
            throw std::runtime_error("TCG directory is not representable in the active Windows code page");
        }
        return result;
    }

    std::wstring canonicalizeAbsolutePath(const std::wstring &path)
    {
        if (path.empty())
            throw std::runtime_error("--tcg-dir is required");
        const bool driveAbsolute = path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
        const bool uncAbsolute = path.size() >= 2 && (path[0] == L'\\' || path[0] == L'/') &&
            (path[1] == L'\\' || path[1] == L'/');
        if (!driveAbsolute && !uncAbsolute)
            throw std::runtime_error("--tcg-dir must be a fully qualified absolute path");

        DWORD required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
        if (required == 0)
            throw std::runtime_error("Could not canonicalize --tcg-dir");

        std::vector<wchar_t> buffer(static_cast<size_t>(required) + 1u, L'\0');
        DWORD written = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
        if (written == 0 || written >= buffer.size())
            throw std::runtime_error("Could not canonicalize --tcg-dir");

        std::wstring result(buffer.data(), written);
        while (result.size() > 3 && (result.back() == L'\\' || result.back() == L'/'))
            result.pop_back();
        return result;
    }

    bool containsProtectedClientComponent(const std::wstring &path)
    {
        size_t begin = 0;
        while (begin < path.size())
        {
            while (begin < path.size() && (path[begin] == L'\\' || path[begin] == L'/'))
                ++begin;
            size_t end = begin;
            while (end < path.size() && path[end] != L'\\' && path[end] != L'/')
                ++end;
            if (end - begin == 7 && _wcsnicmp(path.c_str() + begin, L"_client", 7) == 0)
                return true;
            begin = end;
        }
        return false;
    }

    void verifyWritableDirectory(const std::wstring &directory)
    {
        const DWORD attributes = GetFileAttributesW(directory.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            throw std::runtime_error("--tcg-dir must name an existing directory");
        if (containsProtectedClientComponent(directory))
            throw std::runtime_error("Refusing to load from a protected _client reference directory; copy it to a writable runtime first");

        wchar_t suffix[96] = {};
        _snwprintf_s(suffix, _countof(suffix), _TRUNCATE, L"\\.tcg-host-write-test-%lu-%llu.tmp",
            GetCurrentProcessId(), static_cast<unsigned long long>(GetTickCount64()));
        const std::wstring testPath = directory + suffix;
        HANDLE testFile = CreateFileW(testPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (testFile == INVALID_HANDLE_VALUE)
            throw std::runtime_error("--tcg-dir is not writable");
        CloseHandle(testFile);
    }

    class SecureEnvironmentValue
    {
    public:
        SecureEnvironmentValue(const char *name, DWORD maximumCharacters)
        {
            const DWORD required = GetEnvironmentVariableA(name, nullptr, 0);
            if (required == 0)
                return;

            // Clear the child's private inherited copy before validating or doing
            // anything that could emit diagnostics.
            if (required > maximumCharacters + 1u)
            {
                SetEnvironmentVariableA(name, nullptr);
                throw std::runtime_error(std::string("Inherited environment value is too long: ") + name);
            }

            m_value.resize(required, '\0');
            const DWORD written = GetEnvironmentVariableA(name, m_value.data(), required);
            SetEnvironmentVariableA(name, nullptr);
            if (written == 0 || written >= required)
            {
                clear();
                throw std::runtime_error(std::string("Could not consume inherited environment value: ") + name);
            }
        }

        ~SecureEnvironmentValue()
        {
            clear();
        }

        SecureEnvironmentValue(const SecureEnvironmentValue &) = delete;
        SecureEnvironmentValue &operator=(const SecureEnvironmentValue &) = delete;

        bool empty() const { return m_value.empty() || m_value[0] == '\0'; }
        const char *c_str() const { return empty() ? "" : m_value.data(); }

    private:
        void clear()
        {
            if (!m_value.empty())
                SecureZeroMemory(m_value.data(), m_value.size());
            m_value.clear();
            m_value.shrink_to_fit();
        }

        std::vector<char> m_value;
    };

    bool clearUnexpectedIntegrationTestEnvironment()
    {
        LPWCH const environment = GetEnvironmentStringsW();
        if (!environment)
            throw std::runtime_error("TCG compatibility host could not inspect its environment");

        const wchar_t prefix[] = L"SWGTCG_TEST_";
        const size_t prefixLength = _countof(prefix) - 1u;
        bool found = false;
        bool clearFailed = false;
        try
        {
            for (const wchar_t *current = environment; *current; current += std::wcslen(current) + 1u)
            {
                const wchar_t *const separator = std::wcschr(current, L'=');
                if (!separator || separator < current + prefixLength ||
                    _wcsnicmp(current, prefix, prefixLength) != 0)
                {
                    continue;
                }

                found = true;
                const std::wstring name(current, separator);
                if (!SetEnvironmentVariableW(name.c_str(), nullptr))
                    clearFailed = true;
            }
        }
        catch (...)
        {
            FreeEnvironmentStringsW(environment);
            throw;
        }
        FreeEnvironmentStringsW(environment);
        if (clearFailed)
            throw std::runtime_error("TCG compatibility host could not clear an unexpected integration-test variable");
        return found;
    }

    class SecureArgument
    {
    public:
        SecureArgument() = default;

        SecureArgument(const char *prefix, const SecureEnvironmentValue &value)
        {
            if (value.empty())
                return;
            const size_t prefixLength = std::strlen(prefix);
            const size_t valueLength = std::strlen(value.c_str());
            m_value.resize(prefixLength + valueLength + 1u, '\0');
            std::memcpy(m_value.data(), prefix, prefixLength);
            std::memcpy(m_value.data() + prefixLength, value.c_str(), valueLength);
        }

        ~SecureArgument()
        {
            if (!m_value.empty())
                SecureZeroMemory(m_value.data(), m_value.size());
        }

        SecureArgument(const SecureArgument &) = delete;
        SecureArgument &operator=(const SecureArgument &) = delete;

        bool empty() const { return m_value.empty(); }
        const char *c_str() const { return m_value.empty() ? "" : m_value.data(); }

    private:
        std::vector<char> m_value;
    };

    bool isTrue(const SecureEnvironmentValue &value)
    {
        return !value.empty() &&
            (_stricmp(value.c_str(), "1") == 0 || _stricmp(value.c_str(), "true") == 0 ||
             _stricmp(value.c_str(), "yes") == 0);
    }

    std::string sanitizeNavigationUrl(const char *rawUrl)
    {
        if (rawUrl == nullptr)
            return "<null>";

        std::string url(rawUrl, strnlen_s(rawUrl, 8192));
        for (char &character : url)
        {
            const unsigned char value = static_cast<unsigned char>(character);
            if (value < 0x20 || value == 0x7f)
                character = '?';
        }

        const size_t scheme = url.find("://");
        if (scheme != std::string::npos)
        {
            const size_t authorityStart = scheme + 3u;
            const size_t authorityEnd = url.find_first_of("/?#", authorityStart);
            const size_t at = url.find('@', authorityStart);
            if (at != std::string::npos && (authorityEnd == std::string::npos || at < authorityEnd))
                url.replace(authorityStart, at - authorityStart, "<redacted>");
        }

        const size_t sensitiveStart = url.find_first_of("?#");
        if (sensitiveStart != std::string::npos)
        {
            const char marker = url[sensitiveStart];
            url.erase(sensitiveStart);
            url += marker == '?' ? "?<redacted>" : "#<redacted>";
        }
        if (url.size() > 2048u)
        {
            url.resize(2048u);
            url += "...";
        }
        return url;
    }

    LONG atomicRead(volatile LONG *value)
    {
        return InterlockedCompareExchange(value, 0, 0);
    }

    void signalBridgeReadyNoThrow()
    {
        if (g_bridgeReadyEvent != nullptr && InterlockedCompareExchange(&g_bridgeReadySignaled, 1, 0) == 0)
            (void)SetEvent(g_bridgeReadyEvent);
    }

    void publishBridgeFailureNoThrow(LONG code, const char *message)
    {
        if (g_sharedState != nullptr && InterlockedCompareExchange(&g_bridgeFatal, 1, 0) == 0)
        {
            SecureZeroMemory(g_sharedState->failureMessage, sizeof(g_sharedState->failureMessage));
            if (message != nullptr)
                (void)strncpy_s(g_sharedState->failureMessage, sizeof(g_sharedState->failureMessage), message, _TRUNCATE);
            InterlockedExchange(&g_sharedState->failureCode, code);
            MemoryBarrier();
            InterlockedExchange(&g_sharedState->lifecycle, TcgCompatibilityProtocol::LifecycleFailed);
        }
        signalBridgeReadyNoThrow();
    }

    bool copyCallbackText(char *destination, std::uint32_t capacity, const char *source,
        std::uint32_t &copiedBytes)
    {
        copiedBytes = 0u;
        if (destination == nullptr || capacity == 0u)
            return false;
        destination[0] = '\0';
        if (source == nullptr)
            return true;

        const size_t measured = strnlen_s(source, capacity);
        if (measured >= capacity)
            return false;
        if (measured != 0u)
            std::memcpy(destination, source, measured);
        destination[measured] = '\0';
        copiedBytes = static_cast<std::uint32_t>(measured);
        return true;
    }

    bool queueBridgeCallback(TcgCompatibilityProtocol::CallbackType type, const char *url,
        const char *postData, std::int32_t value)
    {
        if (g_sharedState == nullptr)
            return false;
        ExclusiveSrwLock callbackLock(g_callbackQueueLock);
        if (atomicRead(&g_bridgeFatal) != 0)
            return true;

        std::unique_ptr<TcgCompatibilityProtocol::Callback> prepared(new TcgCompatibilityProtocol::Callback());
        SecureZeroMemory(prepared.get(), sizeof(*prepared));
        prepared->type = static_cast<std::uint32_t>(type);
        prepared->value = value;
        const bool urlValid = copyCallbackText(prepared->url, TcgCompatibilityProtocol::MaximumUrlBytes,
            url, prepared->urlBytes);
        const bool postValid = copyCallbackText(prepared->postData, TcgCompatibilityProtocol::MaximumPostBytes,
            postData, prepared->postBytes);
        if (!urlValid || !postValid)
        {
            SecureZeroMemory(prepared.get(), sizeof(*prepared));
            publishBridgeFailureNoThrow(1006, !urlValid ?
                "TCG bridge callback URL exceeds its transport capacity" :
                "TCG bridge callback POST data exceeds its transport capacity");
            return true;
        }

        const LONG write = atomicRead(&g_sharedState->callbackWrite);
        LONG read = atomicRead(&g_sharedState->callbackRead);
        std::uint32_t pending = static_cast<std::uint32_t>(write) - static_cast<std::uint32_t>(read);
        if (pending >= TcgCompatibilityProtocol::MaximumCallbacks &&
            atomicRead(&g_sharedState->lifecycle) == TcgCompatibilityProtocol::LifecycleStarting)
        {
            const ULONGLONG deadline = GetTickCount64() + 5000u;
            while (pending >= TcgCompatibilityProtocol::MaximumCallbacks && GetTickCount64() < deadline)
            {
                if (g_bridgeCallbackEvent == nullptr || !SetEvent(g_bridgeCallbackEvent))
                    break;
                Sleep(1);
                read = atomicRead(&g_sharedState->callbackRead);
                pending = static_cast<std::uint32_t>(write) - static_cast<std::uint32_t>(read);
            }
        }
        if (pending >= TcgCompatibilityProtocol::MaximumCallbacks)
        {
            SecureZeroMemory(prepared.get(), sizeof(*prepared));
            publishBridgeFailureNoThrow(1001, "TCG bridge callback queue overflow");
            return true;
        }

        TcgCompatibilityProtocol::Callback &callback =
            g_sharedState->callbacks[static_cast<std::uint32_t>(write) % TcgCompatibilityProtocol::MaximumCallbacks];
        std::memcpy(&callback, prepared.get(), sizeof(callback));
        SecureZeroMemory(prepared.get(), sizeof(*prepared));
        MemoryBarrier();
        InterlockedExchange(&g_sharedState->callbackWrite,
            static_cast<LONG>(static_cast<std::uint32_t>(write) + 1u));
        if (g_bridgeCallbackEvent == nullptr || !SetEvent(g_bridgeCallbackEvent))
            publishBridgeFailureNoThrow(1007, "TCG bridge callback wake event failed");
        return true;
    }

    void __stdcall onNavigate(const char *url)
    {
        if (queueBridgeCallback(TcgCompatibilityProtocol::CallbackNavigate, url, nullptr, 0))
            return;
        const std::string safeUrl = sanitizeNavigationUrl(url);
        logMessage("navigation", "url=%s", safeUrl.c_str());
    }

    void __stdcall onNavigateWithPostData(const char *url, const char *postData)
    {
        if (queueBridgeCallback(TcgCompatibilityProtocol::CallbackNavigateWithPost, url, postData, 0))
            return;
        const std::string safeUrl = sanitizeNavigationUrl(url);
        logMessage("navigation", "url=%s post=<redacted>", safeUrl.c_str());
    }

    void __stdcall onPlaySound(char *, unsigned, int) {}
    void __stdcall onPlayMusic(char *, unsigned, int) {}
    void __stdcall onSetSoundVolume(float volume)
    {
        std::int32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(volume), "Volume transport changed");
        std::memcpy(&bits, &volume, sizeof(bits));
        (void)queueBridgeCallback(TcgCompatibilityProtocol::CallbackSetSoundVolume, nullptr, nullptr, bits);
    }
    void __stdcall onSetMusicVolume(float volume)
    {
        std::int32_t bits = 0;
        std::memcpy(&bits, &volume, sizeof(bits));
        (void)queueBridgeCallback(TcgCompatibilityProtocol::CallbackSetMusicVolume, nullptr, nullptr, bits);
    }
    void __stdcall onStopAllSounds()
    {
        (void)queueBridgeCallback(TcgCompatibilityProtocol::CallbackStopAllSounds, nullptr, nullptr, 0);
    }

    void __stdcall onSetWindowState(int state)
    {
        if (queueBridgeCallback(TcgCompatibilityProtocol::CallbackSetWindowState, nullptr, nullptr, state))
            return;
        logMessage("window", "requested-host-state=%d", state);
    }

    CallbackTable g_callbackTable =
    {
        &onNavigate,
        &onNavigateWithPostData,
        &onPlaySound,
        &onPlayMusic,
        &onSetSoundVolume,
        &onSetMusicVolume,
        &onStopAllSounds,
        &onSetWindowState
    };

    BOOL WINAPI consoleControlHandler(DWORD controlType)
    {
        switch (controlType)
        {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                if (g_shutdownEvent != nullptr)
                    SetEvent(g_shutdownEvent);
                return TRUE;
            default:
                return FALSE;
        }
    }

    LRESULT CALLBACK hiddenWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_CLOSE)
        {
            if (g_shutdownEvent != nullptr)
                SetEvent(g_shutdownEvent);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    HWND createHiddenDesktopWindow()
    {
        const wchar_t *className = L"SWG.TcgCompatibilityHost.HiddenDesktop";
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &hiddenWindowProcedure;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);

        if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Could not register the hidden desktop window");

        HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, className,
            L"SWG TCG Compatibility Host", WS_POPUP, 0, 0, 1280, 720,
            nullptr, nullptr, windowClass.hInstance, nullptr);
        if (window == nullptr)
            throw std::runtime_error("Could not create the hidden desktop window");
        return window;
    }

    template <typename Procedure>
    Procedure resolveRequiredExport(HMODULE module, const char *name)
    {
        FARPROC address = GetProcAddress(module, name);
        if (address == nullptr)
            throw std::runtime_error(std::string("SWGTCG.dll is missing required export: ") + name);

        Procedure result = nullptr;
        static_assert(sizeof(result) == sizeof(address), "Function pointer representation is unexpected");
        std::memcpy(&result, &address, sizeof(result));
        return result;
    }

    struct CommandLineOptions
    {
        std::wstring tcgDirectory;
        DWORD parentProcessId = 0;
        ULONGLONG timeoutMilliseconds = 30000;
    };

    unsigned long parseUnsigned(const wchar_t *text, const wchar_t *name)
    {
        if (text == nullptr || *text == L'\0' || *text == L'-')
            throw std::runtime_error(std::string("Invalid numeric option: ") + wideToUtf8(name));
        wchar_t *end = nullptr;
        errno = 0;
        const unsigned long value = std::wcstoul(text, &end, 10);
        if (errno == ERANGE || end == text || *end != L'\0')
            throw std::runtime_error(std::string("Invalid numeric option: ") + wideToUtf8(name));
        return value;
    }

    CommandLineOptions parseCommandLine(int argumentCount, wchar_t *arguments[])
    {
        CommandLineOptions options;
        for (int index = 1; index < argumentCount; ++index)
        {
            const std::wstring argument(arguments[index]);
            auto requireValue = [&](const wchar_t *option) -> const wchar_t *
            {
                if (++index >= argumentCount)
                    throw std::runtime_error(std::string("Missing value for ") + wideToUtf8(option));
                return arguments[index];
            };

            if (argument == L"--tcg-dir")
                options.tcgDirectory = requireValue(L"--tcg-dir");
            else if (argument == L"--parent-pid")
                options.parentProcessId = parseUnsigned(requireValue(L"--parent-pid"), L"--parent-pid");
            else if (argument == L"--timeout-ms")
                options.timeoutMilliseconds = parseUnsigned(requireValue(L"--timeout-ms"), L"--timeout-ms");
            else if (argument == L"--help" || argument == L"-h" || argument == L"/?")
            {
                std::fputs(
                    "TcgCompatibilityHost --tcg-dir <absolute writable TradingCardGame directory>\n"
                    "  [--parent-pid <pid>] [--timeout-ms <100..86400000>]\n"
                    "Credentials are accepted only through inherited SWG_TCG_* environment variables.\n",
                    stdout);
                std::exit(0);
            }
            else
                throw std::runtime_error(std::string("Unknown option: ") + wideToUtf8(argument));
        }

        if (options.timeoutMilliseconds < 100 || options.timeoutMilliseconds > 86400000)
            throw std::runtime_error("--timeout-ms must be between 100 and 86400000");
        options.tcgDirectory = canonicalizeAbsolutePath(options.tcgDirectory);
        return options;
    }

    struct InheritedHandle
    {
        HANDLE value = nullptr;
        bool supplied = false;
    };

    InheritedHandle consumeInheritedHandle(const char *environmentName)
    {
        SecureEnvironmentValue value(environmentName, 32);
        if (value.empty())
            return InheritedHandle();

        char *end = nullptr;
        errno = 0;
        const unsigned long long numeric = std::strtoull(value.c_str(), &end, 0);
        if (errno == ERANGE || end == value.c_str() || *end != '\0' || numeric > std::numeric_limits<uintptr_t>::max())
            throw std::runtime_error(std::string(environmentName) + " is not a valid inherited handle value");

        HANDLE handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(numeric));
        DWORD flags = 0;
        if (handle == nullptr || !GetHandleInformation(handle, &flags))
            throw std::runtime_error(std::string(environmentName) + " was not inherited by this process");

        InheritedHandle result;
        result.value = handle;
        result.supplied = true;
        return result;
    }

    class BridgeTransport
    {
    public:
        BridgeTransport() = default;

        ~BridgeTransport()
        {
            if (g_sharedState == m_state)
                g_sharedState = nullptr;
            if (g_bridgeReadyEvent == m_readyEvent)
                g_bridgeReadyEvent = nullptr;
            if (g_bridgeCallbackEvent == m_callbackEvent)
                g_bridgeCallbackEvent = nullptr;
            if (m_state != nullptr)
                UnmapViewOfFile(m_state);
            if (m_mapping != nullptr)
                CloseHandle(m_mapping);
            if (m_readyEvent != nullptr)
                CloseHandle(m_readyEvent);
            if (m_callbackEvent != nullptr)
                CloseHandle(m_callbackEvent);
        }

        BridgeTransport(const BridgeTransport &) = delete;
        BridgeTransport &operator=(const BridgeTransport &) = delete;

        void open(DWORD commandLineParentProcessId)
        {
            const InheritedHandle mapping = consumeInheritedHandle("SWG_TCG_MAPPING_HANDLE");
            const InheritedHandle ready = consumeInheritedHandle("SWG_TCG_READY_HANDLE");
            const InheritedHandle callback = consumeInheritedHandle("SWG_TCG_CALLBACK_HANDLE");
            m_mapping = mapping.value;
            m_readyEvent = ready.value;
            m_callbackEvent = callback.value;
            g_bridgeReadyEvent = m_readyEvent;
            g_bridgeCallbackEvent = m_callbackEvent;
            InterlockedExchange(&g_bridgeReadySignaled, 0);
            InterlockedExchange(&g_bridgeFatal, 0);

            if (!mapping.supplied)
            {
                if (ready.supplied || callback.supplied)
                {
                    signalBridgeReadyNoThrow();
                    throw std::runtime_error("SWG_TCG_READY_HANDLE requires SWG_TCG_MAPPING_HANDLE");
                }
                return;
            }
            m_enabled = true;
            if (!ready.supplied)
                throw std::runtime_error("Bridge mode requires SWG_TCG_READY_HANDLE");
            if (!callback.supplied)
                throw std::runtime_error("Bridge mode requires SWG_TCG_CALLBACK_HANDLE");

            m_state = static_cast<TcgCompatibilityProtocol::SharedState *>(MapViewOfFile(m_mapping,
                FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, TcgCompatibilityProtocol::SharedStateBytes));
            if (m_state == nullptr)
            {
                signalBridgeReadyNoThrow();
                throw std::runtime_error("Could not map the inherited TCG bridge shared state");
            }

            if (m_state->magic != TcgCompatibilityProtocol::Magic ||
                m_state->version != TcgCompatibilityProtocol::Version ||
                m_state->structureBytes != static_cast<std::uint32_t>(TcgCompatibilityProtocol::SharedStateBytes))
            {
                signalBridgeReadyNoThrow();
                throw std::runtime_error("Inherited TCG bridge mapping has an incompatible magic, version, or size");
            }

            g_sharedState = m_state;
            if (m_state->parentProcessId == 0u || m_state->parentProcessId == GetCurrentProcessId())
            {
                publishBridgeFailureNoThrow(1002, "TCG bridge parent process ID is invalid");
                throw std::runtime_error("TCG bridge parent process ID is invalid");
            }
            if (commandLineParentProcessId != 0u && commandLineParentProcessId != m_state->parentProcessId)
            {
                publishBridgeFailureNoThrow(1003, "TCG bridge parent process ID does not match --parent-pid");
                throw std::runtime_error("TCG bridge parent process ID does not match --parent-pid");
            }

            const LONG lifecycle = atomicRead(&m_state->lifecycle);
            if (lifecycle != TcgCompatibilityProtocol::LifecycleStarting)
            {
                publishBridgeFailureNoThrow(1004, "TCG bridge mapping is not in the starting state");
                throw std::runtime_error("TCG bridge mapping is not in the starting state");
            }
            const LONG activeFrame = atomicRead(&m_state->activeFrame);
            const LONG frameSequence = atomicRead(&m_state->frameSequence);
            const LONG commandWrite = atomicRead(&m_state->commandWrite);
            const LONG commandRead = atomicRead(&m_state->commandRead);
            const LONG callbackWrite = atomicRead(&m_state->callbackWrite);
            const LONG callbackRead = atomicRead(&m_state->callbackRead);
            if ((activeFrame != 0 && activeFrame != 1) || (frameSequence & 1) != 0 ||
                static_cast<std::uint32_t>(commandWrite) - static_cast<std::uint32_t>(commandRead) >
                    TcgCompatibilityProtocol::MaximumCommands ||
                static_cast<std::uint32_t>(callbackWrite) - static_cast<std::uint32_t>(callbackRead) >
                    TcgCompatibilityProtocol::MaximumCallbacks)
            {
                publishBridgeFailureNoThrow(1005, "TCG bridge ring or frame state is invalid");
                throw std::runtime_error("TCG bridge ring or frame state is invalid");
            }

            SecureZeroMemory(m_state->failureMessage, sizeof(m_state->failureMessage));
            InterlockedExchange(&m_state->failureCode, 0);
            InterlockedExchange(&m_state->hostProcessId, static_cast<LONG>(GetCurrentProcessId()));
        }

        bool enabled() const { return m_enabled; }
        DWORD parentProcessId() const { return m_state != nullptr ? m_state->parentProcessId : 0u; }

    private:
        bool m_enabled = false;
        HANDLE m_mapping = nullptr;
        HANDLE m_readyEvent = nullptr;
        HANDLE m_callbackEvent = nullptr;
        TcgCompatibilityProtocol::SharedState *m_state = nullptr;
    };

    HANDLE consumeInheritedStopHandle()
    {
        return consumeInheritedHandle("SWG_TCG_STOP_HANDLE").value;
    }

    void pumpMessages()
    {
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                if (g_shutdownEvent != nullptr)
                    SetEvent(g_shutdownEvent);
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    struct SurfaceObservation
    {
        bool available = false;
        unsigned width = 0;
        unsigned height = 0;
        unsigned stride = 0;
    };

    void observeWindows(GetWindowsProc getWindows, GetWindowSurfaceDataProc getSurface,
        std::unordered_map<unsigned, SurfaceObservation> &observations)
    {
        const unsigned reportedCount = getWindows(nullptr, 0);
        if (reportedCount > 256u)
            throw std::runtime_error("SWGTCG.dll reported an unreasonable window count");

        std::vector<unsigned> identifiers(reportedCount, 0u);
        if (reportedCount != 0)
            getWindows(identifiers.data(), reportedCount);
        std::sort(identifiers.begin(), identifiers.end());
        identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());

        for (const unsigned identifier : identifiers)
        {
            auto inserted = observations.emplace(identifier, SurfaceObservation());
            if (inserted.second)
                logMessage("window", "id=0x%08x created", identifier);

            void *bits = nullptr;
            unsigned width = 0;
            unsigned height = 0;
            unsigned stride = 0;
            const bool available = getSurface(identifier, &bits, &width, &height, &stride) != 0 && bits != nullptr;
            SurfaceObservation current;
            current.available = available;
            current.width = width;
            current.height = height;
            current.stride = stride;

            SurfaceObservation &previous = inserted.first->second;
            if (previous.available != current.available || previous.width != current.width ||
                previous.height != current.height || previous.stride != current.stride)
            {
                if (current.available)
                    logMessage("surface", "window=0x%08x width=%u height=%u stride=%u", identifier, width, height, stride);
                else
                    logMessage("surface", "window=0x%08x unavailable", identifier);
                previous = current;
            }
        }

        for (auto iterator = observations.begin(); iterator != observations.end();)
        {
            if (!std::binary_search(identifiers.begin(), identifiers.end(), iterator->first))
            {
                logMessage("window", "id=0x%08x destroyed", iterator->first);
                iterator = observations.erase(iterator);
            }
            else
                ++iterator;
        }
    }

    std::vector<unsigned> getCurrentWindowIds(GetWindowsProc getWindows)
    {
        const unsigned reportedCount = getWindows(nullptr, 0);
        if (reportedCount > 256u)
            throw std::runtime_error("SWGTCG.dll reported an unreasonable window count");

        std::vector<unsigned> identifiers(reportedCount, 0u);
        if (reportedCount != 0u)
            (void)getWindows(identifiers.data(), reportedCount);
        std::sort(identifiers.begin(), identifiers.end());
        identifiers.erase(std::unique(identifiers.begin(), identifiers.end()), identifiers.end());
        return identifiers;
    }

    HWND findCurrentTcgWindow(const std::vector<unsigned> &identifiers, std::uint32_t identifier)
    {
        if (!std::binary_search(identifiers.begin(), identifiers.end(), identifier))
            return nullptr;
        HWND window = reinterpret_cast<HWND>(static_cast<uintptr_t>(identifier));
        if (!IsWindow(window))
            return nullptr;
        return window;
    }

    void processBridgeCommand(const LegacyApi &api, const TcgCompatibilityProtocol::Command &command,
        const std::vector<unsigned> &identifiers, bool &shutdownRequested)
    {
        using namespace TcgCompatibilityProtocol;
        switch (static_cast<CommandType>(command.type))
        {
            case CommandSetWindowState:
                if (command.values[0] < 0 || command.values[0] > 2)
                    throw std::runtime_error("TCG bridge window-state command is invalid");
                api.onWindowStateChanged(command.values[0]);
                break;

            case CommandMusicCompletion:
                api.onMusicCompletion();
                break;

            case CommandSetFocus:
                if (command.values[0] != 0 && command.values[0] != 1)
                    throw std::runtime_error("TCG bridge focus command is invalid");
                if (findCurrentTcgWindow(identifiers, command.windowId) == nullptr)
                    break;
                api.onFocus(command.windowId, command.values[0] != 0 ? 1 : 0);
                break;

            case CommandSetLocation:
            {
                HWND window = findCurrentTcgWindow(identifiers, command.windowId);
                if (window == nullptr)
                    break;
                if (!SetWindowPos(window, nullptr, command.values[0], command.values[1], 0, 0,
                    SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER))
                {
                    throw std::runtime_error("Could not apply a TCG bridge window-location command");
                }
                break;
            }

            case CommandSetSize:
            {
                const int width = command.values[0];
                const int height = command.values[1];
                if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
                    throw std::runtime_error("TCG bridge window-size command is invalid");
                HWND window = findCurrentTcgWindow(identifiers, command.windowId);
                if (window == nullptr)
                    break;
                if (!SetWindowPos(window, nullptr, 0, 0, width, height,
                    SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER))
                {
                    throw std::runtime_error("Could not apply a TCG bridge window-size command");
                }
                break;
            }

            case CommandClose:
            {
                HWND window = findCurrentTcgWindow(identifiers, command.windowId);
                if (window != nullptr)
                    (void)SendMessageW(window, WM_CLOSE, 0, 0);
                break;
            }

            case CommandMouse:
                if (command.values[0] < 2 || command.values[0] > 5)
                    throw std::runtime_error("TCG bridge mouse event type is invalid");
                if (findCurrentTcgWindow(identifiers, command.windowId) == nullptr)
                    break;
                api.onMouseEvent(command.windowId, command.values[0], command.values[1], command.values[2],
                    command.values[3], command.values[4], command.values[5], command.values[6] & 0xff,
                    command.values[7] & static_cast<int>(0xff000000u));
                break;

            case CommandMouseWheel:
                if (findCurrentTcgWindow(identifiers, command.windowId) == nullptr)
                    break;
                api.onMouseWheelEvent(command.windowId, command.values[0], command.values[1], command.values[2],
                    command.values[3], command.values[4], command.values[5] & 0xff,
                    command.values[6] & static_cast<int>(0xff000000u));
                break;

            case CommandKey:
                if (command.values[0] != 6 && command.values[0] != 7)
                    throw std::runtime_error("TCG bridge key event type is invalid");
                (void)api.onKeyEvent(command.values[0], command.values[1],
                    command.values[2] & static_cast<int>(0xff000000u), command.values[3], command.values[4],
                    command.values[5]);
                break;

            case CommandShutdown:
                shutdownRequested = true;
                break;

            case CommandNone:
            default:
                throw std::runtime_error("TCG bridge command type is invalid");
        }
    }

    bool processBridgeCommands(const LegacyApi &api)
    {
        if (g_sharedState == nullptr)
            return false;

        LONG read = atomicRead(&g_sharedState->commandRead);
        const LONG write = atomicRead(&g_sharedState->commandWrite);
        const std::uint32_t pending = static_cast<std::uint32_t>(write) - static_cast<std::uint32_t>(read);
        if (pending > TcgCompatibilityProtocol::MaximumCommands)
            throw std::runtime_error("TCG bridge command queue is corrupt");

        const std::vector<unsigned> identifiers = getCurrentWindowIds(api.getWindows);
        bool shutdownRequested = false;
        for (std::uint32_t index = 0u; index < pending; ++index)
        {
            TcgCompatibilityProtocol::Command &slot =
                g_sharedState->commands[static_cast<std::uint32_t>(read) % TcgCompatibilityProtocol::MaximumCommands];
            const TcgCompatibilityProtocol::Command command = slot;
            processBridgeCommand(api, command, identifiers, shutdownRequested);
            SecureZeroMemory(&slot, sizeof(slot));
            read = static_cast<LONG>(static_cast<std::uint32_t>(read) + 1u);
            InterlockedExchange(&g_sharedState->commandRead, read);
            if (shutdownRequested)
                break;
        }
        return shutdownRequested;
    }

    std::uint32_t nonNegativeDimension(int value)
    {
        return value > 0 ? static_cast<std::uint32_t>(value) : 0u;
    }

    void populateWindowMetadata(HWND window, TcgCompatibilityProtocol::WindowRecord &record)
    {
        WINDOWINFO windowInfo = {};
        windowInfo.cbSize = sizeof(windowInfo);
        const bool haveWindowInfo = GetWindowInfo(window, &windowInfo) != FALSE;
        if (haveWindowInfo)
        {
            record.x = windowInfo.rcClient.left;
            record.y = windowInfo.rcClient.top;
            record.width = nonNegativeDimension(windowInfo.rcClient.right - windowInfo.rcClient.left);
            record.height = nonNegativeDimension(windowInfo.rcClient.bottom - windowInfo.rcClient.top);
        }

        (void)GetWindowTextA(window, record.title, static_cast<int>(TcgCompatibilityProtocol::MaximumTitleBytes));
        char className[257] = {};
        if (GetClassNameA(window, className, static_cast<int>(_countof(className))) != 0 &&
            std::strcmp(className, "QToolTip") != 0)
        {
            record.canGetFocus = 1u;
        }

        MINMAXINFO minMaxInfo = {};
        minMaxInfo.ptMinTrackSize.x = GetSystemMetrics(SM_CXMINTRACK);
        minMaxInfo.ptMinTrackSize.y = GetSystemMetrics(SM_CYMINTRACK);
        minMaxInfo.ptMaxTrackSize.x = 64000;
        minMaxInfo.ptMaxTrackSize.y = 64000;
        (void)SendMessageW(window, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&minMaxInfo));

        if (haveWindowInfo)
        {
            const int frameWidth = (windowInfo.rcWindow.right - windowInfo.rcWindow.left) -
                (windowInfo.rcClient.right - windowInfo.rcClient.left);
            const int frameHeight = (windowInfo.rcWindow.bottom - windowInfo.rcWindow.top) -
                (windowInfo.rcClient.bottom - windowInfo.rcClient.top);
            record.minimumWidth = nonNegativeDimension(minMaxInfo.ptMinTrackSize.x - frameWidth);
            record.minimumHeight = nonNegativeDimension(minMaxInfo.ptMinTrackSize.y - frameHeight);
            record.maximumWidth = nonNegativeDimension(minMaxInfo.ptMaxTrackSize.x - frameWidth);
            record.maximumHeight = nonNegativeDimension(minMaxInfo.ptMaxTrackSize.y - frameHeight);
        }
    }

    bool publishBridgeFrame(const LegacyApi &api)
    {
        using namespace TcgCompatibilityProtocol;
        if (g_sharedState == nullptr)
            throw std::runtime_error("TCG bridge shared state is unavailable");

        const LONG writingSequence = InterlockedIncrement(&g_sharedState->frameSequence);
        if ((writingSequence & 1) == 0)
            throw std::runtime_error("TCG bridge frame sequence lost seqlock alignment");
        const LONG currentFrame = atomicRead(&g_sharedState->activeFrame);
        if (currentFrame != 0 && currentFrame != 1)
            throw std::runtime_error("TCG bridge active frame index is invalid");
        const LONG inactiveFrame = currentFrame == 0 ? 1 : 0;
        Frame &frame = g_sharedState->frames[inactiveFrame];
        frame.windowCount = 0u;
        frame.pixelBytes = 0u;
        frame.captureWindowId = api.getCaptureWindow();
        frame.reserved = 0u;
        SecureZeroMemory(frame.windows, sizeof(frame.windows));

        const std::vector<unsigned> identifiers = getCurrentWindowIds(api.getWindows);
        const size_t publishedCount = std::min<size_t>(identifiers.size(), MaximumWindows);
        std::uint32_t pixelBytes = 0u;
        bool hasRenderableSurface = false;
        for (size_t index = 0u; index < publishedCount; ++index)
        {
            WindowRecord &record = frame.windows[index];
            record.id = identifiers[index];
            HWND window = reinterpret_cast<HWND>(static_cast<uintptr_t>(record.id));
            if (!IsWindow(window))
                throw std::runtime_error("SWGTCG.dll returned a stale window during frame publication");
            populateWindowMetadata(window, record);

            void *surface = nullptr;
            unsigned width = 0u;
            unsigned height = 0u;
            unsigned stride = 0u;
            if (api.getWindowSurfaceData(record.id, &surface, &width, &height, &stride) != 0u && surface != nullptr)
            {
                if (width == 0u || height == 0u || width > 16384u || height > 16384u ||
                    stride < width * 4u || stride > 16384u * 4u)
                {
                    throw std::runtime_error("SWGTCG.dll returned invalid surface dimensions");
                }
                const std::uint64_t surfaceBytes = static_cast<std::uint64_t>(stride) * height;
                if (surfaceBytes > MaximumFramePixelBytes ||
                    static_cast<std::uint64_t>(pixelBytes) + surfaceBytes > MaximumFramePixelBytes)
                {
                    throw std::runtime_error("TCG window surfaces exceed the bridge frame capacity");
                }

                record.width = width;
                record.height = height;
                record.stride = stride;
                record.surfaceOffset = pixelBytes;
                record.surfaceBytes = static_cast<std::uint32_t>(surfaceBytes);
                std::memcpy(frame.pixels + pixelBytes, surface, static_cast<size_t>(surfaceBytes));
                pixelBytes += record.surfaceBytes;
                hasRenderableSurface = true;
            }
            ++frame.windowCount;
        }
        frame.pixelBytes = pixelBytes;
        MemoryBarrier();
        InterlockedExchange(&g_sharedState->activeFrame, inactiveFrame);
        const LONG publishedSequence = InterlockedIncrement(&g_sharedState->frameSequence);
        if ((publishedSequence & 1) != 0)
            throw std::runtime_error("TCG bridge frame publication did not close its seqlock");
        return hasRenderableSurface;
    }

    int runHost(const CommandLineOptions &options)
    {
        BridgeTransport bridge;
        HANDLE inheritedStopHandle = nullptr;
        HANDLE parentProcess = nullptr;
        HWND desktopWindow = nullptr;
        HMODULE tcgModule = nullptr;
        ShutdownProc shutdown = nullptr;
        bool initialized = false;
        bool controlHandlerInstalled = false;

        try
        {
            bridge.open(options.parentProcessId);
            verifyWritableDirectory(options.tcgDirectory);
            const std::wstring dllPath = options.tcgDirectory + L"\\SWGTCG.dll";
            const DWORD dllAttributes = GetFileAttributesW(dllPath.c_str());
            if (dllAttributes == INVALID_FILE_ATTRIBUTES || (dllAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                throw std::runtime_error("The writable TCG directory does not contain SWGTCG.dll");

            inheritedStopHandle = consumeInheritedStopHandle();
            if (bridge.enabled() && inheritedStopHandle == nullptr)
                throw std::runtime_error("Bridge mode requires SWG_TCG_STOP_HANDLE");
            const DWORD parentProcessId = bridge.enabled() ? bridge.parentProcessId() : options.parentProcessId;
            if (parentProcessId != 0u)
            {
                parentProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentProcessId);
                if (parentProcess == nullptr)
                    throw std::runtime_error("Could not open the parent process for synchronization");
                DWORD parentExitCode = 0u;
                if (!GetExitCodeProcess(parentProcess, &parentExitCode) || parentExitCode != STILL_ACTIVE)
                    throw std::runtime_error("The TCG bridge parent process is not active");
            }

            g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (g_shutdownEvent == nullptr)
                throw std::runtime_error("Could not create the process shutdown event");
            if (!SetConsoleCtrlHandler(&consoleControlHandler, TRUE))
                throw std::runtime_error("Could not install the process control handler");
            controlHandlerInstalled = true;

            desktopWindow = createHiddenDesktopWindow();
            if (!SetCurrentDirectoryW(options.tcgDirectory.c_str()))
                throw std::runtime_error("Could not set the TCG working directory");
            if (!SetDllDirectoryW(options.tcgDirectory.c_str()))
                throw std::runtime_error("Could not set the private DLL search directory");

            logMessage("lifecycle", "loading=%s", wideToUtf8(dllPath).c_str());
            tcgModule = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (tcgModule == nullptr)
            {
                const DWORD error = GetLastError();
                throw std::runtime_error("LoadLibraryExW(SWGTCG.dll) failed with Windows error " + std::to_string(error));
            }

            LegacyApi api;
            api.initialize = resolveRequiredExport<InitializeProc>(tcgModule, "Initialize");
            api.runFrame = resolveRequiredExport<RunFrameProc>(tcgModule, "RunFrame");
            api.shutdown = resolveRequiredExport<ShutdownProc>(tcgModule, "Shutdown");
            api.getWindows = resolveRequiredExport<GetWindowsProc>(tcgModule, "GetWindows");
            api.getCaptureWindow = resolveRequiredExport<GetCaptureWindowProc>(tcgModule, "GetCaptureWindow");
            api.getCurrentCursor = resolveRequiredExport<GetCurrentCursorProc>(tcgModule, "GetCurrentCursor");
            api.getWindowRepaintRects =
                resolveRequiredExport<GetWindowRepaintRectsProc>(tcgModule, "GetWindowRepaintRects");
            api.getWindowSurfaceData =
                resolveRequiredExport<GetWindowSurfaceDataProc>(tcgModule, "GetWindowSurfaceData");
            api.onMouseEvent = resolveRequiredExport<OnMouseEventProc>(tcgModule, "OnMouseEvent");
            api.onMouseWheelEvent = resolveRequiredExport<OnMouseWheelEventProc>(tcgModule, "OnMouseWheelEvent");
            api.onKeyEvent = resolveRequiredExport<OnKeyEventProc>(tcgModule, "OnKeyEvent");
            api.onFocus = resolveRequiredExport<OnFocusProc>(tcgModule, "OnFocus");
            api.onWindowStateChanged =
                resolveRequiredExport<OnWindowStateChangedProc>(tcgModule, "OnWindowStateChanged");
            api.onMusicCompletion = resolveRequiredExport<OnMusicCompletionProc>(tcgModule, "OnMusicCompletion");
            shutdown = api.shutdown;
            logMessage("lifecycle", "required ABI exports resolved");

            SecureEnvironmentValue userName("SWG_TCG_USERNAME", 1024);
            SecureEnvironmentValue sessionId("SWG_TCG_SESSION_ID", 4096);
            SecureEnvironmentValue challenge("SWG_TCG_CHALLENGE", 4096);
            SecureEnvironmentValue characterName("SWG_TCG_CHARACTER", 1024);
            SecureEnvironmentValue challengeFounder("SWG_TCG_CHALLENGE_FOUNDER", 16);
            SecureEnvironmentValue startTutorial("SWG_TCG_START_TUTORIAL", 16);
            SecureEnvironmentValue realm("SWG_TCG_REALM", 16);
            SecureEnvironmentValue language("SWG_TCG_LANGUAGE", 16);

            SecureArgument userNameArgument("--username=", userName);
            SecureArgument sessionArgument("--sessionID=", sessionId);
            SecureArgument challengeArgument("--challenge=", challenge);
            SecureArgument characterArgument("--characterID=", characterName);
            const std::string applicationDirectory = wideToAnsi(options.tcgDirectory);

            std::vector<const char *> tcgArguments;
            tcgArguments.reserve(11);
            tcgArguments.push_back(applicationDirectory.c_str());
            if (!userNameArgument.empty())
                tcgArguments.push_back(userNameArgument.c_str());
            if (!sessionArgument.empty())
                tcgArguments.push_back(sessionArgument.c_str());
            if (!challengeArgument.empty())
            {
                tcgArguments.push_back(challengeArgument.c_str());
                tcgArguments.push_back(isTrue(challengeFounder) ? "--is-founder=true" : "--is-founder=false");
            }
            if (!characterArgument.empty())
                tcgArguments.push_back(characterArgument.c_str());
            if (isTrue(startTutorial))
                tcgArguments.push_back("--post-login=tutorial");
            if (!realm.empty() && _stricmp(realm.c_str(), "stage") == 0)
                tcgArguments.push_back("--realm=stage");
            tcgArguments.push_back("--host=swg");
            if (!language.empty() && _stricmp(language.c_str(), "fr") == 0)
                tcgArguments.push_back("--lang=fr_FR");
            else if (!language.empty() && _stricmp(language.c_str(), "de") == 0)
                tcgArguments.push_back("--lang=de_DE");
            tcgArguments.push_back(nullptr);

            try
            {
                logMessage("lifecycle", "calling Initialize (credential values are never logged)");
                api.initialize(static_cast<int>(tcgArguments.size() - 1u), tcgArguments.data(), desktopWindow, &g_callbackTable);
                initialized = true;
                logMessage("lifecycle", "Initialize returned; frame loop started");

                std::vector<HANDLE> waitHandles;
                waitHandles.push_back(g_shutdownEvent);
                if (inheritedStopHandle != nullptr)
                    waitHandles.push_back(inheritedStopHandle);
                if (parentProcess != nullptr)
                    waitHandles.push_back(parentProcess);

                std::unordered_map<unsigned, SurfaceObservation> observations;
                const ULONGLONG deadline = GetTickCount64() + options.timeoutMilliseconds;
                const char *stopReason = bridge.enabled() ? "bridge-shutdown" : "timeout";
                bool bridgeShutdownRequested = false;

                if (bridge.enabled())
                {
                    bool hasRenderableSurface = false;
                    while (!hasRenderableSurface)
                    {
                        pumpMessages();
                        if (processBridgeCommands(api))
                            throw std::runtime_error("TCG bridge shutdown was requested before an embedded surface became ready");
                        api.runFrame();
                        hasRenderableSurface = publishBridgeFrame(api);
                        if (atomicRead(&g_bridgeFatal) != 0)
                            throw std::runtime_error("TCG bridge callback transport failed during initialization");
                        if (hasRenderableSurface)
                            break;
                        if (GetTickCount64() >= deadline)
                            throw std::runtime_error("Timed out waiting for the first embedded TCG surface");

                        const DWORD startupWait = MsgWaitForMultipleObjects(
                            static_cast<DWORD>(waitHandles.size()), waitHandles.data(), FALSE, 16, QS_ALLINPUT);
                        if (startupWait >= WAIT_OBJECT_0 && startupWait < WAIT_OBJECT_0 + waitHandles.size())
                        {
                            const size_t selected = static_cast<size_t>(startupWait - WAIT_OBJECT_0);
                            if (selected == 0u)
                                throw std::runtime_error("TCG host stop was requested before an embedded surface became ready");
                            if (inheritedStopHandle != nullptr && selected == 1u)
                                throw std::runtime_error("TCG parent stop was requested before an embedded surface became ready");
                            throw std::runtime_error("TCG parent exited before an embedded surface became ready");
                        }
                        if (startupWait == WAIT_FAILED)
                            throw std::runtime_error("Startup MsgWaitForMultipleObjects failed");
                    }
                    InterlockedExchange(&g_sharedState->lifecycle, TcgCompatibilityProtocol::LifecycleReady);
                    signalBridgeReadyNoThrow();
                    logMessage("lifecycle", "bridge ready; first embedded surface published");
                }

                for (;;)
                {
                    if (bridgeShutdownRequested)
                    {
                        stopReason = "bridge-command";
                        break;
                    }
                    pumpMessages();
                    if (bridge.enabled())
                    {
                        if (processBridgeCommands(api))
                        {
                            bridgeShutdownRequested = true;
                            continue;
                        }
                        api.runFrame();
                        (void)publishBridgeFrame(api);
                        if (atomicRead(&g_bridgeFatal) != 0)
                            throw std::runtime_error("TCG bridge callback transport failed");
                    }
                    else
                    {
                        api.runFrame();
                        observeWindows(api.getWindows, api.getWindowSurfaceData, observations);
                        if (GetTickCount64() >= deadline)
                        {
                            stopReason = "timeout";
                            break;
                        }
                    }

                    const DWORD wait = MsgWaitForMultipleObjects(static_cast<DWORD>(waitHandles.size()), waitHandles.data(),
                        FALSE, 16, QS_ALLINPUT);
                    if (wait >= WAIT_OBJECT_0 && wait < WAIT_OBJECT_0 + waitHandles.size())
                    {
                        const size_t selected = static_cast<size_t>(wait - WAIT_OBJECT_0);
                        if (selected == 0)
                            stopReason = "process-signal";
                        else if (inheritedStopHandle != nullptr && selected == 1)
                            stopReason = "parent-stop-signal";
                        else
                            stopReason = "parent-process-exited";
                        break;
                    }
                    if (wait == WAIT_FAILED)
                        throw std::runtime_error("MsgWaitForMultipleObjects failed");
                }

                logMessage("lifecycle", "stopping reason=%s", stopReason);
                shutdown();
                initialized = false;
                shutdown = nullptr;
                if (bridge.enabled() && atomicRead(&g_sharedState->lifecycle) != TcgCompatibilityProtocol::LifecycleFailed)
                    InterlockedExchange(&g_sharedState->lifecycle, TcgCompatibilityProtocol::LifecycleStopped);
                logMessage("lifecycle", "Shutdown returned");
            }
            catch (...)
            {
                // Credential argument storage is still alive in this handler. That
                // matters because the legacy DLL may retain argv pointers.
                if (initialized && shutdown != nullptr)
                {
                    logMessage("lifecycle", "best-effort Shutdown after failure");
                    shutdown();
                    initialized = false;
                    shutdown = nullptr;
                }
                throw;
            }
        }
        catch (const std::exception &error)
        {
            if (bridge.enabled())
                publishBridgeFailureNoThrow(1100, error.what());
            if (tcgModule != nullptr)
            {
                FreeLibrary(tcgModule);
                tcgModule = nullptr;
            }
            if (desktopWindow != nullptr)
                DestroyWindow(desktopWindow);
            SetDllDirectoryW(L"");
            if (controlHandlerInstalled)
                (void)SetConsoleCtrlHandler(&consoleControlHandler, FALSE);
            if (g_shutdownEvent != nullptr)
            {
                CloseHandle(g_shutdownEvent);
                g_shutdownEvent = nullptr;
            }
            if (parentProcess != nullptr)
                CloseHandle(parentProcess);
            if (inheritedStopHandle != nullptr)
                CloseHandle(inheritedStopHandle);
            throw;
        }
        catch (...)
        {
            if (bridge.enabled())
                publishBridgeFailureNoThrow(1199, "Unexpected non-standard TCG bridge failure");
            if (tcgModule != nullptr)
                FreeLibrary(tcgModule);
            if (desktopWindow != nullptr)
                DestroyWindow(desktopWindow);
            SetDllDirectoryW(L"");
            if (controlHandlerInstalled)
                (void)SetConsoleCtrlHandler(&consoleControlHandler, FALSE);
            if (g_shutdownEvent != nullptr)
            {
                CloseHandle(g_shutdownEvent);
                g_shutdownEvent = nullptr;
            }
            if (parentProcess != nullptr)
                CloseHandle(parentProcess);
            if (inheritedStopHandle != nullptr)
                CloseHandle(inheritedStopHandle);
            throw;
        }

        if (tcgModule != nullptr)
            FreeLibrary(tcgModule);
        if (desktopWindow != nullptr)
            DestroyWindow(desktopWindow);
        SetDllDirectoryW(L"");
        if (controlHandlerInstalled)
            (void)SetConsoleCtrlHandler(&consoleControlHandler, FALSE);
        if (g_shutdownEvent != nullptr)
        {
            CloseHandle(g_shutdownEvent);
            g_shutdownEvent = nullptr;
        }
        if (parentProcess != nullptr)
            CloseHandle(parentProcess);
        if (inheritedStopHandle != nullptr)
            CloseHandle(inheritedStopHandle);
        return 0;
    }
}

int wmain(int argumentCount, wchar_t *arguments[])
{
    g_startTick = GetTickCount64();
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    try
    {
        if (clearUnexpectedIntegrationTestEnvironment())
            throw std::runtime_error("TCG compatibility host refused inherited integration-test secrets");
        const CommandLineOptions options = parseCommandLine(argumentCount, arguments);
        return runHost(options);
    }
    catch (const std::exception &error)
    {
        logMessage("error", "%s", error.what());
        return 1;
    }
    catch (...)
    {
        logMessage("error", "unexpected non-standard exception");
        return 1;
    }
}
