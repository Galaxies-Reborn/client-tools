//========================================================================
//
// SwgCuiStatusGround.cpp
// copyright(c) 2004 Sony Online Entertainment
//
//========================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiStatusGround.h"

#include "clientGame/ClientTextManager.h"
#include "clientGame/ConfigClientGame.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/GuildObject.h"
#include "clientGame/PlayerCreatureController.h"
#include "clientGame/PlayerObject.h"
#include "clientGame/RoadmapManager.h"
#include "clientGame/RoleIconManager.h"
#include "clientGame/ShipObject.h"
#include "clientSkeletalAnimation/SkeletalAppearance2.h"
#include "clientUserInterface/ConfigClientUserInterface.h"
#include "clientUserInterface/CuiCombatManager.h"
#include "clientUserInterface/CuiGameColorManager.h"
#include "clientUserInterface/CuiIconManager.h"
#include "clientUserInterface/CuiMenuInfoHelper.h"
#include "clientUserInterface/CuiMenuInfoTypes.h"
#include "clientUserInterface/CuiObjectTextManager.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiRadialMenuManager.h"
#include "clientUserInterface/CuiSettings.h"
#include "clientUserInterface/CuiSkillManager.h"
#include "clientUserInterface/CuiStringIdsGroup.h"
#include "clientUserInterface/CuiStringIdsWho.h"
#include "clientUserInterface/CuiTextManager.h"
#include "clientUserInterface/CuiVoiceChatManager.h"
#include "clientUserInterface/CuiWorkspace.h"
#include "clientUserInterface/CuiWorkspaceIcon.h"
#include "sharedFoundation/CalendarTime.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/ConstCharCrcString.h"
#include "sharedFoundation/FormattedString.h"
#include "sharedFoundation/Production.h"
#include "sharedGame/Buff.h"
#include "sharedGame/GameObjectTypes.h"
#include "sharedGame/GroupPickupPoint.h"
#include "sharedGame/PvpData.h"
#include "sharedGame/SharedCreatureObjectTemplate.h"
#include "sharedMath/VectorArgb.h"
#include "sharedMathArchive/VectorArchive.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetworkMessages/ClientMfdStatusUpdateMessage.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedUtility/Callback.h"
#include "swgClientUserInterface/SwgCuiBuffUtils.h"
#include "swgClientUserInterface/SwgCuiHud.h"
#include "swgClientUserInterface/SwgCuiHudFactory.h"
#include "swgClientUserInterface/SwgCuiMfdStatusBar.h"
#include "swgSharedUtility/Postures.h"
#include "clientGame/ClientCommandQueue.h"
#include "clientGame/GroupObject.h"
#include "sharedFoundation/Crc.h"

#include "UIEffector.h"
#include "UIImage.h"
#include "UIImageStyle.h"
#include "UIManager.h"
#include "UIMessage.h"
#include "UIPopupMenu.h"
#include "UIPopupMenustyle.h"
#include "UIText.h"
#include "UIUtils.h"
#include "UIVolumePage.h"

#include <vector>


//========================================================================

namespace SwgCuiStatusGroundNamespace
{
	
	class BuffIconSettingsChangedCallback : public CallbackReceiver
	{
	public:
		explicit BuffIconSettingsChangedCallback (SwgCuiStatusGround & status)
			: CallbackReceiver(), m_status(&status)
		{		
		}

		virtual void performCallback()
		{
			m_status->onBuffIconSettingsChangedCallback();
		}

		~BuffIconSettingsChangedCallback()
		{
			m_status = 0;
		}

	private:
		// Not implemented.
		BuffIconSettingsChangedCallback();
		BuffIconSettingsChangedCallback & operator=(BuffIconSettingsChangedCallback const &);

	private:
		SwgCuiStatusGround * m_status;
	};


	namespace Properties
	{
		const UILowerString MemberId("MemberId");
	}

	const std::string IconFocusName = "IconFocus";

	bool isFactionalCombatCreature(CreatureObject const & creature)
	{
		uint32 const faction = creature.getPvpFaction();
		return PvpData::isImperialFactionId(faction) || PvpData::isRebelFactionId(faction);
	}

	namespace PopupIds
	{
		const std::string group_kick = "group_kick";
		const std::string group_disband = "group_disband";
		const std::string group_leave = "group_leave";
		const std::string group_masterlooter = "group_master_looter";
		const std::string group_makeleader   = "group_make_leader";
		const std::string group_loot	= "group_loot";
		const std::string group_create_pickup_point = "group_create_pickup_point";
		const std::string group_use_pickup_point = "group_use_pickup_point";
		const std::string group_use_pickup_point_nocamp = "group_use_pickup_point_nocamp";
	}

	//--------------------------------------------------------------------

	char const * const s_statusTypeNames[SwgCuiStatusGround::ST_count] =
	{
		"SwgCuiStatusGroundUndefined",
		"SwgCuiStatusGroundPlayer",
		"SwgCuiStatusLookAtTarget",
		"SwgCuiStatusIntendedTarget",
		"SwgCuiStatusGroundTarget",
		"SwgCuiStatusGroundGroup",
		"SwgCuiStatusGroundPet"
	};

	//--------------------------------------------------------------------

	namespace StringTables
	{
		const std::string planet_n = "planet_n";
	}

	//--------------------------------------------------------------------

	UIColor const s_targetNotObservedColor(96, 96, 96);

	UIColor const s_eliteSkinColor(255,255,255);
	UIColor const s_bossSkinColor(207,191,119);
	UIColor const s_eliteLookAtSkinColor(127,127,127);
	UIColor const s_bossLookAtSkinColor(120,111,82);
	std::set<std::pair<NetworkId, SwgCuiMfdStatus *> > s_mfdStatusObjects;

	//--------------------------------------------------------------------

	std::string ROLE_ICON_MASTER_PATH = "/Styles.Icon.role.";
	std::string ROLE_TOOLTIP_STRING_FILE = "ui_role_tooltip";

	//--------------------------------------------------------------------

	void packAndActivateParent(UIWidget * widget, bool isVisible)
	{
		if(widget)
		{
			UIPage * const parent = UI_ASOBJECT(UIPage, widget->GetParent());
			if(parent)
			{
				if (isVisible)
				{
					if (!parent->IsEnabled())
					{
						parent->SetEnabled(true);
					}

					if (!parent->IsActivated())
					{
						parent->SetActivated(true);
					}
				}

				parent->SetPackDirty(true);
			}
		}
	}

	bool pageSetVisible(UIWidget * widget, bool visible)
	{
		bool stateChanged = false;
		if(widget)
		{
			bool wasVisible = widget->IsVisible();
			stateChanged = !boolEqual(wasVisible, visible);
			if(stateChanged)
			{
				UIPage * const page = UI_ASOBJECT(UIPage, widget);
				
				if (page)
				{
					page->SetContextCapable(visible,visible);
					page->SetContextToParent(visible);
				}

				widget->SetVisible(visible);
				
				if(visible)
				{
					if(page)
					{
						page->Pack();
					}
				}
				packAndActivateParent(widget, visible);
			}
		}

		return stateChanged;
	}

	//--------------------------------------------------------------------

	// RLS TODO - Throw all the individual arrays into one struct.

	// Define the force attribute - special case.
	int const ForceAttribute = -1;

	// The attributes monitored on each page.
	const Attributes::Enumerator s_pageAttributes[SwgCuiStatusGround::HAMBarPageCount] =
	{
		Attributes::Health,
		Attributes::Action,
		Attributes::Mind,
		ForceAttribute,
		Attributes::Health,
		Attributes::Health,
	};

	// The CodeData associated with the page.
	const char * const s_pageNames[SwgCuiStatusGround::HAMBarPageCount] =
	{
		"pageH",
		"pageA",
		"pageM",
		"pageF",
		"pagecreaturehealth", // H only page.
		"pagegreyhealth"
	};

	// The page styles we currently use.
	enum PageStyle
	{
		PS_none,
		PS_healthOnly,
		PS_ham,
		PS_jedi,
		PS_greyHealth,
		PS_count
	};

	// For each page style, which pages are active?
	const bool s_activePagesPerStyle[PS_count][SwgCuiStatusGround::HAMBarPageCount] =
	{
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 1, 0},
		{1, 1, 1, 0, 0, 0},
		{1, 1, 1, 1, 0, 0},
		{0, 0, 0, 0, 0, 1},
	};

	enum EliteStyles
	{
		ES_eliteLeft,
		ES_eliteRight,
		ES_doubleEliteLeft,
		ES_doubleEliteRight,
		ES_count
	};

	//----------------------------------------------------------------------

	void appendPlayerStatusString(PlayerObject const & playerObject, Unicode::String & s)
	{
		if(playerObject.isLinkDead())
		{
			s += Unicode::narrowToWide("(");
			s += CuiStringIdsWho::link_dead.localize();
			s += Unicode::narrowToWide(") ");
		}
		else
		{
			if(playerObject.isAwayFromKeyBoard())
			{
 				s += Unicode::narrowToWide("(");
				s += CuiStringIdsWho::away_from_keyboard.localize();
				s += Unicode::narrowToWide(") ");
			}
			else
			{
				if(playerObject.isHelper())
				{
					s += Unicode::narrowToWide("(");
					s += CuiStringIdsWho::helper.localize();
					s += Unicode::narrowToWide(") ");
				}

				if(playerObject.isRolePlay())
				{
					s += Unicode::narrowToWide("(");
					s += CuiStringIdsWho::roleplay.localize();
					s += Unicode::narrowToWide(") ");
				}

				if(playerObject.isOutOfCharacter())
				{
					s += Unicode::narrowToWide("(");
					s += CuiStringIdsWho::outofcharacter.localize();
					s += Unicode::narrowToWide(") ");
				}

				if(playerObject.isLookingForGroup())
				{
					s += Unicode::narrowToWide("(");
					s += CuiStringIdsWho::looking_for_group.localize();
					s += Unicode::narrowToWide(") ");
				}

				if(playerObject.isLookingForWork())
				{
					s += Unicode::narrowToWide("(");
					s += CuiStringIdsWho::looking_for_work.localize();
					s += Unicode::narrowToWide(") ");
				}
			}
		}
		PlayerObject const* self = Game::getConstPlayerObject();
		bool hasPriviledgedTitle = false;
		
		if (playerObject.isNormalPlayer())
		{
			hasPriviledgedTitle = false;
		}
		else if (playerObject.isCustomerServiceRepresentative())
		{
			if (!s.empty())
				s.push_back('\n');
			s.push_back('(');
			s += CuiStringIdsWho::customer_service_representative.localize();
			s.push_back(')');
			hasPriviledgedTitle = true;
		}
		else if (playerObject.isDeveloper())
		{
			if (!s.empty())
				s.push_back('\n');
			s.push_back('(');
			s += CuiStringIdsWho::developer.localize();
			s.push_back(')');
			hasPriviledgedTitle = true;
		}
		else if (playerObject.isQualityAssurance())
		{
			if (!s.empty())
				s.push_back('\n');
			s.push_back('(');
			s += CuiStringIdsWho::quality_assurance.localize();
			s.push_back(')');
			hasPriviledgedTitle = true;
		}
		
		// If this player is a Warden, and I'm a warden or in God Mode, append (Warden) tag
		else if (playerObject.isWarden() && (self && (self->isWarden() || self->isAdmin())))
		{
			if (!s.empty())
				s.push_back('\n');
			s.push_back('(');
			s += CuiStringIdsWho::warden.localize();
			s.push_back(')');
			hasPriviledgedTitle = true;
			
		}

		// If this player has a squelch/spammer action on them
		// we display the (Squelch/Spammer) tag if I'm a Warden or if in God Mode
		int const secondsUntilUnsquelched = playerObject.getSecondsUntilUnsquelched();
		if (secondsUntilUnsquelched != 0 && (self && (self->isWarden() || self->isAdmin())))
		{
			
			if (hasPriviledgedTitle)
				s.push_back (' ');
			else if (!s.empty())
				s.push_back ('\n');

			s.push_back ('(');

			if (secondsUntilUnsquelched < 0) // if < 0, it's a /squlech
			{
				s += CuiStringIdsWho::squelched.localize();
				s.push_back (' ');
				s += Unicode::narrowToWide(playerObject.getSquelchedByName());
				s.push_back (' ');
				s.push_back ('[');
				s += Unicode::narrowToWide(playerObject.getSquelchedById().getValueString());
				s.push_back (']');
			}
			else // otherwise it's a /spammer
			{
				s += CuiStringIdsWho::spammered.localize();
				s.push_back (' ');
				s += Unicode::narrowToWide(playerObject.getSquelchedByName());
				s.push_back (' ');
				s.push_back ('[');
				s += Unicode::narrowToWide(playerObject.getSquelchedById().getValueString());
				s.push_back (']');
				s.push_back (' ');
				s.push_back ('[');
				s += Unicode::narrowToWide(CalendarTime::convertSecondsToHMS(static_cast<unsigned int>(secondsUntilUnsquelched)));
				s.push_back (']');
			}

			s.push_back (')');
		}

		// If I'm in God Mode, and the other player is in God Mode, display a tag for that
		if(self->isAdmin())
		{
			if(playerObject.isAdmin())
			{
				if(hasPriviledgedTitle)
				{
					s.push_back('\n');
				}
				s += Unicode::narrowToWide("(God Mode)");
			}
		}
	}

	//----------------------------------------------------------------------

	StringId const s_attributeNames[SwgCuiStatusGround::HAMBarPageCount] =
	{
		StringId("att_n", "health"),
		StringId("att_n", "action"),
		StringId("att_n", "mind"),
		StringId("att_n", "force"),
		StringId("att_n", "health")
	};

	StringId const s_attributeDescriptions[SwgCuiStatusGround::HAMBarPageCount] =
	{
		StringId("att_d", "health"),
		StringId("att_d", "action"),
		StringId("att_d", "mind"),
		StringId("att_d", "force"),
		StringId("att_d", "health")
	};

	StringId const s_enteringCombatFlyText = StringId("cbt_spam", "entering_combat_fly");
}

