#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "libEverQuestTCG.h"
#include "TcgCompatibilityProtocol.h"

#include <windows.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#if !defined(_WIN64)
#error libEverQuestTCG_x64.cpp is the 64-bit proxy for the Win32 TCG compatibility host.
#endif

namespace libEverQuestTCG
{
	namespace
	{
		using namespace TcgCompatibilityProtocol;

		class SecureString
		{
		public:
			SecureString() = default;
			~SecureString()
			{
				clear();
			}

			SecureString(SecureString const &) = delete;
			SecureString & operator=(SecureString const &) = delete;

			bool assign(char const * value, std::size_t maximumLength, bool * changed = nullptr)
			{
				char const * const source = value ? value : "";
				std::size_t const length = strnlen_s(source, maximumLength + 1u);
				if (length > maximumLength)
				{
					if (changed)
						*changed = !empty();
					clear();
					return false;
				}

				std::size_t const currentLength = empty() ? 0u : m_value.size() - 1u;
				bool const valueChanged = currentLength != length ||
					(length != 0u && std::memcmp(m_value.data(), source, length) != 0);
				if (changed)
					*changed = valueChanged;
				if (!valueChanged)
					return true;

				clear();
				m_value.assign(source, source + length);
				m_value.push_back('\0');
				return true;
			}

			void reset()
			{
				clear();
			}

			bool empty() const
			{
				return m_value.empty() || m_value[0] == '\0';
			}

			char const * c_str() const
			{
				return m_value.empty() ? "" : m_value.data();
			}

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

		struct CallbackTable
		{
			NavigateProc navigateProc = nullptr;
			NavigateWithPostDataProc navigateWithPostDataProc = nullptr;
			PlaySoundProc playSoundProc = nullptr;
			PlayMusicProc playMusicProc = nullptr;
			SetSoundVolumeProc setSoundVolumeProc = nullptr;
			SetMusicVolumeProc setMusicVolumeProc = nullptr;
			StopAllSoundsProc stopAllSoundsProc = nullptr;
			SetWindowStateProc setWindowStateProc = nullptr;
		};

		std::string s_applicationDirectory;
		HWND s_desktopWindow = nullptr;
		Language s_language = LANG_English;
		HostProcessType s_hostProcessType = HPT_StarWarsGalaxies;
		RealmType s_realmType = REALM_Live;
		SecureString s_userName;
		SecureString s_sessionId;
		SecureString s_challenge;
		SecureString s_characterName;
		bool s_userNameValid = true;
		bool s_sessionIdValid = true;
		bool s_challengeValid = true;
		bool s_characterNameValid = true;
		bool s_challengeFounder = false;
		bool s_startTutorial = false;
		std::uint64_t s_launchIdentityGeneration = 1u;
		CallbackTable s_callbacks;

		void noteLaunchIdentityChanged()
		{
			++s_launchIdentityGeneration;
			if (s_launchIdentityGeneration == 0u)
				++s_launchIdentityGeneration;
		}

		std::wstring ansiToWide(char const * value)
		{
			if (!value || !*value)
				return std::wstring();
			std::size_t const valueLength = std::strlen(value);
			if (valueLength > static_cast<std::size_t>(INT_MAX))
				return std::wstring();

			int const required = MultiByteToWideChar(CP_ACP, 0, value, static_cast<int>(valueLength), nullptr, 0);
			if (required <= 0)
				return std::wstring();

			std::wstring result(static_cast<std::size_t>(required), L'\0');
			if (MultiByteToWideChar(CP_ACP, 0, value, static_cast<int>(valueLength), result.data(), required) != required)
			{
				SecureZeroMemory(result.data(), result.size() * sizeof(wchar_t));
				return std::wstring();
			}
			return result;
		}

		std::wstring quoteWindowsArgument(std::wstring const & value)
		{
			std::wstring result(1, L'"');
			std::size_t backslashes = 0;
			for (wchar_t const character : value)
			{
				if (character == L'\\')
				{
					++backslashes;
					continue;
				}

				if (character == L'"')
				{
					result.append(backslashes * 2u + 1u, L'\\');
					result.push_back(L'"');
					backslashes = 0;
					continue;
				}

				result.append(backslashes, L'\\');
				backslashes = 0;
				result.push_back(character);
			}
			result.append(backslashes * 2u, L'\\');
			result.push_back(L'"');
			return result;
		}

		bool startsWithEnvironmentName(std::wstring const & entry, wchar_t const * name)
		{
			std::size_t const nameLength = std::wcslen(name);
			return entry.size() > nameLength && entry[nameLength] == L'=' &&
				_wcsnicmp(entry.c_str(), name, nameLength) == 0;
		}

		bool isIntegrationTestEnvironmentEntry(std::wstring const & entry)
		{
			wchar_t const prefix[] = L"SWGTCG_TEST_";
			std::size_t const prefixLength = _countof(prefix) - 1u;
			std::wstring::size_type const separator = entry.find(L'=');
			return separator != std::wstring::npos && separator >= prefixLength &&
				_wcsnicmp(entry.c_str(), prefix, prefixLength) == 0;
		}

		void secureClearWideString(std::wstring & value)
		{
			if (!value.empty())
				SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t));
			value.clear();
			value.shrink_to_fit();
		}

		void secureClearEnvironmentEntries(std::vector<std::wstring> & entries)
		{
			for (std::wstring & entry : entries)
				secureClearWideString(entry);
			entries.clear();
			entries.shrink_to_fit();
		}

		void secureClearEnvironmentAdditions(std::vector<std::pair<std::wstring, std::wstring>> & additions)
		{
			for (auto & addition : additions)
			{
				secureClearWideString(addition.first);
				secureClearWideString(addition.second);
			}
			additions.clear();
			additions.shrink_to_fit();
		}

