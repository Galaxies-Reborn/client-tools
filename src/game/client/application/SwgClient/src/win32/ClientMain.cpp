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
		BIC_melee1hBodyHit1WeaponStatus,
		BIC_queueMelee1hBodyHit2,
		BIC_melee1hBodyHit2WeaponStatus,
		BIC_queueMelee1hBodyHit3,
		BIC_melee1hBodyHit3WeaponStatus,
		BIC_queueMelee2hHeadHit1,
		BIC_melee2hHeadHit1WeaponStatus,
		BIC_queueMelee2hHeadHit2,
		BIC_melee2hHeadHit2WeaponStatus,
		BIC_queueMelee2hHeadHit3,
		BIC_melee2hHeadHit3WeaponStatus,
		BIC_queueMelee1hHit1,
		BIC_melee1hHit1WeaponStatus,
		BIC_queueMelee1hHit2,
		BIC_melee1hHit2WeaponStatus,
		BIC_queueMelee2hHit1,
		BIC_melee2hHit1WeaponStatus,
		BIC_queueMelee2hHit2,
		BIC_melee2hHit2WeaponStatus,
		BIC_queuePolearmLegHit2,
		BIC_polearmLegHit2WeaponStatus,
		BIC_queuePolearmLegHit3,
		BIC_polearmLegHit3WeaponStatus,
		BIC_queuePolearmHit1,
		BIC_polearmHit1WeaponStatus,
		BIC_queuePolearmArea1,
		BIC_polearmArea1WeaponStatus,
		BIC_queueMelee2hSpinAttack2,
		BIC_melee2hSpinAttack2WeaponStatus,
		BIC_queueBurstShot1,
		BIC_burstShot1WeaponStatus,
		BIC_queueDisarmingShot1,
		BIC_disarmingShot1WeaponStatus,
		BIC_queueDoubleTap,
		BIC_doubleTapWeaponStatus,
		BIC_queueStoppingShot,
		BIC_stoppingShotWeaponStatus,
		BIC_queueCripplingShot,
		BIC_cripplingShotWeaponStatus,
		BIC_queuePointBlankSingle2,
		BIC_pointBlankSingle2WeaponStatus,
		BIC_queuePointBlankArea1,
		BIC_pointBlankArea1WeaponStatus,
		BIC_queuePointBlankArea2,
		BIC_pointBlankArea2WeaponStatus,
		BIC_queueMultiTargetPistolShot,
		BIC_multiTargetPistolShotWeaponStatus,
		BIC_queueDisarmingShot2,
		BIC_disarmingShot2WeaponStatus,
		BIC_queueFanShot,
		BIC_fanShotWeaponStatus,
		BIC_queueBurstShot2,
		BIC_burstShot2WeaponStatus,
		BIC_queueUnarmedHit1,
		BIC_unarmedHit1WeaponStatus,
		BIC_queueUnarmedHit2,
		BIC_unarmedHit2WeaponStatus,
		BIC_queueUnarmedBodyHit1,
		BIC_unarmedBodyHit1WeaponStatus,
		BIC_queueUnarmedLegHit1,
		BIC_unarmedLegHit1WeaponStatus,
		BIC_queueUnarmedSpinAttack1,
		BIC_unarmedSpinAttack1WeaponStatus,
		BIC_queueUnarmedSpinAttack2,
		BIC_unarmedSpinAttack2WeaponStatus,
		BIC_queueOverChargeShot2,
		BIC_overChargeShot2WeaponStatus,
		BIC_equipFixtureAcid,
		BIC_queueFireAcidSingle1,
		BIC_fireAcidSingle1WeaponStatus,
		BIC_equipFixtureLightning,
		BIC_queueFireLightningSingle1,
		BIC_fireLightningSingle1WeaponStatus,
		BIC_queueFireAcidCone1,
		BIC_fireAcidCone1WeaponStatus,
		BIC_queueFireAcidCone2,
		BIC_fireAcidCone2WeaponStatus,
		BIC_queueFireAcidSingle2,
		BIC_fireAcidSingle2WeaponStatus,
		BIC_queueFireLightningCone1,
		BIC_fireLightningCone1WeaponStatus,
		BIC_queueFireLightningCone2,
		BIC_fireLightningCone2WeaponStatus,
		BIC_queueFireLightningSingle2,
		BIC_fireLightningSingle2WeaponStatus,
		BIC_showMyProfessions,
		BIC_selectMyProfession,
		BIC_queueSampleDna,
		BIC_queueTame,
		BIC_queueEmboldenPets,
		BIC_queueHealMind,
		BIC_queueBerserk1,
		BIC_queueBerserk2,
		BIC_targetSquadCounterpart,
		BIC_queueFormup,
		BIC_queueRetreat,
		BIC_queueBoostMorale,
		BIC_queueSteadyAim,
		BIC_queueApplyPoison,
		BIC_queueApplyDisease,
		BIC_queueAreaTrack,
		BIC_selectAreaTrackType,
		BIC_equipFixtureFlame,
		BIC_queueFlameSingle1,
		BIC_flameSingle1WeaponStatus,
		BIC_queueFlameSingle2,
		BIC_flameSingle2WeaponStatus,
		BIC_queueFlameCone1,
		BIC_flameCone1WeaponStatus,
		BIC_queueFlameCone2,
		BIC_flameCone2WeaponStatus,
		BIC_queueHealthShot1,
		BIC_healthShot1WeaponStatus,
		BIC_queueMindShot1,
		BIC_mindShot1WeaponStatus,
		BIC_queueActionShot1,
		BIC_actionShot1WeaponStatus,
		BIC_queueActionShot2,
		BIC_actionShot2WeaponStatus,
		BIC_queueOverChargeShot1,
		BIC_overChargeShot1WeaponStatus,
		BIC_queuePointBlankSingle1,
		BIC_pointBlankSingle1WeaponStatus,
		BIC_queueThreatenShot,
		BIC_threatenShotWeaponStatus,
		BIC_queueWarningShot,
		BIC_warningShotWeaponStatus,
		BIC_queueAim,
		BIC_aimWeaponStatus,
		BIC_queueSuppressionFire1,
		BIC_suppressionFire1WeaponStatus,
		BIC_queueRollShot,
		BIC_rollShotWeaponStatus,
		BIC_queueDiveShot,
		BIC_diveShotWeaponStatus,
		BIC_queueKipUpShot,
		BIC_kipUpShotWeaponStatus,
		BIC_queueTakeCover,
		BIC_takeCoverWeaponStatus,
		BIC_queueFullAutoSingle1,
		BIC_fullAutoSingle1WeaponStatus,
		BIC_queueScatterShot1,
		BIC_scatterShot1WeaponStatus,
		BIC_queueScatterShot2,
		BIC_scatterShot2WeaponStatus,
		BIC_queueLegShot2,
		BIC_legShot2WeaponStatus,
		BIC_queueLegShot3,
		BIC_legShot3WeaponStatus,
		BIC_queueFullAutoSingle2,
		BIC_fullAutoSingle2WeaponStatus,
		BIC_queueSuppressionFire2,
		BIC_suppressionFire2WeaponStatus,
		BIC_queueWildShot1,
		BIC_wildShot1WeaponStatus,
		BIC_queueWildShot2,
		BIC_wildShot2WeaponStatus,
		BIC_queueFullAutoArea1,
		BIC_fullAutoArea1WeaponStatus,
		BIC_queueChargeShot1,
		BIC_chargeShot1WeaponStatus,
		BIC_queueFullAutoArea2,
		BIC_fullAutoArea2WeaponStatus,
		BIC_queueChargeShot2,
		BIC_chargeShot2WeaponStatus,
		BIC_queueStrafeShot1,
		BIC_strafeShot1WeaponStatus,
		BIC_queueMindShot2,
		BIC_mindShot2WeaponStatus,
		BIC_queueSurpriseShot,
		BIC_surpriseShotWeaponStatus,
		BIC_queueSniperShot,
		BIC_sniperShotWeaponStatus,
		BIC_queueConcealShot,
		BIC_concealShotWeaponStatus,
		BIC_queueFlurryShot1,
		BIC_flurryShot1WeaponStatus,
		BIC_queueFlurryShot2,
		BIC_flurryShot2WeaponStatus,
		BIC_queueStrafeShot2,
		BIC_strafeShot2WeaponStatus,
		BIC_queueStartleShot1,
		BIC_startleShot1WeaponStatus,
		BIC_queueStartleShot2,
		BIC_startleShot2WeaponStatus,
		BIC_queueFlushingShot1,
		BIC_flushingShot1WeaponStatus,
		BIC_queueFlushingShot2,
		BIC_flushingShot2WeaponStatus,
		BIC_queuePolearmLunge1,
		BIC_polearmLunge1WeaponStatus,
		BIC_queueUnarmedLunge1,
		BIC_unarmedLunge1WeaponStatus,
		BIC_queueMelee1hLunge1,
		BIC_melee1hLunge1WeaponStatus,
		BIC_queueMelee2hLunge1,
		BIC_melee2hLunge1WeaponStatus,
		BIC_queueMelee1hDizzyHit1,
		BIC_melee1hDizzyHit1WeaponStatus,
		BIC_queueMelee2hSweep1,
		BIC_melee2hSweep1WeaponStatus,
		BIC_queuePolearmStun1,
		BIC_polearmStun1WeaponStatus,
		BIC_queueUnarmedBlind1,
		BIC_unarmedBlind1WeaponStatus,
		BIC_queueUnarmedStun1,
		BIC_unarmedStun1WeaponStatus,
		BIC_queueIntimidate1,
		BIC_intimidate1WeaponStatus,
		BIC_queueIntimidate2,
		BIC_intimidate2WeaponStatus,
		BIC_queueWarcry1,
		BIC_warcry1WeaponStatus,
		BIC_queueWarcry2,
		BIC_warcry2WeaponStatus,
		BIC_queuePolearmLunge2,
		BIC_polearmLunge2WeaponStatus,
		BIC_queueUnarmedLunge2,
		BIC_unarmedLunge2WeaponStatus,
		BIC_queueMelee1hLunge2,
		BIC_melee1hLunge2WeaponStatus,
		BIC_queueMelee2hLunge2,
		BIC_melee2hLunge2WeaponStatus,
		BIC_queueTaunt,
		BIC_tauntWeaponStatus,
		BIC_queueHealthShot2,
		BIC_healthShot2WeaponStatus,
		BIC_queuePistolMeleeDefense1,
		BIC_pistolMeleeDefense1WeaponStatus,
		BIC_queuePistolMeleeDefense2,
		BIC_pistolMeleeDefense2WeaponStatus,
		BIC_queueTumbleToProne,
		BIC_queueTumbleToKneeling,
		BIC_queueTumbleToStanding,
		BIC_queueMelee1hBlindHit1,
		BIC_queueMelee1hBlindHit2,
		BIC_queueMelee1hScatterHit1,
		BIC_queueMelee1hDizzyHit2,
		BIC_queueMelee1hScatterHit2,
		BIC_queueMelee1hHealthHit1,
		BIC_queueMelee1hSpinAttack2,
		BIC_queueMelee1hHealthHit2,
		BIC_queueMelee1hHit3,
		BIC_melee1hHit3WeaponStatus,
		BIC_queuePolearmHit2,
		BIC_polearmHit2WeaponStatus,
		BIC_queuePolearmStun2,
		BIC_polearmStun2WeaponStatus,
		BIC_queuePolearmSpinAttack2,
		BIC_polearmSpinAttack2WeaponStatus,
		BIC_queuePolearmArea2,
		BIC_polearmArea2WeaponStatus,
		BIC_queuePolearmSweep1,
		BIC_polearmSweep1WeaponStatus,
		BIC_queuePolearmSweep2,
		BIC_polearmSweep2WeaponStatus,
		BIC_queuePolearmActionHit1,
		BIC_polearmActionHit1WeaponStatus,
		BIC_queuePolearmActionHit2,
		BIC_polearmActionHit2WeaponStatus,
		BIC_queuePolearmHit3,
		BIC_polearmHit3WeaponStatus,
		BIC_queueMelee2hArea1,
		BIC_melee2hArea1WeaponStatus,
		BIC_queueMelee2hArea2,
		BIC_melee2hArea2WeaponStatus,
		BIC_queueMelee2hArea3,
		BIC_melee2hArea3WeaponStatus,
		BIC_queueMelee2hSweep2,
		BIC_melee2hSweep2WeaponStatus,
		BIC_queueMelee2hMindHit1,
		BIC_melee2hMindHit1WeaponStatus,
		BIC_queueMelee2hMindHit2,
		BIC_melee2hMindHit2WeaponStatus,
		BIC_queueMelee2hHit3,
		BIC_melee2hHit3WeaponStatus,
		BIC_queueUnarmedKnockdown1,
		BIC_unarmedKnockdown1WeaponStatus,
		BIC_queueUnarmedKnockdown2,
		BIC_unarmedKnockdown2WeaponStatus,
		BIC_queueMeditate
	};

	char const * const cms_backgroundInputMessageName = "SWGSource.PreCU.BackgroundInput.v1";
	LRESULT const cms_backgroundInputProtocolVersion = 240;
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
	bool s_backgroundInputOwnsWindow = false;
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

	LRESULT performBackgroundShowMyProfessions()
	{
		if (!Game::getPlayer())
			return 0;

		CuiMediator * const mediator = CuiMediatorFactory::activateInWorkspace(
			CuiMediatorTypes::WS_Skills);
		SwgCuiSkills * const skills = dynamic_cast<SwgCuiSkills *>(mediator);
		if (!skills)
			return 0;

		int const rowCount = skills->showMyProfessionsForBackgroundValidation();
		if (rowCount < 0 || rowCount > 0xffff)
			return 0;
		return cms_backgroundSkillsStatusMarker | rowCount;
	}

	LRESULT performBackgroundSelectMyProfession(int selectionIndex)
	{
		if (!Game::getPlayer())
			return 0;

		CuiMediator * const mediator = CuiMediatorFactory::activateInWorkspace(
			CuiMediatorTypes::WS_Skills);
		SwgCuiSkills * const skills = dynamic_cast<SwgCuiSkills *>(mediator);
		if (!skills)
			return 0;

		int const selectedRow = skills->selectMyProfessionForBackgroundValidation(selectionIndex);
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

	bool performBackgroundTargetSquadCounterpart()
	{
		Object * const player = Game::getPlayer();
		if (!player)
			return false;

		std::string const playerId = player->getNetworkId().getValueString();
		if (playerId == "44003778")
			CuiCombatManager::setLookAtTarget(NetworkId("207005062"));
		else if (playerId == "207005062")
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

	bool performBackgroundQueueConcealShot(LPARAM const targetValue)
	{
		Object * const player = Game::getPlayer();
		if (!player || targetValue <= 0 ||
			Game::getPlayerNetworkId().getValueString() != "44003778")
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

		// The bridge supplies only the fixture-owned AI OID. The production
		// queue, combat script, result pipeline, miss accounting, and AI hate
		// map retain authority over the complete concealShot transaction.
		CuiCombatManager::setLookAtTarget(targetId);
		CuiCombatManager::setCombatTarget(targetId);
		clearBackgroundCombatTimerCapture();
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"concealShot",
			targetId,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueTaunt(LPARAM const targetValue)
	{
		Object * const player = Game::getPlayer();
		if (!player || targetValue <= 0 ||
			Game::getPlayerNetworkId().getValueString() != "44003778")
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

		// The bridge supplies only the fixture-owned AI OID. The production
		// command queue, taunt roll, AI hate map, and timed restoration retain
		// authority over the complete transaction.
		CuiCombatManager::setLookAtTarget(targetId);
		CuiCombatManager::setCombatTarget(targetId);
		clearBackgroundCombatTimerCapture();
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"taunt",
			targetId,
			Unicode::emptyString) != 0;
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

	bool performBackgroundQueueSampleDna(LPARAM const targetValue)
	{
		// The bridge supplies only the fixture creature OID. The retained
		// Publish 14 Bio-Engineer handler owns skill, HAM, range, DNA, XP,
		// creature-survival, and completion behavior.
		return performBackgroundQueueTending("sampleDNA", targetValue);
	}

	bool performBackgroundQueueTame(LPARAM const targetValue)
	{
		// Supply only the fixture creature OID. The retained Publish 14
		// Creature Handler path owns admission, phased speech, chance, PCD
		// materialization, persistence, callable links, and XP.
		return performBackgroundQueueTending("tame", targetValue);
	}

	bool performBackgroundQueueEmboldenPets()
	{
		if (!Game::getPlayer())
			return false;

		// Embolden Pets has an optional target and is intentionally nonqueued.
		// The server-owned pet-master handler selects and validates the active
		// creature pet, Mind cost, buff, range, and per-pet cooldown.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"emboldenpets",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueHealMind(LPARAM const targetValue)
	{
		// Heal Mind is a real nonqueued targeted command. The server validates
		// Combat Medic ownership, target type, PvP help, range, line of sight,
		// Mind damage, treatment power, and the healer's wound/BF transaction.
		return performBackgroundQueueTending("healMind", targetValue);
	}

	bool performBackgroundQueueBerserk1()
	{
		if (!Game::getPlayer())
			return false;

		// Berserk I is an optional-target, nonqueued self-state command. The
		// server owns Brawler skill, melee/unarmed, chance, HAM, and duration.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"berserk1",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueMeditate()
	{
		if (!Game::getPlayer())
			return false;

		// Meditate is the Teras Kasi novice's nonqueued self-state command.
		// The server owns sitting/combat admission and the recurring heal task.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"meditate",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueBerserk2()
	{
		if (!Game::getPlayer())
			return false;

		// Berserk II is the Brawler-master, optional-target, nonqueued tier.
		// The server owns the modifier, chance, adjusted HAM, and 40s state.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"berserk2",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueFormup()
	{
		if (!Game::getPlayer())
			return false;

		// Form Up is a nonqueued Squad Leader command. The client submits no
		// target; the server owns leadership, group filtering, HAM, and states.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"formup",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueRetreat()
	{
		if (!Game::getPlayer())
			return false;

		// Retreat is a nonqueued Squad Leader command. The server owns group
		// eligibility, HAM, speed/acceleration application, and expiry.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"retreat",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueBoostMorale()
	{
		if (!Game::getPlayer())
			return false;

		// Boost Morale is a nonqueued Squad Leader command. The server owns
		// group eligibility, adjusted HAM, and lossless wound redistribution.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"boostmorale",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueSteadyAim()
	{
		if (!Game::getPlayer())
			return false;

		// Steady Aim is a nonqueued Squad Leader command. The server owns
		// group eligibility, adjusted HAM, ranged filtering, and the buff.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"steadyaim",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	bool performBackgroundQueueApplyPoison(LPARAM const targetValue)
	{
		// The server owns Combat Medic admission, DOT-pack selection, range,
		// cost, DOT resistance, XP, and charge consumption.
		return performBackgroundQueueTending("applyPoison", targetValue);
	}

	bool performBackgroundQueueApplyDisease(LPARAM const targetValue)
	{
		return performBackgroundQueueTending("applyDisease", targetValue);
	}

	bool performBackgroundQueueAreaTrack()
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "44003778")
			return false;

		// The server owns Ranger tiers, outdoor/cooldown admission, option
		// construction, the delayed scan, filtering, and result presentation.
		ClientCommandQueue::clearLastCommandRemoval();
		ClientCommandQueue::commandsAreNowFromToolbar(true);
		bool const queued = ClientCommandQueue::enqueueCommand(
			"areatrack",
			NetworkId::cms_invalid,
			Unicode::emptyString) != 0;
		ClientCommandQueue::commandsAreNowFromToolbar(false);
		return queued;
	}

	LRESULT performBackgroundSelectAreaTrackType(LPARAM const selectionIndex)
	{
		Object * const player = Game::getPlayer();
		if (!player ||
			Game::getPlayerNetworkId().getValueString() != "44003778" ||
			selectionIndex < 0 || selectionIndex > 2)
			return 0;

		return static_cast<LRESULT>(
			CuiDataDrivenPageManager::selectAndConfirmSingleAreaTrackRow(
				static_cast<int>(selectionIndex)));
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

	bool performBackgroundEquipFixtureAcid()
	{
		return performBackgroundEquipCdefWeapon("heavy_acid_beam.iff");
	}

	bool performBackgroundEquipFixtureFlame()
	{
		return performBackgroundEquipCdefWeapon("rifle_flame_thrower.iff");
	}

	bool performBackgroundEquipFixtureLightning()
	{
		return performBackgroundEquipCdefWeapon("rifle_lightning.iff");
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
		// Six marker bits live above the type/flag byte so the middle 32 bits
		// remain available for the complete command valid-weapon mask.
		uint64 result = 0x00005400ULL;
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
		// Aggregate command families (for example RANGED at bit 27) do not fit
		// in the legacy 16-bit diagnostic slice. Protocol 142 gives the valid
		// mask the full middle 32 bits while retaining the low flags/type and
		// the lower 16 bits of the invalid mask.
		result |= static_cast<uint64>(static_cast<uint32>(command.m_weaponTypesValid)) << 16;
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

			case BIC_showMyProfessions:
				return performBackgroundShowMyProfessions();

			case BIC_selectMyProfession:
				return performBackgroundSelectMyProfession(static_cast<int>(lParam));

			case BIC_queueSampleDna:
				if (!performBackgroundQueueSampleDna(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueTame:
				if (!performBackgroundQueueTame(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueEmboldenPets:
				if (!performBackgroundQueueEmboldenPets())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueHealMind:
				if (!performBackgroundQueueHealMind(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueBerserk1:
				if (!performBackgroundQueueBerserk1())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMeditate:
				if (!performBackgroundQueueMeditate())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueBerserk2:
				if (!performBackgroundQueueBerserk2())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_targetSquadCounterpart:
				return performBackgroundTargetSquadCounterpart() ? 1 : 0;

			case BIC_queueFormup:
				if (!performBackgroundQueueFormup())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueRetreat:
				if (!performBackgroundQueueRetreat())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueBoostMorale:
				if (!performBackgroundQueueBoostMorale())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueSteadyAim:
				if (!performBackgroundQueueSteadyAim())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueApplyPoison:
				if (!performBackgroundQueueApplyPoison(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueApplyDisease:
				if (!performBackgroundQueueApplyDisease(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueAreaTrack:
				if (!performBackgroundQueueAreaTrack())
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_selectAreaTrackType:
				return performBackgroundSelectAreaTrackType(lParam);

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

			case BIC_queueMelee1hBodyHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hBodyHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hBodyHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee1hBodyHit2");

			case BIC_queueMelee1hBodyHit3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hBodyHit3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hBodyHit3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee1hBodyHit3");

			case BIC_queueMelee2hHeadHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hHeadHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hHeadHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hHeadHit1");

			case BIC_queueMelee2hHeadHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hHeadHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hHeadHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hHeadHit2");

			case BIC_queueMelee2hHeadHit3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hHeadHit3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hHeadHit3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hHeadHit3");

			case BIC_queueMelee1hHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee1hHit1");

			case BIC_queueMelee1hHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee1hHit2");

			case BIC_queueMelee1hHit3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hHit3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hHit3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee1hHit3");

			case BIC_queueMelee2hHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hHit1");

			case BIC_queueMelee2hHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hHit2");

			case BIC_queuePolearmLegHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmLegHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmLegHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmLegHit2");

			case BIC_queuePolearmLegHit3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmLegHit3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmLegHit3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmLegHit3");

			case BIC_queuePolearmHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmHit1");

			case BIC_queuePolearmHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmHit2");

			case BIC_queuePolearmStun2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmStun2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmStun2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmStun2");

			case BIC_queuePolearmSpinAttack2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmSpinAttack2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmSpinAttack2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"polearmSpinAttack2");

			case BIC_queuePolearmArea2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmArea2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmArea2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmArea2");

			case BIC_queuePolearmSweep1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmSweep1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmSweep1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmSweep1");

			case BIC_queuePolearmSweep2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmSweep2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmSweep2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmSweep2");

			case BIC_queuePolearmActionHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmActionHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmActionHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmActionHit1");

			case BIC_queuePolearmActionHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmActionHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmActionHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmActionHit2");

			case BIC_queuePolearmHit3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmHit3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmHit3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmHit3");

			case BIC_queueMelee2hArea1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hArea1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hArea1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hArea1");

			case BIC_queueMelee2hArea2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hArea2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hArea2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hArea2");

			case BIC_queueMelee2hArea3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hArea3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hArea3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hArea3");

			case BIC_queueMelee2hSweep2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hSweep2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hSweep2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hSweep2");

			case BIC_queueMelee2hMindHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hMindHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hMindHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hMindHit1");

			case BIC_queueMelee2hMindHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hMindHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hMindHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hMindHit2");

			case BIC_queueMelee2hHit3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hHit3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hHit3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hHit3");

			case BIC_queueUnarmedKnockdown1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedKnockdown1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedKnockdown1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedKnockdown1");

			case BIC_queueUnarmedKnockdown2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedKnockdown2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedKnockdown2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedKnockdown2");

			case BIC_queuePolearmArea1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmArea1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmArea1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("polearmArea1");

			case BIC_queueMelee2hSpinAttack2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hSpinAttack2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hSpinAttack2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("melee2hSpinAttack2");

			case BIC_queueBurstShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"burstShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_burstShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("burstShot1");

			case BIC_queueDisarmingShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"disarmingShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_disarmingShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("disarmingShot1");

			case BIC_queueDoubleTap:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"doubleTap", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_doubleTapWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("doubleTap");

			case BIC_queueStoppingShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"stoppingShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_stoppingShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("stoppingShot");

			case BIC_queueCripplingShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"cripplingShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_cripplingShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("cripplingShot");

			case BIC_queuePointBlankSingle2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"pointBlankSingle2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_pointBlankSingle2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("pointBlankSingle2");

			case BIC_queuePointBlankArea1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"pointBlankArea1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_pointBlankArea1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("pointBlankArea1");

			case BIC_queuePointBlankArea2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"pointBlankArea2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_pointBlankArea2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("pointBlankArea2");

			case BIC_queueMultiTargetPistolShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"multiTargetPistolShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_multiTargetPistolShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("multiTargetPistolShot");

			case BIC_queueDisarmingShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"disarmingShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_disarmingShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("disarmingShot2");

			case BIC_queueFanShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fanShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fanShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fanShot");

			case BIC_queueBurstShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"burstShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_burstShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("burstShot2");

			case BIC_queueUnarmedHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedHit1");

			case BIC_queueUnarmedHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedHit2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedHit2");

			case BIC_queueUnarmedBodyHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedBodyHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedBodyHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedBodyHit1");

			case BIC_queueUnarmedLegHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedLegHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedLegHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedLegHit1");

			case BIC_queueUnarmedSpinAttack1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedSpinAttack1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedSpinAttack1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedSpinAttack1");

			case BIC_queueUnarmedSpinAttack2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedSpinAttack2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedSpinAttack2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("unarmedSpinAttack2");

			case BIC_queueOverChargeShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"overChargeShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_overChargeShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("overChargeShot2");

			case BIC_equipFixtureAcid:
				return performBackgroundEquipFixtureAcid() ? 1 : 0;

			case BIC_queueFireAcidSingle1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fireAcidSingle1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fireAcidSingle1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fireAcidSingle1");

			case BIC_equipFixtureLightning:
				return performBackgroundEquipFixtureLightning() ? 1 : 0;

			case BIC_queueFireLightningSingle1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fireLightningSingle1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fireLightningSingle1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fireLightningSingle1");

			case BIC_queueFireAcidCone1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fireAcidCone1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fireAcidCone1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fireAcidCone1");

			case BIC_queueFireAcidCone2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fireAcidCone2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fireAcidCone2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fireAcidCone2");

			case BIC_queueFireAcidSingle2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fireAcidSingle2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fireAcidSingle2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fireAcidSingle2");

			case BIC_queueFireLightningCone1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fireLightningCone1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fireLightningCone1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fireLightningCone1");

			case BIC_queueFireLightningCone2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fireLightningCone2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fireLightningCone2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fireLightningCone2");

			case BIC_queueFireLightningSingle2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fireLightningSingle2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fireLightningSingle2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("fireLightningSingle2");

			case BIC_equipFixtureFlame:
				return performBackgroundEquipFixtureFlame() ? 1 : 0;

			case BIC_queueFlameSingle1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"flameSingle1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_flameSingle1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("flameSingle1");

			case BIC_queueFlameSingle2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"flameSingle2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_flameSingle2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("flameSingle2");

			case BIC_queueFlameCone1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"flameCone1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_flameCone1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("flameCone1");

			case BIC_queueFlameCone2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"flameCone2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_flameCone2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("flameCone2");

			case BIC_queueHealthShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"healthShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_healthShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("healthShot1");

			case BIC_queueMindShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"mindShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_mindShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("mindShot1");

			case BIC_queueActionShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"actionShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_actionShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("actionShot1");

			case BIC_queueActionShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"actionShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_actionShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("actionShot2");

			case BIC_queueOverChargeShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"overChargeShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_overChargeShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("overChargeShot1");

			case BIC_queuePointBlankSingle1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"pointBlankSingle1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_pointBlankSingle1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("pointBlankSingle1");

			case BIC_queueThreatenShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"threatenShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_threatenShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("threatenShot");

			case BIC_queueWarningShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"warningShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_warningShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("warningShot");

			case BIC_queueAim:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"aim", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_aimWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("aim");

			case BIC_queueSuppressionFire1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"suppressionFire1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_suppressionFire1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"suppressionFire1");

			case BIC_queueRollShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"rollShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_rollShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("rollShot");

			case BIC_queueDiveShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"diveShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_diveShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("diveShot");

			case BIC_queueKipUpShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"kipUpShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_kipUpShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("kipUpShot");

			case BIC_queueTakeCover:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"takeCover", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_takeCoverWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("takeCover");

			case BIC_queueFullAutoSingle1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fullAutoSingle1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fullAutoSingle1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"fullAutoSingle1");

			case BIC_queueScatterShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"scatterShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_scatterShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"scatterShot1");

			case BIC_queueScatterShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"scatterShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_scatterShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"scatterShot2");

			case BIC_queueLegShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"legShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_legShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("legShot2");

			case BIC_queueLegShot3:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"legShot3", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_legShot3WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("legShot3");

			case BIC_queueFullAutoSingle2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fullAutoSingle2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fullAutoSingle2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"fullAutoSingle2");

			case BIC_queueSuppressionFire2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"suppressionFire2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_suppressionFire2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"suppressionFire2");

			case BIC_queueWildShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"wildShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_wildShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"wildShot1");

			case BIC_queueWildShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"wildShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_wildShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"wildShot2");

			case BIC_queueFullAutoArea1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fullAutoArea1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fullAutoArea1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"fullAutoArea1");

			case BIC_queueChargeShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"chargeShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_chargeShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"chargeShot1");

			case BIC_queueFullAutoArea2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"fullAutoArea2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_fullAutoArea2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"fullAutoArea2");

			case BIC_queueChargeShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"chargeShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_chargeShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"chargeShot2");

			case BIC_queueStrafeShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"strafeShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_strafeShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"strafeShot1");

			case BIC_queueMindShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"mindShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_mindShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"mindShot2");

			case BIC_queueSurpriseShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"surpriseShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_surpriseShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"surpriseShot");

			case BIC_queueSniperShot:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"sniperShot", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_sniperShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"sniperShot");

			case BIC_queueConcealShot:
				if (!performBackgroundQueueConcealShot(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_concealShotWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"concealShot");

			case BIC_queueFlurryShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"flurryShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_flurryShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"flurryShot1");

			case BIC_queueFlurryShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"flurryShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_flurryShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"flurryShot2");

			case BIC_queueStrafeShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"strafeShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_strafeShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"strafeShot2");

			case BIC_queueStartleShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"startleShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_startleShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"startleShot1");

			case BIC_queueStartleShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"startleShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_startleShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"startleShot2");

			case BIC_queueFlushingShot1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"flushingShot1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_flushingShot1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"flushingShot1");

			case BIC_queueFlushingShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"flushingShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_flushingShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"flushingShot2");

			case BIC_queuePolearmLunge1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmLunge1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmLunge1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"polearmLunge1");

			case BIC_queueUnarmedLunge1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedLunge1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedLunge1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"unarmedLunge1");

			case BIC_queueMelee1hLunge1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hLunge1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hLunge1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"melee1hLunge1");

			case BIC_queueMelee2hLunge1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hLunge1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hLunge1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"melee2hLunge1");

			case BIC_queueMelee1hDizzyHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hDizzyHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hDizzyHit1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"melee1hDizzyHit1");

			case BIC_queueMelee2hSweep1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hSweep1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hSweep1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"melee2hSweep1");

			case BIC_queuePolearmStun1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmStun1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmStun1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"polearmStun1");

			case BIC_queueUnarmedBlind1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedBlind1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedBlind1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"unarmedBlind1");

			case BIC_queueUnarmedStun1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedStun1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedStun1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"unarmedStun1");

			case BIC_queueIntimidate1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"intimidate1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_intimidate1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"intimidate1");

			case BIC_queueIntimidate2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"intimidate2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_intimidate2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"intimidate2");

			case BIC_queueWarcry1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"warcry1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_warcry1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"warcry1");

			case BIC_queueWarcry2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"warcry2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_warcry2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"warcry2");

			case BIC_queuePolearmLunge2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"polearmLunge2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_polearmLunge2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"polearmLunge2");

			case BIC_queueUnarmedLunge2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"unarmedLunge2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_unarmedLunge2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"unarmedLunge2");

			case BIC_queueMelee1hLunge2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hLunge2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee1hLunge2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"melee1hLunge2");

			case BIC_queueMelee2hLunge2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee2hLunge2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_melee2hLunge2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"melee2hLunge2");

			case BIC_queueTaunt:
				if (!performBackgroundQueueTaunt(lParam))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_tauntWeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus("taunt");

			case BIC_queueHealthShot2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"healthShot2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_healthShot2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"healthShot2");

			case BIC_queuePistolMeleeDefense1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"pistolMeleeDefense1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_pistolMeleeDefense1WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"pistolMeleeDefense1");

			case BIC_queuePistolMeleeDefense2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"pistolMeleeDefense2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_pistolMeleeDefense2WeaponStatus:
				return getBackgroundGeneratedCombatWeaponStatus(
					"pistolMeleeDefense2");

			case BIC_queueTumbleToProne:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"tumbleToProne", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueTumbleToKneeling:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"tumbleToKneeling", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueTumbleToStanding:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"tumbleToStanding", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMelee1hBlindHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hBlindHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMelee1hBlindHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hBlindHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMelee1hScatterHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hScatterHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMelee1hDizzyHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hDizzyHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMelee1hScatterHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hScatterHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMelee1hHealthHit1:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hHealthHit1", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMelee1hSpinAttack2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hSpinAttack2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

			case BIC_queueMelee1hHealthHit2:
				if (lParam < 1 || lParam > 16 ||
					!performBackgroundQueueMarksmanTier1(
						"melee1hHealthHit2", static_cast<int>(lParam)))
					return 0;
				return getBackgroundCombatQueueStatus();

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

		HWND window = Os::getWindow();
		bool ownsWindow = false;
		if (!window || !IsWindowVisible(window))
		{
			window = CreateWindowExA(
				0,
				"STATIC",
				"SWGSource Pre-CU Background Input",
				WS_POPUP,
				0,
				0,
				0,
				0,
				0,
				0,
				GetModuleHandle(0),
				0);
			if (!window)
			{
				WARNING(true, ("Pre-CU background input bridge could not create its hidden fallback window"));
				return false;
			}

			ownsWindow = true;
		}

		UINT const registeredMessage = RegisterWindowMessageA(cms_backgroundInputMessageName);
		if (!registeredMessage)
		{
			WARNING(true, ("Pre-CU background input bridge could not register its window message"));
			if (ownsWindow)
				DestroyWindow(window);
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
			if (ownsWindow)
				DestroyWindow(window);
			return false;
		}

		s_backgroundInputMessage = registeredMessage;
		s_backgroundInputWindow = window;
		s_backgroundInputPreviousWindowProc = reinterpret_cast<WNDPROC>(previousWindowProc);
		s_backgroundInputInstalled = true;
		s_backgroundInputOwnsWindow = ownsWindow;
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

		if (s_backgroundInputOwnsWindow && s_backgroundInputWindow && IsWindow(s_backgroundInputWindow))
			DestroyWindow(s_backgroundInputWindow);

		s_backgroundInputInstalled = false;
		s_backgroundInputOwnsWindow = false;
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