using namespace SwgCuiStatusGroundNamespace;

namespace
{
	const UILowerString s_hamEnhanceLayoutRoot("HamEnhanceLayoutRoot");
	const UILowerString s_hamEnhanceHideStates("HamEnhanceHideStates");

	struct HamEnhancePropertyPair
	{
		UILowerString standardName;
		UILowerString enhancedName;
		UILowerString targetName;
	};

	const HamEnhancePropertyPair s_hamEnhancePropertyPairs[] =
	{
		{ UILowerString("HamStandardMinimumSize"), UILowerString("HamEnhancedMinimumSize"), UIWidget::PropertyName::MinimumSize },
		{ UILowerString("HamStandardMaximumSize"), UILowerString("HamEnhancedMaximumSize"), UIWidget::PropertyName::MaximumSize },
		{ UILowerString("HamStandardSize"), UILowerString("HamEnhancedSize"), UIWidget::PropertyName::Size },
		{ UILowerString("HamStandardScrollExtent"), UILowerString("HamEnhancedScrollExtent"), UIWidget::PropertyName::ScrollExtent },
		// Location must follow Size. SetLocation() captures far-edge packing
		// metadata from the widget's current dimensions; capturing it before a
		// target HAM width transition makes the next parent pack move the HAM.
		{ UILowerString("HamStandardLocation"), UILowerString("HamEnhancedLocation"), UIWidget::PropertyName::Location },
		{ UILowerString("HamStandardPackSize"), UILowerString("HamEnhancedPackSize"), UIWidget::PropertyName::PackSize }
	};

	bool isRuntimeSizedHamWidget(UIWidget const & widget)
	{
		// These leaves are resized every update from the live HAM values.  Their
		// containing row/NormalMax/inner pages own the toggle geometry; restoring
		// an authored width here would replace the actual fill percentage.
		return widget.IsName("Current") ||
			widget.IsName("CurrentMax") ||
			widget.IsName("RecentCurrent") ||
			widget.IsName("CurrentTick");
	}

	UIPage * findHamEnhanceLayoutRoot(UIPage & page)
	{
		UIBaseObject * object = &page;
		while (object)
		{
			UIPage * const candidate = dynamic_cast<UIPage *>(object);
			bool isLayoutRoot = false;
			if (candidate && candidate->GetPropertyBoolean(s_hamEnhanceLayoutRoot, isLayoutRoot) && isLayoutRoot)
				return candidate;
			object = object->GetParent();
		}
		return 0;
	}

	void collectHamEnhanceLayoutWidgets(UIBaseObject & object, std::vector<UIWidget *> & widgets)
	{
		UIWidget * const widget = dynamic_cast<UIWidget *>(&object);
		if (widget && !isRuntimeSizedHamWidget(*widget))
		{
			std::string ignored;
			for (size_t i = 0; i < sizeof(s_hamEnhancePropertyPairs) / sizeof(s_hamEnhancePropertyPairs[0]); ++i)
			{
				if (widget->GetPropertyNarrow(s_hamEnhancePropertyPairs[i].standardName, ignored))
				{
					widgets.push_back(widget);
					break;
				}
			}
		}

		UIBaseObject::UIObjectList children;
		object.GetChildren(children);
		for (UIBaseObject::UIObjectList::iterator i = children.begin(); i != children.end(); ++i)
			collectHamEnhanceLayoutWidgets(**i, widgets);
	}
}

class SwgCuiStatusGroundHamEnhanceState
{
public:
	explicit SwgCuiStatusGroundHamEnhanceState(UIPage & root) :
		m_root(&root),
		m_widgets(),
		m_layoutProbes()
	{
		collectHamEnhanceLayoutWidgets(root, m_widgets);

		// The imported standard/enhanced layouts contain exact geometry for the
		// status root and its H/A/M(/F) rows. Keep the outer root out of the legacy
		// packing passes while locating the immediate HAM composite below.
		m_root->SetDoNotPackChildren(true);
		m_layoutProbes = m_widgets;

		UIPage * hamPage = 0;
		for (std::vector<UIWidget *>::iterator widgetIterator = m_widgets.begin(); widgetIterator != m_widgets.end(); ++widgetIterator)
		{
			UIPage * const page = dynamic_cast<UIPage *>(*widgetIterator);
			if (page && page->IsName("ham"))
			{
				hamPage = page;
				break;
			}
		}

		if (hamPage)
		{
			// Protect the full container chain, not only the immediate HAM page.
			// Target has status/composite ancestors whose horizontal Fill pass is
			// dirtied whenever the buff/state volume changes visibility. Letting
			// either ancestor pack would resize the authored target column after an
			// off/on/off cycle; target-of-target duplicates the same hierarchy.
			for (UIBaseObject * boundary = hamPage; boundary && boundary != m_root; boundary = boundary->GetParent())
			{
				UIPage * const boundaryPage = dynamic_cast<UIPage *>(boundary);
				if (boundaryPage)
					boundaryPage->SetDoNotPackChildren(true);
			}
		}
	}

	bool isLayoutApplied(bool enabled) const
	{
		// Validate every structural pair in both axes.  A horizontal shrink-wrap
		// can leave all Y values correct while collapsing the rows back to their
		// retail width; a vertical shrink-wrap clips Mind.  Runtime-sized fill
		// leaves are excluded from m_layoutProbes so their live percentages remain
		// authoritative.
		for (std::vector<UIWidget *>::const_iterator widgetIterator = m_layoutProbes.begin(); widgetIterator != m_layoutProbes.end(); ++widgetIterator)
		{
			UIWidget const * const widget = *widgetIterator;
			for (size_t i = 0; i < sizeof(s_hamEnhancePropertyPairs) / sizeof(s_hamEnhancePropertyPairs[0]); ++i)
			{
				HamEnhancePropertyPair const & propertyPair = s_hamEnhancePropertyPairs[i];
				UILowerString const & sourceName = enabled ? propertyPair.enhancedName : propertyPair.standardName;
				if (i < 5)
				{
					UIPoint expected;
					if (!widget->GetPropertyPoint(sourceName, expected))
						continue;

					UIPoint actual;
					if (!widget->GetPropertyPoint(propertyPair.targetName, actual) || actual != expected)
						return false;
				}
				else
				{
					std::string expected;
					if (!widget->GetPropertyNarrow(sourceName, expected))
						continue;

					std::string actual;
					if (!widget->GetPropertyNarrow(propertyPair.targetName, actual) || _stricmp(actual.c_str(), expected.c_str()) != 0)
						return false;
				}
			}
		}

		return true;
	}

	void apply(bool enabled)
	{
		for (std::vector<UIWidget *>::iterator widgetIterator = m_widgets.begin(); widgetIterator != m_widgets.end(); ++widgetIterator)
		{
			UIWidget * const widget = *widgetIterator;
			std::string minimumSize;
			std::string maximumSize;
			UILowerString const & minimumSourceName = enabled ? s_hamEnhancePropertyPairs[0].enhancedName : s_hamEnhancePropertyPairs[0].standardName;
			UILowerString const & maximumSourceName = enabled ? s_hamEnhancePropertyPairs[1].enhancedName : s_hamEnhancePropertyPairs[1].standardName;
			bool const hasMinimumSize = widget->GetPropertyNarrow(minimumSourceName, minimumSize);
			bool const hasMaximumSize = widget->GetPropertyNarrow(maximumSourceName, maximumSize);

			// Minimum and maximum setters clamp each other. Neutralize the old
			// minimum before changing a paired maximum so expanding and shrinking
			// transitions both reach the exact authored target constraints.
			if (hasMaximumSize)
			{
				UISize const unchangedMinimumSize = widget->GetMinimumSize();
				widget->SetMinimumSize(UISize(0, 0));
				IGNORE_RETURN(widget->SetPropertyNarrow(UIWidget::PropertyName::MaximumSize, maximumSize));
				if (hasMinimumSize)
					IGNORE_RETURN(widget->SetPropertyNarrow(UIWidget::PropertyName::MinimumSize, minimumSize));
				else
					widget->SetMinimumSize(unchangedMinimumSize);
			}
			else if (hasMinimumSize)
				IGNORE_RETURN(widget->SetPropertyNarrow(UIWidget::PropertyName::MinimumSize, minimumSize));

			// Size precedes Location in this table so far/center anchored widgets
			// capture packing metadata from their final dimensions. This order is
			// required for exact repeated standard/enhanced transitions.
			for (size_t i = 2; i < sizeof(s_hamEnhancePropertyPairs) / sizeof(s_hamEnhancePropertyPairs[0]); ++i)
			{
				HamEnhancePropertyPair const & propertyPair = s_hamEnhancePropertyPairs[i];
				std::string value;
				UILowerString const & sourceName = enabled ? propertyPair.enhancedName : propertyPair.standardName;
				if (widget->GetPropertyNarrow(sourceName, value))
					IGNORE_RETURN(widget->SetPropertyNarrow(propertyPair.targetName, value));
			}
		}

		m_root->SetPackDirty(true);
		m_root->ForcePackChildren();
	}

private:
	UIPage * m_root;
	std::vector<UIWidget *> m_widgets;
	std::vector<UIWidget *> m_layoutProbes;
};

//------------------------------------------------------------------------

