// ======================================================================
//
// ClientMain.cpp
// copyright 1998 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgClient.h"
#include "ClientMain.h"

#include "clientAnimation/SetupClientAnimation.h"
#include "clientAudio/Audio.h"
#include "clientAudio/SetupClientAudio.h"
#include "clientBugReporting/SetupClientBugReporting.h"
#include "clientDirectInput/DirectInput.h"
#include "clientDirectInput/SetupClientDirectInput.h"
#include "clientGame/ClientCommandQueue.h"
#include "clientGame/ClientCommandChecks.h"
#include "clientGame/ClientObject.h"
#include "clientGame/ContainerInterface.h"
#include "clientGame/Game.h"
#include "clientGame/PlayerCreatureController.h"
#include "clientGame/SetupClientGame.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ScreenShotHelper.h"
#include "clientGraphics/ShaderTemplate.h"
#include "clientGraphics/SetupClientGraphics.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/VideoList.h"
#include "clientObject/SetupClientObject.h"
#include "clientParticle/SetupClientParticle.h"
#include "clientSkeletalAnimation/SetupClientSkeletalAnimation.h"
#include "clientTerrain/SetupClientTerrain.h"
#include "clientTextureRenderer/SetupClientTextureRenderer.h"
#include "clientUserInterface/CuiAction.h"
#include "clientUserInterface/CuiActionManager.h"
#include "clientUserInterface/CuiActions.h"
#include "clientUserInterface/CuiChatHistory.h"
#include "clientUserInterface/CuiCombatManager.h"
#include "clientUserInterface/CuiDataDrivenPageManager.h"
#include "clientUserInterface/CuiInventoryManager.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiMessageQueueManager.h"
#include "clientUserInterface/CuiSettings.h"
#include "clientUserInterface/CuiWorkspace.h"
#include "clientGraphics/IndexedTriangleListAppearance.h"
#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/DataLint.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/ConstCharCrcLowerString.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ApplicationVersion.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Branch.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Binary.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ConfigFile.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Crc.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/CrashReportInformation.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ExitChain.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation//Os.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Production.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/SetupSharedFoundation.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ConfigSharedFoundation.h"
#include "sharedGame/CommoditiesAdvancedSearchAttribute.h"
#include "sharedGame/CommandTable.h"
#include "sharedGame/SetupSharedGame.h"
#include "sharedImage/SetupSharedImage.h"
#include "sharedIoWin/IoWinManager.h"
#include "sharedIoWin/SetupSharedIoWin.h"
#include "sharedLog/SetupSharedLog.h"
#include "sharedLog/LogManager.h"
#include "sharedMath/SetupSharedMath.h"
#include "sharedMath/VectorArgb.h"
#include "sharedMemoryManager/MemoryManager.h"
#include "sharedNetwork/SetupSharedNetwork.h"
#include "sharedNetworkMessages/MessageQueueCommandTimer.h"
#include "sharedNetworkMessages/SetupSharedNetworkMessages.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/Container.h"
#include "sharedObject/Object.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/SetupSharedObject.h"
#include "sharedObject/SlotIdManager.h"
#include "sharedObject/SlottedContainer.h"
#include "sharedPathfinding/SetupSharedPathfinding.h"
#include "sharedRandom/SetupSharedRandom.h"
#include "sharedRegex/SetupSharedRegex.h"
#include "sharedTerrain/SetupSharedTerrain.h"
#include "sharedTerrain/TerrainAppearance.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedUtility/CurrentUserOptionManager.h"
#include "sharedUtility/LocalMachineOptionManager.h"
#include "sharedUtility/SetupSharedUtility.h"
#include "sharedXml/SetupSharedXml.h"
#include "swgClientUserInterface/SetupSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiAuctionFilter.h"
#include "swgClientUserInterface/SwgCuiChatWindow.h"
#include "swgClientUserInterface/SwgCuiG15Lcd.h"
#include "swgClientUserInterface/SwgCuiManager.h"
#include "swgClientUserInterface/SwgCuiActions.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"
#include "swgClientUserInterface/SwgCuiSkills.h"
#include "swgSharedNetworkMessages/SetupSwgSharedNetworkMessages.h"


#include "Resource.h"

#include "sharedGame/PlatformFeatureBits.h"

#include <algorithm>
#include <dinput.h>
#include <string>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <UnicodeUtils.h>

extern void externalCommandHandler(const char*);

namespace ClientMainNamespace
{
	enum BackgroundInputCommand
	{
		BIC_ping = 0,
		BIC_mouseMove,
		BIC_leftMouseDown,
		BIC_leftMouseUp,
		BIC_rightMouseDown,
		BIC_rightMouseUp,
		BIC_middleMouseDown,
		BIC_middleMouseUp,
		BIC_keyDown,
		BIC_keyUp,
		BIC_character,
		BIC_inputReset,
		BIC_examineCharacterSheet,
		BIC_inviteTarget,
		BIC_joinGroup,
		BIC_disbandGroup,
		BIC_openStatMigration,
		BIC_startImageDesign,
		BIC_targetCounterpart,
		BIC_queueCombatCanary,
		BIC_clearCombatQueue,
		BIC_combatQueueStatus,
		BIC_equipCdefRifle,
		BIC_stand,
		BIC_queueBodyShot1,
		BIC_queueLegShot1,
		BIC_equipCdefPistol,
		BIC_equipCdefCarbine,
		BIC_combatTimerStatus,
		BIC_queueDurationControl,
		BIC_equipFixtureLightsaber,
		BIC_equipFixtureFallbackSword,
		BIC_queueHealWound,
		BIC_queueHealDamage,
		BIC_queueTendDamage,
		BIC_queueTendWound,
		BIC_queueDiagnose,
		BIC_queueMedicalForage,
		BIC_queueFirstAid,
		BIC_queueDragIncapacitatedPlayer,
		BIC_queueQuickHeal,
		BIC_queueHealState,
		BIC_queueCurePoison,
		BIC_queueHealEnhance,
		BIC_queueExtinguishFire,
		BIC_queueCureDisease,
		BIC_queueRevivePlayer,
		BIC_queueDeathBlow,
		BIC_selectCloneLocation,
		BIC_confirmCloneLocation,
		BIC_startDanceRhythmic,
		BIC_flourishOne,
		BIC_stopDance,
		BIC_startMusicStarwars1,
		BIC_stopMusic,
		BIC_startBandStarwars1,
		BIC_bandFlourishOne,
		BIC_stopBand,
		BIC_startMusicRock,
		BIC_surrenderEntertainerMusicOne,
		BIC_startMusicStarwars2,
		BIC_surrenderEntertainerMusicTwo,
		BIC_startMusicFolk,
		BIC_surrenderEntertainerMusicThree,
		BIC_startMusicStarwars3,
		BIC_surrenderEntertainerMusicFour,
		BIC_startMusicCeremonial,
		BIC_surrenderEntertainerMaster,
		BIC_startDanceBasicTwo,
		BIC_surrenderEntertainerDanceOne,
		BIC_startDanceRhythmicTwo,
		BIC_surrenderEntertainerDanceTwo,
		BIC_startDanceFootloose,
		BIC_surrenderEntertainerDanceThree,
		BIC_startDanceFormal,
		BIC_surrenderEntertainerDanceFour,
		BIC_surrenderEntertainerHairstyleOne,
		BIC_surrenderEntertainerHairstyleTwo,
		BIC_surrenderEntertainerHairstyleThree,
		BIC_surrenderEntertainerHairstyleFour,
		BIC_startDancePopular,
		BIC_surrenderDancerNovice,
		BIC_surrenderDancerAbilityOne,
		BIC_surrenderDancerAbilityTwo,
		BIC_surrenderDancerAbilityThree,
		BIC_surrenderDancerAbilityFour,
		BIC_surrenderDancerWoundOne,
		BIC_surrenderDancerWoundTwo,
		BIC_surrenderDancerWoundThree,
		BIC_surrenderDancerWoundFour,
		BIC_surrenderDancerShockOne,
		BIC_surrenderDancerShockTwo,
		BIC_surrenderDancerShockThree,
		BIC_surrenderDancerShockFour,
		BIC_surrenderDancerKnowledgeOne,
		BIC_surrenderDancerKnowledgeTwo,
		BIC_surrenderDancerKnowledgeThree,
		BIC_surrenderDancerKnowledgeFour,
		BIC_surrenderDancerMaster,
		BIC_surrenderMusicianNovice,
		BIC_surrenderMusicianAbilityOne,
		BIC_surrenderMusicianAbilityTwo,
		BIC_surrenderMusicianAbilityThree,
		BIC_surrenderMusicianAbilityFour,
		BIC_surrenderMusicianWoundOne,
		BIC_surrenderMusicianWoundTwo,
		BIC_surrenderMusicianWoundThree,
		BIC_surrenderMusicianWoundFour,
		BIC_surrenderMusicianShockOne,
		BIC_surrenderMusicianShockTwo,
		BIC_surrenderMusicianShockThree,
		BIC_surrenderMusicianShockFour,
		BIC_surrenderMusicianKnowledgeOne,
		BIC_surrenderMusicianKnowledgeTwo,
		BIC_surrenderMusicianKnowledgeThree,
		BIC_surrenderMusicianKnowledgeFour,
		BIC_surrenderMusicianMaster,
		BIC_showAllProfessions,
		BIC_selectAllProfession,
		BIC_equipFixturePolearm,
		BIC_unequipHeldWeapon,
		BIC_queuePolearmLegHit1,
		BIC_queueUnarmedHeadHit1,
		BIC_polearmLegHit1WeaponStatus,
		BIC_unarmedHeadHit1WeaponStatus,
		BIC_queuePolearmSpinAttack1,
		BIC_polearmSpinAttack1WeaponStatus,
		BIC_equipFixtureOneHand,
		BIC_equipFixtureTwoHand,
		BIC_queueMelee1hSpinAttack1,
		BIC_melee1hSpinAttack1WeaponStatus,
		BIC_queueMelee2hSpinAttack1,
		BIC_melee2hSpinAttack1WeaponStatus,
		BIC_queueBodyShot2,
		BIC_bodyShot2WeaponStatus,
		BIC_queueBodyShot3,
		BIC_bodyShot3WeaponStatus,
		BIC_headShot2WeaponStatus,
		BIC_queueHeadShot3,
		BIC_headShot3WeaponStatus,
		BIC_queueMelee1hBodyHit1,
		BIC_melee1hBodyHit1WeaponStatus
	};

