#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "BrowserCompatibilityProtocol.h"
#include "libMozilla.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#if !defined(_M_IX86)
#error BrowserCompatibilityHost must be Win32/x86 to load the legacy Mozilla runtime.
#endif

namespace
{
	using namespace BrowserCompatibilityProtocol;

	static_assert(sizeof(void *) == 4u, "legacy Mozilla broker requires 32-bit pointers");
	static_assert(sizeof(LONG) == sizeof(std::int32_t), "browser transport atomics changed");
	static_assert(sizeof(wchar_t) == sizeof(std::uint16_t), "Windows UTF-16 ABI changed");

	struct Options
	{
		HANDLE mapping = nullptr;
		HANDLE commandEvent = nullptr;
		HANDLE updateEvent = nullptr;
		HANDLE stopEvent = nullptr;
		HANDLE parentProcess = nullptr;
		std::wstring runtimeDirectory;
	};

	LONG atomicRead(volatile std::int32_t const * value)
	{
		return InterlockedCompareExchange(
			reinterpret_cast<volatile LONG *>(const_cast<volatile std::int32_t *>(value)), 0, 0);
	}

	LONG atomicIncrement(volatile std::int32_t * value)
	{
		return InterlockedIncrement(reinterpret_cast<volatile LONG *>(value));
	}

	void atomicWrite(volatile std::int32_t * value, LONG replacement)
	{
		(void)InterlockedExchange(reinterpret_cast<volatile LONG *>(value), replacement);
	}