SwgCuiStatusGround::SwgCuiStatusGround(UIPage & page, StatusType statusType) :
SwgCuiMfdStatus("SwgCuiStatusGround", page),
m_hamBarParent(0),
m_volumeStates(0),
m_sampleStateIcon(0),
m_sampleIconPage(0),
m_displayStats(true),
m_displayStates(true),
m_displayPosture(true),
m_displayNameOnly(true),
m_resizeTargetName(false),
m_pulseNameOnCombat(false),
m_isLookAtTarget(false),
m_isHoverTarget(false),
m_showRange(true),
m_outOfRange(false),
m_isVehicle(false),
m_isPlayerControlled(false),
m_isThePlayer(false),
m_isPlayerMounted(false),
m_isJedi(false),
m_targetNamePrefix(),
m_iconDifficulty(0),
m_iconFaction(0),
m_eliteSkin(0),
m_objectName(),
m_lastClientMfdStatusUpdateMessage(0),
m_pageRole(0),
m_pageRoleIcon(0),
m_effectorBlink(0),
m_effectorCombat(0),
m_effectorCombatTarget(0),
m_statusType(statusType),
m_lastKnownPosition(),
m_lastKnownDistance(-1.0f),
m_textLevel(0),
m_textStatus(0),
m_textTitle(0),
m_textName(0),
m_textDistance(0),
m_textFaction(0),
m_textGuild(0),
m_textDebug(0),
m_eliteLeftPage(0),
m_eliteRightPage(0),
m_compositePage(0),
m_debuffStates(0),
m_speakingIcon(0),
m_buffIconSettingsChangedCallback(0),
m_hamEnhanceState(0),
m_individualUiScaleRoot(0),
m_hamEnhanceApplied(false)
{
	setMediatorDebugName(s_statusTypeNames[statusType]);


	getCodeDataObject(TUIVolumePage, m_volumeStates, "volumeStates", true);
	if(m_volumeStates)
	{
		registerMediatorObject(*m_volumeStates, true);
		// Grab sample before clearing the page.
		getCodeDataObject(TUIImage, m_sampleStateIcon, "sampleStateIcon");
		if(m_sampleStateIcon)
			m_sampleStateIcon->SetVisible(false);

		getCodeDataObject(TUIPage, m_sampleIconPage, "sampleIconPage", true);
		if(m_sampleIconPage)
			pageSetVisible(m_sampleIconPage, false);

		IGNORE_RETURN(pageSetVisible(m_volumeStates, true));

		m_volumeStates->Clear();
	}

	getCodeDataObject(TUIVolumePage, m_debuffStates, "volumedebuff", true);
	if(m_debuffStates)
	{
		IGNORE_RETURN(pageSetVisible(m_debuffStates, true));
		m_debuffStates->Clear();
	}

	getCodeDataObject(TUIImage, m_iconDifficulty, "iconDifficulty", true);
	getCodeDataObject(TUIImage, m_directionArrow, "directionArrow", true);
	getCodeDataObject(TUIImage, m_iconFaction, "faction", true);
	
	getCodeDataObject(TUIImage, m_speakingIcon, "speakingicon", true);
	if(m_speakingIcon)
	{
		IGNORE_RETURN(pageSetVisible(m_speakingIcon, false));
	}

	getCodeDataObject(TUIText, m_textName, "textName", true);
	if(m_textName)
	{
		m_textName->SetPreLocalized(true);
		m_textName->SetLocalText(Unicode::emptyString);
		IGNORE_RETURN(pageSetVisible(m_textName, true));
	}

	getCodeDataObject(TUIText, m_textDistance, "textDistance", true);
	if(m_textDistance)
	{
		m_textDistance->SetPreLocalized(true);
		m_textDistance->SetLocalText(Unicode::emptyString);
		IGNORE_RETURN(pageSetVisible(m_textDistance, false));
	}

	// Publish 14's AllTargets.SampleStatus contains an authored "+50%"
	// Accuracy preview that is not bound to any runtime status data.  Every
	// duplicated overhead status page otherwise exposes that design-time text.
	// Do not present the retired client-side estimate as an authoritative value.
	UIPage * const legacyAccuracyPage = dynamic_cast<UIPage *>(page.GetObjectFromPath("Accuracy", TUIPage));
	if(legacyAccuracyPage)
	{
		UIText * const legacyAccuracyText = dynamic_cast<UIText *>(legacyAccuracyPage->GetObjectFromPath("text", TUIText));
		if(legacyAccuracyText)
		{
			legacyAccuracyText->SetPreLocalized(true);
			legacyAccuracyText->SetLocalText(Unicode::emptyString);
		}

		legacyAccuracyPage->SetVisible(false);
	}

	getCodeDataObject(TUIText, m_textStatus, "textStatus", true);
	if(m_textStatus)
	{
		m_textStatus->SetPreLocalized(true);
		m_textStatus->SetLocalText(Unicode::emptyString);
		IGNORE_RETURN(pageSetVisible(m_textStatus, false));
	}

	getCodeDataObject(TUIText, m_textTitle, "textTitle", true);
	if(m_textTitle)
	{
		m_textTitle->SetPreLocalized(true);
		m_textTitle->SetLocalText(Unicode::emptyString);
		IGNORE_RETURN(pageSetVisible(m_textTitle, false));
	}

	getCodeDataObject(TUIText, m_textFaction, "textFaction", true);
	if(m_textFaction)
	{
		m_textFaction->SetPreLocalized(true);
		m_textFaction->SetLocalText(Unicode::emptyString);
		IGNORE_RETURN(pageSetVisible(m_textFaction, false));
	}

	getCodeDataObject(TUIText, m_textGuild, "textGuild", true);
	if(m_textGuild)
	{
		m_textGuild->SetPreLocalized(true);
		m_textGuild->SetLocalText(Unicode::emptyString);
		IGNORE_RETURN(pageSetVisible(m_textGuild, false));
	}

	getCodeDataObject(TUIText, m_textLevel, "level", true);
	if(m_textLevel)
	{
		m_textLevel->SetPreLocalized(true);
		m_textLevel->SetLocalText(Unicode::emptyString);
		IGNORE_RETURN(pageSetVisible(m_textLevel, false));
	}

	getCodeDataObject(TUIText, m_textDebug, "textDebug", true);
	if(m_textDebug)
	{
		m_textDebug->SetPreLocalized(true);
		m_textDebug->SetLocalText(Unicode::emptyString);
		IGNORE_RETURN(pageSetVisible(m_textDebug, false));
	}

	getCodeDataObject(TUIImage, m_eliteLeftPage, "elite_left", true);
	getCodeDataObject(TUIImage, m_eliteRightPage, "elite_right", true);
	if(m_eliteLeftPage)
		m_eliteLeftPage->SetVisible(false);
	if(m_eliteRightPage)
		m_eliteRightPage->SetVisible(false);


	getCodeDataObject(TUIImage, m_eliteSkin, "eliteSkin", true);
	if(m_eliteSkin)
		m_eliteSkin->SetVisible(false);

	int i;
	for(i = 0; i < CII_MAX; ++i)
		m_conIcons[i] = NULL;

	getCodeDataObject(TUIImage, m_conIcons[CII_lower], "conLower", true);
	getCodeDataObject(TUIImage, m_conIcons[CII_even], "conEven", true);
	getCodeDataObject(TUIImage, m_conIcons[CII_higher], "conHigher", true);

	m_conStyles[ES_eliteLeft] = safe_cast<UIImageStyle *>(UIManager::gUIManager().GetRootPage()->GetObjectFromPath("/Styles.UIv2.CON.con_elite", TUIImageStyle));
	m_conStyles[ES_eliteRight] = safe_cast<UIImageStyle *>(UIManager::gUIManager().GetRootPage()->GetObjectFromPath("/Styles.UIv2.CON.con_elite_right", TUIImageStyle));
	m_conStyles[ES_doubleEliteLeft] = safe_cast<UIImageStyle *>(UIManager::gUIManager().GetRootPage()->GetObjectFromPath("/Styles.UIv2.CON.con_double_elite", TUIImageStyle));
	m_conStyles[ES_doubleEliteRight] = safe_cast<UIImageStyle *>(UIManager::gUIManager().GetRootPage()->GetObjectFromPath("/Styles.UIv2.CON.con_double_elite_right", TUIImageStyle));
	NON_NULL(m_conStyles[ES_eliteLeft]);
	NON_NULL(m_conStyles[ES_eliteRight]);
	NON_NULL(m_conStyles[ES_doubleEliteLeft]);
	NON_NULL(m_conStyles[ES_doubleEliteRight]);
	hideLevelInfo();

	getCodeDataObject(TUIPage, m_pageRole, "role", true);
	getCodeDataObject(TUIImage, m_pageRoleIcon, "roleIcon", true);

	if(m_pageRole)
		registerMediatorObject(*m_pageRole, true);

	registerMediatorObject(page,true);

	getCodeDataObject(TUIEffector, m_effectorBlink, "effectorBlink", true);

	for(i = 0; i < SwgCuiStatusGround::HAMBarPageCount; ++i)
	{
		m_attributes[i] = s_pageAttributes[i];
		UIPage * statusPage = 0;
		getCodeDataObject(TUIPage, statusPage, s_pageNames[i], true);
		if(statusPage)
		{
			m_bars[i] = new SwgCuiMfdStatusBar(*statusPage);
			m_bars[i]->fetch();
			statusPage->SetEnabled(true);
		}
		else
		{
			m_bars[i] = NULL;
		}
	}

	getCodeDataObject(TUIPage, m_hamBarParent, "hamParent", true);
	IGNORE_RETURN(pageSetVisible(m_hamBarParent, true));

	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//-- setup the icon

	CuiWorkspaceIcon * const icon = new CuiWorkspaceIcon(this);
	icon->SetName("ChatIcon");
	icon->SetSize(UISize(32, 32));
	icon->SetBackgroundColor(UIColor(0, 0, 0, 50));
	icon->SetLocation(0, 256);

	setIcon(icon);

	{
		// setup the combat effector pages.
		UIEffector * combatEffector = 0;
		getCodeDataObject(TUIEffector, combatEffector, "combatEffector", true);

		UIWidget * combatEffectorTarget = 0;
		getCodeDataObject(TUIWidget, combatEffectorTarget, "combatEffectorTarget", true);

		if(combatEffector && combatEffectorTarget)
		{
			m_effectorCombatTarget = combatEffectorTarget;
			m_effectorCombat = combatEffector;
		}
	}

	getCodeDataObject(TUIPage, m_compositePage, "info", true);

	m_callback->connect(*this, &SwgCuiStatusGround::onBuffsChanged, static_cast<CreatureObject::Messages::BuffsChanged *>(0));
	m_callback->connect(*this, &SwgCuiStatusGround::onSkillsChanged, static_cast<CreatureObject::Messages::SkillsChanged *>(0));

	m_buffIconSettingsChangedCallback = new BuffIconSettingsChangedCallback (*this);

	UIPage * const hamEnhanceLayoutRoot = findHamEnhanceLayoutRoot(page);
	if (hamEnhanceLayoutRoot)
	{
		m_hamEnhanceState = new SwgCuiStatusGroundHamEnhanceState(*hamEnhanceLayoutRoot);
		std::string individualUiScaleCategory;
		if (hamEnhanceLayoutRoot->GetPropertyNarrow(UILowerString("IndividualUiScale"), individualUiScaleCategory) &&
			_stricmp(individualUiScaleCategory.c_str(), "ham") == 0)
			m_individualUiScaleRoot = hamEnhanceLayoutRoot;
		// Defer the first authored transition until update() has completed the
		// legacy visibility and shrink-wrap pass.  Applying here lets the first
		// update immediately overwrite the restored height before it is rendered.
		m_hamEnhanceApplied = !CuiPreferences::getHamEnhance();
	}
	else
		page.ForcePackChildren();
}

//------------------------------------------------------------------------

SwgCuiStatusGround::~SwgCuiStatusGround()
{
	int i;

	m_callback->disconnect(*this, &SwgCuiStatusGround::onBuffsChanged, static_cast<CreatureObject::Messages::BuffsChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiStatusGround::onSkillsChanged, static_cast<CreatureObject::Messages::SkillsChanged *>(0));
	
	delete m_buffIconSettingsChangedCallback;
	m_buffIconSettingsChangedCallback = 0;

	delete m_hamEnhanceState;
	m_hamEnhanceState = 0;
	m_individualUiScaleRoot = 0;

	for(i = 0; i < SwgCuiStatusGround::HAMBarPageCount; ++i)
	{
		if(m_bars[i])
		{
			m_bars[i]->release();
			m_bars[i] = 0;
		}
	}

	m_volumeStates = 0;
	m_debuffStates = 0;
	m_textName = 0;
	m_textDistance = 0;

	m_textLevel = 0;
	for(i = 0; i < CII_MAX; ++i)
	{
		m_conIcons[i] = 0;
	}

	m_pageRole = 0;
	m_pageRoleIcon = 0;

	m_effectorBlink = 0;

	std::set<std::pair<NetworkId, SwgCuiMfdStatus *> >::iterator it;
	for(it = s_mfdStatusObjects.begin(); it != s_mfdStatusObjects.end();)
	{
		if(it->second == this)
		{
			s_mfdStatusObjects.erase(it++);
		}
		else
		{
			++it;
		}
	}

	delete m_lastClientMfdStatusUpdateMessage;
	m_lastClientMfdStatusUpdateMessage = 0;

	m_effectorCombatTarget = 0;
	m_effectorCombat = 0;
	m_sampleStateIcon = 0;
	m_sampleIconPage = 0;

	m_eliteLeftPage = 0;
	m_eliteRightPage = 0;

	m_compositePage = 0;
	m_speakingIcon = 0;
}

//------------------------------------------------------------------------


