// ======================================================================
//
// SwgCuiCharacterSheet.cpp
// copyright(c) 2001 Sony Online Entertainment
//
// Publish 14 Character Sheet compatibility mediator.
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiCharacterSheet.h"

#include "UIButton.h"
#include "UIData.h"
#include "UIImage.h"
#include "UIPage.h"
#include "UITabbedPane.h"
#include "UITable.h"
#include "UITableModelDefault.h"
#include "UIText.h"
#include "UIUtils.h"
#include "UIWidget.h"

#include "clientGame/ClientCommandQueue.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/GameNetwork.h"
#include "clientGame/GuildObject.h"
#include "clientGame/PlayerObject.h"
#include "clientGame/Species.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiStringIdsCharacterSheet.h"
#include "clientUserInterface/CuiUtils.h"
#include "sharedFoundation/Watcher.h"
#include "sharedGame/CollectionsDataTable.h"
#include "sharedGame/PvpData.h"
#include "sharedGame/SharedCreatureObjectTemplate.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetworkMessages/CharacterSheetResponseMessage.h"
#include "sharedNetworkMessages/FactionRequestMessage.h"
#include "sharedNetworkMessages/FactionResponseMessage.h"
#include "sharedNetworkMessages/GenericValueTypeMessage.h"
#include "sharedNetworkMessages/GuildRequestMessage.h"
#include "sharedNetworkMessages/GuildResponseMessage.h"
#include "swgSharedUtility/Attributes.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"

//-----------------------------------------------------------------------

namespace SwgCuiCharacterSheetNamespace
{
	Unicode::String const s_guildTagPrefix(Unicode::narrowToWide(" <"));
	Unicode::String const s_guildTagSuffix(Unicode::narrowToWide(">"));
	Unicode::String const s_guildMemberTitlePrefix(Unicode::narrowToWide(" ["));
	Unicode::String const s_guildMemberTitleSuffix(Unicode::narrowToWide("]"));
	Unicode::String const s_unavailable(Unicode::narrowToWide("--"));

	float const s_updateInterval = 0.5f;
	int const s_publish14AttributeScale = 1300;

	enum TabPages
	{
		TAB_status,
		TAB_personal,
		TAB_factions,
		TAB_numTabPages
	};

	// The dedicated Publish 14 protocol exposes the original nine attributes in
	// retail order.
	int const s_attributeForRow[9] =
	{
		Attributes::Health,
		Attributes::Strength,
		Attributes::Constitution,
		Attributes::Action,
		Attributes::Quickness,
		Attributes::Stamina,
		Attributes::Mind,
		Attributes::Focus,
		Attributes::Willpower
	};

	int clampValue(int value, int minimum, int maximum)
	{
		if (value < minimum)
			return minimum;
		if (value > maximum)
			return maximum;
		return value;
	}

	Unicode::String formatInteger(int value)
	{
		Unicode::String result;
		UIUtils::FormatLong(result, value);
		return result;
	}

	void setScaledWidth(UIWidget * widget, int value)
	{
		if (!widget)
			return;

		UIWidget * const parent = dynamic_cast<UIWidget *>(widget->GetParent());
		if (!parent)
		{
			widget->SetWidth(0);
			return;
		}

		int const scaledValue = clampValue(value, 0, s_publish14AttributeScale);
		widget->SetWidth(parent->GetWidth() * scaledValue / s_publish14AttributeScale);
	}

	void setImageMeter(UIImage * fill, UIImage * back, int value, int maximum)
	{
		if (!fill || !back)
			return;

		if (maximum <= 0)
		{
			fill->SetWidth(0);
			return;
		}

		int const boundedValue = clampValue(value, 0, maximum);
		fill->SetWidth(back->GetWidth() * boundedValue / maximum);
	}

	Unicode::String formatLocation(Vector const & location, std::string const & planet)
	{
		if (planet.empty())
			return CuiStringIdsCharacterSheet::unknown.localize();

		Unicode::String result;
		CuiUtils::FormatVector(result, location);
		result += Unicode::narrowToWide(", ");
		result += StringId("planet_n", planet.c_str()).localize();
		return result;
	}