	char const * const cms_backgroundInputMessageName = "SWGSource.PreCU.BackgroundInput.v1";
	LRESULT const cms_backgroundInputProtocolVersion = 108;
	LRESULT const cms_backgroundSkillsStatusMarker = 0x534b0000;
	LRESULT const cms_backgroundSkillsSelectionMarker = 0x53500000;
	LRESULT const cms_backgroundCombatQueueStatusMarker = 0x43510000;
	LRESULT const cms_backgroundCombatQueueStatusInCombat = 0x00008000;
	LRESULT const cms_backgroundCombatQueueStatusHasTarget = 0x00004000;
	LRESULT const cms_backgroundCombatQueueStatusCountMask = 0x00003fff;
	LRESULT const cms_backgroundCombatQueueStatusLastResult = static_cast<LRESULT>(0x0100000000000000LL);
	LRESULT const cms_backgroundCombatTimerStatusMarker = 0x544d0000;
	LRESULT const cms_backgroundCombatTimerStatusValid = 0x00000100;
	enum BackgroundCombatTimerCommand
	{
		BCTC_none = 0,
		BCTC_headShot1,
		BCTC_bodyShot1,
		BCTC_legShot1,
		BCTC_headShot2
	};
	struct BackgroundCombatTimerCapture
	{
		BackgroundCombatTimerCapture() :
			valid(false),
			command(BCTC_none),
			executeCurrent(0.0f),
			executeMax(0.0f)
		{
		}

		bool valid;
		BackgroundCombatTimerCommand command;
		float executeCurrent;
		float executeMax;
	};
	class BackgroundCombatTimerReceiver
	{
	public:
		BackgroundCombatTimerReceiver() :
			m_callback(new MessageDispatch::Callback)
		{
			m_callback->connect(
				*this,
				&BackgroundCombatTimerReceiver::onCommandTimerDataReceived,
				static_cast<PlayerCreatureController::Messages::CommandTimerDataReceived *>(0));
		}

		~BackgroundCombatTimerReceiver()
		{
			m_callback->disconnect(
				*this,
				&BackgroundCombatTimerReceiver::onCommandTimerDataReceived,
				static_cast<PlayerCreatureController::Messages::CommandTimerDataReceived *>(0));
			delete m_callback;
		}

		void onCommandTimerDataReceived(MessageQueueCommandTimer const & commandTimerData);

	private:
		BackgroundCombatTimerReceiver(BackgroundCombatTimerReceiver const &);
		BackgroundCombatTimerReceiver & operator=(BackgroundCombatTimerReceiver const &);

		MessageDispatch::Callback * m_callback;
	};
	UINT s_backgroundInputMessage = 0;
	HWND s_backgroundInputWindow = 0;
	WNDPROC s_backgroundInputPreviousWindowProc = 0;
	bool s_backgroundInputInstalled = false;
	BackgroundCombatTimerCapture s_backgroundCombatTimerCapture;
	BackgroundCombatTimerReceiver * s_backgroundCombatTimerReceiver = 0;

	void clearBackgroundCombatTimerCapture()
	{
		s_backgroundCombatTimerCapture = BackgroundCombatTimerCapture();
	}

	void BackgroundCombatTimerReceiver::onCommandTimerDataReceived(
		MessageQueueCommandTimer const & commandTimerData)
	{
		if (!s_backgroundInputInstalled ||
			!commandTimerData.hasTime(MessageQueueCommandTimer::F_execute))
			return;

		uint32 const commandCrc = commandTimerData.getCommandNameCrc();
		BackgroundCombatTimerCommand command = BCTC_none;
		if (commandCrc == Crc::normalizeAndCalculate("headShot1"))
			command = BCTC_headShot1;
		else if (commandCrc == Crc::normalizeAndCalculate("bodyShot1"))
			command = BCTC_bodyShot1;
		else if (commandCrc == Crc::normalizeAndCalculate("legShot1"))
			command = BCTC_legShot1;
		else if (commandCrc == Crc::normalizeAndCalculate("headShot2"))
			command = BCTC_headShot2;
		else
			return;

		float const executeMax =
			commandTimerData.getMaxTime(MessageQueueCommandTimer::F_execute);
		if (executeMax <= 0.0f)
			return;

		s_backgroundCombatTimerCapture.valid = true;
		s_backgroundCombatTimerCapture.command = command;
		s_backgroundCombatTimerCapture.executeCurrent =
			commandTimerData.getCurrentTime(MessageQueueCommandTimer::F_execute);
		s_backgroundCombatTimerCapture.executeMax = executeMax;
	}

	void queueBackgroundMousePosition(LPARAM lParam)
	{
		int const x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
		int const y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
		IoWinManager::queueSetSystemMouseCursorPosition(x, y);
	}

	bool queueBackgroundKey(bool down, LPARAM lParam)
	{
		if (lParam < 0 || lParam > 255)
			return false;

		int const key = static_cast<int>(lParam);
		if (down)
			IoWinManager::queueKeyDown(0, key);
		else
			IoWinManager::queueKeyUp(0, key);

		return true;
	}

	bool performBackgroundExamineCharacterSheet()
	{
		Object * const player = Game::getPlayer();
		Object * const target = CuiAction::findObjectFromFirstParam(
			Unicode::emptyString,
			true,
			false,
			CuiActions::examineCharacterSheet);
		if (!player || !target || target == player)
			return false;

		return CuiActionManager::performAction(
			CuiActions::examineCharacterSheet,
			Unicode::narrowToWide(target->getNetworkId().getValueString()));
	}

	bool performBackgroundTargetCommand(char const * const command)
	{
		Object * const player = Game::getPlayer();
		Object * const target = CuiAction::findObjectFromFirstParam(
			Unicode::emptyString,
			true,
			false,
			CuiActions::radialMenu);
		if (!player || !target || target == player)
			return false;

		return CuiMessageQueueManager::executeCommandByString(command, true);
	}

	bool performBackgroundSelfCommand(char const * const command)
	{
		if (!Game::getPlayer())
			return false;

		return CuiMessageQueueManager::executeCommandByString(command, true);
	}

	bool performBackgroundOpenStatMigration()
	{
		if (!Game::getPlayer())
			return false;

		return CuiMediatorFactory::activateInWorkspace(
			CuiMediatorTypes::WS_StatMigration) != 0;
	}

	LRESULT performBackgroundShowAllProfessions()
	{
		if (!Game::getPlayer())
			return 0;

		CuiMediator * const mediator = CuiMediatorFactory::activateInWorkspace(
			CuiMediatorTypes::WS_Skills);
		SwgCuiSkills * const skills = dynamic_cast<SwgCuiSkills *>(mediator);
		if (!skills)
			return 0;

		int const rowCount = skills->showAllProfessionsForBackgroundValidation();
		if (rowCount < 0 || rowCount > 0xffff)
			return 0;

		return cms_backgroundSkillsStatusMarker | rowCount;
	}