void SwgCuiStatusGround::performActivate()
{
	setIsUpdating(true);

	CreatureObject const * const creature = Game::getPlayerCreature();
	bool const isStatusForPlayer = creature && creature->getNetworkId() == m_objectId;
	PlayerObject const * const player = isStatusForPlayer ? creature->getPlayerObject() : 0;

	if (player && !RoleIconManager::doesQualifyForIcon(creature, player->getRoleIconChoice(), false))
		RoleIconManager::cycleToNextQualifyingIcon(true);

	CuiPreferences::getBuffIconSettingsChangedCallback().attachReceiver(*m_buffIconSettingsChangedCallback);

	//call once to make sure we start in a good state
	onBuffIconSettingsChangedCallback();

	updateRoleIcon();
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::performDeactivate()
{
	setIsUpdating(false);

	CuiPreferences::getBuffIconSettingsChangedCallback().detachReceiver(*m_buffIconSettingsChangedCallback);

	if(m_volumeStates != 0)
		SwgCuiBuffUtils::clearBuffIcons(*m_volumeStates);

	if(m_debuffStates != 0)
		SwgCuiBuffUtils::clearBuffIcons(*m_debuffStates);
}

//----------------------------------------------------------------------

bool SwgCuiStatusGround::OnMessage(UIWidget * context, const UIMessage & msg)
{
	if(context == m_volumeStates && m_objectId.isValid())
	{
		CreatureObject const * player = Game::getPlayerCreature();
		
		if (!player || player->getNetworkId() != m_objectId)
			return true;
			
		if (msg.Type == UIMessage::ContextRequest)
		{
			UIWidget *buffWidget = m_volumeStates->FindCell(msg.MouseCoords);

			if (buffWidget)
			{
				uint32 buffCrc = SwgCuiBuffUtils::getBuffCrc(buffWidget);
				if (buffCrc)
				{
					FormattedString<16> temp;
					char const * buffCrcString = temp.sprintf("%d", buffCrc); 
					if (buffCrcString)
					{
						ClientCommandQueue::enqueueCommand("removeBuff", NetworkId::cms_invalid, Unicode::String(Unicode::narrowToWide(buffCrcString)));
						return false;
					}
				}
		   	}
		}
		return true;
	}

	if(context == m_pageRole)
	{
		if(msg.Type == UIMessage::LeftMouseDown)
		{
			RoleIconManager::cycleToNextQualifyingIcon(false);
		}
	}
	if (msg.Type == UIMessage::LeftMouseUp)
	{
		if (m_objectId.isValid())
		{
			CreatureObject * const player = Game::getPlayerCreature();
			if (player)
			{
				player->setIntendedTarget(m_objectId);
				player->setLookAtTarget(m_objectId);
			}
			return true;
		}
	}
	if (msg.Type == UIMessage::RightMouseUp)
	{
		UIPoint const mouse = context->GetWorldPointFromLocal(msg.MouseCoords);
		bool overBuffs = false;

		if (m_volumeStates)
		{
			UIRect const worldRect = m_volumeStates->GetWorldRect();
			UIPoint const min(worldRect.left, worldRect.top);
			UIPoint const max(worldRect.right, worldRect.bottom);
			overBuffs = (mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y) && m_volumeStates->GetCellCount().y;

			if (overBuffs)
				return true;
		}

		if (m_debuffStates)
		{
			UIRect const worldRect = m_debuffStates->GetWorldRect();
			UIPoint const min(worldRect.left, worldRect.top);
			UIPoint const max(worldRect.right, worldRect.bottom);
			overBuffs = (mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y) && m_debuffStates->GetCellCount().y;

			if (overBuffs)
				return true;
		}

		const CreatureObject * const player = Game::getPlayerCreature();
		if (!player)
			return true;

		UIPopupMenu * const pop = new UIPopupMenu(&getPage()); 
		pop->SetStyle(getPage().FindPopupStyle());

		Object * const obj = NetworkIdManager::getObjectById(m_objectId);
		if (obj && m_objectId != player->getNetworkId())
		{
			CuiMenuInfoHelper * menuHelper = CuiRadialMenuManager::createMenu(*obj, context->GetWorldPointFromLocal(msg.MouseCoords), pop);

			if (menuHelper)
			{

				appendHelperPopupOptions(menuHelper);

				ClientObject * const clientObject = obj->asClientObject();
				const int got = clientObject ? clientObject->getGameObjectType () : 0;

				menuHelper->updatePopupMenu (*pop, got);

				pop->SetLocation(context->GetWorldPointFromLocal(msg.MouseCoords));
				UIManager::gUIManager().PushContextWidget(*pop);
				pop->AddCallback(this);

				return true;
			}
		}
	
		const GroupObject * const group = safe_cast<const GroupObject *>(player->getGroup().getObject());

		if (group)
		{
			const GroupObject::GroupMemberVector gmv = group->getGroupMembers();
			if (!gmv.empty())
			{
				//-- player is the leader
				if (gmv.front().first == player->getNetworkId())
				{
					if (group->getLootMaster() != m_objectId)
						pop->AddItem(PopupIds::group_masterlooter, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_MASTER_LOOTER, 0));

					if( (m_objectId == player->getNetworkId()) )
						pop->AddItem(PopupIds::group_loot, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_LOOT, 0));

					// create or use group pickup point
					if (GroupPickupPoint::isGroupPickupAllowedAtLocation(Game::getSceneId(), *player))
					{
						if (group->getSecondsLeftOnGroupPickup() == 0)
						{
							pop->AddItem(PopupIds::group_create_pickup_point, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_CREATE_PICKUP_POINT, 0));
						}
						else
						{
							int cost;
							if (GroupPickupPoint::getGroupPickupTravelCost(Game::getSceneId(), group->getGroupPickupLocationPlanet(), cost))
							{
								char buffer[64];
								snprintf(buffer, sizeof(buffer)-1, " (%d credits)", cost);
								buffer[sizeof(buffer)-1] = '\0';

								pop->AddItem(PopupIds::group_use_pickup_point, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_USE_PICKUP_POINT, 0) + Unicode::narrowToWide(buffer));
								pop->AddItem(PopupIds::group_use_pickup_point_nocamp, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_USE_PICKUP_POINT_NOCAMP, 0) + Unicode::narrowToWide(buffer));
							}
						}
					}

					pop->AddItem(PopupIds::group_disband, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_DISBAND, 0));
				}
				//-- player is NOT the leader
				else
				{
					if (GroupPickupPoint::isGroupPickupAllowedAtLocation(Game::getSceneId(), *player))
					{
						if (group->getSecondsLeftOnGroupPickup() == 0)
						{
							// an officer group member can create group pickup point
							PlayerObject const * const objAsPlayerObject = player->getPlayerObject();
							if (objAsPlayerObject && (objAsPlayerObject->getSkillTemplate().find("officer_") == 0))
							{
								pop->AddItem(PopupIds::group_create_pickup_point, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_CREATE_PICKUP_POINT, 0));
							}
						}
						else
						{
							int cost;
							if (GroupPickupPoint::getGroupPickupTravelCost(Game::getSceneId(), group->getGroupPickupLocationPlanet(), cost))
							{
								char buffer[64];
								snprintf(buffer, sizeof(buffer)-1, " (%d credits)", cost);
								buffer[sizeof(buffer)-1] = '\0';

								pop->AddItem(PopupIds::group_use_pickup_point, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_USE_PICKUP_POINT, 0) + Unicode::narrowToWide(buffer));
								pop->AddItem(PopupIds::group_use_pickup_point_nocamp, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_USE_PICKUP_POINT_NOCAMP, 0) + Unicode::narrowToWide(buffer));
							}
						}
					}

					if( (m_objectId == player->getNetworkId()) )
						pop->AddItem(PopupIds::group_leave, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::GROUP_LEAVE, 0));
				}
			}
	   	}
		
		appendPopupOptions(pop);
	 
		pop->SetLocation(context->GetWorldPointFromLocal(msg.MouseCoords));
		UIManager::gUIManager().PushContextWidget(*pop);
		pop->AddCallback(this);
	}
	return true;
}