	HANDLE parseHandle(wchar_t const * value)
	{
		if (!value || !*value || *value == L'-')
			throw std::runtime_error("an inherited handle value is missing or invalid");
		wchar_t * end = nullptr;
		errno = 0;
		unsigned long long const number = std::wcstoull(value, &end, 10);
		if (errno == ERANGE || !end || *end != L'\0' || number == 0u ||
			number > static_cast<unsigned long long>(std::numeric_limits<std::uintptr_t>::max()))
		{
			throw std::runtime_error("an inherited handle value is out of range");
		}
		HANDLE const handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(number));
		DWORD flags = 0u;
		if (!GetHandleInformation(handle, &flags) || (flags & HANDLE_FLAG_INHERIT) == 0u)
			throw std::runtime_error("the browser broker received a non-inherited handle");
		return handle;
	}

	Options parseOptions(int argumentCount, wchar_t * arguments[])
	{
		if (argumentCount != 13)
			throw std::runtime_error("browser broker requires exactly six named arguments");
		Options options;
		bool seenMapping = false;
		bool seenCommand = false;
		bool seenUpdate = false;
		bool seenStop = false;
		bool seenParent = false;
		bool seenRuntime = false;
		for (int index = 1; index < argumentCount; index += 2)
		{
			wchar_t const * const name = arguments[index];
			wchar_t const * const value = arguments[index + 1];
			if (std::wcscmp(name, L"--mapping") == 0 && !seenMapping)
			{
				options.mapping = parseHandle(value);
				seenMapping = true;
			}
			else if (std::wcscmp(name, L"--command-event") == 0 && !seenCommand)
			{
				options.commandEvent = parseHandle(value);
				seenCommand = true;
			}
			else if (std::wcscmp(name, L"--update-event") == 0 && !seenUpdate)
			{
				options.updateEvent = parseHandle(value);
				seenUpdate = true;
			}
			else if (std::wcscmp(name, L"--stop-event") == 0 && !seenStop)
			{
				options.stopEvent = parseHandle(value);
				seenStop = true;
			}
			else if (std::wcscmp(name, L"--parent") == 0 && !seenParent)
			{
				options.parentProcess = parseHandle(value);
				seenParent = true;
			}
			else if (std::wcscmp(name, L"--runtime") == 0 && !seenRuntime)
			{
				options.runtimeDirectory = value;
				seenRuntime = true;
			}
			else
				throw std::runtime_error("browser broker received an unknown or duplicate argument");
		}
		if (!seenMapping || !seenCommand || !seenUpdate || !seenStop || !seenParent || !seenRuntime)
			throw std::runtime_error("browser broker command line is incomplete");
		return options;
	}

	std::wstring canonicalPath(std::wstring const & path)
	{
		if (path.empty())
			throw std::runtime_error("the Mozilla broker runtime path is empty");
		DWORD const required = GetFullPathNameW(path.c_str(), 0u, nullptr, nullptr);
		if (required == 0u)
			throw std::runtime_error("the Mozilla broker runtime path cannot be canonicalized");
		std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1u, L'\0');
		DWORD const written = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buffer.size()),
			buffer.data(), nullptr);
		if (written == 0u || written >= buffer.size())
			throw std::runtime_error("the Mozilla broker runtime path cannot be canonicalized");
		std::wstring result(buffer.data(), written);
		while (result.size() > 3u && (result.back() == L'\\' || result.back() == L'/'))
			result.pop_back();
		return result;
	}

	std::wstring executableDirectory()
	{
		std::vector<wchar_t> buffer(32768u, L'\0');
		DWORD const written = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (written == 0u || written >= buffer.size())
			throw std::runtime_error("the browser broker executable path is unavailable");
		std::wstring path(buffer.data(), written);
		std::wstring::size_type const slash = path.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			throw std::runtime_error("the browser broker executable has no parent directory");
		return canonicalPath(path.substr(0u, slash));
	}

	bool containsProtectedClientComponent(std::wstring const & path)
	{
		std::size_t begin = 0u;
		while (begin < path.size())
		{
			while (begin < path.size() && (path[begin] == L'\\' || path[begin] == L'/'))
				++begin;
			std::size_t end = begin;
			while (end < path.size() && path[end] != L'\\' && path[end] != L'/')
				++end;
			if (end - begin == 7u && _wcsnicmp(path.c_str() + begin, L"_client", 7u) == 0)
				return true;
			begin = end;
		}
		return false;
	}

	std::wstring resolvedDirectoryPath(std::wstring const & directory)
	{
		HANDLE const handle = CreateFileW(directory.c_str(), FILE_READ_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
			throw std::runtime_error("the isolated Mozilla runtime directory cannot be opened safely");
		DWORD const required = GetFinalPathNameByHandleW(handle, nullptr, 0u,
			FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		if (required == 0u)
		{
			CloseHandle(handle);
			throw std::runtime_error("the isolated Mozilla runtime target cannot be resolved");
		}
		std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1u, L'\0');
		DWORD const written = GetFinalPathNameByHandleW(handle, buffer.data(),
			static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		CloseHandle(handle);
		if (written == 0u || written >= buffer.size())
			throw std::runtime_error("the isolated Mozilla runtime target cannot be resolved");
		return std::wstring(buffer.data(), written);
	}

	void verifyIsolatedWritableRuntime(std::wstring const & runtimeDirectory)
	{
		if (_wcsicmp(runtimeDirectory.c_str(), executableDirectory().c_str()) != 0)
			throw std::runtime_error("--runtime must be the directory containing BrowserCompatibilityHost.exe");
		std::wstring::size_type const slash = runtimeDirectory.find_last_of(L"\\/");
		std::wstring const leaf = slash == std::wstring::npos ? runtimeDirectory : runtimeDirectory.substr(slash + 1u);
		if (_wcsicmp(leaf.c_str(), L"mozilla-broker") != 0)
			throw std::runtime_error("browser runtime must be isolated under runtime\\mozilla-broker");
		if (containsProtectedClientComponent(runtimeDirectory))
			throw std::runtime_error("refusing to use the protected _client reference tree as a browser runtime");
		DWORD const attributes = GetFileAttributesW(runtimeDirectory.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u)
			throw std::runtime_error("the isolated Mozilla runtime directory does not exist");
		if (containsProtectedClientComponent(resolvedDirectoryPath(runtimeDirectory)))
			throw std::runtime_error("the Mozilla runtime resolves through a reparse point into protected _client data");
		wchar_t suffix[96] = {};
		(void)_snwprintf_s(suffix, _countof(suffix), _TRUNCATE,
			L"\\.browser-host-write-test-%lu-%llu.tmp", GetCurrentProcessId(),
			static_cast<unsigned long long>(GetTickCount64()));
		std::wstring const testPath = runtimeDirectory + suffix;
		HANDLE const file = CreateFileW(testPath.c_str(), GENERIC_WRITE, 0u, nullptr, CREATE_NEW,
			FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			throw std::runtime_error("the isolated Mozilla runtime directory is not writable");
		CloseHandle(file);
	}

	bool clearUnexpectedIntegrationCredentialEnvironment()
	{
		LPWCH const environment = GetEnvironmentStringsW();
		if (!environment)
			throw std::runtime_error("the browser broker could not inspect its environment");

		wchar_t const prefix[] = L"SWGTCG_TEST_";
		std::size_t const prefixLength = _countof(prefix) - 1u;
		bool found = false;
		bool clearFailed = false;
		try
		{
			for (wchar_t const * current = environment; *current;
				current += std::wcslen(current) + 1u)
			{
				wchar_t const * const separator = std::wcschr(current, L'=');
				if (!separator || separator < current + prefixLength ||
					_wcsnicmp(current, prefix, prefixLength) != 0)
				{
					continue;
				}

				found = true;
				std::wstring const name(current, separator);
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
			throw std::runtime_error("the browser broker could not clear an unexpected integration-test variable");
		return found;
	}

	std::string wideToAnsi(std::wstring const & value)
	{
		if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			throw std::runtime_error("the Mozilla runtime path is too long");
		BOOL usedDefault = FALSE;
		int const required = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0, nullptr, &usedDefault);
		if (required <= 0 || usedDefault)
			throw std::runtime_error("the Mozilla runtime path is not representable in the active code page");
		std::string result(static_cast<std::size_t>(required), '\0');
		usedDefault = FALSE;
		if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value.data(),
			static_cast<int>(value.size()), result.data(), required, nullptr, &usedDefault) != required || usedDefault)
		{
			throw std::runtime_error("the Mozilla runtime path is not representable in the active code page");
		}
		return result;
	}

	std::wstring utf8ToWide(char const * value, std::uint32_t bytes)
	{
		if (!value || bytes == 0u || bytes >= MaximumUriBytes ||
			bytes > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
		{
			throw std::runtime_error("a browser navigation URI has an invalid length");
		}
		int const required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
			static_cast<int>(bytes), nullptr, 0);
		if (required <= 0)
			throw std::runtime_error("a browser navigation URI is not valid UTF-8");
		std::wstring result(static_cast<std::size_t>(required), L'\0');
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, static_cast<int>(bytes),
			result.data(), required) != required)
		{
			throw std::runtime_error("a browser navigation URI is not valid UTF-8");
		}
		return result;
	}

	bool isAllowedInternalUri(char const * value, std::size_t length)
	{
		if (!value || length == 0u || length >= MaximumUriBytes)
			return false;
		for (std::size_t index = 0u; index < length; ++index)
		{
			unsigned char const character = static_cast<unsigned char>(value[index]);
			if (character < 0x20u || character == 0x7fu)
				return false;
		}
		std::string const uri(value, length);
		if (_stricmp(uri.c_str(), "about:blank") == 0)
			return true;
		if (uri.size() >= 7u && _strnicmp(uri.c_str(), "http://", 7u) == 0)
			return true;
		if (uri.size() >= 8u && _strnicmp(uri.c_str(), "https://", 8u) == 0)
			return true;
		// Preserve the legacy client's host-name navigation behavior.  Any value
		// with an explicit non-HTTP scheme remains blocked in-process.
		return uri.find(':') == std::string::npos;
	}

	LRESULT CALLBACK hiddenWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		return DefWindowProcW(window, message, wParam, lParam);
	}

	HWND createHiddenWindow()
	{
		wchar_t const className[] = L"SWG Browser Compatibility Host";
		WNDCLASSEXW windowClass = {};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = hiddenWindowProcedure;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = className;
		ATOM const atom = RegisterClassExW(&windowClass);
		if (atom == 0u && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			throw std::runtime_error("the browser broker could not register its private hidden window");
		HWND const window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, className, L"",
			WS_POPUP, -32000, -32000, 1, 1, nullptr, nullptr, windowClass.hInstance, nullptr);
		if (!window)
			throw std::runtime_error("the browser broker could not create its private hidden window");
		return window;
	}

	void pumpMessages()
	{
		MSG message = {};
		while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}

	class Broker;

	class BrokerWindow : public libMozilla::ICallback, public libMozilla::IBlitter
	{
	public:
		BrokerWindow(Broker & broker, std::uint32_t identifier, std::uint32_t stateIndex,
			libMozilla::Window * window, unsigned width, unsigned height);

		void onURIChanged(libMozilla::Window *) override;
		void onProgressChanged(libMozilla::Window *) override;
		void onStatusChanged(libMozilla::Window *) override;
		bool doValidateURI(libMozilla::Window *, char const * uri) override;
		void operator()(void * bits, unsigned width, unsigned height, unsigned stride,
			unsigned bytesPerRow) override;

		void publish(bool uriChanged, bool statusChanged, bool progressChanged,
			WindowLifecycle lifecycle = WindowReady);
		void render();

		Broker & broker;
		std::uint32_t identifier;
		std::uint32_t stateIndex;
		libMozilla::Window * window;
		unsigned width;
		unsigned height;
		std::vector<std::uint8_t> pixels;
		bool surfaceValid;
	};

	class Broker
	{
	public:
		Broker(Options const & options, SharedState * shared)
		: options(options), shared(shared), shutdownRequested(false)
		{
		}

		BrokerWindow * findWindow(std::uint32_t identifier) const;
		std::uint32_t acquireStateSlot() const;
		void processCommands();
		void processCommand(Command const & command);
		void publishFrame();
		void publishFailure(int code, char const * message) const;
		void signalUpdate() const { (void)SetEvent(options.updateEvent); }

		Options const & options;
		SharedState * shared;
		std::vector<std::unique_ptr<BrokerWindow>> windows;
		bool shutdownRequested;
	};

	BrokerWindow::BrokerWindow(Broker & owner, std::uint32_t windowIdentifier,
		std::uint32_t windowStateIndex, libMozilla::Window * browserWindow,
		unsigned initialWidth, unsigned initialHeight)
	: broker(owner)
	, identifier(windowIdentifier)
	, stateIndex(windowStateIndex)
	, window(browserWindow)
	, width(initialWidth)
	, height(initialHeight)
	, surfaceValid(false)
	{
	}

	void BrokerWindow::onURIChanged(libMozilla::Window *)
	{
		publish(true, false, false);
	}

	void BrokerWindow::onProgressChanged(libMozilla::Window *)
	{
		publish(false, false, true);
	}

	void BrokerWindow::onStatusChanged(libMozilla::Window *)
	{
		publish(false, true, false);
	}

	bool BrokerWindow::doValidateURI(libMozilla::Window *, char const * uri)
	{
		return uri && isAllowedInternalUri(uri, strnlen_s(uri, MaximumUriBytes));
	}

	void BrokerWindow::operator()(void * bits, unsigned sourceWidth, unsigned sourceHeight,
		unsigned sourceStride, unsigned sourceBytesPerRow)
	{
		surfaceValid = false;
		if (!bits || sourceWidth == 0u || sourceHeight == 0u ||
			sourceWidth > MaximumSurfaceDimension || sourceHeight > MaximumSurfaceDimension)
		{
			return;
		}
		std::uint64_t const destinationBytes = static_cast<std::uint64_t>(sourceWidth) *
			sourceHeight * 4u;
		if (destinationBytes > MaximumFramePixelBytes ||
			destinationBytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
		{
			return;
		}
		unsigned const sourcePixelBytes = sourceBytesPerRow >= sourceWidth * 4u ? 4u :
			(sourceBytesPerRow >= sourceWidth * 3u ? 3u : 0u);
		if (sourcePixelBytes == 0u || sourceStride < sourceWidth * sourcePixelBytes)
			return;
		pixels.resize(static_cast<std::size_t>(destinationBytes));
		std::uint8_t const * const source = static_cast<std::uint8_t const *>(bits);
		for (unsigned row = 0u; row < sourceHeight; ++row)
		{
			std::uint8_t const * const sourceRow = source + static_cast<std::size_t>(row) * sourceStride;
			std::uint8_t * const destinationRow = pixels.data() +
				static_cast<std::size_t>(row) * sourceWidth * 4u;
			for (unsigned column = 0u; column < sourceWidth; ++column)
			{
				std::uint8_t const * const sourcePixel = sourceRow + column * sourcePixelBytes;
				std::uint8_t * const destinationPixel = destinationRow + column * 4u;
				destinationPixel[0] = sourcePixel[0];
				destinationPixel[1] = sourcePixel[1];
				destinationPixel[2] = sourcePixel[2];
				destinationPixel[3] = 0xffu;
			}
		}
		width = sourceWidth;
		height = sourceHeight;
		surfaceValid = true;
	}

	void BrokerWindow::publish(bool uriChanged, bool statusChanged, bool progressChanged,
		WindowLifecycle lifecycle)
	{
		WindowState & state = broker.shared->windows[stateIndex];
		LONG const opened = atomicIncrement(&state.sequence);
		if ((opened & 1) == 0)
			throw std::runtime_error("browser window-state seqlock lost alignment");
		state.windowId = identifier;
		state.lifecycle = lifecycle;
		state.width = width;
		state.height = height;
		if (window && lifecycle == WindowReady)
		{
			state.canNavigateBack = window->canNavigateBack() ? 1u : 0u;
			state.canNavigateForward = window->canNavigateForward() ? 1u : 0u;
			bool loading = false;
			float const progress = window->getProgress(loading);
			state.isLoading = loading ? 1u : 0u;
			static_assert(sizeof(progress) == sizeof(state.progressBits), "progress transport changed");
			std::memcpy(&state.progressBits, &progress, sizeof(progress));
			int caretX = 0;
			int caretY = 0;
			int caretWidth = 0;
			int caretHeight = 0;
			state.hasCaret = window->getCaret(caretX, caretY, caretWidth, caretHeight) ? 1u : 0u;
			state.caretX = caretX;
			state.caretY = caretY;
			state.caretWidth = caretWidth;
			state.caretHeight = caretHeight;
			char const * const uri = window->getURI();
			std::size_t const uriLength = uri ? strnlen_s(uri, MaximumUriBytes) : 0u;
			state.uriBytes = uriLength < MaximumUriBytes ? static_cast<std::uint32_t>(uriLength) : 0u;
			SecureZeroMemory(state.uri, sizeof(state.uri));
			if (state.uriBytes)
				std::memcpy(state.uri, uri, state.uriBytes);
			wchar_t const * const status = window->getStatus();
			SecureZeroMemory(state.status, sizeof(state.status));
			if (status)
			{
				std::size_t const statusLength = wcsnlen_s(status, MaximumStatusUnits - 1u);
				for (std::size_t index = 0u; index < statusLength; ++index)
					state.status[index] = static_cast<std::uint16_t>(status[index]);
			}
		}
		if (uriChanged)
			++state.uriGeneration;
		if (statusChanged)
			++state.statusGeneration;
		if (progressChanged)
			++state.progressGeneration;
		MemoryBarrier();
		LONG const closed = atomicIncrement(&state.sequence);
		if ((closed & 1) != 0)
			throw std::runtime_error("browser window-state seqlock did not close");
		broker.signalUpdate();
	}

	void BrokerWindow::render()
	{
		if (window)
			(void)window->render(this);
	}

	BrokerWindow * Broker::findWindow(std::uint32_t identifier) const
	{
		for (std::unique_ptr<BrokerWindow> const & candidate : windows)
			if (candidate && candidate->identifier == identifier)
				return candidate.get();
		return nullptr;
	}

	std::uint32_t Broker::acquireStateSlot() const
	{
		for (std::uint32_t index = 0u; index < MaximumWindows; ++index)
		{
			std::uint32_t const lifecycle = shared->windows[index].lifecycle;
			if (lifecycle == WindowEmpty || lifecycle == WindowTombstone || lifecycle == WindowFailed)
				return index;
		}
		throw std::runtime_error("the browser bridge has no free window-state slot");
	}

	void Broker::processCommands()
	{
		for (;;)
		{
			LONG const read = atomicRead(&shared->commandRead);
			LONG const write = atomicRead(&shared->commandWrite);
			if (read == write)
				break;
			if (read < 0 || write < read || write - read > static_cast<LONG>(MaximumCommands))
				throw std::runtime_error("the browser bridge command queue is corrupt");
			Command command = {};
			Command & source = shared->commands[static_cast<std::uint32_t>(read) % MaximumCommands];
			MemoryBarrier();
			std::memcpy(&command, &source, sizeof(command));
			SecureZeroMemory(&source, sizeof(source));
			atomicWrite(&shared->commandRead, read + 1);
			signalUpdate();
			try
			{
				processCommand(command);
			}
			catch (...)
			{
				SecureZeroMemory(&command, sizeof(command));
				throw;
			}
			SecureZeroMemory(&command, sizeof(command));
			if (shutdownRequested)
				break;
		}
	}

	void Broker::processCommand(Command const & command)
	{
		if (command.uriBytes >= MaximumUriBytes || command.postBytes > MaximumPostBytes)
			throw std::runtime_error("the browser bridge received an oversized command payload");
		if (command.type == CommandShutdown)
		{
			shutdownRequested = true;
			return;
		}
		if (command.type == CommandEnableMemoryCache)
		{
			libMozilla::enableMemoryCache(command.values[0] != 0);
			return;
		}
		if (command.type == CommandEnableDiskCache)
		{
			unsigned const maximum = command.values[1] < 0 ? 0u : static_cast<unsigned>(command.values[1]);
			libMozilla::enableDiskCache(command.values[0] != 0, maximum);
			return;
		}
		if (command.type == CommandSetUserAgent)
		{
			if (command.uriBytes >= MaximumUserAgentBytes)
				throw std::runtime_error("the browser user agent exceeds its transport limit");
			char userAgent[MaximumUserAgentBytes] = {};
			std::memcpy(userAgent, command.uri, command.uriBytes);
			libMozilla::setUserAgent(userAgent);
			return;
		}
		if (command.type == CommandCreateWindow)
		{
			if (command.windowId == 0u || findWindow(command.windowId) || windows.size() >= MaximumWindows ||
				command.values[0] <= 0 || command.values[1] <= 0 ||
				!isValidSurfaceSize(static_cast<std::uint32_t>(command.values[0]),
					static_cast<std::uint32_t>(command.values[1])))
			{
				throw std::runtime_error("the browser bridge received an invalid create-window command");
			}
			std::uint32_t const stateIndex = acquireStateSlot();
			WindowState & state = shared->windows[stateIndex];
			LONG const opened = atomicIncrement(&state.sequence);
			if ((opened & 1) == 0)
				throw std::runtime_error("browser window-state seqlock lost alignment");
			SecureZeroMemory(reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(&state) + sizeof(state.sequence)),
				sizeof(state) - sizeof(state.sequence));
			state.windowId = command.windowId;
			state.lifecycle = WindowCreating;
			state.width = static_cast<std::uint32_t>(command.values[0]);
			state.height = static_cast<std::uint32_t>(command.values[1]);
			MemoryBarrier();
			(void)atomicIncrement(&state.sequence);

			libMozilla::Window * const browserWindow = libMozilla::createWindow(state.width, state.height);
			if (!browserWindow)
			{
				state.lifecycle = WindowFailed;
				throw std::runtime_error("legacy Mozilla could not create an embedded browser window");
			}
			std::unique_ptr<BrokerWindow> browser(new BrokerWindow(*this, command.windowId,
				stateIndex, browserWindow, state.width, state.height));
			browserWindow->setCallback(browser.get());
			browser->publish(true, true, true);
			windows.push_back(std::move(browser));
			return;
		}

		BrokerWindow * const browser = findWindow(command.windowId);
		if (!browser || !browser->window)
			return;
		switch (command.type)
		{
		case CommandDestroyWindow:
		{
			browser->publish(false, false, false, WindowTombstone);
			browser->window->setCallback(nullptr);
			libMozilla::destroyWindow(browser->window);
			browser->window = nullptr;
			auto const found = std::find_if(windows.begin(), windows.end(),
				[browser](std::unique_ptr<BrokerWindow> const & value) { return value.get() == browser; });
			if (found != windows.end())
				windows.erase(found);
			break;
		}
		case CommandSetSize:
			if (command.values[0] > 0 && command.values[1] > 0 &&
				isValidSurfaceSize(static_cast<std::uint32_t>(command.values[0]),
					static_cast<std::uint32_t>(command.values[1])))
			{
				browser->width = static_cast<unsigned>(command.values[0]);
				browser->height = static_cast<unsigned>(command.values[1]);
				browser->window->setSize(browser->width, browser->height);
				browser->surfaceValid = false;
				browser->pixels.clear();
				browser->publish(false, false, false);
			}
			break;
		case CommandSetFocus:
			browser->window->setFocus(command.values[0] != 0);
			break;
		case CommandSetRenderOnComplete:
			browser->window->setRenderOnComplete(command.values[0] != 0);
			break;
		case CommandNavigate:
		{
			if (!isAllowedInternalUri(command.uri, command.uriBytes))
				break;
			std::wstring const uri = utf8ToWide(command.uri, command.uriBytes);
			bool const hasPost = (command.flags & CommandFlagHasPostData) != 0u;
			browser->window->navigateTo(uri.c_str(), hasPost ?
				reinterpret_cast<char const *>(command.postData) : nullptr, command.postBytes);
			break;
		}
		case CommandNavigateStop: browser->window->navigateStop(); break;
		case CommandNavigateBack: browser->window->navigateBack(); break;
		case CommandNavigateForward: browser->window->navigateForward(); break;
		case CommandReload: browser->window->reload(); break;
		case CommandLeftMouseDown: browser->window->onLeftMouseDown(command.values[0], command.values[1], static_cast<unsigned>(command.values[2])); break;
		case CommandLeftMouseUp: browser->window->onLeftMouseUp(command.values[0], command.values[1], static_cast<unsigned>(command.values[2])); break;
		case CommandMiddleMouseDown: browser->window->onMiddleMouseDown(command.values[0], command.values[1], static_cast<unsigned>(command.values[2])); break;
		case CommandMiddleMouseUp: browser->window->onMiddleMouseUp(command.values[0], command.values[1], static_cast<unsigned>(command.values[2])); break;
		case CommandRightMouseDown: browser->window->onRightMouseDown(command.values[0], command.values[1], static_cast<unsigned>(command.values[2])); break;
		case CommandRightMouseUp: browser->window->onRightMouseUp(command.values[0], command.values[1], static_cast<unsigned>(command.values[2])); break;
		case CommandMouseMove: browser->window->onMouseMove(command.values[0], command.values[1], static_cast<unsigned>(command.values[2])); break;
		case CommandMouseWheel: browser->window->onMouseWheel(command.values[0], static_cast<unsigned>(command.values[1])); break;
		case CommandKeyPress: browser->window->onKeyPress(command.values[0], command.values[1], static_cast<unsigned>(command.values[2])); break;
		case CommandBrowserCommand:
			if (command.values[0] >= 0 && command.values[0] < static_cast<std::int32_t>(libMozilla::NUM_COMMANDS))
				browser->window->onCommand(static_cast<libMozilla::Command>(command.values[0]));
			break;
		default:
			break;
		}
	}

	void Broker::publishFrame()
	{
		LONG const active = atomicRead(&shared->activeFrame);
		if (active != 0 && active != 1)
			throw std::runtime_error("browser bridge active frame index is invalid");
		LONG const inactive = active == 0 ? 1 : 0;
		Frame & frame = shared->frames[inactive];
		LONG const opened = atomicIncrement(&frame.sequence);
		if ((opened & 1) == 0)
			throw std::runtime_error("browser frame seqlock lost alignment");
		frame.windowCount = 0u;
		frame.pixelBytes = 0u;
		frame.reserved = 0u;
		SecureZeroMemory(frame.surfaces, sizeof(frame.surfaces));
		for (std::unique_ptr<BrokerWindow> const & browser : windows)
		{
			if (!browser || !browser->window)
				continue;
			browser->render();
			if (!browser->surfaceValid || browser->pixels.empty())
				continue;
			std::uint64_t const surfaceBytes = static_cast<std::uint64_t>(browser->width) *
				browser->height * 4u;
			if (surfaceBytes != browser->pixels.size() || surfaceBytes > MaximumFramePixelBytes ||
				static_cast<std::uint64_t>(frame.pixelBytes) + surfaceBytes > MaximumFramePixelBytes)
			{
				continue;
			}
			SurfaceRecord & record = frame.surfaces[frame.windowCount];
			record.windowId = browser->identifier;
			record.width = browser->width;
			record.height = browser->height;
			record.stride = browser->width * 4u;
			record.bytesPerRow = record.stride;
			record.format = SurfaceFormatBgra8;
			record.surfaceOffset = frame.pixelBytes;
			record.surfaceBytes = static_cast<std::uint32_t>(surfaceBytes);
			std::memcpy(frame.pixels + frame.pixelBytes, browser->pixels.data(), browser->pixels.size());
			frame.pixelBytes += record.surfaceBytes;
			++frame.windowCount;
		}
		MemoryBarrier();
		LONG const closed = atomicIncrement(&frame.sequence);
		if ((closed & 1) != 0)
			throw std::runtime_error("browser frame seqlock did not close");
		atomicWrite(&shared->activeFrame, inactive);
		signalUpdate();
	}

	void Broker::publishFailure(int code, char const * message) const
	{
		if (!shared)
			return;
		shared->failureCode = code;
		(void)strncpy_s(shared->failureMessage, message ? message : "unknown browser broker failure", _TRUNCATE);
		atomicWrite(&shared->lifecycle, LifecycleFailed);
		signalUpdate();
	}

	int run(Options options)
	{
		SharedState * shared = static_cast<SharedState *>(MapViewOfFile(options.mapping,
			FILE_MAP_ALL_ACCESS, 0u, 0u, SharedStateBytes));
		if (!shared)
			throw std::runtime_error("the browser broker could not map its inherited shared memory");
		(void)SetHandleInformation(options.mapping, HANDLE_FLAG_INHERIT, 0u);
		(void)SetHandleInformation(options.commandEvent, HANDLE_FLAG_INHERIT, 0u);
		(void)SetHandleInformation(options.updateEvent, HANDLE_FLAG_INHERIT, 0u);
		(void)SetHandleInformation(options.stopEvent, HANDLE_FLAG_INHERIT, 0u);
		(void)SetHandleInformation(options.parentProcess, HANDLE_FLAG_INHERIT, 0u);

		Broker broker(options, shared);
		HWND hiddenWindow = nullptr;
		bool mozillaInitialized = false;
		try
		{
			if (shared->magic != Magic || shared->version != Version ||
				shared->structureBytes != static_cast<std::uint32_t>(SharedStateBytes) ||
				atomicRead(&shared->lifecycle) != LifecycleStarting)
			{
				throw std::runtime_error("the browser bridge protocol version or layout does not match");
			}
			if (clearUnexpectedIntegrationCredentialEnvironment())
				throw std::runtime_error("the browser broker refused inherited integration-test secrets");
			options.runtimeDirectory = canonicalPath(options.runtimeDirectory);
			verifyIsolatedWritableRuntime(options.runtimeDirectory);
			if (!SetCurrentDirectoryW(options.runtimeDirectory.c_str()) ||
				!SetDllDirectoryW(options.runtimeDirectory.c_str()))
			{
				throw std::runtime_error("the browser broker could not select its isolated runtime directory");
			}
			hiddenWindow = createHiddenWindow();
			std::string const runtimeAnsi = wideToAnsi(options.runtimeDirectory);
			if (!libMozilla::init(hiddenWindow, runtimeAnsi.c_str()))
				throw std::runtime_error("libMozilla rejected initialization of the isolated runtime");
			mozillaInitialized = true;
			// The legacy init function is lazy and otherwise reports success without
			// loading XUL.  A private probe forces the real runtime/link check now.
			libMozilla::Window * const probe = libMozilla::createWindow(8u, 8u);
			if (!probe)
				throw std::runtime_error("legacy Mozilla could not create its startup probe window");
			libMozilla::destroyWindow(probe);

			shared->brokerProcessId = static_cast<std::int32_t>(GetCurrentProcessId());
			atomicWrite(&shared->lifecycle, LifecycleReady);
			broker.signalUpdate();

			HANDLE waitHandles[] = {options.stopEvent, options.parentProcess, options.commandEvent};
			while (!broker.shutdownRequested)
			{
				pumpMessages();
				broker.processCommands();
				if (broker.shutdownRequested)
					break;
				libMozilla::update();
				broker.publishFrame();
				DWORD const wait = MsgWaitForMultipleObjects(3u, waitHandles, FALSE, 16u, QS_ALLINPUT);
				if (wait == WAIT_OBJECT_0 || wait == WAIT_OBJECT_0 + 1u)
					break;
				if (wait == WAIT_FAILED)
					throw std::runtime_error("browser broker wait failed");
			}

			atomicWrite(&shared->lifecycle, LifecycleStopping);
			for (std::unique_ptr<BrokerWindow> & browser : broker.windows)
			{
				if (browser && browser->window)
				{
					browser->window->setCallback(nullptr);
					libMozilla::destroyWindow(browser->window);
					browser->window = nullptr;
				}
			}
			broker.windows.clear();
			libMozilla::release();
			mozillaInitialized = false;
			atomicWrite(&shared->lifecycle, LifecycleStopped);
			broker.signalUpdate();
		}
		catch (std::exception const & error)
		{
			broker.publishFailure(1300, error.what());
			if (mozillaInitialized)
				libMozilla::release();
			if (hiddenWindow)
				DestroyWindow(hiddenWindow);
			SetDllDirectoryW(L"");
			UnmapViewOfFile(shared);
			throw;
		}

		if (hiddenWindow)
			DestroyWindow(hiddenWindow);
		SetDllDirectoryW(L"");
		UnmapViewOfFile(shared);
		return 0;
	}
}

int wmain(int argumentCount, wchar_t * arguments[])
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	try
	{
		return run(parseOptions(argumentCount, arguments));
	}
	catch (std::exception const & error)
	{
		std::fprintf(stderr, "BrowserCompatibilityHost: %s\n", error.what());
		return 1;
	}
	catch (...)
	{
		std::fprintf(stderr, "BrowserCompatibilityHost: unexpected non-standard failure\n");
		return 1;
	}
}