	LRESULT performBackgroundSelectAllProfession(int selectionIndex)
	{
		if (!Game::getPlayer())
			return 0;

		CuiMediator * const mediator = CuiMediatorFactory::activateInWorkspace(
			CuiMediatorTypes::WS_Skills);
		SwgCuiSkills * const skills = dynamic_cast<SwgCuiSkills *>(mediator);
		if (!skills)
			return 0;

		int const selectedRow =
			skills->selectAllProfessionForBackgroundValidation(selectionIndex);
		if (selectedRow < 0 || selectedRow > 0xffff)
			return 0;

		return cms_backgroundSkillsSelectionMarker | selectedRow;
	}

	bool performBackgroundTargetCounterpart()
	{
		Object * const player = Game::getPlayer();
		if (!player)
			return false;

		std::string const playerId = player->getNetworkId().getValueString();
		if (playerId == "44003778")
			CuiCombatManager::setLookAtTarget(NetworkId("39008597"));
		else if (playerId == "39008597")
			CuiCombatManager::setLookAtTarget(NetworkId("44003778"));
		else
			return false;

		return true;
	}

	bool isBackgroundCombatCanaryPair(Object const & player, NetworkId const & target)
	{
		std::string const playerId = player.getNetworkId().getValueString();
		std::string const targetId = target.getValueString();
		return (playerId == "44003778" && targetId == "39008597") ||
			(playerId == "39008597" && targetId == "44003778");
	}