void SwgCuiStatusGround::OnPopupMenuSelection (UIWidget * context)
{
	if (!context->IsA(TUIPopupMenu))
		return;

	std::string memberIdStr;
	context->GetPropertyNarrow(Properties::MemberId, memberIdStr);
	const NetworkId id(memberIdStr);
		
	UIPopupMenu * const pop = safe_cast<UIPopupMenu *>(context);

	NOT_NULL(pop);

	const std::string & selection = pop->GetSelectedName();

	if (selection == PopupIds::group_kick)
	{
		static const uint32 hash_group_kick = Crc::normalizeAndCalculate("dismissGroupMember");
		ClientCommandQueue::enqueueCommand(hash_group_kick, id, Unicode::String());
	}
	else if (selection == PopupIds::group_disband)
	{
		static const uint32 hash_group_disband = Crc::normalizeAndCalculate("disband");
		ClientCommandQueue::enqueueCommand(hash_group_disband, NetworkId::cms_invalid, Unicode::String());
	}
	else if (selection == PopupIds::group_leave)
	{
		static const uint32 hash_group_leave = Crc::normalizeAndCalculate("leaveGroup");
		ClientCommandQueue::enqueueCommand(hash_group_leave, NetworkId::cms_invalid, Unicode::String());
	}
	else if (selection == PopupIds::group_masterlooter)
	{
		static uint32 const hash_groupMakeMasterLooter = Crc::normalizeAndCalculate("makeMasterLooter");
		ClientCommandQueue::enqueueCommand(hash_groupMakeMasterLooter, id, Unicode::String());
	}
	else if (selection == PopupIds::group_makeleader)
	{
		static uint32 const hash_groupMakeLeader = Crc::normalizeAndCalculate("makeLeader");
		ClientCommandQueue::enqueueCommand(hash_groupMakeLeader, id, Unicode::String());
	}
	else if (selection == PopupIds::group_loot)
	{
		static uint32 const hash_groupLoot = Crc::normalizeAndCalculate("groupLoot");
		ClientCommandQueue::enqueueCommand(hash_groupLoot, NetworkId::cms_invalid, Unicode::String());
	}
	else if (selection == PopupIds::group_create_pickup_point)
	{
		static uint32 const hash_groupCreatePickupPoint = Crc::normalizeAndCalculate("createGroupPickup");
		ClientCommandQueue::enqueueCommand(hash_groupCreatePickupPoint, NetworkId::cms_invalid, Unicode::String());
	}
	else if (selection == PopupIds::group_use_pickup_point)
	{
		static uint32 const hash_groupUsePickupPoint = Crc::normalizeAndCalculate("useGroupPickup");
		ClientCommandQueue::enqueueCommand(hash_groupUsePickupPoint, NetworkId::cms_invalid, Unicode::String());
	}
	else if (selection == PopupIds::group_use_pickup_point_nocamp)
	{
		static uint32 const hash_groupUsePickupPoint = Crc::normalizeAndCalculate("useGroupPickup");
		ClientCommandQueue::enqueueCommand(hash_groupUsePickupPoint, NetworkId::cms_invalid, Unicode::narrowToWide("nocamp"));
	}
	else
	{
		SwgCuiLockableMediator::OnPopupMenuSelection(context);
		CuiRadialMenuManager::OnPopupMenuSelection(context);
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::onBuffsChanged(CreatureObject const & creature)
{
	if(creature.getNetworkId() == m_objectId)
	{
		updateCreatureBuffs(creature);
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updateRoleIcon()
{
	Object * obj = m_objectId.getObject();
	if(obj)
	{
		ClientObject *objAsClient = obj->asClientObject();
		if(objAsClient)
		{
			CreatureObject *objAsCreature = objAsClient->asCreatureObject();
			if(objAsCreature)
			{
				PlayerObject *objAsPlayer = objAsCreature->getPlayerObject();
				if(objAsPlayer)
				{
					updateRoleIcon(*objAsPlayer);
				}
				else
				{
					if(objAsCreature->getMasterId() != NetworkId::cms_invalid)
					{
						setRoleIconInternal(RoleIconManager::getPetRoleIconChoice());
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updateRoleIcon(PlayerObject const & player)
{
	bool useThisUpdate = false;
	Object * obj = m_objectId.getObject();
	PlayerObject *objAsPlayer = NULL;
	if(obj)
	{
		ClientObject *objAsClient = obj->asClientObject();
		if(objAsClient)
		{
			CreatureObject *objAsCreature = objAsClient->asCreatureObject();
			if(objAsCreature)
			{
				objAsPlayer = objAsCreature->getPlayerObject();
				if(objAsPlayer)
				{
					if(objAsPlayer->getNetworkId() == player.getNetworkId())
						useThisUpdate = true;
				}
			}
		}
	}
	if(m_pageRoleIcon &&(useThisUpdate ||(player.getNetworkId() == m_objectId)))
	{
		setRoleIconInternal(player.getRoleIconChoice());
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setRoleIconInternal(int roleIconChoice)
{
	if(m_pageRoleIcon)
	{
		m_pageRoleIcon->SetTooltip(Unicode::emptyString);

		std::string roleIconPath = ROLE_ICON_MASTER_PATH;
		std::string const & roleIconName = RoleIconManager::getRoleIconName(roleIconChoice);
		IGNORE_RETURN(roleIconPath.append(roleIconName));
		StringId roleTooltip(ROLE_TOOLTIP_STRING_FILE, roleIconName);
		m_pageRoleIcon->SetTooltip(roleTooltip.localize());

		UIImageStyle * imageStyle = safe_cast<UIImageStyle *>(UIManager::gUIManager().GetRootPage()->GetObjectFromPath(roleIconPath.c_str(), TUIImageStyle));
		if(!imageStyle)
			imageStyle = CuiIconManager::getFallback();

		m_pageRoleIcon->SetStyle(imageStyle);
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);

	if(m_lastClientMfdStatusUpdateMessage)
	{
		update(*m_lastClientMfdStatusUpdateMessage);

		delete m_lastClientMfdStatusUpdateMessage;
		m_lastClientMfdStatusUpdateMessage = NULL;
	}
	else if(updateVisiblity())
	{
		bool hamUpdated = false;

		CreatureObject * const player = Game::getPlayerCreature();
		if(player)
		{
			TangibleObject const * const tangible = dynamic_cast<TangibleObject *>(m_objectId.getObject());
			if(tangible)
			{
				bool shouldShowHam = m_statusType != ST_target || m_isLookAtTarget || m_isHoverTarget;

				updateTargetName(*tangible);
				updateDirectionArrow(player, *tangible);
				updateDistance(player, player->getDistanceBetweenCollisionSpheres_w(*tangible));
				updateFaction(*tangible);
				updateVoiceChatIcon(*tangible);

				CreatureObject const * const creature = tangible->asCreatureObject();
				if(creature)
				{
					m_isPlayerMounted = creature &&(creature->getRiderDriverCreature() == player);

					// Don't show the ham if the object can't be damaged or is dead.
					if(shouldShowHam && (!creature->isInvulnerable() || isFactionalCombatCreature(*creature)) && creature->getMaxHitPoints() > 0)
					{
						// Hack for Vehicles having Action and Mind.
						if(m_isVehicle)
						{
							hamUpdated  = updateAttackableTargetHam(*tangible, deltaTimeSecs);
						}
						else
						{
							hamUpdated = updateTargetHam(*creature, deltaTimeSecs);
						}
					}

					updateCombatEffector(*player, *creature);
					updateCreatureBuffs(*creature);
					updateCreatureLevel(*creature, shouldShowHam);
					updateTargetGuild(*creature);
					updateEliteStatus(*creature);


					PlayerObject const * const playerObject = creature->getPlayerObject();
					if(playerObject)
					{
						updatePlayerStatus(*playerObject);
						updatePlayerTitle(*playerObject);
						updatePlayerFaction(*playerObject, *creature);
					}
					else if (creature->getDecoyOrigin() != NetworkId::cms_invalid)
					{
						// Decoy creature.
						Object * originObject = NetworkIdManager::getObjectById(creature->getDecoyOrigin());
						if(originObject)
						{
							CreatureObject * originCreature = NULL;
							if(originObject->asClientObject())
								originCreature = originObject->asClientObject()->asCreatureObject();
							if(originCreature)
							{
								PlayerObject const * const decoyPlayerObject = originCreature->getPlayerObject();
								if(decoyPlayerObject)
								{
									updatePlayerStatus(*decoyPlayerObject);
									updatePlayerTitle(*decoyPlayerObject);
									updatePlayerFaction(*decoyPlayerObject, *originCreature);
								}
							}
						}
					}
				}
				else
				{
					if(shouldShowHam && !tangible->isInvulnerable() && tangible->getMaxHitPoints() > 0)
					{
						if(tangible->isAttackable())
							hamUpdated  = updateAttackableTargetHam(*tangible, deltaTimeSecs);
						else
							hamUpdated = updateTargetHam(*tangible, deltaTimeSecs);
					}
				}
			}
			else
			{
				// Update the direction arrow and range if the target goes out of observable range.
				updateDistance(player, player->getPosition_w().magnitudeBetween(m_lastKnownPosition));
				updateDirectionArrow(player, m_lastKnownPosition);
			}
		}

		if(!hamUpdated)
		{
			hideHamBars();
			hideLevelInfo();
			hideEliteStatus();
		}
	}

	if (m_compositePage && !m_hamEnhanceState)
	{
		m_compositePage->WrapChildren();
	}

	// Visibility changes and the legacy shrink-wrap pass above can repack the
	// status hierarchy.  Apply the selected authored HAM layout last so a
	// standard/enhanced transition cannot be overwritten in the same frame and
	// clip the final Mind row.  This also makes the runtime-duplicated
	// target-of-target obey the same final geometry as the primary target.
	updateHamEnhanceLayout();

	UIPage * const individualUiScaleRoot = getIndividualUiScaleRoot();
	if (individualUiScaleRoot)
	{
		float const uiScale = CuiPreferences::getHamUiScale();
		if (individualUiScaleRoot->GetScale() != uiScale)
			individualUiScaleRoot->SetScale(uiScale);
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updateHamEnhanceLayout()
{
	if (!m_hamEnhanceState)
		return;

	bool const enabled = CuiPreferences::getHamEnhance();
	if (enabled == m_hamEnhanceApplied && m_hamEnhanceState->isLayoutApplied(enabled))
		return;

	m_hamEnhanceState->apply(enabled);
	m_hamEnhanceApplied = enabled;
}

//------------------------------------------------------------------------

UIPage * SwgCuiStatusGround::getIndividualUiScaleRoot() const
{
	return m_individualUiScaleRoot;
}
//------------------------------------------------------------------------

int SwgCuiStatusGround::getBuffIconType()
{
	switch(m_statusType)
	{
	case ST_player:
		return CuiPreferences::BIT_status;
		break;
	case ST_lookAtTarget:
		return CuiPreferences::BIT_secondaryTarget;
		break;
	case ST_intendedTarget:
	case ST_target:
		return CuiPreferences::BIT_target;
		break;
	case ST_group:
		return CuiPreferences::BIT_group;
		break;
	case ST_pet:
		return CuiPreferences::BIT_pet;
		break;
	default:
		DEBUG_FATAL(true, ("SwgCuiStatusGround does not know how to map StatusType %d to BuffIconType", m_statusType));
		return CuiPreferences::BIT_status;
	}
}

void SwgCuiStatusGround::updateCreatureBuffs(CreatureObject const & creature)
{
	if(m_volumeStates && m_sampleStateIcon)
	{
		// Publish 14 owns a dedicated AttribMod panel for the player's timed
		// buffs and debuffs.  Its inline MFD status volume occupies half of the
		// horizontal composite, so exposing that volume also collapses the HAM
		// bars to half width.  Leave target/group volumes intact, but keep the
		// player's authored inline volume empty and hidden.
		if (m_statusType == ST_player && !m_debuffStates)
		{
			bool const hadIcons = m_volumeStates->GetChildCount() > 0;
			if (hadIcons)
				SwgCuiBuffUtils::clearBuffIcons(*m_volumeStates);
			bool const visibilityChanged = pageSetVisible(m_volumeStates, false);
			if (hadIcons || visibilityChanged)
				getPage().ForcePackChildren();
			return;
		}

		UIVolumePage & debuffPage = m_debuffStates ? *m_debuffStates : *m_volumeStates;
		uint32 const buffStates = SwgCuiBuffUtils::updateBuffs(creature, *m_volumeStates, debuffPage, *m_sampleStateIcon, m_effectorBlink, m_sampleIconPage);

		// Publish 14 uses one combined volume for buffs and debuffs.  Later UI
		// layouts split them into two volumes, so preserve that layout below when
		// it is available and use the original combined layout otherwise.
		if (!m_debuffStates)
		{
			bool const hasStatusIcons = (buffStates & (SwgCuiBuffUtils::UBRT_hasBuffs | SwgCuiBuffUtils::UBRT_hasDebufs)) != 0;
			bool suppressInlineStates = false;
			if (CuiPreferences::getHamEnhance() && m_individualUiScaleRoot)
				IGNORE_RETURN(m_individualUiScaleRoot->GetPropertyBoolean(s_hamEnhanceHideStates, suppressInlineStates));

			// Enhanced Target uses the complete 229-pixel backing. Its legacy state
			// volume occupies the left 114 pixels in the standard layout, so keep the
			// volume's contents current but do not draw it over the enhanced bars.
			// Target-of-target duplicates the marked Target root and follows the same
			// path. Turning enhancement off restores the current icon-driven state on
			// this very update rather than waiting for another BuffsChanged message.
			bool const visibilityChanged = pageSetVisible(m_volumeStates, hasStatusIcons && !suppressInlineStates);
			if (!hasStatusIcons || suppressInlineStates)
			{
				if (visibilityChanged && !suppressInlineStates)
					getPage().ForcePackChildren();
				return;
			}

			UISize stateSize = m_volumeStates->GetSize();
			UISize const stateCellCount = m_volumeStates->GetCellCount();
			UISize const stateCellSize = m_volumeStates->GetCellSize();
			int const stateHeight = stateCellSize.y * stateCellCount.y;
			bool const resizeParent = getPage().GetSize().y < m_volumeStates->GetLocation().y + stateHeight;

			if (stateHeight != stateSize.y || resizeParent || visibilityChanged)
			{
				stateSize.y = stateHeight;
				m_volumeStates->SetMaximumSize(stateSize);
				m_volumeStates->SetSize(stateSize);

				UISize parentSize = getPage().GetSize();
				parentSize.y = m_volumeStates->GetLocation().y + stateHeight;
				getPage().SetMaximumSize(parentSize);
				getPage().SetSize(parentSize);
				getPage().ForcePackChildren();
			}
			return;
		}

		bool buffVisibilityChanged = pageSetVisible(m_volumeStates, buffStates&SwgCuiBuffUtils::UBRT_hasBuffs); 
		bool debuffVisibilityChanged = pageSetVisible(m_debuffStates, buffStates&SwgCuiBuffUtils::UBRT_hasDebufs);

		UISize stateSize = m_volumeStates->GetSize ();
		UISize stateCellCount = m_volumeStates->GetCellCount ();
		UISize stateCellSize = m_volumeStates->GetCellSize ();
		int const stateHeight = stateCellSize.y * stateCellCount.y;

		UISize debuffSize = m_debuffStates->GetSize ();
		UISize debuffCellCount = m_debuffStates->GetCellCount ();
		int const debuffHeight = m_debuffStates->GetCellSize ().y * debuffCellCount.y;

		//Special band-aid for group window
		if (m_statusType == ST_group)
		{
			int numWidgits = m_debuffStates->GetWidgetCount ();
			UISize stateCellCountFixed = m_debuffStates->GetCellCountFixed ();
			bool spaceForDebuffs = (numWidgits < stateCellCountFixed.x);
			if(spaceForDebuffs)
			{
				int spacer = numWidgits > 0 ? stateCellSize.x / 4 : 0;
				UISize debuffPageLocation = m_debuffStates->GetLocation ();
				debuffPageLocation.x += numWidgits * stateCellSize.x + spacer;

				debuffSize.x = (stateCellCountFixed.x - numWidgits) * stateCellSize.x;

				m_volumeStates->SetLocation (debuffPageLocation);
				m_volumeStates->SetMaximumSize (debuffSize);
				m_volumeStates->SetSize (debuffSize);
			}
			else
			{
				pageSetVisible (m_volumeStates, false);
			}

			getPage().ForcePackChildren();
			return;
		}

		bool const resizeParent = (getPage().GetSize().y < m_volumeStates->GetLocation().y + stateHeight + debuffHeight);

		if (stateHeight != stateSize.y || debuffHeight != debuffSize.y || resizeParent || buffVisibilityChanged || debuffVisibilityChanged)
		{
			stateSize.y = stateHeight;
			m_volumeStates->SetMaximumSize(stateSize);
			m_volumeStates->SetSize(stateSize);
			
			debuffSize.y = debuffHeight;
			
			m_debuffStates->SetMaximumSize(debuffSize);
			m_debuffStates->SetSize(debuffSize);
				   
			int cellBuffer = (debuffHeight ?  stateCellSize.y / 4 : 0);

			UIPoint statesLoc = m_volumeStates->GetLocation() + stateSize;
			statesLoc.y += cellBuffer;
			statesLoc.x = m_debuffStates->GetLocation().x;

			m_debuffStates->SetLocation(statesLoc);
					
			UISize parentSize = getPage().GetSize();
			UIPoint statePosition = m_volumeStates->GetLocation();
			parentSize.y = statePosition.y + stateSize.y + debuffSize.y + cellBuffer;
			getPage().SetMaximumSize(parentSize);
			getPage().SetSize(parentSize);
			getPage().ForcePackChildren();
		}
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updateCreatureLevel(CreatureObject const & creature, bool const enableCreatureConLevel)
{
	CreatureObject const * player = Game::getPlayerCreature();
	if(player)
	{
		if(m_isVehicle || creature.isInvulnerable() || creature.isDead())
		{
			if(!m_isVehicle)
			{
				hideHamBars();
				hideEliteStatus();
			}

			// don't show level info if the creature is invulnerable
			hideLevelInfo();
			return;
		}

		bool const shouldShowLevel = !m_isVehicle &&
								enableCreatureConLevel &&
								(m_isThePlayer ? !CuiSkillManager::getSkillTemplate().empty() : (m_statusType != ST_target || m_isLookAtTarget || (m_statusType == ST_pet)));

		if(m_iconDifficulty)
		{
			IGNORE_RETURN(pageSetVisible(m_iconDifficulty, shouldShowLevel));

			if(shouldShowLevel)
			{
				UIColor color = CuiCombatManager::getConColor(creature);
				m_iconDifficulty->SetColor(color);
			}
		}

		int const creatureLevel = creature.getLevel();
		int const creatureGroupLevel = m_statusType == ST_group ? creature.getGroupLevel() : creature.getLevel();
		int const playerLevel = m_statusType == ST_group ? player->getGroupLevel() : player->getLevel();
		if(m_textLevel != NULL)
		{
			// Pre-CU: no numeric combat level shown next to names. The con-color difficulty
			// icon (m_iconDifficulty) and the lower/higher/even arrows (m_conIcons) below still
			// convey RELATIVE difficulty (the grey->green->...->red->purple hierarchy). Always
			// hide the numeric level text. (creatureLevel left declared; harmless unused.)
			IGNORE_RETURN(pageSetVisible(m_textLevel, false));
		}

		if(shouldShowLevel && !m_isThePlayer)
		{
			for(unsigned i = 0; i < CII_MAX; ++i)
			{
				if(m_conIcons[i] != NULL)
					m_conIcons[i]->SetVisible(false);
			}
			UIColor color = CuiCombatManager::getConColor(creature);
			if(playerLevel > creatureGroupLevel && m_conIcons[CII_lower] != NULL)
			{
				m_conIcons[CII_lower]->SetColor(color);
				m_conIcons[CII_lower]->SetVisible(true);
			}
			else if(playerLevel < creatureGroupLevel && m_conIcons[CII_higher] != NULL)
			{
				m_conIcons[CII_higher]->SetColor(color);
				m_conIcons[CII_higher]->SetVisible(true);
			}
			else if(playerLevel == creatureGroupLevel && m_conIcons[CII_even] != NULL)
			{
				m_conIcons[CII_even]->SetOpacity(1.0f);
				m_conIcons[CII_even]->SetColor(color);
				m_conIcons[CII_even]->SetVisible(true);
			}
		}
		else
		{
			if(m_conIcons[CII_even])
			{
				m_conIcons[CII_even]->SetOpacity(0.0f);
				m_conIcons[CII_even]->SetVisible(true);
			}
		}
	}
}

//------------------------------------------------------------------------

bool SwgCuiStatusGround::updateTargetHam(CreatureObject const & creature, float deltaTimeSecs)
{
	bool hamUpdated = false;

	PageStyle pageStyle = PS_none;

	if(creature.isPlayer() ||(m_statusType == ST_group))
	{
		// Get a player object for the force power special case.
		PlayerObject const * const playerObject = creature.getPlayerObject();
		if(playerObject && playerObject->getMaxForcePower())
		{
			pageStyle = PS_jedi;
		}
		else
		{
			pageStyle = PS_ham;
		}
	}
	else
	{
		if(creature.isAttackable() || isFactionalCombatCreature(creature) || m_isPlayerControlled || (m_statusType == ST_target && creature.getForceShowHam()))
		{
			// Publish 14 creature targets and overheads always expose all three
			// combat pools.  Zero or delayed Action/Mind replication must not
			// switch the mediator to the later single-health presentation.
			pageStyle = PS_ham;
		}
		else
			pageStyle = PS_greyHealth;
	}

	if(pageStyle == PS_none)
	{
		hideHamBars();
		hideEliteStatus();
	}
	else
	{
		hamUpdated = updateHamBar(creature, pageStyle, deltaTimeSecs);
	}

	return hamUpdated;
}

//------------------------------------------------------------------------

// Show the HUD health bar for attackable objects
bool SwgCuiStatusGround::updateAttackableTargetHam(TangibleObject const &obj, float deltaTimeSecs)
{
	bool needsPacking = false;
	bool hamUpdated = false;

	IGNORE_RETURN(pageSetVisible(m_hamBarParent, true));

	// Get the max attribute to scale page.
	int max_normal_maxes = 0;

	int const maxHit = obj.getMaxHitPoints();
	max_normal_maxes = std::max(max_normal_maxes, maxHit);


	needsPacking |= pageSetVisible(&(m_bars[0]->getPage()), true);
	m_bars[0]->activate();

	if(m_bars[0])
	{
		Attributes::Enumerator const attr = m_attributes[0];

		hamUpdated = true;

		int const current = max_normal_maxes - obj.getDamageTaken();

		m_bars[0]->setAttributeIndex(attr);
		m_bars[0]->setUseVerboseTooltip(false);
		m_bars[0]->updateBar(	max_normal_maxes,
										max_normal_maxes,
										max_normal_maxes,
										current,
										false,
										0,
										deltaTimeSecs);

		updateHamBarToolTips(0, current, max_normal_maxes);
	

		if(needsPacking)
		{
			UIPage * parent = safe_cast<UIPage *>(m_bars[0]->getPage().GetParent());
			if(parent)
				parent->SetPackDirty(true);

			needsPacking = false;
		}
	}

	return hamUpdated;

}

//------------------------------------------------------------------------

void SwgCuiStatusGround::clearTarget()
{
	std::set<std::pair<NetworkId, SwgCuiMfdStatus *> >::iterator f = s_mfdStatusObjects.find(std::make_pair(m_objectId, this));
	if(f != s_mfdStatusObjects.end())
		s_mfdStatusObjects.erase(f);

	m_objectId = NetworkId::cms_invalid;

	onNewTarget();

	update(Clock::frameTime());
}


//------------------------------------------------------------------------

void SwgCuiStatusGround::setTarget(const NetworkId & id)
{
	if ((id != m_objectId) && (id.isValid()))
	{
		Object         const * const object  = NetworkIdManager::getObjectById(id);
		ClientObject   const * const client = object ? object->asClientObject()  : NULL;
		CreatureObject const * const creature = client ? client->asCreatureObject()  : NULL;
		CreatureObject   const * const mainPlayer = Game::getPlayerCreature();
		
		bool isGroupedWithMainPlayer = false;

		if (creature)
		{
			CachedNetworkId const groupId(mainPlayer->getGroup());
			GroupObject const * group = safe_cast<const GroupObject *>(groupId.getObject());

			if (group) 
			{
				bool isLeader;
				isGroupedWithMainPlayer = group->findMember(creature->getNetworkId(), isLeader);
		 	}

			
			if (!isGroupedWithMainPlayer && client && !client->isTargettable())
			{
				if (mainPlayer && mainPlayer->getNetworkId() != creature->getNetworkId())
				{
					return;
				}		
			}
		}
		
		std::set<std::pair<NetworkId, SwgCuiMfdStatus *> >::iterator f = s_mfdStatusObjects.find(std::make_pair(m_objectId, this));
		if(f != s_mfdStatusObjects.end())
			s_mfdStatusObjects.erase(f);

		m_objectId = id;

		onNewTarget();

		IGNORE_RETURN(s_mfdStatusObjects.insert(std::make_pair(id, this)));

		update(Clock::frameTime());
	}
	else if (id == NetworkId::cms_invalid)
	{
		clearTarget();
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setTarget(TangibleObject * tangible)
{
	if(tangible)
		setTarget(tangible->getNetworkId());
	else
		setTarget(NetworkId::cms_invalid);
}

//------------------------------------------------------------------------

// RLS TODO - This needs to be cleaned up.

void SwgCuiStatusGround::updateTargetName(ClientObject const & obj)
{
	TangibleObject const * const tangibleObject = obj.asTangibleObject();

	// choose the color to draw the name in base on the target's level
	if(m_textName)
	{
		if(tangibleObject != NULL)
		{
			// Determine if we are fighting the target.
			CreatureObject const * const playerCreatureObject = Game::getPlayerCreature();

			if(   m_pulseNameOnCombat
			    &&(playerCreatureObject != NULL)
			    &&(playerCreatureObject->getCombatTarget() == obj.getNetworkId())
				&&(obj.asTangibleObject() &&(obj.asTangibleObject()->isAttackable() || obj.asTangibleObject()->isEnemy())))
			{
				const UIColor & aggroColor = CuiGameColorManager::getCombatColor();
				m_textName->SetTextColor(aggroColor);
			}
			else
			{
				m_textName->SetTextColor(CuiGameColorManager::findColorForObject(*tangibleObject, true, true));
			}

			m_textName->SetOpacity(1.0f);

			// fade text out for slealthed characters that can still be seen on this client
			CreatureObject const * const creatureObject = obj.asCreatureObject();
			if( creatureObject
				&& m_statusType == ST_target
			)
			{
				SkeletalAppearance2 * const skelApp = const_cast<SkeletalAppearance2 *>(creatureObject->getAppearance() ? creatureObject->getAppearance()->asSkeletalAppearance2() : 0);	
				if(skelApp 
					&& (skelApp->getFadeState() == SkeletalAppearance2::FS_fadeToHold || skelApp->getFadeState() == SkeletalAppearance2::FS_hold)
				)
				{
					m_textName->SetOpacity(0.08f);
				}
			}
		}
		else
		{
			UIColor const & aggroColor = CuiCombatManager::getInvulnerableConColor();
			m_textName->SetTextColor(aggroColor);
		}

	}

	if(m_displayNameOnly)
		m_objectName = obj.getLocalizedName();
	else
		CuiObjectTextManager::getObjectFullName(m_objectName, obj);

	// Append (Hidden) tag on tangible objects which are hidden to non-CSRs
	if(Game::getPlayerObject()->isAdmin())
	{
		if(!tangibleObject->isVisible())
		{
			m_objectName.append(Unicode::narrowToWide("\n(Hidden)"));
		}
	}
	
	setTargetName(m_objectName);
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setTargetName(const Unicode::String & name)
{
	if(m_textName)
	{
		Unicode::String const & newText = m_targetNamePrefix + name;
		bool const differentNames = newText != m_textName->GetLocalText();
		if(differentNames)
		{
			IGNORE_RETURN(pageSetVisible(m_textName, true));

			m_textName->SetLocalText(newText);

			if(m_resizeTargetName)
			{
				m_textName->SizeToContent();
				m_textName->Center(true, false);

				UIPage * parent = safe_cast<UIPage *>(m_textName->GetParent());
				if(parent)
				{
					parent->Pack();
				}
			}
		}
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setObjectName(const Unicode::String & name)
{
	m_objectName = name;
}


//------------------------------------------------------------------------

void SwgCuiStatusGround::setDisplayStates(bool stats, bool states, bool posture)
{
	m_displayStats = stats;
	m_displayStates = states;
	m_displayPosture = posture;
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setTargetNamePrefix(const Unicode::String & prefix)
{
	m_targetNamePrefix = prefix;
	const ClientObject * const clientObject = safe_cast<const ClientObject *>(m_objectId.getObject());

	if(clientObject)
		setTargetName(m_objectName);
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setShowRange(bool b)
{
	m_showRange = b;
	IGNORE_RETURN(pageSetVisible(m_textDistance, b));
}

// ----------------------------------------------------------------------

void SwgCuiStatusGround::update(const ClientMfdStatusUpdateMessage & updateMessage)
{
	Archive::ByteStream bs;
	updateMessage.pack(bs);
	Archive::ReadIterator ri(bs);
	if(m_lastClientMfdStatusUpdateMessage)
	{
		m_lastClientMfdStatusUpdateMessage->unpack(ri);
	}
	else
	{
		m_lastClientMfdStatusUpdateMessage = new ClientMfdStatusUpdateMessage(ri);
	}

	if(Game::getSceneId() == updateMessage.getSceneName())
	{
		if(m_directionArrow)
			m_directionArrow->SetVisible(true);
	}
	else
	{
		StringId const sceneStringId(StringTables::planet_n, updateMessage.getSceneName());
		setTargetName(m_objectName + Unicode::narrowToWide("(") + sceneStringId.localize() + Unicode::narrowToWide(")"));

		if(m_directionArrow)
			m_directionArrow->SetVisible(false);
	}

	Vector const & position2 = updateMessage.getWorldCoordinates();

	Object const * const player = Game::getPlayer();
	if(player != 0)
	{
		Vector const & position1 = player->findPosition_w();

		float const distance = position1.magnitudeBetween(position2);

		updateDistance(player, distance);
	}

	updateDirectionArrow(Game::getPlayer(), position2);

	if(m_directionArrow)
	{
		m_directionArrow->SetColor(s_targetNotObservedColor);
	}
	updateRoleIcon();

}

// ----------------------------------------------------------------------

void SwgCuiStatusGround::deliverClientMfdStatusUpdateMessage(const ClientMfdStatusUpdateMessage & msg)
{
	std::set<std::pair<NetworkId, SwgCuiMfdStatus *> >::iterator i;
	for(i = s_mfdStatusObjects.begin(); i != s_mfdStatusObjects.end(); ++i)
	{
		if(i->first == msg.getSourceId())
		{
			i->second->update(msg);
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiStatusGround::setDisplayNameOnly(bool display)
{
	if(!boolEqual(m_displayNameOnly, display))
	{
		m_displayNameOnly = display;
	}
}

// ----------------------------------------------------------------------

void SwgCuiStatusGround::setResizeTargetName(bool resize)
{
	m_resizeTargetName = resize;
}

// ----------------------------------------------------------------------

void SwgCuiStatusGround::setPulseNameOnCombat(bool pulse)
{
	m_pulseNameOnCombat = pulse;
}

//----------------------------------------------------------------------

void SwgCuiStatusGround::setShowOutOfRange(bool const outOfRange)
{
	m_outOfRange = outOfRange;
}

//----------------------------------------------------------------------

void SwgCuiStatusGround::hideLevelInfo()
{
	IGNORE_RETURN(pageSetVisible(m_iconDifficulty, false));
	IGNORE_RETURN(pageSetVisible(m_textLevel, false));

	for(int i = 0; i < CII_MAX; ++i)
	{
		IGNORE_RETURN(pageSetVisible(m_conIcons[i], false));
	}
}

//----------------------------------------------------------------------

void SwgCuiStatusGround::enableCombatEffector(bool const enable)
{
	if(m_effectorCombat && m_effectorCombatTarget)
	{
		bool const isEffectorEnabled = UIManager::gUIManager().HasEffector(m_effectorCombat, m_effectorCombatTarget);

		if(!boolEqual(isEffectorEnabled, enable))
		{
			if(enable)
			{
				m_effectorCombatTarget->ExecuteEffector(*m_effectorCombat);

				TangibleObject  *  tangible = static_cast<TangibleObject *>(m_objectId.getObject());
				if (tangible && CuiCombatManager::getShowEnteringCombat())
				{
					CreatureObject const * const playerCreature = Game::getPlayerCreature();
					if (playerCreature && playerCreature->getNetworkId() == tangible->getNetworkId())
					{
						ClientObject  *  clientObject = tangible->asClientObject();
						if (clientObject)
						{
							clientObject->addFlyText(s_enteringCombatFlyText.localize(),1.0f,VectorArgb::solidRed, 0.75f,CuiTextManagerTextEnqueueInfo::TW_starwars);
						}
					}			
				}
			}
			else
			{
				m_effectorCombatTarget->CancelEffector(*m_effectorCombat);
			}
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiStatusGround::updateCombatEffector(CreatureObject const & player, CreatureObject const & creature)
{
	bool inCombat = false;

	if(creature.getState(States::Combat))
	{
		inCombat =(&player == &creature) || player.isGroupedWith(creature);
	}

	enableCombatEffector(inCombat);
}

//----------------------------------------------------------------------

bool SwgCuiStatusGround::updateVisiblity()
{
	bool setVisible = true;

	SwgCuiHud * const hud = SwgCuiHudFactory::findMediatorForCurrentHud();
	if (hud && !hud->getHudEnabled())
	{
		getPage().SetVisible(false);
		return false;
	}

	switch(m_statusType)
	{
	case ST_lookAtTarget:
	case ST_intendedTarget:
	case ST_pet:
		{
			setVisible = m_objectId.isValid() && m_objectId.getObject() != 0;
			getPage().SetVisible(setVisible);
		}
		break;

	case ST_player:
		{
			if (m_objectId.getObject() != 0)
			{
				pageSetVisible(&getPage(), true);
			}
		}
	case ST_target:
		{
			getPage().SetGetsInput(true);
			//getPage().SetAbsorbsInput(false);
		}
		break;
	}

	return setVisible;
}


//------------------------------------------------------------------------

void SwgCuiStatusGround::updateDirectionArrow(Object const * const source, TangibleObject const & target)
{
	if(source && m_directionArrow)
	{
		updateDirectionArrow(source, target.findPosition_w());
		m_directionArrow->SetColor(CuiGameColorManager::findColorForObject(target, false, true));
	}
}

// ----------------------------------------------------------------------

void SwgCuiStatusGround::updateDirectionArrow(Object const * const source, Vector const & target)
{
	if(source && m_directionArrow &&(!m_isPlayerMounted || m_statusType == ST_group)|| (m_statusType == ST_pet))
	{
		IGNORE_RETURN(pageSetVisible(m_directionArrow, true));

		m_lastKnownPosition = target;

		Vector const & cameraPos_w = source->findPosition_w();

		Vector targetPos_w = target;
		targetPos_w.y = cameraPos_w.y;

		float const camera_angle = source->getObjectFrameK_w().theta();
		Vector const delta_vector = targetPos_w - cameraPos_w;
		float const target_angle = delta_vector.theta();
		float const theta = target_angle - camera_angle;

		m_directionArrow->SetRotation(theta / PI_TIMES_2 - 0.25f);
	}
	else
	{
		IGNORE_RETURN(pageSetVisible(m_directionArrow, false));
	}
}

// ----------------------------------------------------------------------

void SwgCuiStatusGround::updateVoiceChatIcon(TangibleObject const & tangibleObject)
{
	if(m_speakingIcon)
	{
		CreatureObject const * const pcreature = tangibleObject.asCreatureObject();
		if(pcreature)
		{
			//at this point isThisObjectSpeaking is quite expensive so we want to eliminate as many
			//object that can't speak as possible.
			if(pcreature->getPlayerObject())
			{
				IGNORE_RETURN(pageSetVisible(m_speakingIcon, CuiVoiceChatManager::isThisObjectSpeaking(m_objectId)));
				return;
			}
		}
	}

	IGNORE_RETURN(pageSetVisible(m_speakingIcon, false));
}

//------------------------------------------------------------------------

bool SwgCuiStatusGround::updateHamBar(CreatureObject const & creature, int const styleIndex, float deltaTimeSecs)
{
	bool needsPacking = false;
	bool hamUpdated = false;

	PageStyle const pageStyle = static_cast<PageStyle>(styleIndex);

	PlayerObject const * const playerObject = creature.getPlayerObject();

	IGNORE_RETURN(pageSetVisible(m_hamBarParent, true));

	// Get the max attribute to scale page.
	int max_normal_maxes = 0;
	{
		for(int pageIndex = 0; pageIndex < SwgCuiStatusGround::HAMBarPageCount; ++pageIndex)
		{
			bool const isPageActive = s_activePagesPerStyle[pageStyle][pageIndex];
			Attributes::Enumerator const attr = m_attributes[pageIndex];

			if(isPageActive)
			{
				if(attr == ForceAttribute)
				{
					if(m_isJedi && playerObject)
					{
						max_normal_maxes = std::max(max_normal_maxes, static_cast<int>(playerObject->getMaxForcePower()));
					}
				}
				else
				{
					max_normal_maxes = std::max(max_normal_maxes, static_cast<int>(creature.getMaxAttribute(m_attributes[pageIndex])));
				}
			}

			// Activate/Deactivate pages if necessary.
			if(m_bars[pageIndex])
			{
				if(isPageActive &&((attr != ForceAttribute) || m_isJedi))
				{
					needsPacking |= pageSetVisible(&(m_bars[pageIndex]->getPage()), true);
					m_bars[pageIndex]->activate();
				}
				else
				{
					needsPacking |= pageSetVisible(&(m_bars[pageIndex]->getPage()), false);
					m_bars[pageIndex]->deactivate();
				}
			}
		}
	}

	// Update the pages.
	{
		for(int pageIndex = 0; pageIndex < SwgCuiStatusGround::HAMBarPageCount; ++pageIndex)
		{
			bool const isPageActive = s_activePagesPerStyle[pageStyle][pageIndex];

			if(isPageActive && m_bars[pageIndex])
			{
				Attributes::Enumerator const attr = m_attributes[pageIndex];

				hamUpdated = true;

				if(attr == ForceAttribute)
				{
					if(m_isJedi && playerObject)
					{
						int const maxForcePower = playerObject->getMaxForcePower();
						int const forcePower = playerObject->getForcePower();

						m_bars[pageIndex]->updateBar(	max_normal_maxes,
														maxForcePower,
														maxForcePower,
														forcePower,
														false,
														0,
														deltaTimeSecs);
						updateHamBarToolTips(pageIndex, forcePower, maxForcePower);
					}
				}
				else
				{
					m_bars[pageIndex]->setAttributeIndex(attr);
					m_bars[pageIndex]->setUseVerboseTooltip(m_isThePlayer);
					m_bars[pageIndex]->updateBar(	max_normal_maxes,
													creature.getMaxAttribute(attr),
													creature.getCurrentMaxAttribute(attr),
													creature.getAttribute(attr),
													false,
													0,
													deltaTimeSecs);

					updateHamBarToolTips(pageIndex, creature.getAttribute(attr), creature.getMaxAttribute(attr));
				}

				if(needsPacking)
				{
					UIPage * parent = safe_cast<UIPage *>(m_bars[pageIndex]->getPage().GetParent());
					if(parent)
						parent->SetPackDirty(true);

					needsPacking = false;
				}
			}
		}
	}

	return hamUpdated;
}
//------------------------------------------------------------------------

void SwgCuiStatusGround::hideHamBars()
{
	IGNORE_RETURN(pageSetVisible(m_hamBarParent, false));

	for(int pageIndex = 0; pageIndex < SwgCuiStatusGround::HAMBarPageCount; ++pageIndex)
	{
		if(m_bars[pageIndex])
		{
			if(m_bars[pageIndex])
			{
				IGNORE_RETURN(pageSetVisible(&(m_bars[pageIndex]->getPage()), false));
				m_bars[pageIndex]->deactivate();
			}
		}
	}
}

//------------------------------------------------------------------------

bool SwgCuiStatusGround::updateTargetHam(TangibleObject const & tangible, float deltaTimeSecs)
{
	bool needsPacking = false;
	bool hamUpdated = false;

	PageStyle pageStyle = PS_healthOnly;

	IGNORE_RETURN(pageSetVisible(m_hamBarParent, true));

	// Get the max attribute to scale page.
	int max_normal_maxes = 0;
	{
		for(int pageIndex = 0; pageIndex < SwgCuiStatusGround::HAMBarPageCount; ++pageIndex)
		{
			Attributes::Enumerator const attr = m_attributes[pageIndex];
			bool const isPageActive = s_activePagesPerStyle[pageStyle][pageIndex] && attr != ForceAttribute;

			if(isPageActive)
			{
				int const maxHit = tangible.getMaxHitPoints();
				max_normal_maxes = std::max(max_normal_maxes, maxHit);
			}

			// Activate/Deactivate pages if necessary.
			if(m_bars[pageIndex])
			{
				if(isPageActive &&
					(attr != ForceAttribute))
				{
					needsPacking |= pageSetVisible(&(m_bars[pageIndex]->getPage()), true);
					m_bars[pageIndex]->activate();
				}
				else
				{
					needsPacking |= pageSetVisible(&(m_bars[pageIndex]->getPage()), false);
					m_bars[pageIndex]->deactivate();
				}
			}
		}
	}

	// Update the pages.
	{
		for(int pageIndex = 0; pageIndex < SwgCuiStatusGround::HAMBarPageCount; ++pageIndex)
		{
			Attributes::Enumerator const attr = m_attributes[pageIndex];
			bool const isPageActive = s_activePagesPerStyle[pageStyle][pageIndex] &&(attr != ForceAttribute);

			if(isPageActive && m_bars[pageIndex])
			{
				hamUpdated = true;

				int const current = max_normal_maxes - tangible.getDamageTaken();

				m_bars[pageIndex]->setAttributeIndex(attr);
				m_bars[pageIndex]->updateBar(	max_normal_maxes,
					max_normal_maxes,
					max_normal_maxes,
					current,
					false,
					0,
					deltaTimeSecs);

				updateHamBarToolTips(pageIndex, current, max_normal_maxes);


				if(needsPacking)
				{
					UIPage * const parent = safe_cast<UIPage *>(m_bars[pageIndex]->getPage().GetParent());
					if(parent)
						parent->SetPackDirty(true);

					needsPacking = false;
				}
			}
		}
	}

	return hamUpdated;
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updateDistance(Object const * const playerObject, float const distance)
{
	if(m_textDistance)
	{
		bool const shouldShowRange =(m_statusType == ST_lookAtTarget) ||(m_statusType == ST_group) || (m_statusType == ST_intendedTarget) || (m_statusType == ST_pet)
			||(!m_isThePlayer && !m_isPlayerMounted && m_showRange &&(m_isHoverTarget || m_isLookAtTarget));

		IGNORE_RETURN(pageSetVisible(m_textDistance, shouldShowRange));

		if(shouldShowRange &&(m_lastKnownDistance != distance))
		{
			m_lastKnownDistance = distance;

			Unicode::String text;

			if(m_outOfRange || !playerObject)
			{
				text += ClientTextManager::getColorCode(PackedRgb::solidRed);
				text += Unicode::narrowToWide("(Out Of Range)");
				text += ClientTextManager::getResetTagCode();
			}
			else
			{
				text += ClientTextManager::getColorCode(PackedRgb::solidWhite);
				text += Unicode::narrowToWide(FormattedString<128>().sprintf(" %.0fm", ceilf(std::max(0.0f, m_lastKnownDistance))));
				text += ClientTextManager::getResetTagCode();
			}

			m_textDistance->SetLocalText(text);
		}
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::showDebugInformation(bool const show)
{
	IGNORE_RETURN(pageSetVisible(m_textDebug, show));
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setDebugInformation(std::string const & debugInfo)
{
	if(m_textDebug)
	{
		m_textDebug->SetPreLocalized(true);
		m_textDebug->SetLocalText(Unicode::narrowToWide(debugInfo));
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updatePlayerStatus(PlayerObject const & playerObject)
{
	if(m_textStatus)
	{
		Unicode::String statusInfo;
		appendPlayerStatusString(playerObject, statusInfo);

		if(statusInfo.empty())
		{
			IGNORE_RETURN(pageSetVisible(m_textStatus, false));
			m_textStatus->SetLocalText(Unicode::emptyString);
		}
		else
		{
			if(m_textStatus->GetLocalText() != statusInfo)
			{
				IGNORE_RETURN(pageSetVisible(m_textStatus, true));
				m_textStatus->SetLocalText(statusInfo);
			}
		}
	}

	updateRoleIcon(playerObject);
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updateTargetGuild(CreatureObject const & creature)
{
	if(m_textGuild)
	{
		Unicode::String guildName;

		int const guildId = creature.getGuildId();
		if(guildId)
		{
			guildName = GuildObject::getGuildAbbrevUnicode(guildId);
		}

		if(guildName.empty())
		{
			IGNORE_RETURN(pageSetVisible(m_textGuild, false));
		}
		else
		{
			if(guildName != m_textGuild->GetLocalText())
			{
				IGNORE_RETURN(pageSetVisible(m_textGuild, true));

				m_textGuild->SetPreLocalized(true);
				m_textGuild->SetLocalText(guildName);
			}
		}
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updatePlayerFaction(PlayerObject const & player, CreatureObject const & creature)
{
	if(m_textFaction)
	{
		Unicode::String factionRank;
		if (player.getCurrentGcwRank() >= 1)
		{
			uint32 const pvpFaction = creature.getPvpFaction();
			if (PvpData::isImperialFactionId(pvpFaction) || PvpData::isRebelFactionId(pvpFaction))
				factionRank = CreatureObject::getLocalizedGcwRankString(player.getCurrentGcwRank(), pvpFaction);
		}

		m_textFaction->SetPreLocalized(true);
		m_textFaction->SetLocalText(factionRank);
		IGNORE_RETURN(pageSetVisible(m_textFaction, player.isDisplayingFactionRank()));
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updatePlayerTitle(PlayerObject const & playerObject)
{
	if(m_textTitle)
	{
		Unicode::String const & title = playerObject.getLocalizedTitle();

		if(title.empty())
		{
			IGNORE_RETURN(pageSetVisible(m_textTitle, false));
		}
		else
		{
			pageSetVisible(m_textTitle, true);
			if(m_textTitle->GetLocalText() != title)
			{
				m_textTitle->SetPreLocalized(true);
				m_textTitle->SetLocalText(title);
			}
		}
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updateFaction(TangibleObject const & tangibleObject)
{
	// if the target is factioned, show the faction icon
	if(m_iconFaction)
	{
		UIImageStyle * icon = NULL;
		icon = CuiObjectTextManager::getObjFactionIcon(tangibleObject);
		if(icon)
		{
			IGNORE_RETURN(pageSetVisible(m_iconFaction, true));
			m_iconFaction->SetOpacity(1.0f);
			m_iconFaction->SetStyle(icon);
		}
		else
		{
			IGNORE_RETURN(pageSetVisible(m_iconFaction, false));
			m_iconFaction->SetOpacity(0.0f);		
		}
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::updateHamBarToolTips(int barIndex, int current, int normalMax)
{
	if(m_statusType != ST_target && barIndex >= 0 && barIndex < SwgCuiStatusGround::HAMBarPageCount)
	{
		SwgCuiMfdStatusBar * const bar = m_bars[barIndex];

		if(bar)
		{
			Unicode::String tooltip = s_attributeNames[barIndex].localize();

			char buf[64];
			snprintf(buf, sizeof(buf) - 1, ": %d/%d", current, normalMax);

			tooltip += Unicode::narrowToWide(buf);
			tooltip.push_back('\n');
			tooltip += s_attributeDescriptions[barIndex].localize();

			bar->getPage().SetLocalTooltip(tooltip);
		}
	}
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::onNewTarget()
{
	Object const * const object = m_objectId.getObject();
	ClientObject const * const clientObject = object ? object->asClientObject() : NULL;
	CreatureObject const * const creatureObject = clientObject ? clientObject->asCreatureObject() : NULL;
	CreatureObject const * const playerCreature = Game::getPlayerCreature();

	m_isVehicle = clientObject && creatureObject &&(clientObject->asShipObject() || GameObjectTypes::isTypeOf(clientObject->getGameObjectType(), SharedObjectTemplate::GOT_vehicle));
	m_isPlayerControlled = playerCreature && creatureObject && (creatureObject->getMasterId() == playerCreature->getNetworkId() || creatureObject->isBeast());
	m_isPlayerMounted = playerCreature && creatureObject &&(creatureObject->getRiderDriverCreature() == playerCreature);
	m_isThePlayer = playerCreature && creatureObject &&(playerCreature->getNetworkId() == creatureObject->getNetworkId());
	m_isJedi = creatureObject && creatureObject->getPlayerObject() && creatureObject->getPlayerObject()->isJedi();

	// Reset buffs.
	IGNORE_RETURN(pageSetVisible(m_volumeStates, false));
	IGNORE_RETURN(pageSetVisible(m_debuffStates, false));

	if(m_volumeStates != 0)
		SwgCuiBuffUtils::clearBuffIcons(*m_volumeStates);

	if(m_debuffStates != 0)
		SwgCuiBuffUtils::clearBuffIcons(*m_debuffStates);

	hideHamBars();
	hideEliteStatus();
	hideLevelInfo();
	updateRoleIcon();
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setLookAtTarget(bool const isLookAt)
{
	m_isLookAtTarget = isLookAt;
}

//------------------------------------------------------------------------

void SwgCuiStatusGround::setIsHoverTarget(bool const isHover)
{
	m_isHoverTarget = isHover;
}

//----------------------------------------------------------------------

void SwgCuiStatusGround::onSkillsChanged(const CreatureObject::Messages::SkillsChanged::Payload & creature)
{
	if ((creature.getNetworkId() == m_objectId) && (&creature == Game::getConstPlayer()))
	{
		if (Game::getPlayerObject() && !RoleIconManager::doesQualifyForIcon(&creature, Game::getPlayerObject()->getRoleIconChoice(), false))
		{
			RoleIconManager::cycleToNextQualifyingIcon(true);
			updateRoleIcon();
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiStatusGround::updateEliteStatus(const CreatureObject & creature)
{

	if(m_eliteSkin)
	{
		CreatureObject::Difficulty const difficulty = creature.getDifficulty();
		
		bool isSecondary = m_statusType == ST_lookAtTarget;
		switch(difficulty)
		{
		case CreatureObject::D_boss:
			{
				m_eliteSkin->SetVisible(true);
				m_eliteSkin->SetColor(isSecondary ? s_bossLookAtSkinColor : s_bossSkinColor);
			}
			break;
		case CreatureObject::D_elite:
			{
				m_eliteSkin->SetVisible(true);
				m_eliteSkin->SetColor(isSecondary ? s_eliteLookAtSkinColor : s_eliteSkinColor);
			}
			break;
		case CreatureObject::D_normal:
			{
				m_eliteSkin->SetVisible(false);
			}
			break;
		}
	}
	if(m_eliteLeftPage && m_eliteRightPage && m_hamBarParent && m_hamBarParent->IsVisible())
	{
		CreatureObject::Difficulty const difficulty = creature.getDifficulty();

		switch(difficulty)
		{
		case CreatureObject::D_boss:
			{
				m_eliteLeftPage->SetVisible(true);
				m_eliteRightPage->SetVisible(true);
				m_eliteLeftPage->SetStyle(m_conStyles[ES_doubleEliteLeft]);
				m_eliteRightPage->SetStyle(m_conStyles[ES_doubleEliteRight]);
			}
			break;
		case CreatureObject::D_elite:
			{
				m_eliteLeftPage->SetVisible(true);
				m_eliteRightPage->SetVisible(true);
				m_eliteLeftPage->SetStyle(m_conStyles[ES_eliteLeft]);
				m_eliteRightPage->SetStyle(m_conStyles[ES_eliteRight]);
			}
			break;
		default:
			{
				m_eliteLeftPage->SetVisible(false);
				m_eliteRightPage->SetVisible(false);
			}
			break;
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiStatusGround::hideEliteStatus()
{
	if(m_eliteLeftPage)
		IGNORE_RETURN(pageSetVisible(m_eliteLeftPage, false));
	if(m_eliteRightPage)
		IGNORE_RETURN(pageSetVisible(m_eliteRightPage, false));
	if(m_eliteSkin)
		m_eliteSkin->SetVisible(false);
}

//----------------------------------------------------------------------

UIScalar SwgCuiStatusGround::getRenderHeight() const
{
	UIScalar height = getPage().GetHeight();
	
	if (m_compositePage) 
	{
		UIBaseObject::UIObjectList children;
		m_compositePage->GetChildren(children);
		
		// Stretch a rect around the children.
		int pageCount = 0;
		for (UIBaseObject::UIObjectList::iterator itChild = children.begin(); itChild != children.end(); ++itChild)
		{
			UIBaseObject * const object = *itChild;
			UIWidget * const widget = UI_ASOBJECT(UIWidget, object);
			
			if (widget && widget->WillDraw())
			{
				pageCount++;
			}
		}

		height = m_textName->GetHeight() * pageCount;
	}
	
	return height;
}

//----------------------------------------------------------------------

bool SwgCuiStatusGround::isStatusBarVisibleAndOverlapping(SwgCuiStatusGround const * const otherStatus) const
{
	if (m_hamBarParent && getPage().IsVisible() && otherStatus && otherStatus->m_hamBarParent && otherStatus->getPage().IsVisible())
		return UIUtils::RectanglesIntersect(m_hamBarParent->GetWorldRect(), otherStatus->m_hamBarParent->GetWorldRect());

	return false;
}

//------------------------------------------------------------------------
void SwgCuiStatusGround::onBuffIconSettingsChangedCallback ()
{
	if (m_volumeStates)
	{
		SwgCuiBuffUtils::setBuffDurationWhirlygigOpacity (*m_volumeStates, CuiPreferences::getBuffIconWhirlygigOpacity ());
	}

	if (m_debuffStates)
	{
		SwgCuiBuffUtils::setBuffDurationWhirlygigOpacity (*m_debuffStates, CuiPreferences::getBuffIconWhirlygigOpacity ());
	}

	int buffIconDim = CuiPreferences::getBuffIconSize (getBuffIconType ());
	if (buffIconDim > 0)
	{
		UISize buffIconSize = UISize (buffIconDim, buffIconDim);
		if(m_volumeStates)
		{
			m_volumeStates->SetCellSize (buffIconSize);
		}
		if(m_debuffStates)
		{
			m_debuffStates->SetCellSize (buffIconSize);
		}
	}
}

//========================================================================