	UIWidget * getForceContainer(UIImage * forcePowerBar)
	{
		UIBaseObject * object = forcePowerBar;
		for (int i = 0; object && i < 3; ++i)
			object = object->GetParent();
		return dynamic_cast<UIWidget *>(object);
	}
}

using namespace SwgCuiCharacterSheetNamespace;

//-----------------------------------------------------------------------

SwgCuiCharacterSheet::SwgCuiCharacterSheet(UIPage & page)
:
CuiMediator("SwgCuiCharacterSheet", page),
UIEventCallback(),
MessageDispatch::Receiver(),
m_tabbedPane(0),
m_lastActiveTab(TAB_status),
m_updateTimer(0.0f),
m_characterName(0),
m_rank(0),
m_pvpStatus(0),
m_attributeTable(0),
m_attributeModel(0),
m_shockWounds(0),
m_foodBar(0),
m_foodBarBack(0),
m_drinkBar(0),
m_drinkBarBack(0),
m_forcePowerBar(0),
m_forcePowerText(0),
m_born(0),
m_species(0),
m_played(0),
m_home(0),
m_married(0),
m_bindLocation(0),
m_bankLocation(0),
m_lotsAvailable(0),
m_guild(0),
m_guildAbbreviation(0),
m_guildTitle(0),
m_badgeWindow(0),
m_bio(0),
m_rebelFaction(0),
m_imperialFaction(0),
m_tableFactions(0),
m_factions(),
m_statMigrationButton(0),
m_callBack(new MessageDispatch::Callback),
m_playerObjectWatcher(new PlayerObjectWatcher),
m_creatureObjectWatcher(new CreatureObjectWatcher)
{
	getCodeDataObject(TUITabbedPane, m_tabbedPane, "tabs");
	getCodeDataObject(TUIText, m_characterName, "textCharacterName");
	getCodeDataObject(TUIText, m_rank, "rank");
	getCodeDataObject(TUIText, m_pvpStatus, "factionPvPStatusText");
	getCodeDataObject(TUIText, m_rebelFaction, "factionRebelText");
	getCodeDataObject(TUIText, m_imperialFaction, "factionImperialText");
	getCodeDataObject(TUITable, m_tableFactions, "tableFactions");

	UIPage * pageAttributes = 0;
	getCodeDataObject(TUIPage, pageAttributes, "pageAttributes");
	UIData const * const attributeCodeData = pageAttributes ? dynamic_cast<UIData const *>(pageAttributes->GetChild("CodeData")) : 0;
	if (pageAttributes && attributeCodeData)
		getCodeDataObject(pageAttributes, attributeCodeData, TUITable, m_attributeTable, "table");
	if (m_attributeTable)
		m_attributeModel = dynamic_cast<UITableModelDefault *>(m_attributeTable->GetTableModel());
	DEBUG_FATAL(!m_attributeTable || !m_attributeModel, ("Publish 14 Character Sheet attribute table contract is incomplete"));

	getCodeDataObject(TUIText, m_shockWounds, "textShockWounds");
	getCodeDataObject(TUIImage, m_foodBar, "imageFoodBar", true);
	getCodeDataObject(TUIImage, m_foodBarBack, "imageFoodBarBack", true);
	getCodeDataObject(TUIImage, m_drinkBar, "imageDrinkBar", true);
	getCodeDataObject(TUIImage, m_drinkBarBack, "imageDrinkBarBack", true);
	getCodeDataObject(TUIImage, m_forcePowerBar, "forcepowerbar", true);
	getCodeDataObject(TUIText, m_forcePowerText, "forcepowertext", true);

	getCodeDataObject(TUIText, m_born, "borndate");
	getCodeDataObject(TUIText, m_species, "species");
	getCodeDataObject(TUIText, m_played, "playedTime");
	getCodeDataObject(TUIText, m_home, "home");
	getCodeDataObject(TUIText, m_married, "married");
	getCodeDataObject(TUIText, m_bindLocation, "bindLocation");
	getCodeDataObject(TUIText, m_bankLocation, "bankLocation");
	getCodeDataObject(TUIText, m_lotsAvailable, "lotsAvailable");
	getCodeDataObject(TUIText, m_guild, "guild");
	getCodeDataObject(TUIText, m_guildAbbreviation, "guildAbbreviation");
	getCodeDataObject(TUIText, m_guildTitle, "title");
	getCodeDataObject(TUIText, m_badgeWindow, "badges");
	getCodeDataObject(TUIText, m_bio, "bio");
	getCodeDataObject(TUIButton, m_statMigrationButton, "buttonStatMigration", true);
	if (m_statMigrationButton)
		registerMediatorObject(*m_statMigrationButton, true);

	m_characterName->Clear();
	m_rank->Clear();
	m_pvpStatus->Clear();
	m_rebelFaction->Clear();
	m_imperialFaction->Clear();
	m_species->Clear();
	m_guild->Clear();
	m_guildAbbreviation->Clear();
	m_guildTitle->Clear();
	m_badgeWindow->Clear();
	m_badgeWindow->SetPreLocalized(true);
	m_bio->Clear();
	m_bio->SetPreLocalized(true);
	m_bio->SetEditable(false);

	clearPrivateFields();

	// The migration action belongs only to the local player's private sheet.
	// setExamineMode() makes it visible after the initial subject is selected.
	if (m_statMigrationButton)
		m_statMigrationButton->SetVisible(false);

	setState(MS_closeable);
	setState(MS_closeDeactivates);
	removeState(MS_iconifiable);

	m_tabbedPane->SetActiveTab(-1);
	m_tabbedPane->SetActiveTab(TAB_status);

	updateFactionTable();
	updateHamTable();

	m_callBack->connect(*this, &SwgCuiCharacterSheet::onBiographyRetrieved, static_cast<PlayerCreatureController::Messages::BiographyRetrieved *>(0));
}

