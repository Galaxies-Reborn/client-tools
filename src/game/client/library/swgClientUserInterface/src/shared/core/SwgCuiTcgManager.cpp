// ======================================================================
//
// SwgCuiTcgManager.cpp
// copyright (c) 2008 Sony Online Entertainment LLC
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiTcgManager.h"

#include "clientAudio/Audio.h"
#include "clientGame/ConfigClientGame.h"
#include "clientGame/Game.h"
#include "clientGame/GameNetwork.h"
#include "clientUserInterface/CuiAction.h"
#include "clientUserInterface/CuiActionManager.h"
#include "clientUserInterface/CuiActions.h"
#include "clientUserInterface/CuiLoginManager.h"
#include "clientUserInterface/CuiMediator.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiMessageBox.h"
#include "clientUserInterface/CuiWorkspace.h"
#include "libEverQuestTCG/libEverQuestTCG.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/Os.h"
#include "sharedInputMap/InputMap.h"
#include "swgClientUserInterface/SwgCuiTcgControl.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"
#include "swgClientUserInterface/SwgCuiButtonBar.h"
#include "swgClientUserInterface/SwgCuiTcgWindow.h"
#include "swgClientUserInterface/SwgCuiWebBrowserManager.h"

#include "UIImage.h"
#include "UIManager.h"
#include "UIPage.h"

#include <list>

// ----------------------------------------------------------------------

namespace SwgCuiTcgManagerNamespace
{
	enum IntegrationTrigger
	{
		IT_splashDirect,
		IT_gameAction
	};

	enum IntegrationBrowserProbePhase
	{
		IBPP_disabled,
		IBPP_waitForEulaInput,
		IBPP_waitForViewMoreInput,
		IBPP_waitForNavigationCallback
	};

	struct IntegrationCredentials
	{
		IntegrationCredentials();
		~IntegrationCredentials();

		void clear();

		char userName[116];
		char sessionId[51];
		char challenge[116];
		char characterId[113];
		bool loaded;

	private:
		IntegrationCredentials(IntegrationCredentials const &);
		IntegrationCredentials & operator=(IntegrationCredentials const &);
	};

	class TcgCoreAction : public CuiAction
	{
	public:
		virtual bool performAction(std::string const & id, Unicode::String const & params) const;
	};

	bool s_installed;
	bool s_integrationTestPending;
	float s_integrationTestDelaySeconds;
	float s_integrationTestTimeoutSeconds;
	std::string s_integrationTestNonce;
	IntegrationTrigger s_integrationTrigger;
	IntegrationCredentials s_integrationCredentials;
	bool s_integrationGameActionDispatchActive;
	bool s_integrationGameActionButtonPressed;
	bool s_integrationGameActionHudHandled;
	bool s_integrationGameActionCredentialsConsumed;
	bool s_integrationGameActionMediatorActivated;

	class IntegrationGameActionHandlerScope
	{
	public:
		explicit IntegrationGameActionHandlerScope(bool active);
		~IntegrationGameActionHandlerScope();

	private:
		bool m_active;
		IntegrationGameActionHandlerScope(IntegrationGameActionHandlerScope const &);
		IntegrationGameActionHandlerScope & operator=(IntegrationGameActionHandlerScope const &);
	};

#if defined(_WIN64)
	TcgCoreAction * s_tcgCoreAction;
#endif
	bool s_integrationSurfaceProbePending;
	float s_integrationSurfaceProbeRemainingSeconds;
	bool s_integrationBrowserProbeEnabled;
	unsigned s_integrationBrowserProbePort;
	IntegrationBrowserProbePhase s_integrationBrowserProbePhase;
	float s_integrationBrowserProbeRemainingSeconds;
	bool readIntegrationCredential(char const * variableName, char * value, DWORD valueCapacity, DWORD maximumLength);
	bool consumeIntegrationCredentials();
	bool applyIntegrationCredentials();
	void clearIntegrationCredentialEnvironment();
	void clearIntegrationBrowserProbeState(bool clearNonce);
	bool isValidIntegrationTestNonce(std::string const & nonce);

	void __stdcall navigateProc(const char * url);
	void __stdcall navigateWithPostDataProc(const char * url, const char * postData);
	void __stdcall playSound(char *buffer, unsigned bufferLenInBytes, libEverQuestTCG::AudioFormatType type);
	void __stdcall playMusic(char *buffer, unsigned bufferLenInBytes, libEverQuestTCG::AudioFormatType type);
	void __stdcall setSoundVolume(float zeroToOne);
	void __stdcall setMusicVolume(float zeroToOne);
	void __stdcall stopAllSounds();
	void __stdcall setWindowState(int windowState);

	char const * getAudioFormatExtension(libEverQuestTCG::AudioFormatType type);
}

using namespace SwgCuiTcgManagerNamespace;

// ----------------------------------------------------------------------

SwgCuiTcgManagerNamespace::IntegrationCredentials::IntegrationCredentials()
:	loaded(false)
{
	SecureZeroMemory(userName, sizeof(userName));
	SecureZeroMemory(sessionId, sizeof(sessionId));
	SecureZeroMemory(challenge, sizeof(challenge));
	SecureZeroMemory(characterId, sizeof(characterId));
}