	bool performBackgroundQueueCombatCanary(int const repeat)
	{
		Object * const player = Game::getPlayer();
		if (!player)
			return false;

		std::string const playerId = player->getNetworkId().getValueString();
		NetworkId const targetId = playerId == "44003778" ? NetworkId("39008597") :
			playerId == "39008597" ? NetworkId("44003778") : NetworkId::cms_invalid;
		if (!targetId.isValid() || !isBackgroundCombatCanaryPair(*player, targetId))
			return false;

		// Exercise the same admission contract used by SwgCuiToolbar with an
		// authentic Publish 14.1 command and the fixture target supplied explicitly. The
		// synchronous status returned by the window procedure observes the real
		// queue before the server can answer; no mediator row or result is forged.
		CuiCombatManager::setCombatTarget(targetId);
		clearBackgroundCombatTimerCapture();
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool queued = true;
		for (int index = 0; index < repeat; ++index)
		{
			if (ClientCommandQueue::enqueueCommand(
				"headShot1",
				targetId,
				Unicode::emptyString) == 0)
			{
				queued = false;
				break;
			}
		}
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueMarksmanTier1(char const * const command, int const repeat)
	{
		Object * const player = Game::getPlayer();
		if (!player || !command ||
			Game::getPlayerNetworkId().getValueString() != "44003778")
			return false;

		NetworkId const targetId("39008597");
		if (!isBackgroundCombatCanaryPair(*player, targetId))
			return false;

		// The fixture owns only reversible world preparation. Admission and
		// dispatch remain the production toolbar/client command queue path.
		CuiCombatManager::setCombatTarget(targetId);
		clearBackgroundCombatTimerCapture();
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool queued = true;
		for (int index = 0; index < repeat; ++index)
		{
			if (ClientCommandQueue::enqueueCommand(
				command,
				targetId,
				Unicode::emptyString) == 0)
			{
				queued = false;
				break;
			}
		}
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueDurationControl()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "44003778")
			return false;

		NetworkId const targetId("39008597");
		if (!isBackgroundCombatCanaryPair(*player, targetId))
			return false;

		// "headShot2" is an authentic Publish 14.1 player combat action with a
		// fixed 1.5-second command-table execute time and no Pre-CU duration
		// override. The identity-bound fixture owns and restores its transient
		// HAM and combat-state effects.
		CuiCombatManager::setCombatTarget(targetId);
		clearBackgroundCombatTimerCapture();
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"headShot2",
			targetId,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueHealWound(LPARAM const targetValue)
	{
		Object * const player = Game::getPlayer();
		if (!player || targetValue <= 0)
			return false;

		char targetBuffer[32];
		_snprintf(
			targetBuffer,
			sizeof(targetBuffer) - 1,
			"%lld",
			static_cast<long long>(targetValue));
		targetBuffer[sizeof(targetBuffer) - 1] = '\0';
		NetworkId const targetId(targetBuffer);
		if (!targetId.isValid())
			return false;

		// Admit the authentic Publish 14.1 command through the same client
		// queue path used by the toolbar. The server fixture supplies only a
		// disposable patient OID; medicine selection remains in the production
		// healWound adapter so no item result is forged here.
		CuiCombatManager::setLookAtTarget(targetId);
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"healWound",
			targetId,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueHealDamage(LPARAM const targetValue)
	{
		Object * const player = Game::getPlayer();
		if (!player || targetValue <= 0)
			return false;

		char targetBuffer[32];
		_snprintf(
			targetBuffer,
			sizeof(targetBuffer) - 1,
			"%lld",
			static_cast<long long>(targetValue));
		targetBuffer[sizeof(targetBuffer) - 1] = '\0';
		NetworkId const targetId(targetBuffer);
		if (!targetId.isValid())
			return false;

		// Admit the authentic five-second Publish 14.1 healDamage command
		// through the normal toolbar queue. Patient and medicine state remain
		// authoritative on the server.
		CuiCombatManager::setLookAtTarget(targetId);
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"healDamage",
			targetId,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueTending(
		char const * const commandName,
		LPARAM const targetValue,
		bool const includeTargetParameter = false)
	{
		Object * const player = Game::getPlayer();
		if (!player || !commandName || targetValue <= 0)
			return false;

		char targetBuffer[32];
		_snprintf(
			targetBuffer,
			sizeof(targetBuffer) - 1,
			"%lld",
			static_cast<long long>(targetValue));
		targetBuffer[sizeof(targetBuffer) - 1] = '\0';
		NetworkId const targetId(targetBuffer);
		if (!targetId.isValid())
			return false;

		// The bridge supplies only the disposable patient OID. The normal
		// toolbar queue and server-owned organic tending handlers retain all
		// timing, treatment, HAM, wound, battle-fatigue, and XP authority.
		CuiCombatManager::setLookAtTarget(targetId);
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		Unicode::String const commandParameters =
			includeTargetParameter
				? Unicode::narrowToWide(targetBuffer)
				: Unicode::emptyString;
		bool const queued = ClientCommandQueue::enqueueCommand(
			commandName,
			targetId,
			commandParameters) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueTendDamage(LPARAM const targetValue)
	{
		return performBackgroundQueueTending("tendDamage", targetValue);
	}

	bool performBackgroundQueueTendWound(LPARAM const targetValue)
	{
		return performBackgroundQueueTending("tendWound", targetValue);
	}

	bool performBackgroundQueueDiagnose(LPARAM const targetValue)
	{
		// Diagnose is intentionally nonqueued in the authentic client table,
		// but still enters through the normal toolbar command admission path.
		return performBackgroundQueueTending("diagnose", targetValue);
	}

	bool performBackgroundQueueMedicalForage()
	{
		if (!Game::getPlayer())
			return false;

		// Medical forage has targetType=none and is intentionally nonqueued.
		// Admit it through the normal toolbar path; the server owns its Action
		// cost, stationary delay, area depletion, chance, and reward.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"medicalForage",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueFirstAid(LPARAM const targetValue)
	{
		// First aid is intentionally nonqueued in the authentic client table.
		// The bridge supplies only the patient OID; the server owns bleeding,
		// eligibility, range, treatment strength, and zero-cost behavior.
		return performBackgroundQueueTending("firstAid", targetValue);
	}

	bool performBackgroundQueueDragIncapacitatedPlayer(LPARAM const targetValue)
	{
		// Publish 14.1 drag is intentionally nonqueued. The bridge supplies
		// only the patient OID; the server owns tier-II skill, incap/death,
		// group-or-consent, LOS, outdoor, range, and movement authority.
		return performBackgroundQueueTending(
			"dragIncapacitatedPlayer",
			targetValue);
	}

	bool performBackgroundQueueQuickHeal(LPARAM const targetValue)
	{
		// Publish 14.1 Quick Heal is intentionally nonqueued. The bridge
		// supplies only the patient OID; the server owns skill, eligibility,
		// range, Health/Action healing, Mind cost, and Focus/Willpower wounds.
		return performBackgroundQueueTending("quickHeal", targetValue);
	}

	bool performBackgroundQueueHealState(LPARAM const targetValue)
	{
		// Publish 14.1 Heal State is queued. The bridge supplies only the
		// patient OID; the server owns state and medicine selection, treatment
		// recovery, range, Mind cost, charge use, state removal, and XP.
		return performBackgroundQueueTending("healState", targetValue);
	}

	bool performBackgroundQueueCurePoison(LPARAM const targetValue)
	{
		// Publish 14.1 Cure Poison is intentionally nonqueued. The bridge
		// supplies only the patient OID; the server owns poison state, antidote
		// selection and power, treatment recovery, range, Mind, charge, and XP.
		return performBackgroundQueueTending("curePoison", targetValue);
	}

	bool performBackgroundQueueHealEnhance(LPARAM const targetValue)
	{
		// Publish 14.1 Heal Enhance is queued. The bridge supplies only the
		// fixture patient OID. The retained optional-target command row can
		// discard an unloaded look-at object, so protocol 26 also transports
		// the same OID as a command parameter for server-side resolution.
		// The server still owns facility/combat admission, enhancement-pack
		// selection, battle fatigue, Mind, charge, XP, and recovery.
		return performBackgroundQueueTending(
			"healEnhance",
			targetValue,
			true);
	}

	bool performBackgroundQueueExtinguishFire(LPARAM const targetValue)
	{
		// Publish 14.1 Extinguish Fire is intentionally nonqueued. The bridge
		// supplies only the patient OID; the server owns fire state, blanket
		// selection and power, treatment recovery, range, Mind, charge, and XP.
		return performBackgroundQueueTending("extinguishFire", targetValue);
	}

	bool performBackgroundQueueCureDisease(LPARAM const targetValue)
	{
		// Publish 14.1 Cure Disease is intentionally nonqueued. The bridge
		// supplies only the patient OID; the server owns disease state,
		// antidote selection and power, treatment recovery, range, Mind,
		// charge, and XP.
		return performBackgroundQueueTending("cureDisease", targetValue);
	}

	bool performBackgroundQueueRevivePlayer(LPARAM const targetValue)
	{
		// Publish 14.1 Revive Player is intentionally nonqueued. The bridge
		// supplies only the dead patient OID; the server owns player/death,
		// resuscitation-window, group-or-consent, PvP, range, pack, six-channel
		// healing, Focus-adjusted Mind, charge, XP, grogginess, and recovery.
		return performBackgroundQueueTending("revivePlayer", targetValue);
	}

	bool performBackgroundQueueDeathBlow(LPARAM const targetValue)
	{
		// The authentic Publish 14.1 row advertises 16 meters and queues for
		// three seconds. The bridge supplies only the victim OID; the server
		// owns the stricter five-meter, LOS, PvP, incap, feign, and death gates.
		return performBackgroundQueueTending("deathBlow", targetValue);
	}

	bool performBackgroundPerformanceCommand(
		char const * const commandName,
		char const * const commandParameters)
	{
		Object * const player = Game::getPlayer();
		if (!player || !commandName || !commandParameters ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		// Protocol 32 supplies only a fixed authentic command and parameter for
		// the identity-bound acceptance player. Skill, posture, current
		// performance, Action cost, heartbeat, and exhaustion remain owned by
		// the normal Publish 14.1 command and authoritative server scripts.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		ClientCommandQueue::enqueueCommand(
			commandName,
			NetworkId::cms_invalid,
			Unicode::narrowToWide(commandParameters));
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		// Immediate Publish 14 commands legitimately return sequence zero even
		// though they were submitted and executed. The server-side fixture owns
		// admission and is the authoritative observation boundary.
		return true;
	}

	bool performBackgroundSurrenderEntertainerMusicOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		// Protocol 35 submits one fixed ordinary Publish 14.1 surrender request
		// for the identity-bound acceptance player. The server owns dependency
		// policy, skill removal, grants/modifiers, point recovery, and XP caps.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_music_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerMusicTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		// Protocol 36 submits the next fixed Publish 14.1 progression request
		// through the ordinary actor-routed surrender command.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_music_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerMusicThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_music_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerMusicFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_music_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerMaster()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_master")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerDanceOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_dance_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerDanceTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_dance_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerDanceThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_dance_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerDanceFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_entertainer_dance_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerHairstyleOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide(
				"social_entertainer_hairstyle_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerHairstyleTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide(
				"social_entertainer_hairstyle_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerHairstyleThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide(
				"social_entertainer_hairstyle_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderEntertainerHairstyleFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide(
				"social_entertainer_hairstyle_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerNovice()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_novice")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerAbilityOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_ability_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerAbilityTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_ability_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerAbilityThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_ability_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerAbilityFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_ability_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerWoundOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_wound_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerWoundTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_wound_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerWoundThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_wound_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerWoundFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_wound_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerShockOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_shock_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerShockTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_shock_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerShockThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_shock_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerShockFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_shock_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerKnowledgeOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_knowledge_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerKnowledgeTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_knowledge_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerKnowledgeThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_knowledge_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerKnowledgeFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_knowledge_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderDancerMaster()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_dancer_master")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianNovice()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_novice")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianAbilityOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_ability_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianAbilityTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_ability_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianAbilityThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;

		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_ability_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianAbilityFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_ability_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianWoundOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_wound_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianWoundTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_wound_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianWoundThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_wound_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianWoundFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_wound_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianShockOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_shock_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianShockTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_shock_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianShockThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_shock_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianShockFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_shock_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianKnowledgeOne()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_knowledge_01")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianKnowledgeTwo()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_knowledge_02")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianKnowledgeThree()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_knowledge_03")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianKnowledgeFour()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_knowledge_04")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundSurrenderMusicianMaster()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "39008597")
			return false;
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"surrenderSkill",
			NetworkId::cms_invalid,
			Unicode::narrowToWide("social_musician_master")) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	LRESULT performBackgroundSelectCloneLocation(
		LPARAM const selectionIndex, bool const confirm)
	{
		if (!Game::getPlayer() ||
			Game::getPlayerNetworkId().getValueString() != "39008597" ||
			selectionIndex < 0 ||
			selectionIndex > 127)
			return 0;

		// The generic SUI manager requires one unambiguous active list box with
		// the exact server-authored clone title and sends its normal selection
		// and OK notifications. This bridge supplies only the row for the fixed
		// live-test victim; the server owns death state, clone locations, the
		// selected destination, penalties, and teleport.
		return static_cast<LRESULT>(
			CuiDataDrivenPageManager::selectOrConfirmSingleListRow(
				static_cast<int>(selectionIndex), confirm));
	}

	bool performBackgroundClearCombatQueue()
	{
		if (!Game::getPlayer())
			return false;

		return CuiActionManager::performAction(
			SwgCuiActions::clearCombatQueue,
			Unicode::emptyString);
	}

	bool performBackgroundEquipCdefWeapon(char const * const templateSuffix)
	{
		Object * const player = Game::getPlayer();
		std::string const playerId = Game::getPlayerNetworkId().getValueString();
		if (!player || !templateSuffix ||
			(playerId != "44003778" && playerId != "39008597"))
			return false;

		ClientObject * const inventory = CuiInventoryManager::getPlayerInventory();
		Container const * const container = inventory ? ContainerInterface::getContainer(*inventory) : 0;
		if (!container)
			return false;

		for (ContainerConstIterator iterator = container->begin(); iterator != container->end(); ++iterator)
		{
			ClientObject * const object = dynamic_cast<ClientObject *>((*iterator).getObject());
			char const * const templateName = object ? object->getObjectTemplateName() : 0;
			if (!templateName || !strstr(templateName, templateSuffix))
				continue;

			return CuiInventoryManager::equipObject(object->getNetworkId());
		}

		return false;
	}

	bool performBackgroundEquipCdefRifle()
	{
		return performBackgroundEquipCdefWeapon("rifle_cdef.iff");
	}

	bool performBackgroundEquipCdefPistol()
	{
		return performBackgroundEquipCdefWeapon("pistol_cdef.iff");
	}

	bool performBackgroundEquipCdefCarbine()
	{
		return performBackgroundEquipCdefWeapon("carbine_cdef.iff");
	}

	bool performBackgroundEquipFixtureLightsaber()
	{
		return performBackgroundEquipCdefWeapon("pistol_dl44.iff");
	}

	bool performBackgroundEquipFixtureFallbackSword()
	{
		return performBackgroundEquipCdefWeapon("pistol_dl44_metal.iff");
	}

	bool performBackgroundEquipFixturePolearm()
	{
		return performBackgroundEquipCdefWeapon("lance_staff_wood_s2.iff");
	}

	bool performBackgroundEquipFixtureOneHand()
	{
		return performBackgroundEquipCdefWeapon("sword_rantok.iff");
	}

	bool performBackgroundEquipFixtureTwoHand()
	{
		return performBackgroundEquipCdefWeapon("2h_sword_cleaver.iff");
	}

	bool performBackgroundUnequipHeldWeapon()
	{
	Object * const player = Game::getPlayer();
		if (!player || Game::getPlayerNetworkId().getValueString() != "44003778")
			return false;

		SlottedContainer * const container =
			ContainerInterface::getSlottedContainer(*player);
		if (!container)
			return false;

		Container::ContainerErrorCode error = Container::CEC_Success;
		CachedNetworkId const weaponId = container->getObjectInSlot(
			SlotIdManager::findSlotId(ConstCharCrcLowerString("hold_r")),
			error);
		if (weaponId == NetworkId::cms_invalid)
			return true;

		CuiInventoryManager::unequipObject(weaponId);
		return true;
	}

	LRESULT getBackgroundCombatQueueStatus()
	{
		CuiCombatManager::IntVector sequenceIds;
		CuiCombatManager::getCombatCommandsFromQueue(sequenceIds);

		LRESULT result = cms_backgroundCombatQueueStatusMarker;
		CachedNetworkId combatTarget;
		if (CuiCombatManager::isInCombat(Game::getPlayerCreature(), combatTarget))
			result |= cms_backgroundCombatQueueStatusInCombat;
		if (CuiCombatManager::getCombatTarget().isValid())
			result |= cms_backgroundCombatQueueStatusHasTarget;

		size_t const count = sequenceIds.size();
		result |= static_cast<LRESULT>(std::min(
			count,
			static_cast<size_t>(cms_backgroundCombatQueueStatusCountMask)));

		uint32 sequenceId = 0;
		Command::ErrorCode status = Command::CEC_Success;
		int statusDetail = 0;
		if (ClientCommandQueue::getLastCommandRemoval(sequenceId, status, statusDetail))
		{
			UNREF(sequenceId);
			result |= cms_backgroundCombatQueueStatusLastResult;
			result |= static_cast<LRESULT>(static_cast<int>(status) & 0xff) << 32;
			result |= static_cast<LRESULT>(static_cast<unsigned int>(statusDetail) & 0xffffu) << 40;
		}
		return result;
	}

	LRESULT getBackgroundCombatTimerStatus()
	{
		LRESULT result = cms_backgroundCombatTimerStatusMarker;
		if (!s_backgroundCombatTimerCapture.valid)
			return result;

		int const currentMilliseconds = std::max(
			0,
			std::min(
				65535,
				static_cast<int>(
					s_backgroundCombatTimerCapture.executeCurrent * 1000.0f + 0.5f)));
		int const maxMilliseconds = std::max(
			0,
			std::min(
				65535,
				static_cast<int>(
					s_backgroundCombatTimerCapture.executeMax * 1000.0f + 0.5f)));
		result |= cms_backgroundCombatTimerStatusValid;
		result |= static_cast<LRESULT>(s_backgroundCombatTimerCapture.command) & 0xff;
		result |= static_cast<LRESULT>(maxMilliseconds & 0xffff) << 32;
		result |= static_cast<LRESULT>(currentMilliseconds & 0xffff) << 48;
		return result;
	}

	LRESULT getBackgroundGeneratedCombatWeaponStatus(char const * const commandName)
	{
		uint64 result = 0x57440000ULL;
		CreatureObject const * const player = Game::getPlayerCreature();
		int const weaponType =
			ClientCommandChecks::getCurrentWeaponTypeForDiagnostics(player);

		Command const & command =
			CommandTable::getCommand(Crc::normalizeAndCalculate(commandName));
		if (!command.isNull())
			result |= 0x00000200ULL;
		if (weaponType >= 0 && !command.isNull() &&
			!ClientCommandChecks::doesWeaponInvalidateCommand(&command, player))
		{
			result |= 0x00000100ULL;
		}

		result |= static_cast<uint64>((weaponType >= 0 ? weaponType : 0xff) & 0xff);
		result |= static_cast<uint64>(command.m_weaponTypesValid & 0xffffU) << 32;
		result |= static_cast<uint64>(command.m_weaponTypesInvalid & 0xffffU) << 48;
		return static_cast<LRESULT>(result);
	}

	LRESULT CALLBACK backgroundInputWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (s_backgroundInputInstalled && message == s_backgroundInputMessage)
		{
			switch (static_cast<int>(wParam))
			{
			case BIC_ping:
				return cms_backgroundInputProtocolVersion;

			case BIC_mouseMove:
				queueBackgroundMousePosition(lParam);
				return 1;

			case BIC_leftMouseDown:
				queueBackgroundMousePosition(lParam);
				IoWinManager::queueMouseButtonDown(0, 0);
				return 1;

			case BIC_leftMouseUp:
				queueBackgroundMousePosition(lParam);
				IoWinManager::queueMouseButtonUp(0, 0);
				return 1;

			case BIC_rightMouseDown:
				queueBackgroundMousePosition(lParam);
				IoWinManager::queueMouseButtonDown(0, 1);
				return 1;

			case BIC_rightMouseUp:
				queueBackgroundMousePosition(lParam);
				IoWinManager::queueMouseButtonUp(0, 1);
				return 1;

			case BIC_middleMouseDown:
				queueBackgroundMousePosition(lParam);
				IoWinManager::queueMouseButtonDown(0, 2);
				return 1;

			case BIC_middleMouseUp:
				queueBackgroundMousePosition(lParam);
				IoWinManager::queueMouseButtonUp(0, 2);
				return 1;

			case BIC_keyDown:
				return queueBackgroundKey(true, lParam) ? 1 : 0;

			case BIC_keyUp:
				return queueBackgroundKey(false, lParam) ? 1 : 0;

			case BIC_character:
				if (lParam <= 0 || lParam > 0xffff)
					return 0;
				IoWinManager::queueCharacter(0, static_cast<int>(lParam));
				return 1;

			case BIC_inputReset:
				IoWinManager::queueInputReset();
				return 1;

			case BIC_examineCharacterSheet:
				return performBackgroundExamineCharacterSheet() ? 1 : 0;

			case BIC_inviteTarget:
				return performBackgroundTargetCommand("/invite") ? 1 : 0;

			case BIC_joinGroup:
				return performBackgroundSelfCommand("/join") ? 1 : 0;

			case BIC_disbandGroup:
				return performBackgroundSelfCommand("/disband") ? 1 : 0;

			case BIC_openStatMigration:
				return performBackgroundOpenStatMigration() ? 1 : 0;

			case BIC_startImageDesign:
				return performBackgroundTargetCommand("/imagedesign") ? 1 : 0;

			case BIC_targetCounterpart:
				return performBackgroundTargetCounterpart() ? 1 : 0;

			case BIC_queueCombatCanary:
				if (lParam < 1 || lParam > 16 || !performBackgroundQueueCombatCanary(static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_clearCombatQueue:
				return performBackgroundClearCombatQueue() ? 1 : 0;

			case BIC_combatQueueStatus:
				return getBackgroundCombatQueueStatus();

			case BIC_equipCdefRifle:
				return performBackgroundEquipCdefRifle() ? 1 : 0;

			case BIC_stand:
				return performBackgroundSelfCommand("/stand") ? 1 : 0;

			case BIC_queueBodyShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1("bodyShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueLegShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1("legShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_equipCdefPistol:
				return performBackgroundEquipCdefPistol() ? 1 : 0;

			case BIC_equipCdefCarbine:
				return performBackgroundEquipCdefCarbine() ? 1 : 0;

			case BIC_combatTimerStatus:
				return getBackgroundCombatTimerStatus();

			case BIC_queueDurationControl:
				if (!performBackgroundQueueDurationControl())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_equipFixtureLightsaber:
				return performBackgroundEquipFixtureLightsaber() ? 1 : 0;

			case BIC_equipFixtureFallbackSword:
				return performBackgroundEquipFixtureFallbackSword() ? 1 : 0;

			case BIC_queueHealWound:
				if (!performBackgroundQueueHealWound(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueHealDamage:
				if (!performBackgroundQueueHealDamage(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueTendDamage:
				if (!performBackgroundQueueTendDamage(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueTendWound:
				if (!performBackgroundQueueTendWound(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueDiagnose:
				if (!performBackgroundQueueDiagnose(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMedicalForage:
				if (!performBackgroundQueueMedicalForage())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueFirstAid:
				if (!performBackgroundQueueFirstAid(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueDragIncapacitatedPlayer:
				if (!performBackgroundQueueDragIncapacitatedPlayer(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueQuickHeal:
				if (!performBackgroundQueueQuickHeal(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueHealState:
				if (!performBackgroundQueueHealState(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueCurePoison:
				if (!performBackgroundQueueCurePoison(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueHealEnhance:
				if (!performBackgroundQueueHealEnhance(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueExtinguishFire:
				if (!performBackgroundQueueExtinguishFire(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueCureDisease:
				if (!performBackgroundQueueCureDisease(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueRevivePlayer:
				if (!performBackgroundQueueRevivePlayer(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueDeathBlow:
				if (!performBackgroundQueueDeathBlow(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_selectCloneLocation:
				return performBackgroundSelectCloneLocation(lParam, false);

			case BIC_confirmCloneLocation:
				return performBackgroundSelectCloneLocation(lParam, true);

			case BIC_startDanceRhythmic:
				return performBackgroundPerformanceCommand(
					"startDance", "rhythmic") ? 1 : 0;

			case BIC_flourishOne:
				return performBackgroundPerformanceCommand(
					"flourish", "1") ? 1 : 0;

			case BIC_stopDance:
				return performBackgroundPerformanceCommand(
					"stopDance", "") ? 1 : 0;

			case BIC_startMusicStarwars1:
				return performBackgroundPerformanceCommand(
					"startMusic", "starwars1") ? 1 : 0;

			case BIC_stopMusic:
				return performBackgroundPerformanceCommand(
					"stopMusic", "") ? 1 : 0;

			case BIC_startBandStarwars1:
				return performBackgroundPerformanceCommand(
					"startBand", "starwars1") ? 1 : 0;

			case BIC_bandFlourishOne:
				return performBackgroundPerformanceCommand(
					"bandFlourish", "1") ? 1 : 0;

			case BIC_stopBand:
				return performBackgroundPerformanceCommand(
					"stopBand", "") ? 1 : 0;

			case BIC_startMusicRock:
				return performBackgroundPerformanceCommand(
					"startMusic", "rock") ? 1 : 0;

			case BIC_surrenderEntertainerMusicOne:
				return performBackgroundSurrenderEntertainerMusicOne()
					? 1
					: 0;

			case BIC_startMusicStarwars2:
				return performBackgroundPerformanceCommand(
					"startMusic", "starwars2") ? 1 : 0;

			case BIC_surrenderEntertainerMusicTwo:
				return performBackgroundSurrenderEntertainerMusicTwo()
					? 1
					: 0;

			case BIC_startMusicFolk:
				return performBackgroundPerformanceCommand(
					"startMusic", "folk") ? 1 : 0;

			case BIC_surrenderEntertainerMusicThree:
				return performBackgroundSurrenderEntertainerMusicThree()
					? 1
					: 0;

			case BIC_startMusicStarwars3:
				return performBackgroundPerformanceCommand(
					"startMusic", "starwars3") ? 1 : 0;

			case BIC_surrenderEntertainerMusicFour:
				return performBackgroundSurrenderEntertainerMusicFour()
					? 1
					: 0;

			case BIC_startMusicCeremonial:
				return performBackgroundPerformanceCommand(
					"startMusic", "ceremonial") ? 1 : 0;

			case BIC_surrenderEntertainerMaster:
				return performBackgroundSurrenderEntertainerMaster()
					? 1
					: 0;

			case BIC_startDanceBasicTwo:
				return performBackgroundPerformanceCommand(
					"startDance", "basic2") ? 1 : 0;

			case BIC_surrenderEntertainerDanceOne:
				return performBackgroundSurrenderEntertainerDanceOne()
					? 1
					: 0;

			case BIC_startDanceRhythmicTwo:
				return performBackgroundPerformanceCommand(
					"startDance", "rhythmic2") ? 1 : 0;

			case BIC_surrenderEntertainerDanceTwo:
				return performBackgroundSurrenderEntertainerDanceTwo()
					? 1
					: 0;

			case BIC_startDanceFootloose:
				return performBackgroundPerformanceCommand(
					"startDance", "footloose") ? 1 : 0;

			case BIC_surrenderEntertainerDanceThree:
				return performBackgroundSurrenderEntertainerDanceThree()
					? 1
					: 0;

			case BIC_startDanceFormal:
				return performBackgroundPerformanceCommand(
					"startDance", "formal") ? 1 : 0;

			case BIC_surrenderEntertainerDanceFour:
				return performBackgroundSurrenderEntertainerDanceFour()
					? 1
					: 0;

			case BIC_surrenderEntertainerHairstyleOne:
				return performBackgroundSurrenderEntertainerHairstyleOne()
					? 1
					: 0;

			case BIC_surrenderEntertainerHairstyleTwo:
				return performBackgroundSurrenderEntertainerHairstyleTwo()
					? 1
					: 0;

			case BIC_surrenderEntertainerHairstyleThree:
				return performBackgroundSurrenderEntertainerHairstyleThree()
					? 1
					: 0;

			case BIC_surrenderEntertainerHairstyleFour:
				return performBackgroundSurrenderEntertainerHairstyleFour()
					? 1
					: 0;

			case BIC_startDancePopular:
				return performBackgroundPerformanceCommand(
					"startDance", "popular") ? 1 : 0;

			case BIC_surrenderDancerNovice:
				return performBackgroundSurrenderDancerNovice()
					? 1
					: 0;

			case BIC_surrenderDancerAbilityOne:
				return performBackgroundSurrenderDancerAbilityOne()
					? 1
					: 0;

			case BIC_surrenderDancerAbilityTwo:
				return performBackgroundSurrenderDancerAbilityTwo()
					? 1
					: 0;

			case BIC_surrenderDancerAbilityThree:
				return performBackgroundSurrenderDancerAbilityThree()
					? 1
					: 0;

			case BIC_surrenderDancerAbilityFour:
				return performBackgroundSurrenderDancerAbilityFour()
					? 1
					: 0;

			case BIC_surrenderDancerWoundOne:
				return performBackgroundSurrenderDancerWoundOne()
					? 1
					: 0;

			case BIC_surrenderDancerWoundTwo:
				return performBackgroundSurrenderDancerWoundTwo()
					? 1
					: 0;

			case BIC_surrenderDancerWoundThree:
				return performBackgroundSurrenderDancerWoundThree()
					? 1
					: 0;

			case BIC_surrenderDancerWoundFour:
				return performBackgroundSurrenderDancerWoundFour()
					? 1
					: 0;

			case BIC_surrenderDancerShockOne:
				return performBackgroundSurrenderDancerShockOne()
					? 1
					: 0;

			case BIC_surrenderDancerShockTwo:
				return performBackgroundSurrenderDancerShockTwo()
					? 1
					: 0;

			case BIC_surrenderDancerShockThree:
				return performBackgroundSurrenderDancerShockThree()
					? 1
					: 0;

			case BIC_surrenderDancerShockFour:
				return performBackgroundSurrenderDancerShockFour()
					? 1
					: 0;

			case BIC_surrenderDancerKnowledgeOne:
				return performBackgroundSurrenderDancerKnowledgeOne()
					? 1
					: 0;

			case BIC_surrenderDancerKnowledgeTwo:
				return performBackgroundSurrenderDancerKnowledgeTwo()
					? 1
					: 0;

			case BIC_surrenderDancerKnowledgeThree:
				return performBackgroundSurrenderDancerKnowledgeThree()
					? 1
					: 0;

			case BIC_surrenderDancerKnowledgeFour:
				return performBackgroundSurrenderDancerKnowledgeFour()
					? 1
					: 0;

			case BIC_surrenderDancerMaster:
				return performBackgroundSurrenderDancerMaster()
					? 1
					: 0;

			case BIC_surrenderMusicianNovice:
				return performBackgroundSurrenderMusicianNovice()
					? 1
					: 0;

			case BIC_surrenderMusicianAbilityOne:
				return performBackgroundSurrenderMusicianAbilityOne()
					? 1
					: 0;

			case BIC_surrenderMusicianAbilityTwo:
				return performBackgroundSurrenderMusicianAbilityTwo()
					? 1
					: 0;

			case BIC_surrenderMusicianAbilityThree:
				return performBackgroundSurrenderMusicianAbilityThree()
					? 1
					: 0;

			case BIC_surrenderMusicianAbilityFour:
				return performBackgroundSurrenderMusicianAbilityFour()
					? 1
					: 0;

			case BIC_surrenderMusicianWoundOne:
				return performBackgroundSurrenderMusicianWoundOne()
					? 1
					: 0;

			case BIC_surrenderMusicianWoundTwo:
				return performBackgroundSurrenderMusicianWoundTwo()
					? 1
					: 0;

			case BIC_surrenderMusicianWoundThree:
				return performBackgroundSurrenderMusicianWoundThree()
					? 1
					: 0;

			case BIC_surrenderMusicianWoundFour:
				return performBackgroundSurrenderMusicianWoundFour()
					? 1
					: 0;

			case BIC_surrenderMusicianShockOne:
				return performBackgroundSurrenderMusicianShockOne()
					? 1
					: 0;

			case BIC_surrenderMusicianShockTwo:
				return performBackgroundSurrenderMusicianShockTwo()
					? 1
					: 0;

			case BIC_surrenderMusicianShockThree:
				return performBackgroundSurrenderMusicianShockThree()
					? 1
					: 0;

			case BIC_surrenderMusicianShockFour:
				return performBackgroundSurrenderMusicianShockFour()
					? 1
					: 0;

			case BIC_surrenderMusicianKnowledgeOne:
				return performBackgroundSurrenderMusicianKnowledgeOne()
					? 1
					: 0;

			case BIC_surrenderMusicianKnowledgeTwo:
				return performBackgroundSurrenderMusicianKnowledgeTwo()
					? 1
					: 0;

			case BIC_surrenderMusicianKnowledgeThree:
				return performBackgroundSurrenderMusicianKnowledgeThree()
					? 1
					: 0;

			case BIC_surrenderMusicianKnowledgeFour:
				return performBackgroundSurrenderMusicianKnowledgeFour()
					? 1
					: 0;

			case BIC_surrenderMusicianMaster:
				return performBackgroundSurrenderMusicianMaster()
					? 1
					: 0;

			case BIC_showAllProfessions:
				return performBackgroundShowAllProfessions();

			case BIC_selectAllProfession:
				return performBackgroundSelectAllProfession(static_cast<int>(lParam));

			case BIC_equipFixturePolearm:
				return performBackgroundEquipFixturePolearm() ? 1 : 0;

			case BIC_unequipHeldWeapon:
				return performBackgroundUnequipHeldWeapon() ? 1 : 0;

			case BIC_queuePolearmLegHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmLegHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueUnarmedHeadHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedHeadHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmLegHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmLegHit1");

			case BIC_unarmedHeadHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedHeadHit1");

			case BIC_queuePolearmSpinAttack1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmSpinAttack1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmSpinAttack1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmSpinAttack1");

			case BIC_equipFixtureOneHand:
				return performBackgroundEquipFixtureOneHand() ? 1 : 0;

			case BIC_equipFixtureTwoHand:
				return performBackgroundEquipFixtureTwoHand() ? 1 : 0;

			case BIC_queueMelee1hSpinAttack1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hSpinAttack1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hSpinAttack1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee1hSpinAttack1");

			case BIC_queueMelee2hSpinAttack1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hSpinAttack1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hSpinAttack1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hSpinAttack1");

			case BIC_queueBodyShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1("bodyShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_bodyShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("bodyShot2");

			case BIC_queueBodyShot3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1("bodyShot3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_bodyShot3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("bodyShot3");

			case BIC_headShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("headShot2");

			case BIC_queueHeadShot3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1("headShot3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_headShot3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("headShot3");

			case BIC_queueMelee1hBodyHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hBodyHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hBodyHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee1hBodyHit1");

			default:
				return 0;
			}
		}

		if (s_backgroundInputPreviousWindowProc)
			return CallWindowProc(s_backgroundInputPreviousWindowProc, hwnd, message, wParam, lParam);

		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	bool installBackgroundInputBridge()
	{
		if (!ConfigFile::getKeyBool("SwgClient", "enableBackgroundInputBridge", false))
			return false;

		if (s_backgroundInputInstalled)
			return true;

		HWND const window = Os::getWindow();
		if (!window)
		{
			WARNING(true, ("Pre-CU background input bridge has no client window"));
			return false;
		}

		UINT const registeredMessage = RegisterWindowMessageA(cms_backgroundInputMessageName);
		if (!registeredMessage)
		{
			WARNING(true, ("Pre-CU background input bridge could not register its window message"));
			return false;
		}

		SetLastError(ERROR_SUCCESS);
		LONG_PTR const previousWindowProc = SetWindowLongPtr(
			window,
			GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(&backgroundInputWindowProc));
		if (previousWindowProc == 0 && GetLastError() != ERROR_SUCCESS)
		{
			WARNING(true, ("Pre-CU background input bridge could not subclass the client window"));
			return false;
		}

		s_backgroundInputMessage = registeredMessage;
		s_backgroundInputWindow = window;
		s_backgroundInputPreviousWindowProc = reinterpret_cast<WNDPROC>(previousWindowProc);
		s_backgroundInputInstalled = true;
		clearBackgroundCombatTimerCapture();
		s_backgroundCombatTimerReceiver = new BackgroundCombatTimerReceiver;

		REPORT_LOG(true, ("Pre-CU background input bridge enabled (message=0x%04x, protocol=%d)\n",
			static_cast<unsigned int>(s_backgroundInputMessage),
			static_cast<int>(cms_backgroundInputProtocolVersion)));
		return true;
	}

	void removeBackgroundInputBridge()
	{
		if (!s_backgroundInputInstalled)
			return;

		if (s_backgroundInputWindow && IsWindow(s_backgroundInputWindow) && s_backgroundInputPreviousWindowProc)
		{
			WNDPROC const currentWindowProc = reinterpret_cast<WNDPROC>(
				GetWindowLongPtr(s_backgroundInputWindow, GWLP_WNDPROC));
			if (currentWindowProc != &backgroundInputWindowProc)
			{
				WARNING(true, ("Pre-CU background input bridge is no longer the active client window subclass"));
				return;
			}

			SetLastError(ERROR_SUCCESS);
			LONG_PTR const result = SetWindowLongPtr(
				s_backgroundInputWindow,
				GWLP_WNDPROC,
				reinterpret_cast<LONG_PTR>(s_backgroundInputPreviousWindowProc));
			if (result == 0 && GetLastError() != ERROR_SUCCESS)
			{
				WARNING(true, ("Pre-CU background input bridge could not restore the client window procedure"));
				return;
			}
		}

		s_backgroundInputInstalled = false;
		delete s_backgroundCombatTimerReceiver;
		s_backgroundCombatTimerReceiver = 0;
		clearBackgroundCombatTimerCapture();
		s_backgroundInputPreviousWindowProc = 0;
		s_backgroundInputWindow = 0;
		s_backgroundInputMessage = 0;
	}

	void installConfigFileOverride ()
	{
		AbstractFile * const abstractFile = TreeFile::open ("misc/override.cfg", AbstractFile::PriorityData, true);
		if (abstractFile)
		{
			int const length = abstractFile->length ();
			byte * const data = abstractFile->readEntireFileAndClose ();
			IGNORE_RETURN (ConfigFile::loadFromBuffer (reinterpret_cast<char const *> (data), length));
			delete [] data;
			delete abstractFile;
		}
	}
}

using namespace ClientMainNamespace;

// ======================================================================
// Entry point for the application
//
// Return Value:
//
//   Result code to return to the operating system
//
// Remarks:
//
//   This routine should set up the engine, invoke the main game loop,
//   and then tear down the engine.

int ClientMain(
	HINSTANCE hInstance,      // handle to current instance
	HINSTANCE hPrevInstance,  // handle to previous instance
	LPSTR     lpCmdLine,      // pointer to command line
	int       nCmdShow        // show state of window
)
{
	UNREF(hPrevInstance);
	UNREF(nCmdShow);


	//-- thread
	SetupSharedThread::install();

	//-- debug
	SetupSharedDebug::install(4096);

	InstallTimer rootInstallTimer("root");

	char clientWindowName[128] = "Star Wars Galaxies";

#if PRODUCTION != 1
	snprintf(clientWindowName, sizeof(clientWindowName), "SwgClient (%s.%s)", Branch().getBranchName().c_str(), ApplicationVersion::getPublicVersion());
	clientWindowName[sizeof(clientWindowName) - 1] = '\0';
#endif


	//-- foundation
	SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_game);
	data.windowName = clientWindowName;
	data.windowNormalIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	data.windowSmallIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2));
	data.hInstance = hInstance;
	data.commandLine = lpCmdLine;
#if PRODUCTION == 0
	data.configFile = "client_d.cfg";
#else
	data.configFile = "client.cfg";
#endif
	data.clockUsesSleep = true;
	data.minFrameRate = 1.f;
	data.frameRateLimit = 144.f;
#if PRODUCTION
	data.demoMode = true;
#endif
	data.writeMiniDumps = true; // SWG Source Change - Just always write crash log .txt files, there's no reason not to

	SetupSharedFoundation::install(data);

	REPORT_LOG(true, ("ClientMain: Command Line = \"%s\"\n", lpCmdLine));
	REPORT_LOG(true, ("ClientMain: Memory size = %i MB\n", MemoryManager::getLimit()));

	// check for any config file entries
	if (ConfigFile::isEmpty())
		FATAL(true, ("Config file not specified"));

	InstallTimer::checkConfigFile();

	SetLastError(0);
	HANDLE semaphore = CreateSemaphore(NULL, 0, 1, "SwgClientInstanceRunning");
	if (GetLastError() == ERROR_ALREADY_EXISTS && !ConfigFile::getKeyBool("SwgClient", "allowMultipleInstances", PRODUCTION ? false : true))
	{
		MessageBox(NULL, "Another instance of this application is already running.  Application will now close.", NULL, MB_OK | MB_ICONSTOP);
	}
	else
	{
		{
			uint32 gameFeatures = ConfigFile::getKeyInt("Station", "gameFeatures", 0) & ~ConfigFile::getKeyInt("ClientGame", "gameBitsToClear", 0);
			// hack to set retail if beta or preorder
			if (ConfigFile::getKeyBool("ClientGame", "setJtlRetailIfBetaIsSet", 0))
			{
				if (gameFeatures & (ClientGameFeature::SpaceExpansionBeta | ClientGameFeature::SpaceExpansionPreOrder))
					gameFeatures |= ClientGameFeature::SpaceExpansionRetail;
			}

			//-- set ep3 retail if beta or preorder
			if (gameFeatures & (ClientGameFeature::Episode3PreorderDownload))
				gameFeatures |= ClientGameFeature::Episode3ExpansionRetail;

			//-- set Obiwan retail if beta or preorder
			if (gameFeatures & ClientGameFeature::TrialsOfObiwanPreorder)
				gameFeatures |= ClientGameFeature::TrialsOfObiwanRetail;
			Game::setGameFeatureBits(gameFeatures);
			Game::setSubscriptionFeatureBits(ConfigFile::getKeyInt("Station", "subscriptionFeatures", 0));
			Game::setExternalCommandHandler(externalCommandHandler);
		}

		{
			SetupSharedCompression::Data data;
			data.numberOfThreadsAccessingZlib = 3;
			SetupSharedCompression::install(data);
		}

		//-- Regular expression support.
		SetupSharedRegex::install();

		//-- file
		{
			// figure out what skus we need to support in the tree file system
			uint32 skuBits = 0;
			if ((Game::getGameFeatureBits() & ClientGameFeature::Base) != 0)
				skuBits |= BINARY1(0001);
			if ((Game::getGameFeatureBits() & ClientGameFeature::SpaceExpansionRetail) != 0)
				skuBits |= BINARY1(0010);
			if ((Game::getGameFeatureBits() & ClientGameFeature::Episode3ExpansionRetail) != 0)
				skuBits |= BINARY1(0100);
			if ((Game::getGameFeatureBits() & ClientGameFeature::TrialsOfObiwanRetail) != 0)
				skuBits |= BINARY1(1000);

			SetupSharedFile::install(true, skuBits);
		}

		installConfigFileOverride();

		//-- math
		SetupSharedMath::install();

		//-- utility
		SetupSharedUtility::Data setupUtilityData;
		SetupSharedUtility::setupGameData(setupUtilityData);
		setupUtilityData.m_allowFileCaching = true;
		SetupSharedUtility::install(setupUtilityData);

		//-- random
		SetupSharedRandom::install(static_cast<uint32>(time(NULL)));

		SetupSharedLog::install("SwgClient");

		//-- image
		SetupSharedImage::Data setupImageData;
		SetupSharedImage::setupDefaultData(setupImageData);
		SetupSharedImage::install(setupImageData);

		//-- network
		SetupSharedNetwork::SetupData  networkSetupData;
		SetupSharedNetwork::getDefaultClientSetupData(networkSetupData);
		SetupSharedNetwork::install(networkSetupData);

		SetupSharedNetworkMessages::install();
		SetupSwgSharedNetworkMessages::install();

		//-- object
		SetupSharedObject::Data setupObjectData;
		SetupSharedObject::setupDefaultGameData(setupObjectData);
		setupObjectData.useTimedAppearanceTemplates = true;
		// we want the SlotIdManager initialized, and we need the associated hardpoint names loaded.
		SetupSharedObject::addSlotIdManagerData(setupObjectData, true);
		// we want CustomizationData support on the client.
		SetupSharedObject::addCustomizationSupportData(setupObjectData);
		SetupSharedObject::addMovementTableData(setupObjectData);
		SetupSharedObject::install(setupObjectData);

		//-- game
		SetupSharedGame::Data setupSharedGameData;

		setupSharedGameData.setUseGameScheduler(true);
		setupSharedGameData.setUseMountValidScaleRangeTable(true);
		setupSharedGameData.m_debugBadStringsFunc = CuiManager::debugBadStringIdsFunc;
		SetupSharedGame::install(setupSharedGameData);

		CommoditiesAdvancedSearchAttribute::install();
		SwgCuiAuctionFilter::buildAttributeFilterDisplayString(); // must be called after CommoditiesAdvancedSearchAttribute::install()

		//-- terrain
		SetupSharedTerrain::Data setupSharedTerrainData;
		SetupSharedTerrain::setupGameData(setupSharedTerrainData);
		SetupSharedTerrain::install(setupSharedTerrainData);

		//-- SharedXml
		SetupSharedXml::install();

		//-- pathfinding
		SetupSharedPathfinding::install();

		//-- setup client

		//-- audio
		SetupClientAudio::install();

		//-- graphics
		SetupClientGraphics::Data setupGraphicsData;
		setupGraphicsData.screenWidth = 1024;
		setupGraphicsData.screenHeight = 768;
		setupGraphicsData.alphaBufferBitDepth = 0;
		SetupClientGraphics::setupDefaultGameData(setupGraphicsData);

		if (SetupClientGraphics::install(setupGraphicsData))
		{
			VideoList::install(Audio::getMilesDigitalDriver());

			//-- directinput
			SetupClientDirectInput::install(hInstance, Os::getWindow(), DIK_LCONTROL, Graphics::isWindowed);
			DirectInput::setScreenShotFunction(ScreenShotHelper::screenShot);
			DirectInput::setToggleWindowedModeFunction(Graphics::toggleWindowedMode);
			DirectInput::setRequestDebugMenuFunction(Os::requestPopupDebugMenu);
			Os::setLostFocusHookFunction(DirectInput::unacquireAllDevices);

			//-- object
			SetupClientObject::Data setupClientObjectData;
			SetupClientObject::setupGameData(setupClientObjectData);
			SetupClientObject::install(setupClientObjectData);

			//-- animation and skeletal animation
			SetupClientAnimation::install();

			SetupClientSkeletalAnimation::Data  saData;
			SetupClientSkeletalAnimation::setupGameData(saData);
			SetupClientSkeletalAnimation::install(saData);

			//-- texture renderer
			SetupClientTextureRenderer::install();

			//-- terrain
			SetupClientTerrain::install();

			//-- particle system
			SetupClientParticle::install();

			//-- game
			SetupClientGame::Data data;
			SetupClientGame::setupGameData(data);
			SetupClientGame::install(data);

			CuiManager::setImplementationInstallFunctions(SwgCuiManager::install, SwgCuiManager::remove, SwgCuiManager::update);
			CuiManager::setImplementationTestFunction(SwgCuiManager::test);

			SetupClientBugReporting::install();

			//-- iowin
			SetupSharedIoWin::install();

			//-- setup the client user interface.
			SetupSwgClientUserInterface::install();
			bool const backgroundInputBridgeInstalled = installBackgroundInputBridge();

			//-- G15 LCD
			SwgCuiG15Lcd::initializeLcd();

			//-- run game
			rootInstallTimer.manualExit();
			SetupSharedFoundation::callbackWithExceptionHandling(Game::run);
			if (backgroundInputBridgeInstalled)
				removeBackgroundInputBridge();

			//-- save options
			// @todo: write a flexible options load/save system, both of ours suck
			CuiWorkspace * workspace = CuiWorkspace::getGameWorkspace();
			if (workspace != NULL)
			{
				workspace->saveAllSettings();
				SwgCuiChatWindow * chatWindow = safe_cast<SwgCuiChatWindow *>(workspace->findMediatorByType(typeid(SwgCuiChatWindow)));
				if (chatWindow != NULL)
					chatWindow->saveSettings();
			}
			CuiSettings::save();
			CuiChatHistory::save();
			CurrentUserOptionManager::save();
			LocalMachineOptionManager::save();
		}
	}

	SetupSharedFoundation::remove();
	SetupSharedThread::remove();

	if (semaphore)
		CloseHandle(semaphore);
	return 0;

}
// ======================================================================