//-----------------------------------------------------------------------

SwgCuiCharacterSheet::~SwgCuiCharacterSheet()
{
	m_callBack->disconnect(*this, &SwgCuiCharacterSheet::onBiographyRetrieved, static_cast<PlayerCreatureController::Messages::BiographyRetrieved *>(0));

	delete m_callBack;
	m_callBack = 0;

	delete m_playerObjectWatcher;
	m_playerObjectWatcher = 0;

	delete m_creatureObjectWatcher;
	m_creatureObjectWatcher = 0;
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::performActivate()
{
	CuiManager::requestPointer(true);

	if (!m_creatureObjectWatcher->getPointer())
		setExamineMode(0);

	connectToMessage(FactionResponseMessage::MessageType);
	connectToMessage(GuildResponseMessage::MessageType);
	connectToMessage(CharacterSheetResponseMessage::cms_name);
	connectToMessage("CharacterSheetResponseResLoc");

	clearPrivateFields();
	updateIdentity();
	updatePvpStatus();
	updateHamTable();
	updateStatusBars();
	updateFactionTable();

	PlayerObject * const playerObject = m_playerObjectWatcher->getPointer();
	if (playerObject)
	{
		playerObject->requestBiography();
		if (playerObject->haveBiography())
			m_bio->SetLocalText(playerObject->getBiography());
		refreshBadgeWindow();
	}

	requestServerInfo();
	m_updateTimer = 0.0f;
	setIsUpdating(true);
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::performDeactivate()
{
	setIsUpdating(false);

	disconnectFromMessage("CharacterSheetResponseResLoc");
	disconnectFromMessage(CharacterSheetResponseMessage::cms_name);
	disconnectFromMessage(GuildResponseMessage::MessageType);
	disconnectFromMessage(FactionResponseMessage::MessageType);

	if (isExaminingSelf())
	{
		long const activeTab = m_tabbedPane->GetActiveTab();
		if (activeTab >= TAB_status && activeTab < TAB_numTabPages)
			m_lastActiveTab = activeTab;
	}

	CuiManager::requestPointer(false);
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::receiveMessage(const MessageDispatch::Emitter &, const MessageDispatch::MessageBase & message)
{
	if (message.isType(FactionResponseMessage::MessageType))
	{
		if (!isExaminingSelf())
			return;

		Archive::ReadIterator ri = NON_NULL(safe_cast<GameNetworkMessage const *>(&message))->getByteStream().begin();
		FactionResponseMessage const msg(ri);

		m_rebelFaction->SetLocalText(formatInteger(msg.getFactionRebel()));
		m_imperialFaction->SetLocalText(formatInteger(msg.getFactionImperial()));

		std::vector<std::string> const & names = msg.getNPCFactionNames();
		std::vector<float> const & values = msg.getNPCFactionValues();
		if (names.size() == values.size())
		{
			m_factions.clear();
			for (size_t i = 0; i < names.size(); ++i)
				m_factions[names[i]] = static_cast<int>(values[i]);
		}

		updateFactionTable();
	}
	else if (message.isType(GuildResponseMessage::MessageType))
	{
		Archive::ReadIterator ri = NON_NULL(safe_cast<GameNetworkMessage const *>(&message))->getByteStream().begin();
		GuildResponseMessage const msg(ri);
		CreatureObject const * const creature = m_creatureObjectWatcher->getPointer();
		if (creature && creature->getNetworkId() == msg.getTargetId())
			updateGuild(msg.getGuildName(), isExaminingSelf() ? msg.getMemberTitle() : std::string());
	}
	else if (message.isType(CharacterSheetResponseMessage::cms_name))
	{
		if (!isExaminingSelf())
			return;

		Archive::ReadIterator ri = NON_NULL(safe_cast<GameNetworkMessage const *>(&message))->getByteStream().begin();
		CharacterSheetResponseMessage const msg(ri);

		m_born->SetLocalText(formatInteger(msg.getBornDate()));
		m_played->SetLocalText(formatInteger(msg.getPlayed()));
		m_bindLocation->SetLocalText(formatLocation(msg.getBindLoc(), msg.getBindPlanet()));
		m_bankLocation->SetLocalText(formatLocation(msg.getBankLoc(), msg.getBankPlanet()));
		m_lotsAvailable->SetLocalText(formatInteger(msg.getLotsUsed()));

		Unicode::String residence;
		if (msg.getResidencePlanet().empty())
			residence = CuiStringIdsCharacterSheet::homeless.localize();
		else
			residence = formatLocation(msg.getResidenceLoc(), msg.getResidencePlanet());

		if (!msg.getCitizensOf().empty())
		{
			residence += Unicode::narrowToWide(" (");
			residence += Unicode::narrowToWide(msg.getCitizensOf());
			residence += Unicode::narrowToWide(")");
		}
		m_home->SetLocalText(residence);

		if (msg.getSpouseName().empty())
			m_married->SetLocalText(CuiStringIdsCharacterSheet::unmarried.localize());
		else
			m_married->SetLocalText(msg.getSpouseName());
	}
	else if (message.isType("CharacterSheetResponseResLoc"))
	{
		if (!isExaminingSelf())
			return;

		Archive::ReadIterator ri = NON_NULL(safe_cast<GameNetworkMessage const *>(&message))->getByteStream().begin();
		GenericValueTypeMessage<std::pair<std::string, std::string> > const msg(ri);
		std::string const & residenceLocation = msg.getValue().first;
		std::string const & citizensOf = msg.getValue().second;

		char sceneId[100];
		int x = 0;
		int y = 0;
		int z = 0;
		if (!residenceLocation.empty() && 4 == ::sscanf(residenceLocation.c_str(), "%99s %d %d %d", sceneId, &x, &y, &z))
		{
			Unicode::String residence = formatLocation(Vector(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)), sceneId);
			if (!citizensOf.empty())
			{
				residence += Unicode::narrowToWide(" (");
				residence += Unicode::narrowToWide(citizensOf);
				residence += Unicode::narrowToWide(")");
			}
			m_home->SetLocalText(residence);
		}
	}
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::setExamineMode(CreatureObject * playerToExamine)
{
	CreatureObject * const creature = playerToExamine ? playerToExamine : Game::getPlayerCreature();
	*m_creatureObjectWatcher = creature;
	*m_playerObjectWatcher = creature ? creature->getPlayerObject() : 0;

	// These values arrive asynchronously and belong to the previous subject
	// until their matching responses are received. Never display them while
	// switching between self and examine modes.
	m_guild->Clear();
	m_guildAbbreviation->Clear();
	m_guildTitle->Clear();
	m_badgeWindow->Clear();
	m_bio->Clear();

	bool const examiningSelf = creature && creature == Game::getPlayerCreature();
	if (m_statMigrationButton)
		m_statMigrationButton->SetVisible(examiningSelf);
	UIButton * const factionsTab = m_tabbedPane->GetTabButton(TAB_factions);
	if (factionsTab)
		factionsTab->SetVisible(examiningSelf);

	if (examiningSelf)
	{
		long const targetTab = (m_lastActiveTab >= TAB_status && m_lastActiveTab < TAB_numTabPages) ? m_lastActiveTab : TAB_status;
		m_tabbedPane->SetActiveTab(targetTab);
	}
	else
	{
		m_tabbedPane->SetActiveTab(TAB_status);
		clearPrivateFields();
		m_factions.clear();
		m_rebelFaction->Clear();
		m_imperialFaction->Clear();
		updateFactionTable();
	}
}

//-----------------------------------------------------------------------

bool SwgCuiCharacterSheet::isExaminingSelf() const
{
	return m_creatureObjectWatcher->getPointer() == Game::getPlayerCreature();
}

//-----------------------------------------------------------------------

CreatureObject * SwgCuiCharacterSheet::getCreatureToExamine() const
{
	return m_creatureObjectWatcher->getPointer();
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::OnButtonPressed(UIWidget * context)
{
	if (context == m_statMigrationButton && isExaminingSelf())
		CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_StatMigration);
}

//-----------------------------------------------------------------------

SwgCuiCharacterSheet* SwgCuiCharacterSheet::createInto(UIPage & parent)
{
	UIPage * const dupe = NON_NULL(UIPage::DuplicateInto(parent, "/PDA.CharacterSheet"));
	return new SwgCuiCharacterSheet(*dupe);
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);

	CreatureObject const * const creature = m_creatureObjectWatcher->getPointer();
	if (!creature)
	{
		closeNextFrame();
		return;
	}

	if (creature->getNetworkId() != Game::getPlayerNetworkId() &&
		!PlayerObject::isAdmin() &&
		!creature->getCoverVisibility() &&
		!creature->isPassiveRevealPlayerCharacter(Game::getPlayerNetworkId()))
	{
		closeNextFrame();
		return;
	}

	m_updateTimer += deltaTimeSecs;
	if (m_updateTimer >= s_updateInterval)
	{
		m_updateTimer = 0.0f;
		updatePvpStatus();
		updateHamTable();
		updateStatusBars();
	}
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::requestServerInfo() const
{
	CreatureObject const * const creature = m_creatureObjectWatcher->getPointer();
	if (creature)
		GameNetwork::send(GuildRequestMessage(creature->getNetworkId()), true);

	if (isExaminingSelf())
	{
		FactionRequestMessage const factionMessage;
		GameNetwork::send(factionMessage, true);
		IGNORE_RETURN(ClientCommandQueue::enqueueCommand("requestCharacterSheetInfo", NetworkId::cms_invalid, Unicode::emptyString));
	}
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::updateIdentity()
{
	CreatureObject const * const creature = m_creatureObjectWatcher->getPointer();
	PlayerObject const * const playerObject = m_playerObjectWatcher->getPointer();
	if (!creature)
	{
		m_characterName->Clear();
		m_rank->Clear();
		m_species->Clear();
		return;
	}

	m_characterName->SetLocalText(creature->getLocalizedName());

	ObjectTemplate const * const objectTemplate = creature->getObjectTemplate();
	SharedCreatureObjectTemplate const * const creatureTemplate = dynamic_cast<SharedCreatureObjectTemplate const *>(objectTemplate);
	if (creatureTemplate)
		m_species->SetLocalText(Species::getLocalizedName(creatureTemplate->getSpecies()));
	else
		m_species->Clear();

	if (playerObject)
		m_rank->SetLocalText(CreatureObject::getLocalizedGcwRankString(playerObject->getCurrentGcwRank(), creature->getPvpFaction()));
	else
		m_rank->Clear();

	if (isExaminingSelf() && playerObject)
	{
		m_born->SetLocalText(formatInteger(playerObject->getBornDate()));
		m_played->SetLocalText(formatInteger(playerObject->getPlayedTime()));
	}
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::updatePvpStatus()
{
	CreatureObject const * const creature = m_creatureObjectWatcher->getPointer();
	if (!creature)
	{
		m_pvpStatus->Clear();
		return;
	}

	uint32 const faction = creature->getPvpFaction();
	int const pvpType = creature->getPvpType();
	if (PvpData::isRebelFactionId(faction))
	{
		switch (pvpType)
		{
		case 0: m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_rebelonleave.localize()); break;
		case 1: m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_rebelcovert.localize()); break;
		case 2: m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_rebeldeclared.localize()); break;
		default: m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_neutral.localize()); break;
		}
	}
	else if (PvpData::isImperialFactionId(faction))
	{
		switch (pvpType)
		{
		case 0: m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_imperialonleave.localize()); break;
		case 1: m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_imperialcovert.localize()); break;
		case 2: m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_imperialdeclared.localize()); break;
		default: m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_neutral.localize()); break;
		}
	}
	else
	{
		m_pvpStatus->SetLocalText(CuiStringIdsCharacterSheet::faction_neutral.localize());
	}
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::updateHamTable()
{
	CreatureObject const * const creature = m_creatureObjectWatcher->getPointer();
	if (!m_attributeModel)
		return;

	for (int row = 0; row < 9; ++row)
	{
		int const attribute = s_attributeForRow[row];
		UIWidget * displayWidget = 0;
		IGNORE_RETURN(m_attributeModel->GetValueAtWidget(row, 2, displayWidget));

		if (!creature || attribute < 0 || attribute >= Attributes::NumberOfAttributes)
		{
			IGNORE_RETURN(m_attributeModel->SetValueAtText(row, 1, s_unavailable));
			IGNORE_RETURN(m_attributeModel->SetValueAtInteger(row, 3, 0));
			IGNORE_RETURN(m_attributeModel->SetValueAtInteger(row, 4, 0));
			IGNORE_RETURN(m_attributeModel->SetValueAtInteger(row, 5, 0));
			if (displayWidget)
			{
				setScaledWidth(dynamic_cast<UIWidget *>(displayWidget->GetChild("maxvalue")), 0);
				setScaledWidth(dynamic_cast<UIWidget *>(displayWidget->GetChild("currentmaxvalue")), 0);
				setScaledWidth(dynamic_cast<UIWidget *>(displayWidget->GetChild("value")), 0);
			}
			continue;
		}

		Attributes::Enumerator const enumerator = static_cast<Attributes::Enumerator>(attribute);
		int const value = creature->getAttribute(enumerator);
		int const maximum = creature->getMaxAttribute(enumerator);
		int const currentMaximum = creature->getCurrentMaxAttribute(enumerator);
		int const unmodifiedMaximum = creature->getUnmodifiedMaxAttribute(enumerator);
		int const wounds = maximum > currentMaximum ? maximum - currentMaximum : 0;
		int const buff = currentMaximum - unmodifiedMaximum;

		IGNORE_RETURN(m_attributeModel->SetValueAtText(row, 1, formatInteger(value)));
		IGNORE_RETURN(m_attributeModel->SetValueAtInteger(row, 3, wounds));
		IGNORE_RETURN(m_attributeModel->SetValueAtInteger(row, 4, buff));
		IGNORE_RETURN(m_attributeModel->SetValueAtInteger(row, 5, 0));

		if (displayWidget)
		{
			setScaledWidth(dynamic_cast<UIWidget *>(displayWidget->GetChild("maxvalue")), maximum);
			setScaledWidth(dynamic_cast<UIWidget *>(displayWidget->GetChild("currentmaxvalue")), currentMaximum);
			setScaledWidth(dynamic_cast<UIWidget *>(displayWidget->GetChild("value")), value);
		}
	}

	if (creature)
		m_shockWounds->SetLocalText(formatInteger(creature->getShockWounds()));
	else
		m_shockWounds->SetLocalText(formatInteger(0));

	m_attributeModel->fireDataChanged();
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::updateStatusBars()
{
	// Stomach and Force values are local-player status, not examine data. The
	// original mediator reads the local player and does not expose them for a
	// remote subject.
	PlayerObject const * const playerObject = isExaminingSelf() ? m_playerObjectWatcher->getPointer() : 0;
	if (!playerObject)
	{
		setImageMeter(m_foodBar, m_foodBarBack, 0, 1);
		setImageMeter(m_drinkBar, m_drinkBarBack, 0, 1);
		if (m_forcePowerBar)
			m_forcePowerBar->SetWidth(0);
		if (m_forcePowerText)
			m_forcePowerText->Clear();
		UIWidget * const forceContainer = getForceContainer(m_forcePowerBar);
		if (forceContainer)
			forceContainer->SetVisible(false);
		return;
	}

	setImageMeter(m_foodBar, m_foodBarBack, playerObject->getFood(), playerObject->getMaxFood());
	setImageMeter(m_drinkBar, m_drinkBarBack, playerObject->getDrink(), playerObject->getMaxDrink());

	int const maximumForce = playerObject->getMaxForcePower();
	int const force = playerObject->getForcePower();
	UIWidget * const forceContainer = getForceContainer(m_forcePowerBar);
	if (forceContainer)
		forceContainer->SetVisible(maximumForce > 0);

	if (m_forcePowerBar)
	{
		UIWidget * const parent = dynamic_cast<UIWidget *>(m_forcePowerBar->GetParent());
		m_forcePowerBar->SetWidth(parent && maximumForce > 0 ? parent->GetWidth() * clampValue(force, 0, maximumForce) / maximumForce : 0);
	}

	if (m_forcePowerText)
	{
		if (maximumForce > 0)
		{
			Unicode::String text = formatInteger(force);
			text += Unicode::narrowToWide(" / ");
			text += formatInteger(maximumForce);
			m_forcePowerText->SetLocalText(text);
		}
		else
		{
			m_forcePowerText->Clear();
		}
	}
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::updateGuild(std::string const & guildName, std::string const & memberTitle)
{
	CreatureObject const * const creature = m_creatureObjectWatcher->getPointer();
	Unicode::String abbreviation;
	if (creature && creature->getGuildId() != 0)
		abbreviation = GuildObject::getGuildAbbrevUnicode(creature->getGuildId());

	m_guild->SetLocalText(Unicode::narrowToWide(guildName));
	m_guildAbbreviation->SetLocalText(abbreviation);
	m_guildTitle->SetLocalText(Unicode::narrowToWide(memberTitle));

	if (!creature)
		return;

	Unicode::String caption = creature->getLocalizedName();
	if (!abbreviation.empty())
	{
		caption += s_guildTagPrefix;
		caption += abbreviation;
		caption += s_guildTagSuffix;
	}
	if (!memberTitle.empty())
	{
		caption += s_guildMemberTitlePrefix;
		caption += Unicode::narrowToWide(memberTitle);
		caption += s_guildMemberTitleSuffix;
	}
	m_characterName->SetLocalText(caption);
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::updateFactionTable()
{
	if (!m_tableFactions)
		return;

	UITableModelDefault * const model = dynamic_cast<UITableModelDefault *>(m_tableFactions->GetTableModel());
	if (!model)
		return;

	model->Attach(0);
	m_tableFactions->SetTableModel(0);
	model->ClearData();

	for (std::map<std::string, int>::const_iterator i = m_factions.begin(); i != m_factions.end(); ++i)
	{
		std::string const factionName = Unicode::toLower(i->first);
		Unicode::String localizedName;
		StringId("faction/faction_names", factionName).localize(localizedName);
		IGNORE_RETURN(model->AppendCell(0, factionName.c_str(), localizedName));
		IGNORE_RETURN(model->AppendCell(1, 0, formatInteger(i->second)));
	}

	m_tableFactions->SetTableModel(model);
	model->Detach(0);
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::refreshBadgeWindow()
{
	m_badgeWindow->Clear();

	PlayerObject const * const playerObject = m_playerObjectWatcher->getPointer();
	if (!playerObject)
		return;

	std::vector<CollectionsDataTable::CollectionInfoSlot const *> earnedBadges;
	IGNORE_RETURN(playerObject->getCompletedCollectionSlotCountInBook("badge_book", &earnedBadges));

	Unicode::String current = CuiStringIdsCharacterSheet::badges_earned.localize();
	current.push_back('\n');
	m_badgeWindow->SetLocalText(current);

	if (earnedBadges.empty())
	{
		current += Unicode::narrowToWide("\\>025");
		current += CuiStringIdsCharacterSheet::badges_none.localize();
		current += Unicode::narrowToWide("\\>000\n");
		m_badgeWindow->SetLocalText(current);
	}
	else
	{
		for (std::vector<CollectionsDataTable::CollectionInfoSlot const *>::const_iterator i = earnedBadges.begin(); i != earnedBadges.end(); ++i)
		{
			current += Unicode::narrowToWide("\\#pcontrast1 x\\>025");
			current += CollectionsDataTable::localizeCollectionName((*i)->name);
			current += Unicode::narrowToWide("\\>000\\#.\n");
		}
		m_badgeWindow->SetLocalText(current);
	}

	if (!isExaminingSelf())
		return;

	current += CuiStringIdsCharacterSheet::badges_unearned.localize();
	current.push_back('\n');
	std::vector<CollectionsDataTable::CollectionInfoSlot const *> const & allBadges = CollectionsDataTable::getSlotsInBook("badge_book");
	if (allBadges.size() == earnedBadges.size())
	{
		current += Unicode::narrowToWide("\\>025");
		current += CuiStringIdsCharacterSheet::badges_all.localize();
		current += Unicode::narrowToWide("\\>000\n");
	}
	else
	{
		for (std::vector<CollectionsDataTable::CollectionInfoSlot const *>::const_iterator i = allBadges.begin(); i != allBadges.end(); ++i)
		{
			if (!playerObject->hasCompletedCollectionSlot(**i) && (*i)->showIfNotYetEarned != CollectionsDataTable::SE_none)
			{
				current += Unicode::narrowToWide("\\>025");
				current += CollectionsDataTable::localizeCollectionDescription((*i)->name);
				current += Unicode::narrowToWide("\\>000\n");
			}
		}
	}
	m_badgeWindow->SetLocalText(current);
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::clearPrivateFields()
{
	m_born->Clear();
	m_played->Clear();
	m_home->SetLocalText(CuiStringIdsCharacterSheet::homeless.localize());
	m_married->SetLocalText(CuiStringIdsCharacterSheet::unmarried.localize());
	m_bindLocation->SetLocalText(CuiStringIdsCharacterSheet::unknown.localize());
	m_bankLocation->SetLocalText(CuiStringIdsCharacterSheet::unknown.localize());
	m_lotsAvailable->SetLocalText(CuiStringIdsCharacterSheet::unknown.localize());
}

//-----------------------------------------------------------------------

void SwgCuiCharacterSheet::onBiographyRetrieved(PlayerCreatureController::Messages::BiographyRetrieved::BiographyOwner const & biographyOwner)
{
	CreatureObject const * const creature = m_creatureObjectWatcher->getPointer();
	PlayerObject const * const playerObject = m_playerObjectWatcher->getPointer();
	if (creature && playerObject && biographyOwner.first == creature->getNetworkId() && biographyOwner.second == playerObject)
		m_bio->SetLocalText(playerObject->getBiography());
}

// ======================================================================