		std::vector<wchar_t> buildEnvironmentBlock(std::vector<std::pair<std::wstring, std::wstring>> const & additions)
		{
			std::vector<std::wstring> entries;
			LPWCH const environment = GetEnvironmentStringsW();
			if (!environment)
				return std::vector<wchar_t>();

			for (wchar_t const * current = environment; *current; current += std::wcslen(current) + 1u)
			{
				std::wstring entry(current);
				bool replaced = isIntegrationTestEnvironmentEntry(entry);
				for (auto const & addition : additions)
				{
					if (startsWithEnvironmentName(entry, addition.first.c_str()))
					{
						replaced = true;
						break;
					}
				}
				if (!replaced)
					entries.push_back(std::move(entry));
				else
					secureClearWideString(entry);
			}
			FreeEnvironmentStringsW(environment);

			for (auto const & addition : additions)
			{
				if (addition.first.empty() || addition.first.find(L'=') != std::wstring::npos ||
					addition.second.find(L'\0') != std::wstring::npos)
				{
					secureClearEnvironmentEntries(entries);
					return std::vector<wchar_t>();
				}
				entries.push_back(addition.first + L"=" + addition.second);
			}

			std::sort(entries.begin(), entries.end(), [](std::wstring const & left, std::wstring const & right)
			{
				return _wcsicmp(left.c_str(), right.c_str()) < 0;
			});

			std::size_t characters = 1u;
			for (std::wstring const & entry : entries)
				characters += entry.size() + 1u;

			std::vector<wchar_t> block;
			block.reserve(characters);
			for (std::wstring const & entry : entries)
			{
				block.insert(block.end(), entry.begin(), entry.end());
				block.push_back(L'\0');
			}
			block.push_back(L'\0');
			secureClearEnvironmentEntries(entries);
			return block;
		}

		class SecureWideBuffer
		{
		public:
			explicit SecureWideBuffer(std::vector<wchar_t> && value)
			: m_value(std::move(value))
			{
			}

			~SecureWideBuffer()
			{
				if (!m_value.empty())
					SecureZeroMemory(m_value.data(), m_value.size() * sizeof(wchar_t));
			}

			SecureWideBuffer(SecureWideBuffer const &) = delete;
			SecureWideBuffer & operator=(SecureWideBuffer const &) = delete;

			bool empty() const { return m_value.empty(); }
			wchar_t * data() { return m_value.data(); }

		private:
			std::vector<wchar_t> m_value;
		};