SwgCuiTcgManagerNamespace::IntegrationCredentials::~IntegrationCredentials()
{
	clear();
}

void SwgCuiTcgManagerNamespace::IntegrationCredentials::clear()
{
	SecureZeroMemory(userName, sizeof(userName));
	SecureZeroMemory(sessionId, sizeof(sessionId));
	SecureZeroMemory(challenge, sizeof(challenge));
	SecureZeroMemory(characterId, sizeof(characterId));
	loaded = false;
}

// ----------------------------------------------------------------------

SwgCuiTcgManagerNamespace::IntegrationGameActionHandlerScope::IntegrationGameActionHandlerScope(bool active)
:	m_active(active)
{
}

SwgCuiTcgManagerNamespace::IntegrationGameActionHandlerScope::~IntegrationGameActionHandlerScope()
{
	if (m_active)
	{
		s_integrationGameActionDispatchActive = false;
		s_integrationGameActionHudHandled = true;
		s_integrationCredentials.clear();
	}
}

// ----------------------------------------------------------------------

bool SwgCuiTcgManagerNamespace::TcgCoreAction::performAction(std::string const & id, Unicode::String const & params) const
{
	if (id != CuiActions::tcg)
		return false;

#if defined(_WIN64)
	if (SwgCuiTcgManager::isIntegrationGameActionDispatch())
	{
		std::string const nonce = Unicode::wideToNarrow(params);
		REPORT_LOG(true, ("TCG integration: game-action-core-fallback nonce=[%s] pid=%lu.\n",
			nonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
	}
#else
	UNREF(params);
#endif

	SwgCuiTcgManager::LaunchResult const launchResult = SwgCuiTcgManager::performAction();
	UNREF(launchResult);
	return true;
}

// ----------------------------------------------------------------------

void SwgCuiTcgManager::install()
{
	DEBUG_FATAL(s_installed, ("already installed\n"));
	s_installed = true;

#if defined(_WIN64)
	s_tcgCoreAction = new TcgCoreAction;
	if (!CuiActionManager::addAction(CuiActions::tcg, s_tcgCoreAction, false))
	{
		delete s_tcgCoreAction;
		s_tcgCoreAction = 0;
		WARNING(true, ("TCG: unable to register the core TCG action fallback.\n"));
	}
#endif

	bool const integrationTestEnabled = ConfigFile::getKeyBool("ClientGame/TcgIntegrationTest", "enabled", false);
	char const * const integrationTestNonce = ConfigFile::getKeyString("ClientGame/TcgIntegrationTest", "nonce", "");
	char const * const integrationTrigger = ConfigFile::getKeyString("ClientGame/TcgIntegrationTest", "trigger", "splashDirect");
	int const integrationTimeoutSeconds = ConfigFile::getKeyInt("ClientGame/TcgIntegrationTest", "timeoutSeconds", 90);
	int const integrationBrowserProbePort = ConfigFile::getKeyInt("ClientGame/TcgIntegrationTest", "browserProbePort", 0);
	s_integrationTestNonce = integrationTestNonce ? integrationTestNonce : "";
	s_integrationTrigger = IT_splashDirect;
	bool integrationTriggerValid = true;
	if (integrationTrigger && _stricmp(integrationTrigger, "gameAction") == 0)
		s_integrationTrigger = IT_gameAction;
	else if (!integrationTrigger || _stricmp(integrationTrigger, "splashDirect") != 0)
		integrationTriggerValid = false;
	bool const integrationTimeoutValid = integrationTimeoutSeconds >= 10 && integrationTimeoutSeconds <= 300;
	bool const integrationNonceValid = isValidIntegrationTestNonce(s_integrationTestNonce);
	bool const integrationBrowserProbePortValid =
		integrationBrowserProbePort >= 49152 && integrationBrowserProbePort <= 65535;
	s_integrationTestPending = integrationTestEnabled && integrationNonceValid && integrationTriggerValid &&
		(s_integrationTrigger != IT_gameAction || integrationTimeoutValid);
	s_integrationTestDelaySeconds = 1.0f;
	s_integrationTestTimeoutSeconds = static_cast<float>(integrationTimeoutSeconds);
	s_integrationCredentials.clear();
	s_integrationGameActionDispatchActive = false;
	s_integrationGameActionButtonPressed = false;
	s_integrationGameActionHudHandled = false;
	s_integrationGameActionCredentialsConsumed = false;
	s_integrationGameActionMediatorActivated = false;
	s_integrationBrowserProbeEnabled = integrationTestEnabled && integrationNonceValid && integrationTriggerValid &&
		s_integrationTrigger == IT_gameAction && integrationBrowserProbePortValid;
	s_integrationBrowserProbePort = s_integrationBrowserProbeEnabled
		? static_cast<unsigned>(integrationBrowserProbePort)
		: 0;
	s_integrationBrowserProbePhase = IBPP_disabled;
	s_integrationBrowserProbeRemainingSeconds = 0.0f;
	if (integrationTestEnabled && !integrationNonceValid)
	{
		WARNING(true, ("TCG integration: refusing an invalid test nonce; use 1-64 ASCII letters, digits, underscores, or hyphens.\n"));
	}
	if (integrationTestEnabled && !integrationTriggerValid)
		WARNING(true, ("TCG integration: refusing an invalid trigger; use splashDirect or gameAction.\n"));
	if (integrationTestEnabled && s_integrationTrigger == IT_gameAction && !integrationTimeoutValid)
		WARNING(true, ("TCG integration: refusing a gameAction timeout outside 10-300 seconds.\n"));
	if (integrationTestEnabled && integrationBrowserProbePort != 0 && !integrationBrowserProbePortValid)
		WARNING(true, ("TCG integration: refusing a browser probe port outside the dynamic high-port range 49152-65535.\n"));
	if (s_integrationTestPending && !consumeIntegrationCredentials())
	{
		WARNING(true, ("TCG integration: test credentials are missing or exceed the adapter ABI limits; no credential values were logged.\n"));
		s_integrationTestPending = false;
	}
	if (integrationTestEnabled && !s_integrationTestPending)
		s_integrationTestNonce.clear();
	if (!s_integrationTestPending)
		s_integrationBrowserProbeEnabled = false;
	if (!s_integrationTestPending)
		clearIntegrationCredentialEnvironment();

	s_integrationSurfaceProbePending = false;
	s_integrationSurfaceProbeRemainingSeconds = 0.0f;

	static std::string startupDirectory = Os::getProgramStartupDirectory();


#if 0 // _DEBUG
	startupDirectory += "\\TradingCardGameD";
#else
	startupDirectory += "\\" + ConfigClientGame::getTcgDirectory();
#endif

	libEverQuestTCG::RealmType const realmType = ConfigClientGame::getUseTcgRealmTypeStage() ? libEverQuestTCG::REALM_Stage : libEverQuestTCG::REALM_Live;

	libEverQuestTCG::init(startupDirectory.c_str(), libEverQuestTCG::HPT_StarWarsGalaxies, realmType);
	libEverQuestTCG::setDesktopWindow(Os::getWindow());

	libEverQuestTCG::setNavigateCallback(navigateProc);
	libEverQuestTCG::setNavigateWithPostDataCallback(navigateWithPostDataProc);
	libEverQuestTCG::setPlaySoundCallback(playSound);
	libEverQuestTCG::setPlayMusicCallback(playMusic);
	libEverQuestTCG::setSetSoundVolumeCallback(setSoundVolume);
	libEverQuestTCG::setSetMusicVolumeCallback(setMusicVolume);
	libEverQuestTCG::setStopAllSoundsCallback(stopAllSounds);
	libEverQuestTCG::setSetWindowStateCallback(setWindowState);
}

// ----------------------------------------------------------------------

void SwgCuiTcgManager::remove()
{
	DEBUG_FATAL(!s_installed, ("not installed\n"));

#if defined(_WIN64)
	if (s_tcgCoreAction)
	{
		IGNORE_RETURN(CuiActionManager::removeAction(s_tcgCoreAction));
		delete s_tcgCoreAction;
		s_tcgCoreAction = 0;
	}
#endif

	s_installed = false;

	libEverQuestTCG::release();

	clearIntegrationCredentialEnvironment();
	s_integrationCredentials.clear();
	s_integrationTestPending = false;
	s_integrationTestDelaySeconds = 0.0f;
	s_integrationTestTimeoutSeconds = 0.0f;
	s_integrationTestNonce.clear();
	s_integrationTrigger = IT_splashDirect;
	s_integrationGameActionDispatchActive = false;
	s_integrationGameActionButtonPressed = false;
	s_integrationGameActionHudHandled = false;
	s_integrationGameActionCredentialsConsumed = false;
	s_integrationGameActionMediatorActivated = false;
	s_integrationSurfaceProbePending = false;
	s_integrationSurfaceProbeRemainingSeconds = 0.0f;
	clearIntegrationBrowserProbeState(true);
}

// ----------------------------------------------------------------------

SwgCuiTcgManager::LaunchResult SwgCuiTcgManager::performAction(char const * integrationActionNonce)
{
	bool const gameActionDispatch = s_integrationGameActionDispatchActive;
	IntegrationGameActionHandlerScope handlerScope(gameActionDispatch);
	bool const useIntegrationCredentials = gameActionDispatch && integrationActionNonce &&
		s_integrationTestNonce == integrationActionNonce && s_integrationCredentials.loaded;
	if (gameActionDispatch && !useIntegrationCredentials)
	{
		WARNING(true, ("TCG integration: gameAction reached a non-HUD or nonce-mismatched handler; launch refused.\n"));
		clearIntegrationBrowserProbeState(true);
		return LR_failed;
	}

	if (useIntegrationCredentials)
	{
		if (!applyIntegrationCredentials())
		{
			clearIntegrationBrowserProbeState(true);
			return LR_failed;
		}
		s_integrationGameActionCredentialsConsumed = true;
	}
	else
		setLoginInfo(GameNetwork::getUserName().c_str(), CuiLoginManager::getSessionIdKey(true));

	SwgCuiTcgWindow * tcgWindow = safe_cast<SwgCuiTcgWindow * >(CuiMediatorFactory::getInWorkspace(CuiMediatorTypes::WS_TcgWindow, false, false, false));

	if (tcgWindow && tcgWindow->isActive() && libEverQuestTCG::isLaunched())
	{
		CuiMediatorFactory::deactivateInWorkspace(CuiMediatorTypes::WS_TcgWindow);
		return LR_embedded;
	}

	if (tcgWindow && tcgWindow->isActive())
		CuiMediatorFactory::deactivateInWorkspace(CuiMediatorTypes::WS_TcgWindow);

	LaunchResult const launchResult = launch();
	SwgCuiTcgWindow * activatedWindow = 0;
	if (launchResult == LR_embedded)
		activatedWindow = safe_cast<SwgCuiTcgWindow * >(CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_TcgWindow, false, false));
	else if (!gameActionDispatch && CuiWorkspace::getGameWorkspace())
		IGNORE_RETURN(CuiMessageBox::createInfoBox(Unicode::narrowToWide("The Trading Card Game could not be launched. Check the client log and TCG configuration.")));

	if (gameActionDispatch)
	{
		s_integrationGameActionMediatorActivated = activatedWindow &&
			(activatedWindow->isActive() || activatedWindow->isOpenNextFrame());
		REPORT_LOG(true, ("TCG integration: embedded-launch nonce=[%s] pid=%lu result=%d.\n",
			s_integrationTestNonce.c_str(),
			static_cast<unsigned long>(Os::getProcessId()),
			static_cast<int>(launchResult)));
		REPORT_LOG(true, ("TCG integration: game-action-mediator-activation nonce=[%s] pid=%lu activated=%s.\n",
			s_integrationTestNonce.c_str(),
			static_cast<unsigned long>(Os::getProcessId()),
			s_integrationGameActionMediatorActivated ? "true" : "false"));
	}

	return launchResult;
}

// ----------------------------------------------------------------------

bool SwgCuiTcgManager::isIntegrationGameActionDispatch()
{
	return s_integrationGameActionDispatchActive;
}

// ----------------------------------------------------------------------

char const * SwgCuiTcgManager::getIntegrationGameActionNonce()
{
	return s_integrationGameActionDispatchActive ? s_integrationTestNonce.c_str() : 0;
}

// ----------------------------------------------------------------------

SwgCuiTcgManager::LaunchResult SwgCuiTcgManager::launch()
{
	if (!libEverQuestTCG::isLaunched())
	{
		libEverQuestTCG::launch();
		libEverQuestTCG::update();
	}

	return libEverQuestTCG::isLaunched() ? LR_embedded : LR_failed;
}

// ----------------------------------------------------------------------

void SwgCuiTcgManager::update(float deltaTimeSecs)
{
	if (s_integrationTestPending)
	{
		if (s_integrationTrigger == IT_splashDirect)
		{
			s_integrationTestDelaySeconds -= deltaTimeSecs;
			if (s_integrationTestDelaySeconds <= 0.0f)
			{
				s_integrationTestPending = false;
				bool const noScene = Game::getScene() == 0;
				bool const noGameWorkspace = CuiWorkspace::getGameWorkspace() == 0;
				REPORT_LOG(true, ("TCG integration: embedded-dispatch-begin nonce=[%s] pid=%lu scene=%s workspace=%s.\n",
					s_integrationTestNonce.c_str(),
					static_cast<unsigned long>(Os::getProcessId()),
					noScene ? "none" : "present",
					noGameWorkspace ? "none" : "present"));

				if (!noScene || !noGameWorkspace)
				{
					clearIntegrationCredentialEnvironment();
					s_integrationCredentials.clear();
					WARNING(true, ("TCG integration: refusing the embedded one-shot trigger outside Splash/LoginScreen.\n"));
					clearIntegrationBrowserProbeState(false);
					s_integrationTestNonce.clear();
				}
				else if (!s_integrationCredentials.loaded)
				{
					WARNING(true, ("TCG integration: embedded test credentials are missing or exceed the adapter ABI limits; no credential values were logged.\n"));
					clearIntegrationBrowserProbeState(false);
					s_integrationTestNonce.clear();
				}
				else if (applyIntegrationCredentials())
				{
					LaunchResult const launchResult = launch();
					REPORT_LOG(true, ("TCG integration: embedded-launch nonce=[%s] pid=%lu result=%d.\n",
						s_integrationTestNonce.c_str(),
						static_cast<unsigned long>(Os::getProcessId()),
						static_cast<int>(launchResult)));
					s_integrationSurfaceProbePending = launchResult == LR_embedded;
					s_integrationSurfaceProbeRemainingSeconds = 45.0f;
					if (!s_integrationSurfaceProbePending)
					{
						clearIntegrationBrowserProbeState(false);
						s_integrationTestNonce.clear();
					}
				}
			}
		}
		else
		{
			s_integrationTestTimeoutSeconds -= deltaTimeSecs;
			bool const sceneReady = Game::getScene() != 0;
			bool const workspaceReady = CuiWorkspace::getGameWorkspace() != 0;
			InputMap * const gameInputMap = Game::getGameInputMap();
			bool const inputQueueReady = gameInputMap != 0 &&
				Game::getGameMessageQueue() == gameInputMap->getMessageQueue();
			CuiAction const * const registeredTcgAction = CuiActionManager::findAction(CuiActions::tcg);
			bool hudActionReady = registeredTcgAction != 0;
#if defined(_WIN64)
			hudActionReady = hudActionReady && registeredTcgAction != s_tcgCoreAction;
#endif
			hudActionReady = hudActionReady && SwgCuiButtonBar::isTcgButtonReadyForIntegrationTest();
			if (s_integrationTestTimeoutSeconds > 0.0f && !s_integrationGameActionButtonPressed &&
				sceneReady && workspaceReady && inputQueueReady && hudActionReady)
			{
				if (!s_integrationCredentials.loaded)
				{
					s_integrationTestPending = false;
					WARNING(true, ("TCG integration: gameAction credentials are missing or exceed the adapter ABI limits; no credential values were logged.\n"));
					clearIntegrationBrowserProbeState(false);
					s_integrationTestNonce.clear();
				}
				else
				{
					s_integrationGameActionDispatchActive = true;
					s_integrationGameActionButtonPressed = true;
					s_integrationGameActionHudHandled = false;
					s_integrationGameActionCredentialsConsumed = false;
					s_integrationGameActionMediatorActivated = false;
					bool const buttonPressed = SwgCuiButtonBar::pressTcgButtonForIntegrationTest(
						s_integrationTestNonce.c_str());
					if (buttonPressed)
					{
						REPORT_LOG(true, ("TCG integration: game-action-dispatch nonce=[%s] pid=%lu scene=present workspace=present handler=hud-queued.\n",
							s_integrationTestNonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
					}
					else
					{
						s_integrationGameActionDispatchActive = false;
						s_integrationGameActionButtonPressed = false;
					}
				}
			}

			if (s_integrationTestPending && s_integrationGameActionButtonPressed &&
				s_integrationGameActionHudHandled)
			{
				s_integrationTestPending = false;
				bool const launchReady = libEverQuestTCG::isLaunched();
				bool const actionComplete = s_integrationGameActionCredentialsConsumed &&
					s_integrationGameActionMediatorActivated && launchReady;
				REPORT_LOG(true, ("TCG integration: game-action-result nonce=[%s] pid=%lu handled=true credentials=%s mediator=%s launched=%s.\n",
					s_integrationTestNonce.c_str(),
					static_cast<unsigned long>(Os::getProcessId()),
					s_integrationGameActionCredentialsConsumed ? "consumed" : "not-consumed",
					s_integrationGameActionMediatorActivated ? "active" : "inactive",
					launchReady ? "true" : "false"));
				if (actionComplete)
				{
					s_integrationSurfaceProbePending = true;
					s_integrationSurfaceProbeRemainingSeconds = 45.0f;
				}
				else
				{
					libEverQuestTCG::release();
					WARNING(true, ("TCG integration: gameAction did not complete through the HUD action and active TCG mediator.\n"));
					clearIntegrationBrowserProbeState(false);
					s_integrationTestNonce.clear();
				}
				s_integrationGameActionDispatchActive = false;
				s_integrationGameActionButtonPressed = false;
				s_integrationGameActionHudHandled = false;
			}

			if (s_integrationTestPending && s_integrationTestTimeoutSeconds <= 0.0f)
			{
				bool const buttonWasPressed = s_integrationGameActionButtonPressed;
				s_integrationTestPending = false;
				s_integrationGameActionDispatchActive = false;
				s_integrationGameActionButtonPressed = false;
				s_integrationGameActionHudHandled = false;
				clearIntegrationCredentialEnvironment();
				s_integrationCredentials.clear();
				WARNING(true, ("TCG integration: game-action-timeout nonce=[%s] pid=%lu scene=%s workspace=%s queue=%s handler=%s button=%s.\n",
					s_integrationTestNonce.c_str(),
					static_cast<unsigned long>(Os::getProcessId()),
					sceneReady ? "present" : "none",
					workspaceReady ? "present" : "none",
					inputQueueReady ? "ready" : "mismatch",
					hudActionReady ? "hud-ready" : "missing",
					buttonWasPressed ? "pressed" : "not-pressed"));
				clearIntegrationBrowserProbeState(false);
				s_integrationTestNonce.clear();
			}
		}
	}

	if (s_integrationSurfaceProbePending)
	{
		s_integrationSurfaceProbeRemainingSeconds -= deltaTimeSecs;
#if defined(_WIN64)
		bool const backendReady = libEverQuestTCG::isLaunched();
		char const * const backendName = "compatibility-host";
#else
		bool const backendReady = GetModuleHandleA("SWGTCG.dll") != 0;
		char const * const backendName = backendReady ? "loaded" : "missing";
#endif
		unsigned const windowCount = libEverQuestTCG::getWindows(0, 0);
		libEverQuestTCG::Window * windows[16] = { 0 };
		unsigned const inspectedWindowCount = windowCount < (sizeof(windows) / sizeof(*windows))
			? windowCount
			: static_cast<unsigned>(sizeof(windows) / sizeof(*windows));
		if (inspectedWindowCount)
			IGNORE_RETURN(libEverQuestTCG::getWindows(windows, inspectedWindowCount));

		bool surfaceReady = false;
		unsigned readyWidth = 0;
		unsigned readyHeight = 0;
		unsigned readyStride = 0;
		for (unsigned index = 0; index < inspectedWindowCount && !surfaceReady; ++index)
		{
			void * bits = 0;
			unsigned width = 0;
			unsigned height = 0;
			unsigned stride = 0;
			if (windows[index] &&
				windows[index]->getWindowSurfaceData(&bits, &width, &height, &stride) &&
				bits && width && height && width <= 16384 && height <= 16384 &&
				stride >= width * 4 && stride <= 16384 * 4)
			{
				surfaceReady = true;
				readyWidth = width;
				readyHeight = height;
				readyStride = stride;
			}
		}

		if (backendReady && surfaceReady)
		{
			REPORT_LOG(true, ("TCG integration: embedded-surface-ready nonce=[%s] pid=%lu module=%s windows=%u width=%u height=%u stride=%u.\n",
				s_integrationTestNonce.c_str(),
				static_cast<unsigned long>(Os::getProcessId()),
				backendName,
				windowCount,
				readyWidth,
				readyHeight,
				readyStride));
			s_integrationSurfaceProbePending = false;
			if (s_integrationBrowserProbeEnabled)
			{
				s_integrationBrowserProbePhase = IBPP_waitForEulaInput;
				s_integrationBrowserProbeRemainingSeconds = 2.0f;
			}
			else
				s_integrationTestNonce.clear();
		}
		else if (s_integrationSurfaceProbeRemainingSeconds <= 0.0f)
		{
			WARNING(true, ("TCG integration: embedded-surface-timeout nonce=[%s] pid=%lu module=%s windows=%u.\n",
				s_integrationTestNonce.c_str(),
				static_cast<unsigned long>(Os::getProcessId()),
				backendReady ? backendName : "missing",
				windowCount));
			s_integrationSurfaceProbePending = false;
			clearIntegrationBrowserProbeState(true);
		}
	}

	if (s_integrationBrowserProbeEnabled && s_integrationBrowserProbePhase != IBPP_disabled)
	{
		s_integrationBrowserProbeRemainingSeconds -= deltaTimeSecs;
		if (s_integrationBrowserProbeRemainingSeconds <= 0.0f)
		{
			if (s_integrationBrowserProbePhase == IBPP_waitForNavigationCallback)
			{
				WARNING(true, ("TCG integration: browser-probe-callback-timeout nonce=[%s] pid=%lu.\n",
					s_integrationTestNonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
				clearIntegrationBrowserProbeState(true);
			}
			else
			{
				SwgCuiTcgWindow * const tcgWindow = safe_cast<SwgCuiTcgWindow *>(
					CuiMediatorFactory::getInWorkspace(CuiMediatorTypes::WS_TcgWindow, false, false, false));
				if (!tcgWindow || !tcgWindow->isActive())
				{
					WARNING(true, ("TCG integration: browser-probe-input-target-missing nonce=[%s] pid=%lu.\n",
						s_integrationTestNonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
					clearIntegrationBrowserProbeState(true);
				}
				else if (s_integrationBrowserProbePhase == IBPP_waitForEulaInput)
				{
					s_integrationBrowserProbePhase = IBPP_waitForViewMoreInput;
					s_integrationBrowserProbeRemainingSeconds = 3.0f;
					REPORT_LOG(true, ("TCG integration: browser-probe-eula-input nonce=[%s] pid=%lu normalized=1075/1772,904/1293.\n",
						s_integrationTestNonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
					if (!tcgWindow->dispatchIntegrationTestClick(1075, 1772, 904, 1293))
					{
						WARNING(true, ("TCG integration: browser-probe-eula-input-failed nonce=[%s] pid=%lu.\n",
							s_integrationTestNonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
						clearIntegrationBrowserProbeState(true);
					}
				}
				else if (s_integrationBrowserProbePhase == IBPP_waitForViewMoreInput)
				{
					s_integrationBrowserProbePhase = IBPP_waitForNavigationCallback;
					s_integrationBrowserProbeRemainingSeconds = 15.0f;
					REPORT_LOG(true, ("TCG integration: browser-probe-view-more-input nonce=[%s] pid=%lu normalized=1288/1772,1101/1293.\n",
						s_integrationTestNonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
					if (!tcgWindow->dispatchIntegrationTestClick(1288, 1772, 1101, 1293))
					{
						WARNING(true, ("TCG integration: browser-probe-view-more-input-failed nonce=[%s] pid=%lu.\n",
							s_integrationTestNonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
						clearIntegrationBrowserProbeState(true);
					}
				}
			}
		}
	}
	
	// This is being moved to CuiManager.cpp. We need to update the TCG BEFORE we start resizing windows, otherwise
	// we can end up with a UI page that is 1 frame behind in terms of size. This will cause a crash when we do our
	// texture memcpy.
	//if (libEverQuestTCG::isLaunched())
	//	libEverQuestTCG::update();
}

// ----------------------------------------------------------------------

void SwgCuiTcgManager::setLoginInfo(char const * const username, char const * const sessionId)
{
	char const * const u = username ? username : "";
	char const * const s = sessionId ? sessionId : "";

	// A normal in-game launch must never inherit the challenge/character state
	// used by either isolated integration-test trigger.
	libEverQuestTCG::setChallenge("", false);
	libEverQuestTCG::setCharacterName("");
	libEverQuestTCG::setStartTutorial(false);
	libEverQuestTCG::setUserName(u);
	libEverQuestTCG::setSessionID(s);
}

// ======================================================================

bool SwgCuiTcgManagerNamespace::isValidIntegrationTestNonce(std::string const & nonce)
{
	if (nonce.empty() || nonce.length() > 64)
		return false;

	for (std::string::const_iterator iterator = nonce.begin(); iterator != nonce.end(); ++iterator)
	{
		char const value = *iterator;
		bool const valid =
			(value >= 'A' && value <= 'Z') ||
			(value >= 'a' && value <= 'z') ||
			(value >= '0' && value <= '9') ||
			value == '_' || value == '-';
		if (!valid)
			return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool SwgCuiTcgManagerNamespace::readIntegrationCredential(char const * variableName, char * value, DWORD valueCapacity, DWORD maximumLength)
{
	if (!variableName || !value || valueCapacity == 0)
		return false;

	SecureZeroMemory(value, valueCapacity);
	DWORD const length = GetEnvironmentVariableA(variableName, value, valueCapacity);
	BOOL const removed = SetEnvironmentVariableA(variableName, 0);
	if (!removed || length == 0 || length >= valueCapacity || length > maximumLength)
	{
		SecureZeroMemory(value, valueCapacity);
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool SwgCuiTcgManagerNamespace::consumeIntegrationCredentials()
{
	s_integrationCredentials.clear();
	bool const haveUserName = readIntegrationCredential(
		"SWGTCG_TEST_USERNAME", s_integrationCredentials.userName,
		sizeof(s_integrationCredentials.userName), 115);
	bool const haveSessionId = readIntegrationCredential(
		"SWGTCG_TEST_SESSION", s_integrationCredentials.sessionId,
		sizeof(s_integrationCredentials.sessionId), 50);
	bool const haveChallenge = readIntegrationCredential(
		"SWGTCG_TEST_CHALLENGE", s_integrationCredentials.challenge,
		sizeof(s_integrationCredentials.challenge), 115);
	bool const haveCharacterId = readIntegrationCredential(
		"SWGTCG_TEST_CHARACTER_ID", s_integrationCredentials.characterId,
		sizeof(s_integrationCredentials.characterId), 112);
	int const validCredentialCount =
		(haveUserName ? 1 : 0) +
		(haveSessionId ? 1 : 0) +
		(haveChallenge ? 1 : 0) +
		(haveCharacterId ? 1 : 0);
	s_integrationCredentials.loaded = validCredentialCount == 4;
	if (!s_integrationCredentials.loaded)
		s_integrationCredentials.clear();
	return s_integrationCredentials.loaded;
}

// ----------------------------------------------------------------------

bool SwgCuiTcgManagerNamespace::applyIntegrationCredentials()
{
	if (!s_integrationCredentials.loaded)
		return false;

	libEverQuestTCG::setUserName(s_integrationCredentials.userName);
	libEverQuestTCG::setSessionID(s_integrationCredentials.sessionId);
	libEverQuestTCG::setChallenge(s_integrationCredentials.challenge, false);
	libEverQuestTCG::setCharacterName(s_integrationCredentials.characterId);
	libEverQuestTCG::setStartTutorial(false);
	s_integrationCredentials.clear();
	return true;
}

// ----------------------------------------------------------------------

void SwgCuiTcgManagerNamespace::clearIntegrationCredentialEnvironment()
{
	char const * const names[] =
	{
		"SWGTCG_TEST_USERNAME",
		"SWGTCG_TEST_SESSION",
		"SWGTCG_TEST_CHALLENGE",
		"SWGTCG_TEST_CHARACTER_ID",
		"SWGTCG_TEST_SWG_LOGIN_PASSWORD"
	};
	for (char const * const name : names)
		IGNORE_RETURN(SetEnvironmentVariableA(name, 0));
}

// ----------------------------------------------------------------------

void SwgCuiTcgManagerNamespace::clearIntegrationBrowserProbeState(bool clearNonce)
{
	s_integrationBrowserProbeEnabled = false;
	s_integrationBrowserProbePort = 0;
	s_integrationBrowserProbePhase = IBPP_disabled;
	s_integrationBrowserProbeRemainingSeconds = 0.0f;
	if (clearNonce)
		s_integrationTestNonce.clear();
}

// ======================================================================

void __stdcall SwgCuiTcgManagerNamespace::navigateProc(const char * url)
{
	if (!url)
		return;

	if (s_integrationBrowserProbeEnabled)
	{
		if (s_integrationBrowserProbePhase != IBPP_waitForNavigationCallback ||
			!isValidIntegrationTestNonce(s_integrationTestNonce) ||
			s_integrationBrowserProbePort < 49152 || s_integrationBrowserProbePort > 65535)
		{
			WARNING(true, ("TCG integration: browser-probe-navigation-out-of-order pid=%lu.\n",
				static_cast<unsigned long>(Os::getProcessId())));
			clearIntegrationBrowserProbeState(true);
			return;
		}

		std::string const nonce = s_integrationTestNonce;
		char probeUrl[160] = { 0 };
		int const written = _snprintf_s(
			probeUrl,
			sizeof(probeUrl),
			_TRUNCATE,
			"http://127.0.0.1:%u/tcg-browser-probe/%s",
			s_integrationBrowserProbePort,
			nonce.c_str());
		if (written < 0)
		{
			WARNING(true, ("TCG integration: browser-probe-destination-build-failed nonce=[%s] pid=%lu.\n",
				nonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
			SecureZeroMemory(probeUrl, sizeof(probeUrl));
			clearIntegrationBrowserProbeState(true);
			return;
		}

		REPORT_LOG(true, ("TCG integration: browser-probe-authentic-callback nonce=[%s] pid=%lu.\n",
			nonce.c_str(), static_cast<unsigned long>(Os::getProcessId())));
		SwgCuiWebBrowserManager::beginIntegrationBrowserProbe(nonce.c_str());
		SwgCuiWebBrowserManager::createWebBrowserPage(false);
		SwgCuiWebBrowserManager::setURL(probeUrl, true);
		SecureZeroMemory(probeUrl, sizeof(probeUrl));
		clearIntegrationBrowserProbeState(true);
		return;
	}

	DEBUG_REPORT_LOG(true, ("SwgCuiTcgManagerNamespace::navigateProc() - navigation request received; URL omitted.\n"));
	SwgCuiWebBrowserManager::createWebBrowserPage(false);
	SwgCuiWebBrowserManager::setURL(url, true);
}

// ----------------------------------------------------------------------

void __stdcall SwgCuiTcgManagerNamespace::navigateWithPostDataProc(const char * url, const char * postData)
{
	if (!url)
		return;

	if (s_integrationBrowserProbeEnabled)
	{
		WARNING(true, ("TCG integration: browser-probe-unexpected-post-callback pid=%lu; destination and POST body omitted.\n",
			static_cast<unsigned long>(Os::getProcessId())));
		clearIntegrationBrowserProbeState(true);
		return;
	}

	DEBUG_REPORT_LOG(true, ("SwgCuiTcgManagerNamespace::navigateWithPostDataProc() - navigation request received; URL and POST body omitted.\n"));
	SwgCuiWebBrowserManager::createWebBrowserPage(false);

	if (postData)
		SwgCuiWebBrowserManager::setURL(url, true, postData, strlen(postData));
	else
		SwgCuiWebBrowserManager::setURL(url, true);
}

// ----------------------------------------------------------------------

void __stdcall SwgCuiTcgManagerNamespace::playSound(char *buffer, unsigned bufferLenInBytes, libEverQuestTCG::AudioFormatType type)
{
	DEBUG_REPORT_LOG(true, ("SwgCuiTcgManagerNamespace::playSound() - %d, %d\n", bufferLenInBytes, type));

	Audio::playBufferedSound(buffer, bufferLenInBytes, getAudioFormatExtension(type));
}

// ----------------------------------------------------------------------

void __stdcall SwgCuiTcgManagerNamespace::playMusic(char *buffer, unsigned bufferLenInBytes, libEverQuestTCG::AudioFormatType type)
{
	DEBUG_REPORT_LOG(true, ("SwgCuiTcgManagerNamespace::playMusic() - %d, %d\n", bufferLenInBytes, type));

	Audio::playBufferedMusic(buffer, bufferLenInBytes, getAudioFormatExtension(type));
}


// ----------------------------------------------------------------------

void __stdcall SwgCuiTcgManagerNamespace::setSoundVolume(float zeroToOne)
{
	DEBUG_REPORT_LOG(true, ("SwgCuiTcgManagerNamespace::setSoundVolume() - %f\n", zeroToOne));

	Audio::setBufferedSoundVolume(zeroToOne);
}

// ----------------------------------------------------------------------

void __stdcall SwgCuiTcgManagerNamespace::setMusicVolume(float zeroToOne)
{
	DEBUG_REPORT_LOG(true, ("SwgCuiTcgManagerNamespace::setMusicVolume() - %f\n", zeroToOne));

	Audio::setBufferedMusicVolume(zeroToOne);
}

// ----------------------------------------------------------------------

void __stdcall SwgCuiTcgManagerNamespace::stopAllSounds()
{
	DEBUG_REPORT_LOG(true, ("SwgCuiTcgManagerNamespace::stopAllSounds()\n"));

	Audio::stopBufferedSound();
	Audio::stopBufferedMusic();
}

// ----------------------------------------------------------------------

void __stdcall SwgCuiTcgManagerNamespace::setWindowState(int windowState)
{
	UNREF(windowState);
	DEBUG_REPORT_LOG(true, ("SwgCuiTcgManagerNamespace::setWindowState() - %d\n", windowState));
}

// ----------------------------------------------------------------------

char const * SwgCuiTcgManagerNamespace::getAudioFormatExtension(libEverQuestTCG::AudioFormatType type)
{
	char const * extension = 0;

	switch (type)
	{
	case libEverQuestTCG::WAVAudioFormat:
		extension = ".wav";
		break;
	case libEverQuestTCG::MP3AudioFormat:
		extension = ".mp3";
		break;
	case libEverQuestTCG::OGGAudioFormat:
		extension = ".ogg";
		break;
	default :
		DEBUG_FATAL(true, ("Invalid audio format %d.", type));
		break;
	}

	return extension;
}

// ======================================================================
