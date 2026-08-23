#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "libMozilla.h"
#include "BrowserCompatibilityProtocol.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#if !defined(_WIN64)
#error libMozillaProxy.cpp is the 64-bit proxy for BrowserCompatibilityHost.
#endif

namespace libMozilla
{
	namespace
	{
		using namespace BrowserCompatibilityProtocol;
		using BridgeCommand = BrowserCompatibilityProtocol::Command;

		static_assert(sizeof(LONG) == sizeof(std::int32_t), "browser transport atomics changed");
		static_assert(sizeof(wchar_t) == sizeof(std::uint16_t), "Windows UTF-16 ABI changed");

		LONG atomicRead(volatile std::int32_t const * value)
		{
			return InterlockedCompareExchange(
				reinterpret_cast<volatile LONG *>(const_cast<volatile std::int32_t *>(value)), 0, 0);
		}

		void atomicWrite(volatile std::int32_t * value, LONG replacement)
		{
			(void)InterlockedExchange(reinterpret_cast<volatile LONG *>(value), replacement);
		}

		void debugMessage(char const * message)
		{
			OutputDebugStringA("Mozilla x64 bridge: ");
			OutputDebugStringA(message ? message : "unknown error");
			OutputDebugStringA("\n");
		}

		std::wstring ansiToWide(char const * value)
		{
			if (!value || !*value)
				return std::wstring();
			std::size_t const length = std::strlen(value);
			if (length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
				return std::wstring();
			int const required = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, value,
				static_cast<int>(length), nullptr, 0);
			if (required <= 0)
				return std::wstring();
			std::wstring result(static_cast<std::size_t>(required), L'\0');
			if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, value, static_cast<int>(length),
				result.data(), required) != required)
			{
				return std::wstring();
			}
			return result;
		}

		bool wideToUtf8(wchar_t const * value, std::string & result)
		{
			result.clear();
			if (!value)
				return false;
			std::size_t const length = std::wcslen(value);
			if (length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
				return false;
			if (length == 0u)
				return true;
			int const required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
				static_cast<int>(length), nullptr, 0, nullptr, nullptr);
			if (required <= 0 || static_cast<std::uint32_t>(required) >= MaximumUriBytes)
				return false;
			result.resize(static_cast<std::size_t>(required));
			return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, static_cast<int>(length),
				result.data(), required, nullptr, nullptr) == required;
		}

		std::wstring quoteWindowsArgument(std::wstring const & value)
		{
			std::wstring result(1u, L'"');
			std::size_t slashes = 0u;
			for (wchar_t const character : value)
			{
				if (character == L'\\')
				{
					++slashes;
					continue;
				}
				if (character == L'"')
				{
					result.append(slashes * 2u + 1u, L'\\');
					result.push_back(character);
					slashes = 0u;
					continue;
				}
				result.append(slashes, L'\\');
				slashes = 0u;
				result.push_back(character);
			}
			result.append(slashes * 2u, L'\\');
			result.push_back(L'"');
			return result;
		}

		std::wstring handleValue(HANDLE handle)
		{
			return std::to_wstring(static_cast<unsigned long long>(
				reinterpret_cast<std::uintptr_t>(handle)));
		}

		void clearHandleInheritance(HANDLE handle)
		{
			if (handle)
				(void)SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0u);
		}

		bool isIntegrationTestEnvironmentEntry(wchar_t const * entry)
		{
			if (!entry)
				return false;
			wchar_t const prefix[] = L"SWGTCG_TEST_";
			std::size_t const prefixLength = _countof(prefix) - 1u;
			wchar_t const * const separator = std::wcschr(entry, L'=');
			return separator && separator >= entry + prefixLength &&
				_wcsnicmp(entry, prefix, prefixLength) == 0;
		}

		std::vector<wchar_t> buildSanitizedEnvironmentBlock()
		{
			std::vector<wchar_t> block;
			LPWCH const environment = GetEnvironmentStringsW();
			if (!environment)
				return block;

			for (wchar_t const * current = environment; *current;
				current += std::wcslen(current) + 1u)
			{
				if (isIntegrationTestEnvironmentEntry(current))
					continue;
				std::size_t const characters = std::wcslen(current) + 1u;
				block.insert(block.end(), current, current + characters);
			}
			FreeEnvironmentStringsW(environment);

			// CreateProcess requires a double-NUL-terminated Unicode environment.
			// A normal process environment is nonempty, but retain a valid block if
			// an unusually constrained parent supplied only filtered entries.
			if (block.empty())
				block.push_back(L'\0');
			block.push_back(L'\0');
			return block;
		}

		class SecureEnvironmentBlock
		{
		public:
			explicit SecureEnvironmentBlock(std::vector<wchar_t> && value)
			: m_value(std::move(value))
			{
			}

			~SecureEnvironmentBlock()
			{
				if (!m_value.empty())
					SecureZeroMemory(m_value.data(), m_value.size() * sizeof(wchar_t));
			}

			SecureEnvironmentBlock(SecureEnvironmentBlock const &) = delete;
			SecureEnvironmentBlock & operator=(SecureEnvironmentBlock const &) = delete;

			bool empty() const { return m_value.empty(); }
			wchar_t * data() { return m_value.data(); }

		private:
			std::vector<wchar_t> m_value;
		};

		bool copyWindowState(WindowState const & source, WindowState & destination)
		{
			for (int attempt = 0; attempt != 4; ++attempt)
			{
				LONG const before = atomicRead(&source.sequence);
				if ((before & 1) != 0)
					continue;
				MemoryBarrier();
				std::memcpy(&destination, &source, sizeof(destination));
				MemoryBarrier();
				LONG const after = atomicRead(&source.sequence);
				if (before == after && (after & 1) == 0)
					return true;
			}
			return false;
		}

		bool s_memoryCacheEnabled = true;
		bool s_diskCacheEnabled = true;
		unsigned s_diskCacheKilobytes = 10u * 1024u;
		char s_userAgent[MaximumUserAgentBytes] = {};
	}

	class WindowImpl;

	class Manager
	{
	public:
		Manager();
		~Manager();

		bool start(char const * applicationDirectory);
		void update();
		void shutdown();
		Window * createWindow(unsigned width, unsigned height);
		void destroyWindow(Window * window);
		bool queue(BridgeCommand const & command, bool discardable = false);
		bool render(std::uint32_t windowId, IBlitter * blitter);
		bool isGameThread() const { return GetCurrentThreadId() == m_gameThreadId; }
		bool isReady() const;
		void sendMemoryCache(bool enabled);
		void sendDiskCache(bool enabled, unsigned maximumKilobytes);
		void sendUserAgent(char const * userAgent);

	private:
		void closeTransport();
		void retireAllWindows();
		Window * findWindow(std::uint32_t windowId) const;

		DWORD m_gameThreadId;
		std::uint32_t m_nextWindowId;
		HANDLE m_mapping;
		SharedState * m_shared;
		HANDLE m_commandEvent;
		HANDLE m_updateEvent;
		HANDLE m_stopEvent;
		HANDLE m_process;
		HANDLE m_job;
		std::vector<Window *> m_windows;
	};

	class WindowImpl
	{
	public:
		WindowImpl(Window * owner, Manager * manager, std::uint32_t identifier,
			unsigned width, unsigned height);

		std::uint32_t identifier() const { return m_identifier; }
		bool active() const { return m_active; }
		void retire();
		void applyState(WindowState const & state);
		bool send(CommandType type, std::int32_t value0 = 0, std::int32_t value1 = 0,
			std::int32_t value2 = 0, bool discardable = false);
		bool navigate(wchar_t const * uri, char const * postData, unsigned postBytes);
		bool render(IBlitter * blitter);

		Window * m_owner;
		Manager * m_manager;
		std::uint32_t m_identifier;
		bool m_active;
		unsigned m_width;
		unsigned m_height;
		bool m_loading;
		float m_progress;
		bool m_canBack;
		bool m_canForward;
		bool m_hasCaret;
		int m_caretX;
		int m_caretY;
		int m_caretWidth;
		int m_caretHeight;
		std::uint32_t m_uriGeneration;
		std::uint32_t m_statusGeneration;
		std::uint32_t m_progressGeneration;
		char m_uri[MaximumUriBytes];
		wchar_t m_status[MaximumStatusUnits];
		ICallback * m_callback;
	};

	namespace
	{
		Manager * s_manager = nullptr;
		// UI code holds raw Window pointers.  Destroyed proxy objects therefore
		// remain inert process-lifetime tombstones instead of becoming UAF hazards.
		std::vector<Window *> s_processLifetimeTombstones;
	}

	Manager::Manager()
	: m_gameThreadId(GetCurrentThreadId())
	, m_nextWindowId(1u)
	, m_mapping(nullptr)
	, m_shared(nullptr)
	, m_commandEvent(nullptr)
	, m_updateEvent(nullptr)
	, m_stopEvent(nullptr)
	, m_process(nullptr)
	, m_job(nullptr)
	{
	}

	Manager::~Manager()
	{
		shutdown();
	}

	bool Manager::start(char const * applicationDirectory)
	{
		if (!isGameThread())
			return false;
		std::wstring const runtimeDirectory = ansiToWide(applicationDirectory);
		if (runtimeDirectory.empty())
		{
			debugMessage("the isolated Mozilla broker directory is empty or invalid");
			return false;
		}
		std::wstring const hostExecutable = runtimeDirectory + L"\\BrowserCompatibilityHost.exe";
		DWORD const attributes = GetFileAttributesW(hostExecutable.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u)
		{
			debugMessage("BrowserCompatibilityHost.exe is missing from runtime\\mozilla-broker");
			return false;
		}

		SECURITY_ATTRIBUTES inheritable = {};
		inheritable.nLength = sizeof(inheritable);
		inheritable.bInheritHandle = TRUE;
		std::uint64_t const mappingBytes = static_cast<std::uint64_t>(SharedStateBytes);
		m_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &inheritable, PAGE_READWRITE,
			static_cast<DWORD>(mappingBytes >> 32u), static_cast<DWORD>(mappingBytes), nullptr);
		m_commandEvent = CreateEventW(&inheritable, FALSE, FALSE, nullptr);
		m_updateEvent = CreateEventW(&inheritable, FALSE, FALSE, nullptr);
		m_stopEvent = CreateEventW(&inheritable, TRUE, FALSE, nullptr);
		HANDLE parentProcess = OpenProcess(SYNCHRONIZE, TRUE, GetCurrentProcessId());
		if (!m_mapping || !m_commandEvent || !m_updateEvent || !m_stopEvent || !parentProcess)
		{
			debugMessage("could not create the browser bridge's unnamed inherited handles");
			if (parentProcess)
				CloseHandle(parentProcess);
			closeTransport();
			return false;
		}

		m_shared = static_cast<SharedState *>(MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS,
			0u, 0u, SharedStateBytes));
		if (!m_shared)
		{
			CloseHandle(parentProcess);
			closeTransport();
			return false;
		}
		SecureZeroMemory(m_shared, SharedStateBytes);
		m_shared->magic = Magic;
		m_shared->version = Version;
		m_shared->structureBytes = static_cast<std::uint32_t>(SharedStateBytes);
		atomicWrite(&m_shared->lifecycle, LifecycleStarting);
		atomicWrite(&m_shared->activeFrame, 0);

		m_job = CreateJobObjectW(nullptr, nullptr);
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!m_job || !SetInformationJobObject(m_job, JobObjectExtendedLimitInformation,
			&limits, sizeof(limits)))
		{
			debugMessage("mandatory browser broker job containment could not be created");
			CloseHandle(parentProcess);
			closeTransport();
			return false;
		}

		std::wstring commandLine = quoteWindowsArgument(hostExecutable) +
			L" --mapping " + handleValue(m_mapping) +
			L" --command-event " + handleValue(m_commandEvent) +
			L" --update-event " + handleValue(m_updateEvent) +
			L" --stop-event " + handleValue(m_stopEvent) +
			L" --parent " + handleValue(parentProcess) +
			L" --runtime " + quoteWindowsArgument(runtimeDirectory);
		std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
		mutableCommandLine.push_back(L'\0');
		SecureEnvironmentBlock childEnvironment(buildSanitizedEnvironmentBlock());
		if (childEnvironment.empty())
		{
			debugMessage("could not build the browser broker's sanitized environment");
			CloseHandle(parentProcess);
			closeTransport();
			return false;
		}

		SIZE_T attributeBytes = 0u;
		(void)InitializeProcThreadAttributeList(nullptr, 1u, 0u, &attributeBytes);
		if (attributeBytes == 0u)
		{
			debugMessage("could not size the browser broker handle allowlist");
			CloseHandle(parentProcess);
			closeTransport();
			return false;
		}
		std::vector<std::uint8_t> attributeStorage(attributeBytes, 0u);
		STARTUPINFOEXW startup = {};
		startup.StartupInfo.cb = sizeof(startup);
		startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
		if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1u, 0u, &attributeBytes))
		{
			CloseHandle(parentProcess);
			closeTransport();
			return false;
		}
		HANDLE inheritedHandles[] = {m_mapping, m_commandEvent, m_updateEvent, m_stopEvent, parentProcess};
		BOOL const allowlistReady = UpdateProcThreadAttribute(startup.lpAttributeList, 0u,
			PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inheritedHandles, sizeof(inheritedHandles), nullptr, nullptr);
		if (!allowlistReady)
		{
			DeleteProcThreadAttributeList(startup.lpAttributeList);
			CloseHandle(parentProcess);
			closeTransport();
			return false;
		}

		PROCESS_INFORMATION processInformation = {};
		BOOL const created = CreateProcessW(hostExecutable.c_str(), mutableCommandLine.data(), nullptr,
			nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT |
			EXTENDED_STARTUPINFO_PRESENT, childEnvironment.data(), runtimeDirectory.c_str(),
			&startup.StartupInfo, &processInformation);
		DeleteProcThreadAttributeList(startup.lpAttributeList);
		clearHandleInheritance(m_mapping);
		clearHandleInheritance(m_commandEvent);
		clearHandleInheritance(m_updateEvent);
		clearHandleInheritance(m_stopEvent);
		clearHandleInheritance(parentProcess);
		CloseHandle(parentProcess);

		if (!created)
		{
			debugMessage("CreateProcess(BrowserCompatibilityHost) failed");
			closeTransport();
			return false;
		}
		m_process = processInformation.hProcess;
		if (!AssignProcessToJobObject(m_job, m_process))
		{
			debugMessage("browser broker could not be contained before execution");
			(void)TerminateProcess(m_process, 1201u);
			(void)WaitForSingleObject(m_process, 5000u);
			CloseHandle(processInformation.hThread);
			closeTransport();
			return false;
		}
		if (ResumeThread(processInformation.hThread) == static_cast<DWORD>(-1))
		{
			(void)TerminateProcess(m_process, 1202u);
			(void)WaitForSingleObject(m_process, 5000u);
			CloseHandle(processInformation.hThread);
			closeTransport();
			return false;
		}
		CloseHandle(processInformation.hThread);

		ULONGLONG const deadline = GetTickCount64() + 30000u;
		for (;;)
		{
			LONG const lifecycle = atomicRead(&m_shared->lifecycle);
			if (lifecycle == LifecycleReady)
				break;
			if (lifecycle == LifecycleFailed || lifecycle == LifecycleStopped)
			{
				debugMessage(m_shared->failureMessage[0] ? m_shared->failureMessage :
					"browser compatibility host failed during startup");
				shutdown();
				return false;
			}
			ULONGLONG const now = GetTickCount64();
			if (now >= deadline)
			{
				debugMessage("browser compatibility host startup timed out");
				shutdown();
				return false;
			}
			HANDLE waits[] = {m_updateEvent, m_process};
			DWORD const remaining = static_cast<DWORD>(std::min<ULONGLONG>(deadline - now, 1000u));
			DWORD const wait = WaitForMultipleObjects(2u, waits, FALSE, remaining);
			if (wait == WAIT_OBJECT_0 + 1u || wait == WAIT_FAILED)
			{
				if (atomicRead(&m_shared->lifecycle) == LifecycleFailed && m_shared->failureMessage[0])
					debugMessage(m_shared->failureMessage);
				else
				{
					DWORD exitCode = 0u;
					char diagnostic[160] = {};
					if (GetExitCodeProcess(m_process, &exitCode))
					{
						(void)sprintf_s(diagnostic, "browser compatibility host exited before becoming ready (exit=%lu)",
							static_cast<unsigned long>(exitCode));
						debugMessage(diagnostic);
					}
					else
						debugMessage("browser compatibility host exited before becoming ready");
				}
				shutdown();
				return false;
			}
		}

		sendMemoryCache(s_memoryCacheEnabled);
		sendDiskCache(s_diskCacheEnabled, s_diskCacheKilobytes);
		sendUserAgent(s_userAgent);
		return true;
	}

	bool Manager::isReady() const
	{
		return m_shared && m_process && WaitForSingleObject(m_process, 0u) == WAIT_TIMEOUT &&
			atomicRead(&m_shared->lifecycle) == LifecycleReady;
	}

	bool Manager::queue(BridgeCommand const & command, bool discardable)
	{
		if (!isGameThread() || !isReady())
			return false;
		ULONGLONG const deadline = GetTickCount64() + (discardable ? 0u : 20u);
		for (;;)
		{
			LONG const write = atomicRead(&m_shared->commandWrite);
			LONG const read = atomicRead(&m_shared->commandRead);
			if (write < 0 || read < 0 || write < read ||
				write - read > static_cast<LONG>(MaximumCommands))
			{
				return false;
			}
			if (write - read < static_cast<LONG>(MaximumCommands))
			{
				BridgeCommand & destination = m_shared->commands[
					static_cast<std::uint32_t>(write) % MaximumCommands];
				std::memcpy(&destination, &command, sizeof(command));
				MemoryBarrier();
				atomicWrite(&m_shared->commandWrite, write + 1);
				(void)SetEvent(m_commandEvent);
				return true;
			}
			if (discardable || GetTickCount64() >= deadline)
				return false;
			(void)WaitForSingleObject(m_updateEvent, 1u);
		}
	}

	Window * Manager::createWindow(unsigned width, unsigned height)
	{
		if (!isGameThread() || !isReady() || !isValidSurfaceSize(width, height) ||
			m_windows.size() >= MaximumWindows)
		{
			return nullptr;
		}
		std::uint32_t const identifier = m_nextWindowId++;
		if (identifier == 0u)
			return nullptr;
		Window * const window = new Window;
		window->m_pImpl = new WindowImpl(window, this, identifier, width, height);
		BridgeCommand command = {};
		command.type = CommandCreateWindow;
		command.windowId = identifier;
		command.values[0] = static_cast<std::int32_t>(width);
		command.values[1] = static_cast<std::int32_t>(height);
		if (!queue(command))
		{
			delete window;
			return nullptr;
		}
		m_windows.push_back(window);
		return window;
	}

	void Manager::destroyWindow(Window * window)
	{
		if (!window || !window->m_pImpl || !isGameThread())
			return;
		auto const found = std::find(m_windows.begin(), m_windows.end(), window);
		if (found == m_windows.end())
			return;
		BridgeCommand command = {};
		command.type = CommandDestroyWindow;
		command.windowId = window->m_pImpl->identifier();
		(void)queue(command);
		window->m_pImpl->retire();
		s_processLifetimeTombstones.push_back(window);
		m_windows.erase(found);
	}

	Window * Manager::findWindow(std::uint32_t windowId) const
	{
		for (Window * const window : m_windows)
			if (window && window->m_pImpl && window->m_pImpl->identifier() == windowId)
				return window;
		return nullptr;
	}

	void Manager::update()
	{
		if (!isGameThread() || !isReady())
			return;
		std::vector<Window *> const windows(m_windows);
		for (Window * const window : windows)
		{
			if (!window || !window->m_pImpl || !window->m_pImpl->active())
				continue;
			for (std::uint32_t index = 0u; index < MaximumWindows; ++index)
			{
				WindowState state = {};
				if (!copyWindowState(m_shared->windows[index], state) ||
					state.windowId != window->m_pImpl->identifier())
				{
					continue;
				}
				window->m_pImpl->applyState(state);
				break;
			}
		}
	}

	bool Manager::render(std::uint32_t windowId, IBlitter * blitter)
	{
		if (!blitter || !isGameThread() || !isReady())
			return false;
		for (int attempt = 0; attempt != 4; ++attempt)
		{
			LONG const frameIndex = atomicRead(&m_shared->activeFrame);
			if (frameIndex < 0 || frameIndex > 1)
				return false;
			Frame const & frame = m_shared->frames[frameIndex];
			LONG const before = atomicRead(&frame.sequence);
			if ((before & 1) != 0)
				continue;
			MemoryBarrier();
			if (frame.windowCount > MaximumWindows || frame.pixelBytes > MaximumFramePixelBytes)
				return false;
			SurfaceRecord selected = {};
			bool found = false;
			for (std::uint32_t index = 0u; index < frame.windowCount; ++index)
			{
				if (frame.surfaces[index].windowId == windowId)
				{
					selected = frame.surfaces[index];
					found = true;
					break;
				}
			}
			if (!found || selected.format != SurfaceFormatBgra8 || selected.width == 0u ||
				selected.height == 0u || selected.width > MaximumSurfaceDimension ||
				selected.height > MaximumSurfaceDimension || selected.stride != selected.width * 4u ||
				selected.bytesPerRow != selected.width * 4u)
			{
				return false;
			}
			std::uint64_t const expected = static_cast<std::uint64_t>(selected.stride) * selected.height;
			if (expected != selected.surfaceBytes || selected.surfaceOffset > frame.pixelBytes ||
				selected.surfaceBytes > frame.pixelBytes - selected.surfaceOffset)
			{
				return false;
			}
			std::vector<std::uint8_t> pixels(selected.surfaceBytes);
			std::memcpy(pixels.data(), frame.pixels + selected.surfaceOffset, pixels.size());
			MemoryBarrier();
			LONG const after = atomicRead(&frame.sequence);
			if (before != after || (after & 1) != 0)
				continue;
			(*blitter)(pixels.data(), selected.width, selected.height, selected.stride,
				selected.bytesPerRow);
			return true;
		}
		return false;
	}

	void Manager::sendMemoryCache(bool enabled)
	{
		BridgeCommand command = {};
		command.type = CommandEnableMemoryCache;
		command.values[0] = enabled ? 1 : 0;
		(void)queue(command);
	}

	void Manager::sendDiskCache(bool enabled, unsigned maximumKilobytes)
	{
		BridgeCommand command = {};
		command.type = CommandEnableDiskCache;
		command.values[0] = enabled ? 1 : 0;
		command.values[1] = maximumKilobytes > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max())
			? std::numeric_limits<std::int32_t>::max() : static_cast<std::int32_t>(maximumKilobytes);
		(void)queue(command);
	}

	void Manager::sendUserAgent(char const * userAgent)
	{
		BridgeCommand command = {};
		command.type = CommandSetUserAgent;
		char const * const source = userAgent ? userAgent : "";
		std::size_t const length = strnlen_s(source, MaximumUserAgentBytes);
		if (length >= MaximumUserAgentBytes)
			return;
		command.uriBytes = static_cast<std::uint32_t>(length);
		if (length)
			std::memcpy(command.uri, source, length);
		(void)queue(command);
	}

	void Manager::retireAllWindows()
	{
		for (Window * const window : m_windows)
		{
			if (window && window->m_pImpl)
				window->m_pImpl->retire();
			s_processLifetimeTombstones.push_back(window);
		}
		m_windows.clear();
	}

	void Manager::shutdown()
	{
		if (m_shared && m_process && WaitForSingleObject(m_process, 0u) == WAIT_TIMEOUT)
		{
			if (atomicRead(&m_shared->lifecycle) == LifecycleReady)
			{
				BridgeCommand command = {};
				command.type = CommandShutdown;
				(void)queue(command);
			}
			(void)SetEvent(m_stopEvent);
			if (WaitForSingleObject(m_process, 5000u) == WAIT_TIMEOUT)
			{
				(void)TerminateProcess(m_process, 1203u);
				(void)WaitForSingleObject(m_process, 5000u);
			}
		}
		retireAllWindows();
		closeTransport();
	}

	void Manager::closeTransport()
	{
		if (m_process)
		{
			CloseHandle(m_process);
			m_process = nullptr;
		}
		if (m_job)
		{
			CloseHandle(m_job);
			m_job = nullptr;
		}
		if (m_shared)
		{
			UnmapViewOfFile(m_shared);
			m_shared = nullptr;
		}
		if (m_mapping)
		{
			CloseHandle(m_mapping);
			m_mapping = nullptr;
		}
		if (m_commandEvent)
		{
			CloseHandle(m_commandEvent);
			m_commandEvent = nullptr;
		}
		if (m_updateEvent)
		{
			CloseHandle(m_updateEvent);
			m_updateEvent = nullptr;
		}
		if (m_stopEvent)
		{
			CloseHandle(m_stopEvent);
			m_stopEvent = nullptr;
		}
	}

	WindowImpl::WindowImpl(Window * owner, Manager * manager, std::uint32_t identifier,
		unsigned width, unsigned height)
	: m_owner(owner)
	, m_manager(manager)
	, m_identifier(identifier)
	, m_active(true)
	, m_width(width)
	, m_height(height)
	, m_loading(false)
	, m_progress(0.0f)
	, m_canBack(false)
	, m_canForward(false)
	, m_hasCaret(false)
	, m_caretX(0)
	, m_caretY(0)
	, m_caretWidth(0)
	, m_caretHeight(0)
	, m_uriGeneration(0u)
	, m_statusGeneration(0u)
	, m_progressGeneration(0u)
	, m_callback(nullptr)
	{
		m_uri[0] = '\0';
		m_status[0] = L'\0';
	}

	void WindowImpl::retire()
	{
		m_active = false;
		m_manager = nullptr;
		m_callback = nullptr;
		m_loading = false;
	}

	void WindowImpl::applyState(WindowState const & state)
	{
		if (!m_active || state.windowId != m_identifier)
			return;
		if (state.lifecycle == WindowTombstone || state.lifecycle == WindowFailed)
		{
			retire();
			return;
		}
		m_width = state.width;
		m_height = state.height;
		m_canBack = state.canNavigateBack != 0u;
		m_canForward = state.canNavigateForward != 0u;
		m_loading = state.isLoading != 0u;
		std::memcpy(&m_progress, &state.progressBits, sizeof(m_progress));
		m_hasCaret = state.hasCaret != 0u;
		m_caretX = state.caretX;
		m_caretY = state.caretY;
		m_caretWidth = state.caretWidth;
		m_caretHeight = state.caretHeight;
		if (state.uriBytes < MaximumUriBytes)
		{
			std::memcpy(m_uri, state.uri, state.uriBytes);
			m_uri[state.uriBytes] = '\0';
		}
		else
			m_uri[0] = '\0';
		for (std::uint32_t index = 0u; index < MaximumStatusUnits; ++index)
		{
			m_status[index] = static_cast<wchar_t>(state.status[index]);
			if (state.status[index] == 0u)
				break;
			if (index + 1u == MaximumStatusUnits)
				m_status[index] = L'\0';
		}

		ICallback * const callback = m_callback;
		if (state.uriGeneration != m_uriGeneration)
		{
			m_uriGeneration = state.uriGeneration;
			if (callback && m_active)
				callback->onURIChanged(m_owner);
		}
		if (state.statusGeneration != m_statusGeneration)
		{
			m_statusGeneration = state.statusGeneration;
			if (callback && m_active)
				callback->onStatusChanged(m_owner);
		}
		if (state.progressGeneration != m_progressGeneration)
		{
			m_progressGeneration = state.progressGeneration;
			if (callback && m_active)
				callback->onProgressChanged(m_owner);
		}
	}

	bool WindowImpl::send(CommandType type, std::int32_t value0, std::int32_t value1,
		std::int32_t value2, bool discardable)
	{
		if (!m_active || !m_manager)
			return false;
		BridgeCommand command = {};
		command.type = type;
		command.windowId = m_identifier;
		command.values[0] = value0;
		command.values[1] = value1;
		command.values[2] = value2;
		return m_manager->queue(command, discardable);
	}

	bool WindowImpl::navigate(wchar_t const * uri, char const * postData, unsigned postBytes)
	{
		if (!m_active || !m_manager || (!postData && postBytes != 0u) || postBytes > MaximumPostBytes)
			return false;
		std::string encodedUri;
		if (!wideToUtf8(uri, encodedUri) || encodedUri.empty())
			return false;
		if (m_callback && !m_callback->doValidateURI(m_owner, encodedUri.c_str()))
			return false;
		BridgeCommand command = {};
		command.type = CommandNavigate;
		command.windowId = m_identifier;
		command.uriBytes = static_cast<std::uint32_t>(encodedUri.size());
		std::memcpy(command.uri, encodedUri.data(), encodedUri.size());
		if (postData)
		{
			command.flags |= CommandFlagHasPostData;
			command.postBytes = postBytes;
			if (postBytes)
				std::memcpy(command.postData, postData, postBytes);
		}
		return m_manager->queue(command);
	}

	bool WindowImpl::render(IBlitter * blitter)
	{
		return m_active && m_manager && m_manager->render(m_identifier, blitter);
	}

	bool init(void * nativeWindow, char const * applicationDirectory)
	{
		(void)nativeWindow;
		if (s_manager || !applicationDirectory || !*applicationDirectory)
			return false;
		Manager * const manager = new Manager;
		if (!manager->start(applicationDirectory))
		{
			delete manager;
			return false;
		}
		s_manager = manager;
		return true;
	}

	void update()
	{
		if (s_manager)
			s_manager->update();
	}

	void release()
	{
		delete s_manager;
		s_manager = nullptr;
	}

	void enableMemoryCache(bool enabled)
	{
		s_memoryCacheEnabled = enabled;
		if (s_manager)
			s_manager->sendMemoryCache(enabled);
	}

	void enableDiskCache(bool enabled, unsigned maximumKilobytes)
	{
		s_diskCacheEnabled = enabled;
		s_diskCacheKilobytes = maximumKilobytes;
		if (s_manager)
			s_manager->sendDiskCache(enabled, maximumKilobytes);
	}

	void setUserAgent(char const * userAgent)
	{
		char const * const source = userAgent ? userAgent : "";
		(void)strncpy_s(s_userAgent, source, _TRUNCATE);
		if (s_manager)
			s_manager->sendUserAgent(s_userAgent);
	}

	Window * createWindow(unsigned width, unsigned height)
	{
		return s_manager ? s_manager->createWindow(width, height) : nullptr;
	}

	void destroyWindow(Window * window)
	{
		if (s_manager)
			s_manager->destroyWindow(window);
	}

	Window::Window()
	: m_pImpl(nullptr)
	{
	}

	Window::~Window()
	{
		delete m_pImpl;
		m_pImpl = nullptr;
	}

	void Window::setSize(unsigned width, unsigned height)
	{
		if (m_pImpl && isValidSurfaceSize(width, height))
		{
			m_pImpl->m_width = width;
			m_pImpl->m_height = height;
			(void)m_pImpl->send(CommandSetSize, static_cast<std::int32_t>(width),
				static_cast<std::int32_t>(height));
		}
	}

	void Window::setFocus(bool focused)
	{
		if (m_pImpl)
			(void)m_pImpl->send(CommandSetFocus, focused ? 1 : 0);
	}

	void Window::setCallback(ICallback * callback)
	{
		if (m_pImpl && m_pImpl->active())
			m_pImpl->m_callback = callback;
	}

	void Window::setRenderOnComplete(bool enabled)
	{
		if (m_pImpl)
			(void)m_pImpl->send(CommandSetRenderOnComplete, enabled ? 1 : 0);
	}

	float Window::getProgress(bool & loading)
	{
		loading = m_pImpl && m_pImpl->active() ? m_pImpl->m_loading : false;
		return m_pImpl && m_pImpl->active() ? m_pImpl->m_progress : 0.0f;
	}

	wchar_t const * Window::getStatus() const
	{
		static wchar_t const empty[] = L"";
		return m_pImpl && m_pImpl->active() ? m_pImpl->m_status : empty;
	}

	char const * Window::getURI() const
	{
		return m_pImpl && m_pImpl->active() ? m_pImpl->m_uri : "";
	}

	bool Window::getCaret(int & x, int & y, int & width, int & height)
	{
		if (!m_pImpl || !m_pImpl->active() || !m_pImpl->m_hasCaret)
			return false;
		x = m_pImpl->m_caretX;
		y = m_pImpl->m_caretY;
		width = m_pImpl->m_caretWidth;
		height = m_pImpl->m_caretHeight;
		return true;
	}

	void Window::navigateTo(wchar_t const * uri, char const * postData, unsigned postDataLength)
	{
		if (m_pImpl)
			(void)m_pImpl->navigate(uri, postData, postDataLength);
	}

	void Window::navigateStop() { if (m_pImpl) (void)m_pImpl->send(CommandNavigateStop); }
	bool Window::canNavigateBack() { return m_pImpl && m_pImpl->active() && m_pImpl->m_canBack; }
	void Window::navigateBack() { if (m_pImpl) (void)m_pImpl->send(CommandNavigateBack); }
	bool Window::canNavigateForward() { return m_pImpl && m_pImpl->active() && m_pImpl->m_canForward; }
	void Window::navigateForward() { if (m_pImpl) (void)m_pImpl->send(CommandNavigateForward); }
	void Window::reload() { if (m_pImpl) (void)m_pImpl->send(CommandReload); }

	void Window::onLeftMouseDown(int x, int y, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandLeftMouseDown, x, y, static_cast<std::int32_t>(flags)); }
	void Window::onLeftMouseUp(int x, int y, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandLeftMouseUp, x, y, static_cast<std::int32_t>(flags)); }
	void Window::onMiddleMouseDown(int x, int y, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMiddleMouseDown, x, y, static_cast<std::int32_t>(flags)); }
	void Window::onMiddleMouseUp(int x, int y, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMiddleMouseUp, x, y, static_cast<std::int32_t>(flags)); }
	void Window::onRightMouseDown(int x, int y, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandRightMouseDown, x, y, static_cast<std::int32_t>(flags)); }
	void Window::onRightMouseUp(int x, int y, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandRightMouseUp, x, y, static_cast<std::int32_t>(flags)); }
	void Window::onMouseMove(int x, int y, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouseMove, x, y, static_cast<std::int32_t>(flags), true); }
	void Window::onMouseWheel(int ticks, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouseWheel, ticks, static_cast<std::int32_t>(flags)); }
	void Window::onKeyPress(int character, int key, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandKeyPress, character, key, static_cast<std::int32_t>(flags)); }

	void Window::onCommand(Command command)
	{
		if (m_pImpl && static_cast<unsigned>(command) < static_cast<unsigned>(NUM_COMMANDS))
			(void)m_pImpl->send(CommandBrowserCommand, static_cast<std::int32_t>(command));
	}

	bool Window::render(IBlitter * blitter)
	{
		return m_pImpl && m_pImpl->render(blitter);
	}
}