		std::wstring handleValue(HANDLE handle)
		{
			return std::to_wstring(static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(handle)));
		}

		LONG atomicRead(volatile LONG const * value)
		{
			return InterlockedCompareExchange(const_cast<volatile LONG *>(value), 0, 0);
		}

		void debugMessage(char const * message)
		{
			OutputDebugStringA("TCG x64 bridge: ");
			OutputDebugStringA(message ? message : "unknown error");
			OutputDebugStringA("\n");
		}
	}

	class WindowImpl;

	struct PendingCallback
	{
		std::uint32_t type = CallbackNone;
		std::int32_t value = 0;
		std::string url;
		std::string postData;
	};

	namespace
	{
		constexpr std::size_t MaximumPendingCallbacks = 256u;

		void secureClearString(std::string & value)
		{
			if (!value.empty())
				SecureZeroMemory(value.data(), value.size());
			value.clear();
			value.shrink_to_fit();
		}

		void secureClearPendingCallback(PendingCallback & callback)
		{
			secureClearString(callback.url);
			secureClearString(callback.postData);
			callback.type = CallbackNone;
			callback.value = 0;
		}
	}

	class Manager
	{
	public:
		Manager();
		~Manager();

		bool init();
		void update();
		void shutdown();
		bool isAlive() const;
		bool matchesCurrentLaunchIdentity() const;

		unsigned getWindows(Window ** windows, unsigned capacity);
		Window * getWindow(std::uint32_t id) const;
		Window * getCaptureWindow() const;
		bool queue(Command const & command);

	private:
		bool startHost();
		void synchronizeWindows();
		bool drainCallbacks();
		bool enqueuePendingCallback(Callback const & callback);
		void dispatchCallbacks();
		void clearPendingCallbacks();
		void destroyWindows();
		void closeTransport();

		HANDLE m_mapping;
		SharedState * m_shared;
		HANDLE m_stopEvent;
		HANDLE m_readyEvent;
		HANDLE m_callbackEvent;
		HANDLE m_process;
		HANDLE m_job;
		std::vector<Window *> m_windows;
		std::vector<Window *> m_retiredWindows;
		std::deque<PendingCallback> m_pendingCallbacks;
		LONG m_observedFrameSequence;
		std::uint64_t m_launchIdentityGeneration;
	};

	class WindowImpl
	{
	public:
		WindowImpl(Window * owner, Manager * manager, std::uint32_t id);

		std::uint32_t id() const { return m_id; }
		void updateRecord(WindowRecord const & record, std::vector<std::uint8_t> && pixels);
		void retire();
		bool getSurface(void ** bits, unsigned * width, unsigned * height, unsigned * stride);
		bool canGetFocus() const { return m_canGetFocus; }
		void getTitle(char * title, unsigned capacity) const;
		void getMinMaxInfo(unsigned & minimumWidth, unsigned & minimumHeight, unsigned & maximumWidth, unsigned & maximumHeight) const;

		bool send(CommandType type, std::int32_t value0 = 0, std::int32_t value1 = 0,
			std::int32_t value2 = 0, std::int32_t value3 = 0, std::int32_t value4 = 0,
			std::int32_t value5 = 0, std::int32_t value6 = 0, std::int32_t value7 = 0);

	private:
		Window * m_owner;
		Manager * m_manager;
		std::uint32_t m_id;
		bool m_active;
		bool m_canGetFocus;
		unsigned m_minimumWidth;
		unsigned m_minimumHeight;
		unsigned m_maximumWidth;
		unsigned m_maximumHeight;
		unsigned m_surfaceWidth;
		unsigned m_surfaceHeight;
		unsigned m_surfaceStride;
		char m_title[MaximumTitleBytes];
		std::vector<std::uint8_t> m_pixels;
	};

	namespace
	{
		Manager * s_manager = nullptr;
		// The legacy UI retains raw Window pointers briefly after a TCG window
		// disappears and even across adapter release in the same UI frame.  Keep
		// retired proxy objects as inert process-lifetime tombstones.
		std::vector<Window *> s_processLifetimeWindows;
	}

	Manager::Manager()
	: m_mapping(nullptr)
	, m_shared(nullptr)
	, m_stopEvent(nullptr)
	, m_readyEvent(nullptr)
	, m_callbackEvent(nullptr)
	, m_process(nullptr)
	, m_job(nullptr)
	, m_observedFrameSequence(-1)
	, m_launchIdentityGeneration(s_launchIdentityGeneration)
	{
	}

	Manager::~Manager()
	{
		shutdown();
	}

	bool Manager::init()
	{
		return startHost();
	}

	bool Manager::startHost()
	{
		std::wstring const tcgDirectory = ansiToWide(s_applicationDirectory.c_str());
		if (tcgDirectory.empty())
		{
			debugMessage("the TCG directory is empty or cannot be encoded");
			return false;
		}

		std::wstring::size_type const separator = tcgDirectory.find_last_of(L"\\/");
		if (separator == std::wstring::npos)
		{
			debugMessage("the TCG directory has no parent runtime directory");
			return false;
		}

		std::wstring const hostExecutable = tcgDirectory.substr(0, separator) + L"\\TcgCompatibilityHost.exe";
		DWORD const attributes = GetFileAttributesW(hostExecutable.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			debugMessage("TcgCompatibilityHost.exe is missing from the writable runtime");
			return false;
		}

		SECURITY_ATTRIBUTES inheritable = {};
		inheritable.nLength = sizeof(inheritable);
		inheritable.bInheritHandle = TRUE;

		std::uint64_t const mappingSize = static_cast<std::uint64_t>(SharedStateBytes);
		m_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &inheritable, PAGE_READWRITE,
			static_cast<DWORD>(mappingSize >> 32u), static_cast<DWORD>(mappingSize), nullptr);
		if (!m_mapping)
		{
			debugMessage("CreateFileMapping failed");
			return false;
		}

		m_shared = static_cast<SharedState *>(MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, SharedStateBytes));
		m_stopEvent = CreateEventW(&inheritable, TRUE, FALSE, nullptr);
		m_readyEvent = CreateEventW(&inheritable, TRUE, FALSE, nullptr);
		m_callbackEvent = CreateEventW(&inheritable, FALSE, FALSE, nullptr);
		if (!m_shared || !m_stopEvent || !m_readyEvent || !m_callbackEvent)
		{
			debugMessage("could not create the shared transport handles");
			closeTransport();
			return false;
		}

		ZeroMemory(m_shared, SharedStateBytes);
		m_shared->magic = Magic;
		m_shared->version = Version;
		m_shared->structureBytes = static_cast<std::uint32_t>(SharedStateBytes);
		m_shared->parentProcessId = GetCurrentProcessId();
		InterlockedExchange(&m_shared->lifecycle, LifecycleStarting);

		m_job = CreateJobObjectW(nullptr, nullptr);
		if (!m_job)
		{
			debugMessage("could not create the mandatory TCG host job");
			closeTransport();
			return false;
		}
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
		{
			debugMessage("could not configure kill-on-close containment for the TCG host");
			closeTransport();
			return false;
		}

		std::vector<std::pair<std::wstring, std::wstring>> additions;
		additions.emplace_back(L"SWG_TCG_MAPPING_HANDLE", handleValue(m_mapping));
		additions.emplace_back(L"SWG_TCG_READY_HANDLE", handleValue(m_readyEvent));
		additions.emplace_back(L"SWG_TCG_STOP_HANDLE", handleValue(m_stopEvent));
		additions.emplace_back(L"SWG_TCG_CALLBACK_HANDLE", handleValue(m_callbackEvent));
		additions.emplace_back(L"SWG_TCG_USERNAME", ansiToWide(s_userName.c_str()));
		additions.emplace_back(L"SWG_TCG_SESSION_ID", ansiToWide(s_sessionId.c_str()));
		additions.emplace_back(L"SWG_TCG_CHALLENGE", ansiToWide(s_challenge.c_str()));
		additions.emplace_back(L"SWG_TCG_CHARACTER", ansiToWide(s_characterName.c_str()));
		additions.emplace_back(L"SWG_TCG_CHALLENGE_FOUNDER", s_challengeFounder ? L"1" : L"0");
		additions.emplace_back(L"SWG_TCG_START_TUTORIAL", s_startTutorial ? L"1" : L"0");
		additions.emplace_back(L"SWG_TCG_REALM", s_realmType == REALM_Stage ? L"stage" : L"live");
		additions.emplace_back(L"SWG_TCG_LANGUAGE", s_language == LANG_French ? L"fr" : (s_language == LANG_German ? L"de" : L"en"));

		SecureWideBuffer environment(buildEnvironmentBlock(additions));
		secureClearEnvironmentAdditions(additions);
		if (environment.empty())
		{
			debugMessage("could not build the child environment block");
			closeTransport();
			return false;
		}

		std::wstring commandLine = quoteWindowsArgument(hostExecutable) + L" --tcg-dir " +
			quoteWindowsArgument(tcgDirectory) + L" --parent-pid " + std::to_wstring(GetCurrentProcessId()) +
			L" --timeout-ms 86400000";
		std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
		mutableCommandLine.push_back(L'\0');

		SIZE_T attributeBytes = 0;
		InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
		if (attributeBytes == 0)
		{
			debugMessage("could not size the child handle allowlist");
			closeTransport();
			return false;
		}

		std::vector<std::uint8_t> attributeStorage(attributeBytes, 0u);
		STARTUPINFOEXW startup = {};
		startup.StartupInfo.cb = sizeof(startup);
		startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
		if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeBytes))
		{
			debugMessage("could not initialize the child handle allowlist");
			closeTransport();
			return false;
		}

		HANDLE inheritedHandles[] = { m_mapping, m_readyEvent, m_stopEvent, m_callbackEvent };
		bool const allowlistReady = UpdateProcThreadAttribute(startup.lpAttributeList, 0,
			PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inheritedHandles, sizeof(inheritedHandles), nullptr, nullptr) != FALSE;
		if (!allowlistReady)
		{
			DeleteProcThreadAttributeList(startup.lpAttributeList);
			debugMessage("could not apply the child handle allowlist");
			closeTransport();
			return false;
		}

		PROCESS_INFORMATION processInformation = {};
		BOOL const created = CreateProcessW(hostExecutable.c_str(), mutableCommandLine.data(), nullptr, nullptr, TRUE,
			CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
			environment.data(), tcgDirectory.c_str(), &startup.StartupInfo, &processInformation);
		DeleteProcThreadAttributeList(startup.lpAttributeList);
		SecureZeroMemory(mutableCommandLine.data(), mutableCommandLine.size() * sizeof(wchar_t));
		SetHandleInformation(m_mapping, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(m_readyEvent, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(m_stopEvent, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(m_callbackEvent, HANDLE_FLAG_INHERIT, 0);

		if (!created)
		{
			debugMessage("CreateProcess(TcgCompatibilityHost) failed");
			closeTransport();
			return false;
		}

		m_process = processInformation.hProcess;
		if (!AssignProcessToJobObject(m_job, m_process))
		{
			debugMessage("could not contain the suspended TCG host in its mandatory job");
			TerminateProcess(m_process, 1);
			WaitForSingleObject(m_process, 5000);
			CloseHandle(processInformation.hThread);
			shutdown();
			return false;
		}
		if (ResumeThread(processInformation.hThread) == static_cast<DWORD>(-1))
		{
			debugMessage("could not resume the contained TCG host");
			TerminateProcess(m_process, 1);
			WaitForSingleObject(m_process, 5000);
			CloseHandle(processInformation.hThread);
			shutdown();
			return false;
		}
		CloseHandle(processInformation.hThread);

		HANDLE waitHandles[] = { m_readyEvent, m_callbackEvent, m_process };
		ULONGLONG const startupDeadline = GetTickCount64() + 30000u;
		DWORD waitResult = WAIT_FAILED;
		bool startupCallbacksValid = true;
		for (;;)
		{
			ULONGLONG const now = GetTickCount64();
			DWORD const remaining = now >= startupDeadline ? 0u :
				static_cast<DWORD>(startupDeadline - now);
			waitResult = WaitForMultipleObjects(3, waitHandles, FALSE, remaining);
			if (waitResult == WAIT_OBJECT_0 + 1u)
			{
				startupCallbacksValid = drainCallbacks();
				if (startupCallbacksValid)
					continue;
				break;
			}
			if (waitResult == WAIT_OBJECT_0)
			{
				startupCallbacksValid = drainCallbacks();
				break;
			}
			break;
		}
		if (!startupCallbacksValid || waitResult != WAIT_OBJECT_0 ||
			atomicRead(&m_shared->lifecycle) != LifecycleReady)
		{
			if (m_shared->failureMessage[0])
				debugMessage(m_shared->failureMessage);
			else if (!startupCallbacksValid)
				debugMessage("compatibility host startup callback queue is invalid or exceeded its bounded pending capacity");
			else
				debugMessage(waitResult == WAIT_TIMEOUT ? "compatibility host startup timed out" : "compatibility host startup failed");
			shutdown();
			return false;
		}

		m_observedFrameSequence = -1;
		synchronizeWindows();
		return true;
	}

	void Manager::update()
	{
		if (!matchesCurrentLaunchIdentity())
		{
			shutdown();
			return;
		}
		if (!isAlive())
		{
			clearPendingCallbacks();
			for (Window * const window : m_windows)
			{
				window->m_pImpl->retire();
				m_retiredWindows.push_back(window);
			}
			m_windows.clear();
			return;
		}
		if (!drainCallbacks())
		{
			debugMessage("compatibility host callback queue is invalid or exceeded its bounded pending capacity");
			shutdown();
			return;
		}
		dispatchCallbacks();
		LONG const sequence = atomicRead(&m_shared->frameSequence);
		if (sequence != m_observedFrameSequence)
			synchronizeWindows();
	}

	bool Manager::matchesCurrentLaunchIdentity() const
	{
		return m_launchIdentityGeneration == s_launchIdentityGeneration;
	}

	bool Manager::isAlive() const
	{
		return m_process && WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT && m_shared &&
			atomicRead(&m_shared->lifecycle) == LifecycleReady;
	}

	void Manager::synchronizeWindows()
	{
		if (!m_shared)
			return;

		struct PendingWindow
		{
			WindowRecord record = {};
			std::vector<std::uint8_t> pixels;
		};

		std::vector<PendingWindow> pending;
		LONG acceptedSequence = -1;
		for (int attempt = 0; attempt != 3; ++attempt)
		{
			pending.clear();
			LONG const sequenceBefore = atomicRead(&m_shared->frameSequence);
			if ((sequenceBefore & 1) != 0)
				continue;
			LONG const active = atomicRead(&m_shared->activeFrame);
			if (active < 0 || active > 1)
				return;
			MemoryBarrier();
			Frame const & frame = m_shared->frames[active];
			if (frame.windowCount > MaximumWindows || frame.pixelBytes > MaximumFramePixelBytes)
				return;

			pending.reserve(frame.windowCount);
			bool valid = true;
			for (std::uint32_t index = 0; index < frame.windowCount; ++index)
			{
				WindowRecord const & record = frame.windows[index];
				if (!record.id || std::find_if(pending.begin(), pending.end(), [&record](PendingWindow const & value)
					{ return value.record.id == record.id; }) != pending.end())
				{
					valid = false;
					break;
				}

				PendingWindow value;
				value.record = record;
				if (record.surfaceBytes)
				{
					std::uint64_t const requiredBytes = static_cast<std::uint64_t>(record.stride) * record.height;
					if (!record.width || !record.height || record.width > 16384u || record.height > 16384u ||
						record.stride < record.width * 4u || record.stride > 16384u * 4u ||
						requiredBytes != record.surfaceBytes || record.surfaceOffset > frame.pixelBytes ||
						record.surfaceBytes > frame.pixelBytes - record.surfaceOffset)
					{
						valid = false;
						break;
					}
					value.pixels.resize(record.surfaceBytes);
					std::memcpy(value.pixels.data(), frame.pixels + record.surfaceOffset, record.surfaceBytes);
				}
				pending.push_back(std::move(value));
			}

			MemoryBarrier();
			LONG const sequenceAfter = atomicRead(&m_shared->frameSequence);
			if (!valid)
				return;
			if (sequenceBefore == sequenceAfter && (sequenceAfter & 1) == 0)
			{
				acceptedSequence = sequenceAfter;
				break;
			}
		}
		if (acceptedSequence < 0)
			return;

		std::vector<std::uint32_t> present;
		present.reserve(pending.size());
		for (PendingWindow & value : pending)
		{
			present.push_back(value.record.id);
			Window * window = getWindow(value.record.id);
			if (!window)
			{
				window = new Window;
				window->m_pImpl = new WindowImpl(window, this, value.record.id);
				m_windows.push_back(window);
			}
			window->m_pImpl->updateRecord(value.record, std::move(value.pixels));
		}

		for (auto iterator = m_windows.begin(); iterator != m_windows.end();)
		{
			Window * const window = *iterator;
			if (std::find(present.begin(), present.end(), window->m_pImpl->id()) == present.end())
			{
				window->m_pImpl->retire();
				m_retiredWindows.push_back(window);
				iterator = m_windows.erase(iterator);
			}
			else
				++iterator;
		}

		m_observedFrameSequence = acceptedSequence;
	}

	bool Manager::enqueuePendingCallback(Callback const & callback)
	{
		PendingCallback pending;
		pending.type = callback.type;
		pending.value = callback.value;
		switch (static_cast<CallbackType>(callback.type))
		{
		case CallbackNavigate:
			if (callback.postBytes != 0u)
				return false;
			pending.url.assign(callback.url, callback.urlBytes);
			break;
		case CallbackNavigateWithPost:
			pending.url.assign(callback.url, callback.urlBytes);
			pending.postData.assign(callback.postData, callback.postBytes);
			break;
		case CallbackSetSoundVolume:
		case CallbackSetMusicVolume:
		case CallbackStopAllSounds:
		case CallbackSetWindowState:
			if (callback.urlBytes != 0u || callback.postBytes != 0u)
				return false;
			break;
		case CallbackNone:
		default:
			return false;
		}

		if (m_pendingCallbacks.size() >= MaximumPendingCallbacks)
		{
			bool const coalescible = callback.type == CallbackSetSoundVolume ||
				callback.type == CallbackSetMusicVolume || callback.type == CallbackSetWindowState;
			if (!coalescible)
			{
				secureClearPendingCallback(pending);
				return false;
			}

			for (auto iterator = m_pendingCallbacks.end(); iterator != m_pendingCallbacks.begin();)
			{
				--iterator;
				if (iterator->type == callback.type)
				{
					secureClearPendingCallback(*iterator);
					m_pendingCallbacks.erase(iterator);
					break;
				}
			}
			if (m_pendingCallbacks.size() >= MaximumPendingCallbacks)
			{
				secureClearPendingCallback(pending);
				return false;
			}
		}

		m_pendingCallbacks.push_back(std::move(pending));
		return true;
	}

	bool Manager::drainCallbacks()
	{
		if (!m_shared)
			return false;

		std::uint32_t read = static_cast<std::uint32_t>(atomicRead(&m_shared->callbackRead));
		std::uint32_t const write = static_cast<std::uint32_t>(atomicRead(&m_shared->callbackWrite));
		std::uint32_t const pendingCount = write - read;
		if (pendingCount > MaximumCallbacks)
			return false;

		for (std::uint32_t index = 0u; index < pendingCount; ++index)
		{
			Callback callback = {};
			Callback & sharedCallback = m_shared->callbacks[read % MaximumCallbacks];
			MemoryBarrier();
			std::memcpy(&callback, &sharedCallback, sizeof(callback));
			SecureZeroMemory(&sharedCallback, sizeof(sharedCallback));
			++read;
			InterlockedExchange(&m_shared->callbackRead, static_cast<LONG>(read));

			bool const validText = callback.urlBytes < MaximumUrlBytes && callback.postBytes < MaximumPostBytes &&
				callback.url[callback.urlBytes] == '\0' && callback.postData[callback.postBytes] == '\0';
			bool const enqueued = validText && enqueuePendingCallback(callback);
			SecureZeroMemory(&callback, sizeof(callback));
			if (!enqueued)
				return false;
		}
		return true;
	}

	void Manager::dispatchCallbacks()
	{
		while (!m_pendingCallbacks.empty())
		{
			PendingCallback callback = std::move(m_pendingCallbacks.front());
			m_pendingCallbacks.pop_front();
			switch (static_cast<CallbackType>(callback.type))
			{
			case CallbackNavigate:
				if (s_callbacks.navigateProc)
					s_callbacks.navigateProc(callback.url.c_str());
				break;
			case CallbackNavigateWithPost:
				if (s_callbacks.navigateWithPostDataProc)
					s_callbacks.navigateWithPostDataProc(callback.url.c_str(), callback.postData.c_str());
				break;
			case CallbackSetSoundVolume:
				if (s_callbacks.setSoundVolumeProc)
				{
					float value = 0.0f;
					static_assert(sizeof(value) == sizeof(callback.value), "float callback transport changed");
					std::memcpy(&value, &callback.value, sizeof(value));
					s_callbacks.setSoundVolumeProc(value);
				}
				break;
			case CallbackSetMusicVolume:
				if (s_callbacks.setMusicVolumeProc)
				{
					float value = 0.0f;
					std::memcpy(&value, &callback.value, sizeof(value));
					s_callbacks.setMusicVolumeProc(value);
				}
				break;
			case CallbackStopAllSounds:
				if (s_callbacks.stopAllSoundsProc)
					s_callbacks.stopAllSoundsProc();
				break;
			case CallbackSetWindowState:
				if (s_callbacks.setWindowStateProc)
					s_callbacks.setWindowStateProc(callback.value);
				break;
			default:
				break;
			}
			secureClearPendingCallback(callback);
		}
	}

	void Manager::clearPendingCallbacks()
	{
		for (PendingCallback & callback : m_pendingCallbacks)
			secureClearPendingCallback(callback);
		m_pendingCallbacks.clear();
	}

	bool Manager::queue(Command const & command)
	{
		if (!isAlive() || !matchesCurrentLaunchIdentity())
			return false;

		std::uint32_t const write = static_cast<std::uint32_t>(atomicRead(&m_shared->commandWrite));
		std::uint32_t const read = static_cast<std::uint32_t>(atomicRead(&m_shared->commandRead));
		if (write - read >= MaximumCommands)
			return false;

		Command & destination = m_shared->commands[write % MaximumCommands];
		std::memcpy(&destination, &command, sizeof(command));
		MemoryBarrier();
		InterlockedExchange(&m_shared->commandWrite, static_cast<LONG>(write + 1u));
		return true;
	}

	unsigned Manager::getWindows(Window ** windows, unsigned capacity)
	{
		if (!isAlive() || !matchesCurrentLaunchIdentity())
			return 0u;
		unsigned const count = static_cast<unsigned>(m_windows.size());
		unsigned const copied = std::min(count, capacity);
		if (windows && copied)
			std::memcpy(windows, m_windows.data(), copied * sizeof(Window *));
		return count;
	}

	Window * Manager::getWindow(std::uint32_t id) const
	{
		for (Window * const window : m_windows)
			if (window && window->m_pImpl->id() == id)
				return window;
		return nullptr;
	}

	Window * Manager::getCaptureWindow() const
	{
		if (!m_shared || !isAlive() || !matchesCurrentLaunchIdentity())
			return nullptr;
		LONG const active = atomicRead(&m_shared->activeFrame);
		if (active < 0 || active > 1)
			return nullptr;
		return getWindow(m_shared->frames[active].captureWindowId);
	}

	void Manager::destroyWindows()
	{
		for (Window * const window : m_windows)
		{
			window->m_pImpl->retire();
			m_retiredWindows.push_back(window);
		}
		m_windows.clear();
		s_processLifetimeWindows.insert(s_processLifetimeWindows.end(), m_retiredWindows.begin(), m_retiredWindows.end());
		m_retiredWindows.clear();
	}

	void Manager::shutdown()
	{
		clearPendingCallbacks();
		if (m_shared && m_process && WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT)
		{
			Command command = {};
			command.type = CommandShutdown;
			(void)queue(command);
			SetEvent(m_stopEvent);
			if (WaitForSingleObject(m_process, 10000) == WAIT_TIMEOUT)
				TerminateProcess(m_process, 1);
			WaitForSingleObject(m_process, 5000);
		}
		destroyWindows();
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
		if (m_stopEvent)
		{
			CloseHandle(m_stopEvent);
			m_stopEvent = nullptr;
		}
		if (m_readyEvent)
		{
			CloseHandle(m_readyEvent);
			m_readyEvent = nullptr;
		}
		if (m_callbackEvent)
		{
			CloseHandle(m_callbackEvent);
			m_callbackEvent = nullptr;
		}
	}

	WindowImpl::WindowImpl(Window * owner, Manager * manager, std::uint32_t id)
	: m_owner(owner)
	, m_manager(manager)
	, m_id(id)
	, m_active(true)
	, m_canGetFocus(true)
	, m_minimumWidth(1)
	, m_minimumHeight(1)
	, m_maximumWidth(64000)
	, m_maximumHeight(64000)
	, m_surfaceWidth(0)
	, m_surfaceHeight(0)
	, m_surfaceStride(0)
	{
		ZeroMemory(m_title, sizeof(m_title));
	}

	void WindowImpl::updateRecord(WindowRecord const & record, std::vector<std::uint8_t> && pixels)
	{
		m_active = true;
		m_owner->m_iX = record.x;
		m_owner->m_iY = record.y;
		m_owner->m_uWidth = record.width;
		m_owner->m_uHeight = record.height;
		m_canGetFocus = record.canGetFocus != 0;
		m_minimumWidth = record.minimumWidth;
		m_minimumHeight = record.minimumHeight;
		m_maximumWidth = record.maximumWidth;
		m_maximumHeight = record.maximumHeight;
		std::memcpy(m_title, record.title, sizeof(m_title));
		m_title[sizeof(m_title) - 1u] = '\0';
		m_surfaceWidth = record.surfaceBytes ? record.width : 0u;
		m_surfaceHeight = record.surfaceBytes ? record.height : 0u;
		m_surfaceStride = record.surfaceBytes ? record.stride : 0u;
		m_pixels = std::move(pixels);
	}

	void WindowImpl::retire()
	{
		m_active = false;
		m_surfaceWidth = 0;
		m_surfaceHeight = 0;
		m_surfaceStride = 0;
		m_pixels.clear();
		m_pixels.shrink_to_fit();
	}

	bool WindowImpl::getSurface(void ** bits, unsigned * width, unsigned * height, unsigned * stride)
	{
		if (!bits || !width || !height || !stride || m_pixels.empty() ||
			!m_surfaceWidth || !m_surfaceHeight || !m_surfaceStride)
		{
			return false;
		}
		*bits = m_pixels.data();
		*width = m_surfaceWidth;
		*height = m_surfaceHeight;
		*stride = m_surfaceStride;
		return true;
	}

	void WindowImpl::getTitle(char * title, unsigned capacity) const
	{
		if (!title || !capacity)
			return;
		strncpy_s(title, capacity, m_title, _TRUNCATE);
	}

	void WindowImpl::getMinMaxInfo(unsigned & minimumWidth, unsigned & minimumHeight,
		unsigned & maximumWidth, unsigned & maximumHeight) const
	{
		minimumWidth = m_minimumWidth;
		minimumHeight = m_minimumHeight;
		maximumWidth = m_maximumWidth;
		maximumHeight = m_maximumHeight;
	}

	bool WindowImpl::send(CommandType type, std::int32_t value0, std::int32_t value1,
		std::int32_t value2, std::int32_t value3, std::int32_t value4,
		std::int32_t value5, std::int32_t value6, std::int32_t value7)
	{
		if (!m_active || !m_manager)
			return false;
		Command command = {};
		command.type = type;
		command.windowId = m_id;
		command.values[0] = value0;
		command.values[1] = value1;
		command.values[2] = value2;
		command.values[3] = value3;
		command.values[4] = value4;
		command.values[5] = value5;
		command.values[6] = value6;
		command.values[7] = value7;
		return m_manager->queue(command);
	}

	bool init(char const * applicationDirectory, HostProcessType hostProcessType, RealmType realmType)
	{
		if (s_manager || !applicationDirectory || !*applicationDirectory)
			return false;
		bool const changed = s_applicationDirectory != applicationDirectory ||
			s_hostProcessType != hostProcessType || s_realmType != realmType;
		s_applicationDirectory = applicationDirectory;
		s_hostProcessType = hostProcessType;
		s_realmType = realmType;
		if (changed)
			noteLaunchIdentityChanged();
		return true;
	}

	void setDesktopWindow(HWND window) { s_desktopWindow = window; }
	void setLanguage(Language language)
	{
		if (s_language != language)
		{
			s_language = language;
			noteLaunchIdentityChanged();
		}
	}
	void setUserName(char const * value)
	{
		bool changed = false;
		bool const wasValid = s_userNameValid;
		s_userNameValid = s_userName.assign(value, 115u, &changed);
		if (changed || wasValid != s_userNameValid)
			noteLaunchIdentityChanged();
	}
	void setSessionID(char const * value)
	{
		bool changed = false;
		bool const wasValid = s_sessionIdValid;
		s_sessionIdValid = s_sessionId.assign(value, 50u, &changed);
		if (changed || wasValid != s_sessionIdValid)
			noteLaunchIdentityChanged();
	}
	void setCharacterName(char const * value)
	{
		bool changed = false;
		bool const wasValid = s_characterNameValid;
		s_characterNameValid = s_characterName.assign(value, 112u, &changed);
		if (changed || wasValid != s_characterNameValid)
			noteLaunchIdentityChanged();
	}
	void setChallenge(char const * value, bool founder)
	{
		bool changed = false;
		bool const wasValid = s_challengeValid;
		s_challengeValid = s_challenge.assign(value, 115u, &changed);
		changed = changed || wasValid != s_challengeValid || s_challengeFounder != founder;
		s_challengeFounder = founder;
		if (changed)
			noteLaunchIdentityChanged();
	}
	void setStartTutorial(bool enabled)
	{
		if (s_startTutorial != enabled)
		{
			s_startTutorial = enabled;
			noteLaunchIdentityChanged();
		}
	}

	void setWindowState(WindowState state)
	{
		if (!s_manager)
			return;
		Command command = {};
		command.type = CommandSetWindowState;
		command.values[0] = state;
		(void)s_manager->queue(command);
	}

	void onMusicCompletion()
	{
		if (!s_manager)
			return;
		Command command = {};
		command.type = CommandMusicCompletion;
		(void)s_manager->queue(command);
	}

	void setNavigateCallback(NavigateProc callback) { s_callbacks.navigateProc = callback; }
	void setNavigateWithPostDataCallback(NavigateWithPostDataProc callback) { s_callbacks.navigateWithPostDataProc = callback; }
	void setPlaySoundCallback(PlaySoundProc callback) { s_callbacks.playSoundProc = callback; }
	void setPlayMusicCallback(PlayMusicProc callback) { s_callbacks.playMusicProc = callback; }
	void setSetSoundVolumeCallback(SetSoundVolumeProc callback) { s_callbacks.setSoundVolumeProc = callback; }
	void setSetMusicVolumeCallback(SetMusicVolumeProc callback) { s_callbacks.setMusicVolumeProc = callback; }
	void setStopAllSoundsCallback(StopAllSoundsProc callback) { s_callbacks.stopAllSoundsProc = callback; }
	void setSetWindowStateCallback(SetWindowStateProc callback) { s_callbacks.setWindowStateProc = callback; }

	void hintPrepareToLaunch() {}
	void hintAbortLaunch() {}

	bool launch()
	{
		if (s_manager)
		{
			if (s_manager->isAlive() && s_manager->matchesCurrentLaunchIdentity())
				return true;
			delete s_manager;
			s_manager = nullptr;
		}
		if (s_applicationDirectory.empty() || !s_userNameValid || !s_sessionIdValid ||
			!s_challengeValid || !s_characterNameValid || s_hostProcessType != HPT_StarWarsGalaxies)
		{
			debugMessage("launch configuration is invalid");
			return false;
		}

		Manager * const manager = new Manager;
		if (!manager->init())
		{
			delete manager;
			return false;
		}
		s_manager = manager;
		return true;
	}

	bool isLaunched()
	{
		return s_manager && s_manager->isAlive() && s_manager->matchesCurrentLaunchIdentity();
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
		setUserName("");
		setSessionID("");
		setChallenge("", false);
		setCharacterName("");
		setStartTutorial(false);
	}

	unsigned getWindows(Window ** windows, unsigned capacity)
	{
		return s_manager ? s_manager->getWindows(windows, capacity) : 0u;
	}

	Window * getCaptureWindow()
	{
		return s_manager ? s_manager->getCaptureWindow() : nullptr;
	}

	HCURSOR getCursor()
	{
		return LoadCursor(nullptr, IDC_ARROW);
	}

	Window::Window()
	: m_pImpl(nullptr)
	, m_pUserData(nullptr)
	, m_iX(0)
	, m_iY(0)
	, m_uHeight(0)
	, m_uWidth(0)
	{
	}

	Window::~Window()
	{
		delete m_pImpl;
		m_pImpl = nullptr;
	}

	bool Window::canGetFocus() const { return m_pImpl && m_pImpl->canGetFocus(); }
	void Window::getTitle(char * title, unsigned capacity) const { if (m_pImpl) m_pImpl->getTitle(title, capacity); }
	void Window::setFocus(bool focused) { if (m_pImpl) (void)m_pImpl->send(CommandSetFocus, focused ? 1 : 0); }
	void Window::setLocation(int x, int y)
	{
		m_iX = x;
		m_iY = y;
		if (m_pImpl) (void)m_pImpl->send(CommandSetLocation, x, y);
	}
	void Window::setSize(unsigned width, unsigned height)
	{
		m_uWidth = width;
		m_uHeight = height;
		if (m_pImpl) (void)m_pImpl->send(CommandSetSize, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height));
	}
	void Window::getMinMaxInfo(unsigned & minimumWidth, unsigned & minimumHeight, unsigned & maximumWidth, unsigned & maximumHeight)
	{
		if (m_pImpl) m_pImpl->getMinMaxInfo(minimumWidth, minimumHeight, maximumWidth, maximumHeight);
	}
	unsigned Window::getWindowRepaintRects(RECT * rectangles, unsigned capacity)
	{
		if (rectangles && capacity)
		{
			rectangles[0].left = 0;
			rectangles[0].top = 0;
			rectangles[0].right = static_cast<LONG>(m_uWidth);
			rectangles[0].bottom = static_cast<LONG>(m_uHeight);
		}
		return 1u;
	}
	bool Window::getWindowSurfaceData(void ** bits, unsigned * width, unsigned * height, unsigned * stride)
	{
		return m_pImpl && m_pImpl->getSurface(bits, width, height, stride);
	}
	void Window::close() { if (m_pImpl) (void)m_pImpl->send(CommandClose); }
	void Window::onLeftMouseDown(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 2, x, y, gx, gy, LeftButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onLeftMouseUp(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 3, x, y, gx, gy, LeftButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onLeftMouseDoubleClick(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 4, x, y, gx, gy, LeftButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onMiddleMouseDown(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 2, x, y, gx, gy, MiddleButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onMiddleMouseUp(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 3, x, y, gx, gy, MiddleButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onMiddleMouseDoubleClick(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 4, x, y, gx, gy, MiddleButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onRightMouseDown(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 2, x, y, gx, gy, RightButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onRightMouseUp(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 3, x, y, gx, gy, RightButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onRightMouseDoubleClick(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 4, x, y, gx, gy, RightButton, flags & MouseMask, flags & KeyboardMask); }
	void Window::onMouseMove(int x, int y, int gx, int gy, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouse, 5, x, y, gx, gy, 0, flags & MouseMask, flags & KeyboardMask); }
	void Window::onMouseWheel(int x, int y, int gx, int gy, int ticks, unsigned flags) { if (m_pImpl) (void)m_pImpl->send(CommandMouseWheel, x, y, gx, gy, ticks, flags & MouseMask, flags & KeyboardMask); }
	bool Window::onKeyDown(int key, unsigned flags, int code, int vkey, int nativeMods) { return m_pImpl && m_pImpl->send(CommandKey, 6, key, flags & KeyboardMask, code, vkey, nativeMods); }
	bool Window::onKeyUp(int key, unsigned flags, int code, int vkey, int nativeMods) { return m_pImpl && m_pImpl->send(CommandKey, 7, key, flags & KeyboardMask, code, vkey, nativeMods); }
}
