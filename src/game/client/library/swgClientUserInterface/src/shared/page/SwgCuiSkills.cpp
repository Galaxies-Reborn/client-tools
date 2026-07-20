// ======================================================================
//
// SwgCuiSkills.cpp
// Pre-CU skill window mediator. See SwgCuiSkills.h.
//
// V4 rewrite: V1-V3 widget bindings were all silently failing because
// the actual UI tree uses different types and paths than I'd assumed
// (profession list is a TreeView with a UIDataSourceContainer; tables
// are nested under a Composite with column DataSources). This pass
// binds via correct paths and populates the existing widget data
// sources directly. REPORT_LOG every binding to make further debugging
// possible from the client log.
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiSkills.h"

#include "UIButton.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIDataSourceContainer.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UITabbedPane.h"
#include "UIText.h"
#include "UITreeView.h"

#include "clientGame/ClientCommandQueue.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/DraftSchematicInfo.h"
#include "clientGame/DraftSchematicManager.h"
#include "clientGame/Game.h"
#include "clientGame/PlayerObject.h"
#include "clientUserInterface/CuiDeleteSkillConfirmation.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiMediatorTypes.h"
#include "clientUserInterface/CuiMessageBox.h"
#include "clientUserInterface/CuiSkillManager.h"
#include "clientUserInterface/CuiStringIdsSkill.h"
#include "clientUserInterface/CuiStringVariablesData.h"
#include "clientUserInterface/CuiStringVariablesManager.h"
#include "sharedDebug/Report.h"
#include "sharedFoundation/Crc.h"
#include "sharedFoundation/FormattedString.h"
#include "sharedGame/Command.h"
#include "sharedGame/CommandTable.h"
#include "sharedGame/DraftSchematicGroupManager.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedSkillSystem/SkillManager.h"
#include "sharedSkillSystem/SkillObject.h"

#include "SwgCuiSkillsData.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
	// Find a ProfessionDef by its novice skill name. Returns 0 if the
	// passed skill name isn't a canonical Pre-CU profession root.
	ProfessionDef const * findProfessionDef(std::string const & noviceName)
	{
		for (int i = 0; i < k_professionDefCount; ++i)
		{
			if (noviceName == k_professionDefs[i].noviceSkill)
				return &k_professionDefs[i];
		}
		return 0;
	}

	int calculateAvailableSkillPoints(int usedSkillPoints)
	{
		return std::max(0, std::min(k_skillPointCap, k_skillPointCap - usedSkillPoints));
	}

	UIScalar calculateProportionalWidth(UIScalar fullWidth, int value, int maximum)
	{
		UIScalar const boundedFullWidth = std::max<UIScalar>(0, fullWidth);
		if (boundedFullWidth == 0 || maximum <= 0)
			return 0;

		int const boundedValue = std::max(0, std::min(maximum, value));
		UIScalar const width = static_cast<UIScalar>(
			(boundedFullWidth * boundedValue) / maximum);
		return std::max<UIScalar>(0, std::min(boundedFullWidth, width));
	}

	void setHorizontalBarRange(UIPage * page, UIScalar start, UIScalar width, UIScalar fullWidth)
	{
		if (!page)
			return;

		UIScalar const boundedFullWidth = std::max<UIScalar>(0, fullWidth);
		UIScalar const boundedStart = std::max<UIScalar>(0, std::min(boundedFullWidth, start));
		UIScalar const requestedEnd = start + std::max<UIScalar>(0, width);
		UIScalar const boundedEnd = std::max<UIScalar>(0, std::min(boundedFullWidth, requestedEnd));
		UIScalar const boundedWidth = std::max<UIScalar>(0, boundedEnd - boundedStart);

		UIPoint location = page->GetLocation();
		location.x = boundedStart;
		page->SetLocation(location);
		page->SetWidth(boundedWidth);
	}

	bool isCanonicalProfession(std::string const & name)
	{
		return findProfessionDef(name) != 0;
	}

	// Walk SkillObject parent chain (via getPrevSkill) until we hit a
	// canonical pre-CU profession root, or run out. Returns 0 if the
	// player's skill isn't part of any pre-CU profession tree.
	SkillObject const * walkToCanonicalProfession(SkillObject const * skill)
	{
		for (int hops = 0; skill && hops < 16; ++hops)
		{
			if (isCanonicalProfession(skill->getSkillName()))
				return skill;
			skill = skill->getPrevSkill();
		}
		return 0;
	}

	// Strip the "_novice" suffix to recover the canonical profession key.
	std::string stripNoviceSuffix(std::string const & skillName)
	{
		std::string::size_type const tail = skillName.rfind("_novice");
		if (tail != std::string::npos && tail + 7 == skillName.size())
			return skillName.substr(0, tail);
		return skillName;
	}

	Unicode::String localizeProfessionDisplay(std::string const & noviceSkillName)
	{
		// First preference: the calculator's canonical display name
		// (e.g. "Brawler", "Teras Kasi Artist").
		ProfessionDef const * const def = findProfessionDef(noviceSkillName);
		if (def && def->displayName && def->displayName[0])
			return Unicode::narrowToWide(def->displayName);

		// Fallbacks: try the profession-family key in skl_t.stf / skl_n.stf,
		// then the novice name, then raw.
		std::string const familyKey = stripNoviceSuffix(noviceSkillName);
		Unicode::String out;
		if (CuiSkillManager::localizeSkillTitle(familyKey, out) && !out.empty())
			return out;
		if (CuiSkillManager::localizeSkillName(familyKey, out) && !out.empty())
			return out;
		if (CuiSkillManager::localizeSkillName(noviceSkillName, out) && !out.empty())
			return out;
		return Unicode::narrowToWide(familyKey);
	}

	void appendTableRow(UIDataSource * nameDs, UIDataSource * pointsDs,
	                    Unicode::String const & nameText, Unicode::String const & pointsText,
	                    int rowIndex)
	{
		if (!nameDs || !pointsDs)
			return;

		char rowKey[16];
		snprintf(rowKey, sizeof(rowKey), "row%d", rowIndex);

		// Table cells render from the "Value" property -- UITableModelDefault::
		// GetValueAtText reads LocalValue||Value, NOT Text/LocalText. Setting
		// Text/LocalText (as before) left every XP / skill-mod row blank.
		UIData * const nameRow = new UIData;
		nameRow->SetName(rowKey);
		nameRow->SetProperty(UILowerString("Value"), nameText);
		nameDs->AddChild(nameRow);

		UIData * const pointsRow = new UIData;
		pointsRow->SetName(rowKey);
		pointsRow->SetProperty(UILowerString("Value"), pointsText);
		pointsDs->AddChild(pointsRow);
	}

	void appendGrantedDetailRow(UIDataSource * nameDs, UIDataSource * iconDs,
	                            Unicode::String const & nameText,
	                            std::string const & iconPath, int rowIndex)
	{
		if (!nameDs)
			return;

		char rowKey[16];
		snprintf(rowKey, sizeof(rowKey), "row%d", rowIndex);

		UIData * const nameRow = new UIData;
		nameRow->SetName(rowKey);
		nameRow->SetProperty(UILowerString("Value"), nameText);
		nameDs->AddChild(nameRow);

		if (iconDs)
		{
			UIData * const iconRow = new UIData;
			iconRow->SetName(rowKey);
			iconRow->SetProperty(UILowerString("Value"), Unicode::narrowToWide(iconPath));
			iconDs->AddChild(iconRow);
		}
	}

	// "private_*" commands and skill mods are internal markers (skill-tree
	// bookkeeping, combat-difficulty flags), not player-facing abilities/stats.
	// Retail hides them from the granted lists; so do we.
	bool isPrivateName(std::string const & name)
	{
		return name.size() >= 8 && name.compare(0, 8, "private_") == 0;
	}

	// Fallback display name when no cmd_n / skill-mod string entry exists:
	// drop a trailing level digit, split camelCase + underscores into words,
	// Title-Case. "suppressionFire1" -> "Suppression Fire";
	// "cert_rifle_dlt20" -> "Cert Rifle Dlt"; "pointBlankArea1" -> "Point Blank Area".
	Unicode::String prettifyKey(std::string s)
	{
		while (!s.empty() && isdigit(static_cast<unsigned char>(s[s.size() - 1])))
			s.erase(s.size() - 1);

		std::string out;
		bool startWord = true;
		for (size_t i = 0; i < s.size(); ++i)
		{
			char const c = s[i];
			if (c == '_')
			{
				if (!out.empty() && out[out.size() - 1] != ' ')
					out += ' ';
				startWord = true;
				continue;
			}
			if (i > 0 && isupper(static_cast<unsigned char>(c)) &&
			    islower(static_cast<unsigned char>(s[i - 1])))
			{
				out += ' ';
				startWord = true;
			}
			if (startWord)
			{
				out += static_cast<char>(toupper(static_cast<unsigned char>(c)));
				startWord = false;
			}
			else
				out += static_cast<char>(tolower(static_cast<unsigned char>(c)));
		}
		return Unicode::narrowToWide(out);
	}

	SkillObject const * findOwnedSkill(CreatureObject const & player, std::string const & skillName)
	{
		CreatureObject::SkillList const & playerSkills = player.getSkills();
		for (CreatureObject::SkillList::const_iterator it = playerSkills.begin(); it != playerSkills.end(); ++it)
		{
			SkillObject const * const skill = *it;
			if (skill && skill->getSkillName() == skillName)
				return skill;
		}
		return 0;
	}

	bool hasAllPrerequisiteSkills(CreatureObject const & player, SkillObject const & skill)
	{
		SkillObject::SkillVector const & prerequisites = skill.getPrerequisiteSkills();
		for (SkillObject::SkillVector::const_iterator it = prerequisites.begin();
			it != prerequisites.end(); ++it)
		{
			SkillObject const * const prerequisite = *it;
			if (!prerequisite || !findOwnedSkill(player, prerequisite->getSkillName()))
				return false;
		}
		return true;
	}

	void findLearnedDependentSkills(CreatureObject const & player, SkillObject const & selectedSkill,
	                                std::vector<SkillObject const *> & dependents)
	{
		dependents.clear();
		CreatureObject::SkillList const & playerSkills = player.getSkills();
		for (CreatureObject::SkillList::const_iterator it = playerSkills.begin(); it != playerSkills.end(); ++it)
		{
			SkillObject const * const candidate = *it;
			// dependsUponSkill() intentionally returns true for the skill itself.
			if (candidate && candidate != &selectedSkill && candidate->dependsUponSkill(selectedSkill))
				dependents.push_back(candidate);
		}
	}

	void showSurrenderDependencies(std::vector<SkillObject const *> const & dependents)
	{
		std::vector<Unicode::String> localizedNames;
		localizedNames.reserve(dependents.size());
		for (std::vector<SkillObject const *>::const_iterator it = dependents.begin(); it != dependents.end(); ++it)
		{
			Unicode::String localized;
			if (!CuiSkillManager::localizeSkillName((*it)->getSkillName(), localized) || localized.empty())
				localized = Unicode::narrowToWide((*it)->getSkillName());
			localizedNames.push_back(localized);
		}
		std::sort(localizedNames.begin(), localizedNames.end());

		Unicode::String message = CuiStringIdsSkill::err_surrender_deps.localize();
		for (std::vector<Unicode::String>::const_iterator it = localizedNames.begin(); it != localizedNames.end(); ++it)
		{
			message += Unicode::narrowToWide("\n- ");
			message += *it;
		}
		IGNORE_RETURN(CuiMessageBox::createInfoBox(message));
	}
}

//-----------------------------------------------------------------------

SwgCuiSkills::SwgCuiSkills(UIPage & page)
:
CuiMediator         ("SwgCuiSkills", page),
UIEventCallback     (),
m_tabs              (0),
m_pageProfessionList(0),
m_pageMyStats       (0),
m_pageProfession    (0),
m_buttonClose       (0),
m_textProfName      (0),
m_textProfessionBody(0),
m_pageGraphs        (0),
m_pageGraph4x4      (0),
m_pageGraph2x4      (0),
m_pageGraph1x4      (0),
m_pageGraphPyramid  (0),
m_dsProfTree        (0),
m_treeProf          (0),
m_dsExpName         (0),
m_dsExpPoints       (0),
m_dsModsName        (0),
m_dsModsPoints      (0),
m_dsCertsName       (0),
m_dsInfoModsName    (0),
m_dsInfoModsPoints  (0),
m_dsInfoCmdsName    (0),
m_dsInfoCmdsIcons   (0),
m_textSkillPoints   (0),
m_textAcquire       (0),
m_textSurrender     (0),
m_textExpRequired   (0),
m_pageLearningCurrent(0),
m_pageLearningCost (0),
m_pageLearningRecover(0),
m_barExp            (0),
m_buttonSurrender   (0),
m_buttonSkills      (),
m_selectedProfession(),
m_selectedSkill     (),
m_confirmationSkill (),
m_confirmationPlayerId(NetworkId::cms_invalid),
m_pendingSurrenderSkill(),
m_pendingSurrenderPlayerId(NetworkId::cms_invalid),
m_surrenderSequenceId(0),
m_callback          (new MessageDispatch::Callback)
{
	getCodeDataObject(TUITabbedPane, m_tabs,               "tabs");
	getCodeDataObject(TUIPage,       m_pageProfessionList, "pageProfessionList");
	getCodeDataObject(TUIPage,       m_pageMyStats,        "pageMyStats");
	getCodeDataObject(TUIPage,       m_pageProfession,     "pageProfession");
	getCodeDataObject(TUIButton,     m_buttonClose,        "buttonclose");

	REPORT_LOG(true, ("SwgCuiSkills: ctor bind: tabs=%p profList=%p myStats=%p profession=%p close=%p\n",
		(void *)m_tabs, (void *)m_pageProfessionList, (void *)m_pageMyStats, (void *)m_pageProfession, (void *)m_buttonClose));

	// Profession TreeView + its DataSourceContainer.
	if (m_pageProfessionList)
	{
		UIBaseObject * const treeObj = m_pageProfessionList->GetObjectFromPath("tree", TUITreeView);
		if (treeObj)
			m_treeProf = static_cast<UITreeView *>(treeObj);

		UIBaseObject * const dsObj = m_pageProfessionList->GetObjectFromPath("data", TUIDataSourceContainer);
		if (dsObj)
			m_dsProfTree = static_cast<UIDataSourceContainer *>(dsObj);

		REPORT_LOG(true, ("SwgCuiSkills:   pageProfList children: tree=%p data=%p\n",
			(void *)m_treeProf, (void *)m_dsProfTree));
	}

	// XP, Skill-Mod, and weapon-certification table column DataSources, nested
	// under the authentic myStats.comp tables.
	if (m_pageMyStats)
	{
		UIBaseObject * const expNameObj   = m_pageMyStats->GetObjectFromPath("comp.TableExp.containerall.name",    TUIDataSource);
		UIBaseObject * const expPointsObj = m_pageMyStats->GetObjectFromPath("comp.TableExp.containerall.points",  TUIDataSource);
		UIBaseObject * const modsNameObj  = m_pageMyStats->GetObjectFromPath("comp.TableMods.containerall.name",   TUIDataSource);
		UIBaseObject * const modsPtsObj   = m_pageMyStats->GetObjectFromPath("comp.TableMods.containerall.points", TUIDataSource);
		UIBaseObject * const certsNameObj = m_pageMyStats->GetObjectFromPath("comp.TableCerts.containerall.name",  TUIDataSource);
		if (expNameObj)
			m_dsExpName = static_cast<UIDataSource *>(expNameObj);
		if (expPointsObj)
			m_dsExpPoints = static_cast<UIDataSource *>(expPointsObj);
		if (modsNameObj)
			m_dsModsName = static_cast<UIDataSource *>(modsNameObj);
		if (modsPtsObj)
			m_dsModsPoints = static_cast<UIDataSource *>(modsPtsObj);
		if (certsNameObj)
			m_dsCertsName = static_cast<UIDataSource *>(certsNameObj);

		REPORT_LOG(true, ("SwgCuiSkills:   myStats columns: expName=%p expPoints=%p modsName=%p modsPoints=%p certsName=%p\n",
			(void *)m_dsExpName, (void *)m_dsExpPoints, (void *)m_dsModsName,
			(void *)m_dsModsPoints, (void *)m_dsCertsName));
	}

	// Right-panel header + V2 text-body fallback + V3 graph containers.
	if (m_pageProfession)
	{
		UIBaseObject * const profNameObj = m_pageProfession->GetObjectFromPath("textProfName",      TUIText);
		UIBaseObject * const profBodyObj = m_pageProfession->GetObjectFromPath("all.textSkillName", TUIText);
		if (profNameObj)
			m_textProfName = static_cast<UIText *>(profNameObj);
		if (profBodyObj)
			m_textProfessionBody = static_cast<UIText *>(profBodyObj);

		// Per-selected-skill detail panels (right-panel bottom row).
		UIBaseObject * const infoModsNameObj   = m_pageProfession->GetObjectFromPath("all.info.mods.containerall.name",     TUIDataSource);
		UIBaseObject * const infoModsPointsObj = m_pageProfession->GetObjectFromPath("all.info.mods.containerall.points",   TUIDataSource);
		UIBaseObject * const infoCmdsNameObj   = m_pageProfession->GetObjectFromPath("all.info.commands.containerall.name", TUIDataSource);
		UIBaseObject * const infoCmdsIconsObj  = m_pageProfession->GetObjectFromPath("all.info.commands.containerall.icons", TUIDataSource);
		if (infoModsNameObj)   m_dsInfoModsName   = static_cast<UIDataSource *>(infoModsNameObj);
		if (infoModsPointsObj) m_dsInfoModsPoints = static_cast<UIDataSource *>(infoModsPointsObj);
		if (infoCmdsNameObj)   m_dsInfoCmdsName   = static_cast<UIDataSource *>(infoCmdsNameObj);
		if (infoCmdsIconsObj)  m_dsInfoCmdsIcons  = static_cast<UIDataSource *>(infoCmdsIconsObj);
		REPORT_LOG(true, ("SwgCuiSkills:   info detail: modsName=%p modsPts=%p cmdsName=%p cmdsIcons=%p\n",
			(void *)m_dsInfoModsName, (void *)m_dsInfoModsPoints,
			(void *)m_dsInfoCmdsName, (void *)m_dsInfoCmdsIcons));

		// The selected-skill presentation is an immediate CodeData contract on
		// both.right in the authentic patch-13 ui_skill.inc. Bind through those
		// names so the mediator follows the retail paths without an asset shim.
		UIData const * const rightCodeData = static_cast<UIData const *>(
			m_pageProfession->GetObjectFromPath("CodeData", TUIData));
		if (rightCodeData)
		{
			getCodeDataObject(m_pageProfession, rightCodeData, TUIText, m_textSkillPoints, "textSkillPoints");
			getCodeDataObject(m_pageProfession, rightCodeData, TUIText, m_textAcquire, "textAcquire");
			getCodeDataObject(m_pageProfession, rightCodeData, TUIText, m_textSurrender, "textSurrender");
			getCodeDataObject(m_pageProfession, rightCodeData, TUIText, m_textExpRequired, "textExpRequired");
			getCodeDataObject(m_pageProfession, rightCodeData, TUIPage, m_pageLearningCurrent, "pageLearningCurrent");
			getCodeDataObject(m_pageProfession, rightCodeData, TUIPage, m_pageLearningCost, "pageLearningCost");
			getCodeDataObject(m_pageProfession, rightCodeData, TUIPage, m_pageLearningRecover, "pageLearningRecover");
			getCodeDataObject(m_pageProfession, rightCodeData, TUIPage, m_barExp, "barExp");
			getCodeDataObject(m_pageProfession, rightCodeData, TUIButton, m_buttonSurrender, "buttonSurrender");
		}
		if (m_buttonSurrender)
			registerMediatorObject(*m_buttonSurrender, true);
		REPORT_LOG(true, ("SwgCuiSkills:   selected skill: textSP=%p acquire=%p surrender=%p expText=%p current=%p cost=%p recover=%p expBar=%p btn=%p\n",
			(void *)m_textSkillPoints, (void *)m_textAcquire, (void *)m_textSurrender,
			(void *)m_textExpRequired, (void *)m_pageLearningCurrent,
			(void *)m_pageLearningCost, (void *)m_pageLearningRecover,
			(void *)m_barExp, (void *)m_buttonSurrender));

		UIBaseObject * const graphsObj    = m_pageProfession->GetObjectFromPath("all.graphs",              TUIPage);
		UIBaseObject * const graph4x4Obj  = m_pageProfession->GetObjectFromPath("all.graphs.graph4x4",     TUIPage);
		UIBaseObject * const graph2x4Obj  = m_pageProfession->GetObjectFromPath("all.graphs.graph2x4",     TUIPage);
		UIBaseObject * const graph1x4Obj  = m_pageProfession->GetObjectFromPath("all.graphs.graph1x4",     TUIPage);
		UIBaseObject * const graphPyrObj  = m_pageProfession->GetObjectFromPath("all.graphs.graphPyramid", TUIPage);
		if (graphsObj)     m_pageGraphs       = static_cast<UIPage *>(graphsObj);
		if (graph4x4Obj)   m_pageGraph4x4     = static_cast<UIPage *>(graph4x4Obj);
		if (graph2x4Obj)   m_pageGraph2x4     = static_cast<UIPage *>(graph2x4Obj);
		if (graph1x4Obj)   m_pageGraph1x4     = static_cast<UIPage *>(graph1x4Obj);
		if (graphPyrObj)   m_pageGraphPyramid = static_cast<UIPage *>(graphPyrObj);

		REPORT_LOG(true, ("SwgCuiSkills:   right-panel: profName=%p profBody=%p graphs=%p g4x4=%p g2x4=%p g1x4=%p gPyr=%p\n",
			(void *)m_textProfName, (void *)m_textProfessionBody,
			(void *)m_pageGraphs, (void *)m_pageGraph4x4, (void *)m_pageGraph2x4,
			(void *)m_pageGraph1x4, (void *)m_pageGraphPyramid));
	}

	if (m_buttonClose)
		registerMediatorObject(*m_buttonClose, true);
	if (m_tabs)
		registerMediatorObject(*m_tabs, true);
	if (m_treeProf)
		registerMediatorObject(*m_treeProf, true);

	setState(MS_closeable);
	setState(MS_closeDeactivates);

	// Confirmation and command completion must be observed even if the skills
	// page is closed while its confirmation dialog or request remains in flight.
	// Live UI refresh subscriptions remain activation-scoped below.
	m_callback->connect(*this, &SwgCuiSkills::onSceneChanged,
		static_cast<Game::Messages::SceneChanged *>(0));
	m_callback->connect(*this, &SwgCuiSkills::onDeleteSkillConfirmation,
		static_cast<CuiDeleteSkillConfirmation::Message::DeleteSkillConfirmation *>(0));
	m_callback->connect(*this, &SwgCuiSkills::onCommandRemoving,
		static_cast<ClientCommandQueue::Messages::Removing *>(0));
}

//-----------------------------------------------------------------------

SwgCuiSkills::~SwgCuiSkills()
{
	m_callback->disconnect(*this, &SwgCuiSkills::onSceneChanged,
		static_cast<Game::Messages::SceneChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiSkills::onDeleteSkillConfirmation,
		static_cast<CuiDeleteSkillConfirmation::Message::DeleteSkillConfirmation *>(0));
	m_callback->disconnect(*this, &SwgCuiSkills::onCommandRemoving,
		static_cast<ClientCommandQueue::Messages::Removing *>(0));
	delete m_callback;
	m_callback = 0;
}

//-----------------------------------------------------------------------

void SwgCuiSkills::performActivate()
{
	CuiMediator::performActivate();
	setIsUpdating(true);

	m_callback->connect(*this, &SwgCuiSkills::onSkillsChanged,
		static_cast<CreatureObject::Messages::SkillsChanged *>(0));
	m_callback->connect(*this, &SwgCuiSkills::onExperienceChanged,
		static_cast<PlayerObject::Messages::ExperienceChanged *>(0));
	m_callback->connect(*this, &SwgCuiSkills::onSkillModsChanged,
		static_cast<CreatureObject::Messages::SkillModsChanged *>(0));
	m_callback->connect(*this, &SwgCuiSkills::onCommandsChanged,
		static_cast<CreatureObject::Messages::CommandsChanged *>(0));

	// A confirmation or successful request may have become stale while the page
	// was inactive or while the active player was replaced.
	CreatureObject const * const player = Game::getPlayerCreature();
	if (!m_confirmationSkill.empty() &&
		(!player || player->getNetworkId() != m_confirmationPlayerId ||
		 !findOwnedSkill(*player, m_confirmationSkill)))
		clearConfirmationSnapshot();
	reconcilePendingSurrender();

	REPORT_LOG(true, ("SwgCuiSkills: performActivate (activeTab=%ld)\n",
		m_tabs ? static_cast<long>(m_tabs->GetActiveTab()) : -1L));

	populateProfessionList();
	populateExperience();
	populateSkillMods();
	populateCertifications();
	populateSelectedProfession();
	updateSkillPointsDisplay();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::performDeactivate()
{
	setIsUpdating(false);
	m_callback->disconnect(*this, &SwgCuiSkills::onSkillsChanged,
		static_cast<CreatureObject::Messages::SkillsChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiSkills::onExperienceChanged,
		static_cast<PlayerObject::Messages::ExperienceChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiSkills::onSkillModsChanged,
		static_cast<CreatureObject::Messages::SkillModsChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiSkills::onCommandsChanged,
		static_cast<CreatureObject::Messages::CommandsChanged *>(0));
	CuiMediator::performDeactivate();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);
	reconcilePendingSurrender();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::clearConfirmationSnapshot()
{
	m_confirmationSkill.clear();
	m_confirmationPlayerId = NetworkId::cms_invalid;
}

//-----------------------------------------------------------------------

void SwgCuiSkills::clearPendingSurrender()
{
	m_pendingSurrenderSkill.clear();
	m_pendingSurrenderPlayerId = NetworkId::cms_invalid;
	m_surrenderSequenceId = 0;
}

//-----------------------------------------------------------------------

void SwgCuiSkills::reconcilePendingSurrender()
{
	if (m_pendingSurrenderSkill.empty())
	{
		m_pendingSurrenderPlayerId = NetworkId::cms_invalid;
		m_surrenderSequenceId = 0;
		return;
	}

	CreatureObject const * const player = Game::getPlayerCreature();
	bool const wrongPlayer = !player ||
		player->getNetworkId() != m_pendingSurrenderPlayerId;
	bool const skillRemoved = player && !wrongPlayer &&
		!findOwnedSkill(*player, m_pendingSurrenderSkill);
	bool const queueEntryLost = m_surrenderSequenceId != 0 &&
		ClientCommandQueue::findEntry(m_surrenderSequenceId) == 0;

	if (wrongPlayer || skillRemoved || queueEntryLost)
	{
		REPORT_LOG(true, ("SwgCuiSkills: cleared stale surrender '%s' (player=%d, removed=%d, queue=%d)\n",
			m_pendingSurrenderSkill.c_str(), wrongPlayer ? 1 : 0,
			skillRemoved ? 1 : 0, queueEntryLost ? 1 : 0));
		clearPendingSurrender();
		if (isActive())
			updateSurrenderButton();
	}
}

//-----------------------------------------------------------------------

void SwgCuiSkills::onSkillsChanged(CreatureObject const & creature)
{
	if (&creature != Game::getPlayerCreature())
		return;

	if (!m_confirmationSkill.empty() &&
		(creature.getNetworkId() != m_confirmationPlayerId ||
		 !findOwnedSkill(creature, m_confirmationSkill)))
		clearConfirmationSnapshot();
	reconcilePendingSurrender();

	populateProfessionList();
	populateSelectedProfession();
	updateSkillPointsDisplay();
	updateSurrenderButton();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::onExperienceChanged(PlayerObject const & player)
{
	if (&player != Game::getConstPlayerObject())
		return;

	populateExperience();
	// XP bars and their tooltips are rendered as part of the profession graph.
	populateSelectedProfession();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::onSkillModsChanged(CreatureObject const & creature)
{
	if (&creature == Game::getPlayerCreature())
		populateSkillMods();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::onCommandsChanged(CreatureObject const & creature)
{
	if (&creature == Game::getPlayerCreature())
		populateCertifications();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::onDeleteSkillConfirmation(std::string const & skillName)
{
	if (skillName.empty() || skillName != m_confirmationSkill)
	{
		REPORT_LOG(true, ("SwgCuiSkills: ignored stale surrender confirmation for '%s'\n",
			skillName.c_str()));
		return;
	}

	// Clear first so a duplicate transceiver emission cannot submit twice.
	NetworkId const confirmationPlayerId = m_confirmationPlayerId;
	clearConfirmationSnapshot();
	if (!m_pendingSurrenderSkill.empty())
		return;

	CreatureObject const * const player = Game::getPlayerCreature();
	SkillObject const * const selectedSkill = player &&
		player->getNetworkId() == confirmationPlayerId ?
		findOwnedSkill(*player, skillName) : 0;
	if (!player || !selectedSkill)
	{
		updateSurrenderButton();
		return;
	}

	// Ownership and dependency state can change while the dialog is open, so
	// repeat both checks at the point of submission. The server remains final
	// authority and performs the same policy checks.
	std::vector<SkillObject const *> dependents;
	findLearnedDependentSkills(*player, *selectedSkill, dependents);
	if (!dependents.empty())
	{
		showSurrenderDependencies(dependents);
		updateSurrenderButton();
		return;
	}

	Command const & surrenderCommand = CommandTable::getCommand(
		Crc::normalizeAndCalculate("surrenderSkill"));
	if (surrenderCommand.isNull() || !surrenderCommand.m_visibleToClients)
	{
		REPORT_LOG(true, ("SwgCuiSkills: surrenderSkill is unavailable as a client-visible command\n"));
		updateSurrenderButton();
		return;
	}

	m_pendingSurrenderSkill = skillName;
	m_pendingSurrenderPlayerId = player->getNetworkId();
	updateSurrenderButton();
	// surrenderSkill is actor-routed and declares targetType=none. Supplying
	// the player as a target makes the authoritative command gate reject an
	// otherwise valid request with CEC_TargetType.
	m_surrenderSequenceId = ClientCommandQueue::enqueueCommand(
		surrenderCommand, NetworkId::cms_invalid, Unicode::narrowToWide(skillName));
	if (m_surrenderSequenceId == 0)
	{
		REPORT_LOG(true, ("SwgCuiSkills: surrenderSkill enqueue failed for '%s'\n", skillName.c_str()));
		clearPendingSurrender();
		updateSurrenderButton();
		return;
	}

	REPORT_LOG(true, ("SwgCuiSkills: surrenderSkill enqueued for '%s' (sequence=%u)\n",
		skillName.c_str(), static_cast<unsigned int>(m_surrenderSequenceId)));
}

//-----------------------------------------------------------------------

void SwgCuiSkills::onSceneChanged(bool const &)
{
	clearConfirmationSnapshot();
	clearPendingSurrender();
	IGNORE_RETURN(CuiMediatorFactory::deactivateInWorkspace(
		CuiMediatorTypes::DeleteSkillConfirmation));
	if (isActive())
		updateSurrenderButton();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::onCommandRemoving(ClientCommandQueue::Messages::Removing::Payload const & payload)
{
	if (m_surrenderSequenceId == 0 || payload.sequenceId != m_surrenderSequenceId)
		return;

	if (payload.status != Command::CEC_Success)
	{
		REPORT_LOG(true, ("SwgCuiSkills: surrenderSkill rejected (status=%d, detail=%d)\n",
			static_cast<int>(payload.status), payload.statusDetail));
	}
	// Native command hooks are void and can report queue success for a policy
	// rejection/no-op, so every matching completion ends the in-flight guard.
	// A successful authoritative SkillsChanged delta refreshes the display.
	clearPendingSurrender();
	if (isActive())
		updateSurrenderButton();
}

//-----------------------------------------------------------------------

bool SwgCuiSkills::OnMessage(UIWidget * context, const UIMessage & msg)
{
	// "To: <profession>" links are UIText, which never delivers OnButtonPressed.
	// Catch the left-click here and jump the tree to that profession.
	if (msg.Type == UIMessage::LeftMouseUp)
	{
		std::map<UIWidget *, std::string>::const_iterator const link = m_linkSkills.find(context);
		if (link != m_linkSkills.end())
		{
			m_selectedProfession = link->second;
			synchronizeProfessionTreeSelection();
			REPORT_LOG(true, ("SwgCuiSkills: link click -> profession='%s'\n", m_selectedProfession.c_str()));
			populateSelectedProfession();
			return false;   // consume
		}
	}
	return true;            // default processing for everything else
}

//-----------------------------------------------------------------------

void SwgCuiSkills::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonClose)
	{
		closeThroughWorkspace();
		return;
	}

	if (context == m_buttonSurrender)
	{
		if (m_selectedSkill.empty() || !m_pendingSurrenderSkill.empty())
		{
			REPORT_LOG(true, ("SwgCuiSkills: surrender ignored (selection='%s', pending='%s')\n",
				m_selectedSkill.c_str(), m_pendingSurrenderSkill.c_str()));
			return;
		}

		CreatureObject const * const player = Game::getPlayerCreature();
		SkillObject const * const selectedSkill = player ? findOwnedSkill(*player, m_selectedSkill) : 0;
		if (!player || !selectedSkill)
		{
			REPORT_LOG(true, ("SwgCuiSkills: surrender ignored; player does not own '%s'\n",
				m_selectedSkill.c_str()));
			updateSurrenderButton();
			return;
		}

		std::vector<SkillObject const *> dependents;
		findLearnedDependentSkills(*player, *selectedSkill, dependents);
		if (!dependents.empty())
		{
			showSurrenderDependencies(dependents);
			return;
		}

		// Snapshot before opening the retained typed confirmation mediator.
		// The callback revalidates this exact skill before enqueueing.
		m_confirmationSkill = m_selectedSkill;
		m_confirmationPlayerId = player->getNetworkId();
		CuiDeleteSkillConfirmation * const confirmation = dynamic_cast<CuiDeleteSkillConfirmation *>(
			CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::DeleteSkillConfirmation));
		if (!confirmation)
		{
			REPORT_LOG(true, ("SwgCuiSkills: unable to activate surrender confirmation\n"));
			clearConfirmationSnapshot();
			return;
		}
		confirmation->setSelectedSkill(m_confirmationSkill);
		return;
	}

	// "To: <profession>" link click: jump the tree to the linked profession.
	std::map<UIWidget *, std::string>::const_iterator const link = m_linkSkills.find(context);
	if (link != m_linkSkills.end())
	{
		m_selectedProfession = link->second;
		synchronizeProfessionTreeSelection();
		REPORT_LOG(true, ("SwgCuiSkills: link click -> profession='%s'\n", m_selectedProfession.c_str()));
		populateSelectedProfession();
		return;
	}

	// Tree-cell button click: look up which skill this button currently
	// represents (populated by tryPopulateGraph4x4) and refresh the
	// per-selected-skill bottom panels.
	std::map<UIWidget *, std::string>::const_iterator const it = m_buttonSkills.find(context);
	if (it != m_buttonSkills.end())
	{
		m_selectedSkill = it->second;
		REPORT_LOG(true, ("SwgCuiSkills: selected skill='%s'\n", m_selectedSkill.c_str()));
		populateSelectedSkill();
	}
}

//-----------------------------------------------------------------------

void SwgCuiSkills::updateSkillPointsDisplay()
{
	if (!m_textSkillPoints)
		return;

	int usedSkillPoints = 0;
	CreatureObject const * const player = Game::getPlayerCreature();
	if (player)
	{
		CreatureObject::SkillList const & playerSkills = player->getSkills();
		for (CreatureObject::SkillList::const_iterator it = playerSkills.begin(); it != playerSkills.end(); ++it)
		{
			if (*it)
				usedSkillPoints += std::max(0, (*it)->getSkillPointsRequired());
		}
	}

	int const availableSkillPoints = calculateAvailableSkillPoints(usedSkillPoints);
	char buf[64];
	snprintf(buf, sizeof(buf), "%d / %d", availableSkillPoints, k_skillPointCap);
	m_textSkillPoints->SetLocalText(Unicode::narrowToWide(buf));
}

//-----------------------------------------------------------------------

void SwgCuiSkills::updateSurrenderButton()
{
	CreatureObject const * const player = Game::getPlayerCreature();
	bool const ownsSelectedSkill = player && !m_selectedSkill.empty() &&
		findOwnedSkill(*player, m_selectedSkill);
	if (m_buttonSurrender)
		m_buttonSurrender->SetEnabled(ownsSelectedSkill && m_pendingSurrenderSkill.empty());

	// The retail button's OnEnable/OnDisable scripts also toggle these labels.
	// Restore selection semantics after changing enabled state so a pending
	// owned-skill request never displays the acquisition prose.
	if (m_textSurrender)
		m_textSurrender->SetVisible(ownsSelectedSkill);
	if (m_textAcquire)
		m_textAcquire->SetVisible(!m_selectedSkill.empty() && !ownsSelectedSkill);
}

//-----------------------------------------------------------------------

void SwgCuiSkills::OnGenericSelectionChanged(UIWidget * context)
{
	if (context == m_treeProf && m_treeProf)
	{
		long const row = m_treeProf->GetLastSelectedRow();
		if (row >= 0)
		{
			UIDataSourceContainer * const data = m_treeProf->GetDataSourceContainerAtRow(row);
			if (data)
			{
				m_selectedProfession = data->GetName();
				REPORT_LOG(true, ("SwgCuiSkills: selected profession='%s'\n", m_selectedProfession.c_str()));
				populateSelectedProfession();
			}
		}
	}
}

//-----------------------------------------------------------------------

void SwgCuiSkills::OnTabbedPaneChanged(UIWidget * context)
{
	REPORT_LOG(true, ("SwgCuiSkills::OnTabbedPaneChanged: context=%p m_tabs=%p activeTab=%ld\n",
		(void *)context, (void *)m_tabs,
		m_tabs ? static_cast<long>(m_tabs->GetActiveTab()) : -1L));
	populateProfessionList();
	populateSelectedProfession();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::populateExperience()
{
	if (m_dsExpName)  m_dsExpName->Clear();
	if (m_dsExpPoints) m_dsExpPoints->Clear();
	if (!m_dsExpName || !m_dsExpPoints)
		return;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
		return;

	CreatureObject::ExperiencePointMap const & xpMap = player->getExperiencePointMap();
	int rowIdx = 0;
	for (CreatureObject::ExperiencePointMap::const_iterator it = xpMap.begin(); it != xpMap.end(); ++it, ++rowIdx)
	{
		std::string const & xpType = it->first;
		int const xpValue = it->second;

		Unicode::String localized;
		if (!CuiSkillManager::localizeExpName(xpType, localized) || localized.empty())
			localized = Unicode::narrowToWide(xpType);

		appendTableRow(m_dsExpName, m_dsExpPoints, localized,
			Unicode::narrowToWide(FormattedString<32>().sprintf("%d", xpValue)), rowIdx);
	}
}

//-----------------------------------------------------------------------

void SwgCuiSkills::populateSkillMods()
{
	if (m_dsModsName)  m_dsModsName->Clear();
	if (m_dsModsPoints) m_dsModsPoints->Clear();
	if (!m_dsModsName || !m_dsModsPoints)
		return;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
		return;

	CreatureObject::SkillModMap const & modMap = player->getSkillModMap();
	int rowIdx = 0;
	for (CreatureObject::SkillModMap::const_iterator it = modMap.begin(); it != modMap.end(); ++it)
	{
		std::string const & modName = it->first;
		int const modValue = it->second.first + it->second.second;
		if (modValue == 0)
			continue;

		Unicode::String localized;
		if (!CuiSkillManager::localizeSkillModName(modName, localized) || localized.empty())
			localized = Unicode::narrowToWide(modName);

		appendTableRow(m_dsModsName, m_dsModsPoints, localized,
			Unicode::narrowToWide(FormattedString<32>().sprintf("%d", modValue)), rowIdx);
		++rowIdx;
	}
}

//-----------------------------------------------------------------------

void SwgCuiSkills::populateCertifications()
{
	// ui_skill.inc intentionally carries four editor sample rows in this data
	// source. Retail clears them and rebuilds the list from the player's granted
	// cert_* commands; clearing before the player check prevents those samples
	// from leaking through during login or scene transitions.
	if (m_dsCertsName)
		m_dsCertsName->Clear();
	if (!m_dsCertsName)
		return;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
		return;

	std::map<std::string, int> const & commands = player->getCommands();
	int rowIdx = 0;
	for (std::map<std::string, int>::const_iterator it = commands.begin();
		it != commands.end(); ++it)
	{
		std::string const & commandName = it->first;
		if (commandName.compare(0, 5, "cert_") != 0)
			continue;

		Unicode::String localizedName;
		if (!CuiSkillManager::localizeCmdName(Unicode::toLower(commandName), localizedName) ||
			localizedName.empty())
		{
			localizedName = prettifyKey(commandName);
		}

		appendGrantedDetailRow(m_dsCertsName, 0, localizedName, std::string(), rowIdx);
		++rowIdx;
	}
}

//-----------------------------------------------------------------------

void SwgCuiSkills::populateProfessionList()
{
	if (!m_dsProfTree)
	{
		REPORT_LOG(true, ("SwgCuiSkills: populateProfessionList skipped: m_dsProfTree is null\n"));
		return;
	}

	m_dsProfTree->Clear();

	// Determine the set of profession-novice skills to display based on
	// the active tab.
	bool const showAll = (m_tabs && m_tabs->GetActiveTab() == 1);

	std::set<std::string> noviceSet;

	if (showAll)
	{
		// "All Professions": iterate the canonical k_professionDefs table
		// (derived from the SWG Profession Calculator). Include each
		// entry whose novice skill exists in SkillManager.
		SkillManager & skillMgr = SkillManager::getInstance();
		for (int i = 0; i < k_professionDefCount; ++i)
		{
			char const * const nov = k_professionDefs[i].noviceSkill;
			if (nov && skillMgr.getSkill(nov))
				noviceSet.insert(nov);
		}
	}
	else
	{
		// "My Character": canonical professions the player has any skill
		// from. Walk parent chain (getPrevSkill) for each granted skill
		// until we find a pre-CU profession root; ignore NGE expertise /
		// chronicler / pilot trees whose roots aren't pre-CU.
		CreatureObject const * const player = Game::getPlayerCreature();
		if (player)
		{
			CreatureObject::SkillList const & playerSkills = player->getSkills();
			for (CreatureObject::SkillList::const_iterator it = playerSkills.begin(); it != playerSkills.end(); ++it)
			{
				SkillObject const * const skill = *it;
				if (!skill)
					continue;
				SkillObject const * const prof = walkToCanonicalProfession(skill);
				if (prof)
					noviceSet.insert(prof->getSkillName());
			}
		}
	}

	REPORT_LOG(true, ("SwgCuiSkills: populating profession list (%s) with %zu entries\n",
		showAll ? "all" : "mine", noviceSet.size()));

	// Sort by localized profession name and add a UIDataSourceContainer
	// child to the tree for each. The TreeView renders each child's Text.
	typedef std::pair<Unicode::String, std::string> NameRow;
	std::vector<NameRow> rows;
	rows.reserve(noviceSet.size());
	for (std::set<std::string>::const_iterator it = noviceSet.begin(); it != noviceSet.end(); ++it)
		rows.push_back(std::make_pair(localizeProfessionDisplay(*it), *it));
	std::sort(rows.begin(), rows.end());

	// Keep the model selection sane before rebuilding the matching visible row.
	if (!m_selectedProfession.empty() && noviceSet.find(m_selectedProfession) == noviceSet.end())
		m_selectedProfession.clear();
	if (m_selectedProfession.empty() && !rows.empty())
		m_selectedProfession = rows.front().second;

	for (std::vector<NameRow>::const_iterator it = rows.begin(); it != rows.end(); ++it)
	{
		UIDataSourceContainer * const child = new UIDataSourceContainer;
		child->SetName(it->second);
		child->SetProperty(UILowerString("Text"),      it->first);
		child->SetProperty(UILowerString("LocalText"), it->first);
		m_dsProfTree->AddChild(child);
	}

	synchronizeProfessionTreeSelection();
}

//-----------------------------------------------------------------------

void SwgCuiSkills::synchronizeProfessionTreeSelection()
{
	if (!m_treeProf)
		return;

	long selectedRow = -1;
	for (long row = 0; row < m_treeProf->GetRowCount(); ++row)
	{
		UIDataSourceContainer const * const data =
			m_treeProf->GetDataSourceContainerAtRow(row);
		if (data && m_selectedProfession == data->GetName())
		{
			selectedRow = row;
			break;
		}
	}

	m_treeProf->SelectRow(selectedRow);
	if (selectedRow >= 0)
		m_treeProf->ScrollToRow(static_cast<int>(selectedRow));
}

//-----------------------------------------------------------------------

void SwgCuiSkills::populateSelectedProfession()
{
	if (!m_pageProfession)
		return;

	hideAllGraphs();

	if (m_selectedProfession.empty())
	{
		m_selectedSkill.clear();
		if (m_textProfName)
			m_textProfName->SetLocalText(Unicode::emptyString);
		populateSelectedSkill();
		return;
	}

	SkillObject const * const profSkill = SkillManager::getInstance().getSkill(m_selectedProfession);
	if (!profSkill)
		return;

	Unicode::String const localizedProfName = localizeProfessionDisplay(m_selectedProfession);
	if (m_textProfName)
		m_textProfName->SetLocalText(localizedProfName);

	CreatureObject const * const player = Game::getPlayerCreature();
	std::set<std::string> playerSkillNames;
	if (player)
	{
		CreatureObject::SkillList const & playerSkills = player->getSkills();
		for (CreatureObject::SkillList::const_iterator it = playerSkills.begin(); it != playerSkills.end(); ++it)
		{
			if (*it)
				playerSkillNames.insert((*it)->getSkillName());
		}
	}

	bool const handledVisually = tryPopulateGraph4x4(profSkill, playerSkillNames);
	if (handledVisually)
	{
		if (m_textProfessionBody)
			m_textProfessionBody->SetLocalText(Unicode::emptyString);
		return;
	}

	// Non-4x4 fallback: text dump of descendants with [X]/[ ] indicators.
	m_selectedSkill.clear();
	populateSelectedSkill();
	std::set<SkillObject const *> visited;
	std::vector<SkillObject const *> stack;
	stack.push_back(profSkill);
	std::vector<std::string> descendants;
	while (!stack.empty())
	{
		SkillObject const * const current = stack.back();
		stack.pop_back();
		if (!current || !visited.insert(current).second)
			continue;
		descendants.push_back(current->getSkillName());
		SkillObject::SkillVector const & next = current->getNextSkillBoxes();
		for (SkillObject::SkillVector::const_iterator it = next.begin(); it != next.end(); ++it)
			stack.push_back(*it);
	}
	std::sort(descendants.begin(), descendants.end());

	Unicode::String body;
	for (std::vector<std::string>::const_iterator it = descendants.begin(); it != descendants.end(); ++it)
	{
		bool const has = playerSkillNames.find(*it) != playerSkillNames.end();
		Unicode::String localized;
		if (!CuiSkillManager::localizeSkillName(*it, localized) || localized.empty())
			localized = Unicode::narrowToWide(*it);
		body += Unicode::narrowToWide(has ? "[X] " : "[ ] ");
		body += localized;
		body += Unicode::narrowToWide("\n");
	}
	if (m_textProfessionBody)
		m_textProfessionBody->SetLocalText(body);
}

//-----------------------------------------------------------------------

void SwgCuiSkills::hideAllGraphs()
{
	resetGraph4x4Presentation();
	if (m_pageGraphs)         m_pageGraphs->SetVisible(false);
	if (m_pageGraph4x4)     m_pageGraph4x4->SetVisible(false);
	if (m_pageGraph2x4)     m_pageGraph2x4->SetVisible(false);
	if (m_pageGraph1x4)     m_pageGraph1x4->SetVisible(false);
	if (m_pageGraphPyramid) m_pageGraphPyramid->SetVisible(false);
}

//-----------------------------------------------------------------------

void SwgCuiSkills::resetGraph4x4Presentation()
{
	m_buttonSkills.clear();
	m_linkSkills.clear();

	if (!m_pageGraph4x4)
		return;

	// Publish 14's ui_skill.inc is an editor-authored template. Every graph
	// button and transition label contains visible sample text, so a character
	// with no profession (or an unrecognized profession) must explicitly erase
	// the complete template before the graph can fail closed.
	char path[64];
	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			snprintf(path, sizeof(path), "graph.row%d.%d", row, col);
			UIBaseObject * const object =
				m_pageGraph4x4->GetObjectFromPath(path, TUIButton);
			if (!object)
				continue;

			UIButton * const button = static_cast<UIButton *>(object);
			button->SetText(Unicode::emptyString);
			button->SetLocalTooltip(Unicode::emptyString);
		}

		for (int slot = 0; slot < 6; ++slot)
		{
			snprintf(path, sizeof(path), "graph.disciplineNext.%d.%d", col, slot);
			UIBaseObject * const object =
				m_pageGraph4x4->GetObjectFromPath(path, TUIText);
			if (!object)
				continue;

			UIText * const text = static_cast<UIText *>(object);
			text->SetLocalText(Unicode::emptyString);
			text->SetVisible(false);
		}
	}

	char const * const terminalButtons[] =
	{
		"graph.master.b",
		"graph.novice.b"
	};
	for (int terminal = 0; terminal < 2; ++terminal)
	{
		UIBaseObject * const object =
			m_pageGraph4x4->GetObjectFromPath(terminalButtons[terminal], TUIButton);
		if (!object)
			continue;

		UIButton * const button = static_cast<UIButton *>(object);
		button->SetText(Unicode::emptyString);
		button->SetLocalTooltip(Unicode::emptyString);
	}

	for (int slot = 0; slot < 4; ++slot)
	{
		char const * const linkGroups[] =
		{
			"graph.next.%d",
			"graph.prev.%d"
		};
		for (int group = 0; group < 2; ++group)
		{
			snprintf(path, sizeof(path), linkGroups[group], slot);
			UIBaseObject * const object =
				m_pageGraph4x4->GetObjectFromPath(path, TUIText);
			if (!object)
				continue;

			UIText * const text = static_cast<UIText *>(object);
			text->SetLocalText(Unicode::emptyString);
			text->SetVisible(false);
		}
	}
}

//-----------------------------------------------------------------------

void SwgCuiSkills::applyTreeBox(char const * path, std::string const & skillName,
                                std::set<std::string> const & playerSkills)
{
	if (!m_pageGraph4x4 || !path || !path[0])
		return;

	// Publish 14 uses a nested .b button for novice/master and bare buttons for
	// branch rows. Resolve both authentic shapes without requiring per-box
	// child widgets from a modified asset.
	std::string const btnPath = std::string(path) + ".b";
	UIBaseObject * obj = m_pageGraph4x4->GetObjectFromPath(btnPath.c_str(), TUIButton);
	if (!obj)
		obj = m_pageGraph4x4->GetObjectFromPath(path, TUIButton);
	if (!obj)
		return;

	UIButton * const btn = static_cast<UIButton *>(obj);

	Unicode::String localized;
	if (!CuiSkillManager::localizeSkillName(skillName, localized) || localized.empty())
		localized = Unicode::narrowToWide(skillName);
	// SetText (NOT SetLocalText): UIButton::RenderText early-returns when mText
	// is empty, and SetLocalText only sets mLocalText -- so the box rendered
	// blank. SetText sets BOTH mText (satisfies the guard) and mLocalText (drawn).
	btn->SetText(localized);

	// Green tree_acquired for skills the player has trained, dark tree_default
	// otherwise (both defined in the ui_skill.inc styles section).
	bool const has = playerSkills.find(skillName) != playerSkills.end();
	btn->SetProperty(UILowerString("Style"), Unicode::narrowToWide(
		has ? "/Styles.New.tree_acquired.style" : "/Styles.New.tree_default.style"));

	// Click identifies which skill the user picked for the detail panels.
	m_buttonSkills[btn] = skillName;
	if (!isRegisteredMediatorObject(*btn))
		registerMediatorObject(*btn, true);
	btn->SetLocalTooltip(localized);
}

//-----------------------------------------------------------------------

bool SwgCuiSkills::tryPopulateGraph4x4(SkillObject const * novice, std::set<std::string> const & playerSkills)
{
	REPORT_LOG(true, ("SwgCuiSkills::tryPopulateGraph4x4: pageGraph4x4=%p novice=%p (%s)\n",
		(void *)m_pageGraph4x4, (void *)novice,
		novice ? novice->getSkillName().c_str() : "(null)"));

	if (!m_pageGraph4x4 || !novice)
		return false;

	// Use the canonical profession definition table extracted from the
	// SWG Profession Calculator. Each ProfessionDef carries the explicit
	// branch -> skill mapping in canonical column order, so we no longer
	// rely on client-side SkillObject graph linkage (which isn't built)
	// or alphabetical-sort name pattern guessing.
	std::string const noviceName = novice->getSkillName();
	ProfessionDef const * const def = findProfessionDef(noviceName);
	if (!def)
	{
		REPORT_LOG(true, ("SwgCuiSkills::tryPopulateGraph4x4: no ProfessionDef for '%s'\n", noviceName.c_str()));
		return false;
	}

	REPORT_LOG(true, ("SwgCuiSkills::tryPopulateGraph4x4: using ProfessionDef for '%s' (master=%s)\n",
		def->displayName, def->masterSkill ? def->masterSkill : "(none)"));

	// hideAllGraphs() reset the complete authentic graph template before this
	// valid definition was selected. Re-register only the live cells and links.
	std::string const masterName = (def->masterSkill && def->masterSkill[0])
		? std::string(def->masterSkill)
		: stripNoviceSuffix(novice->getSkillName()) + "_master";

	// The Publish 14 graph contract supplies six branch-continuation text slots
	// per column. The canonical profession map currently needs at most three,
	// but every slot must be cleared because the authentic asset initializes all
	// six with its editor-only "xxx to: specialist" sample.
	for (int col = 0; col < 4; ++col)
	{
		for (int slot = 0; slot < 6; ++slot)
		{
			char path[64];
			snprintf(path, sizeof(path), "graph.disciplineNext.%d.%d", col, slot);
			UIBaseObject * const linkObj = m_pageGraph4x4->GetObjectFromPath(path, TUIText);
			if (!linkObj)
				continue;
			UIText * const linkText = static_cast<UIText *>(linkObj);
			linkText->SetLocalText(Unicode::emptyString);
			linkText->SetVisible(false);
		}
	}

	// The four graph.next slots describe continuations gated by this profession's
	// master box. Clear the asset samples first, then discover those links from
	// the runtime SkillObject prerequisites used by acquisition authority.
	for (int slot = 0; slot < 4; ++slot)
	{
		char path[64];
		snprintf(path, sizeof(path), "graph.next.%d", slot);
		UIBaseObject * const linkObj = m_pageGraph4x4->GetObjectFromPath(path, TUIText);
		if (linkObj)
		{
			UIText * const linkText = static_cast<UIText *>(linkObj);
			linkText->SetLocalText(Unicode::emptyString);
			linkText->SetVisible(false);
		}
	}

	int nextSlot = 0;
	for (int i = 0; i < k_professionDefCount && nextSlot < 4; ++i)
	{
		ProfessionDef const & candidate = k_professionDefs[i];
		if (!candidate.noviceSkill || !candidate.noviceSkill[0] ||
			novice->getSkillName() == candidate.noviceSkill)
		{
			continue;
		}

		SkillObject const * const candidateNovice =
			SkillManager::getInstance().getSkill(candidate.noviceSkill);
		if (!candidateNovice)
			continue;

		bool requiresThisMaster = false;
		SkillObject::SkillVector const & prerequisites =
			candidateNovice->getPrerequisiteSkills();
		for (SkillObject::SkillVector::const_iterator prerequisite = prerequisites.begin();
			prerequisite != prerequisites.end(); ++prerequisite)
		{
			if (*prerequisite && (*prerequisite)->getSkillName() == masterName)
			{
				requiresThisMaster = true;
				break;
			}
		}
		if (!requiresThisMaster)
			continue;

		char path[64];
		snprintf(path, sizeof(path), "graph.next.%d", nextSlot);
		UIBaseObject * const linkObj = m_pageGraph4x4->GetObjectFromPath(path, TUIText);
		if (linkObj && candidate.displayName && candidate.displayName[0])
		{
			UIText * const linkText = static_cast<UIText *>(linkObj);
			Unicode::String label = Unicode::narrowToWide("To: ");
			label += Unicode::narrowToWide(candidate.displayName);
			linkText->SetLocalText(label);
			linkText->SetVisible(true);
			m_linkSkills[linkText] = candidate.noviceSkill;
			if (!isRegisteredMediatorObject(*linkText))
				registerMediatorObject(*linkText, true);
		}
		++nextSlot;
	}

	// Populate the branch transitions backed by the canonical profession map.
	for (int col = 0; col < 4; ++col)
	{
		for (int slot = 0; slot < 3; ++slot)
		{
			char const * const linkRoot = def->branchLinks[col][slot];
			if (!linkRoot || !linkRoot[0])
				continue;

			char path[64];
			snprintf(path, sizeof(path), "graph.disciplineNext.%d.%d", col, slot);
			UIBaseObject * const linkObj = m_pageGraph4x4->GetObjectFromPath(path, TUIText);
			if (!linkObj)
				continue;
			UIText * const linkText = static_cast<UIText *>(linkObj);
			// Resolve link root (e.g. "social_imagedesigner") to a
			// canonical display name by finding the ProfessionDef whose
			// noviceSkill is "<root>_novice". If no match, fall back to
			// localizing the raw key.
			std::string const noviceCandidate = std::string(linkRoot) + "_novice";
			ProfessionDef const * const linkedDef = findProfessionDef(noviceCandidate);
			Unicode::String display;
			if (linkedDef && linkedDef->displayName && linkedDef->displayName[0])
				display = Unicode::narrowToWide(linkedDef->displayName);
			else if (!CuiSkillManager::localizeSkillName(linkRoot, display) || display.empty())
				display = Unicode::narrowToWide(linkRoot);

			Unicode::String label = Unicode::narrowToWide("To: ");
			label += display;
			linkText->SetLocalText(label);
			linkText->SetVisible(true);

			// Make the link a clickable shortcut to that profession's tree
			// (handled in OnButtonPressed via m_linkSkills).
			if (linkedDef)
			{
				m_linkSkills[linkText] = noviceCandidate;
				if (!isRegisteredMediatorObject(*linkText))
					registerMediatorObject(*linkText, true);
			}
		}
	}

	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			char const * const skillName = def->branchSkills[col][row];
			if (!skillName || !skillName[0])
				continue;

			char path[64];
			snprintf(path, sizeof(path), "graph.row%d.%d", row, col);
			applyTreeBox(path, skillName, playerSkills);
		}
	}

	// Master + Novice boxes (graph.master.b / graph.novice.b). Previously this
	// wrote the names into the graph.next.0 / graph.prev.0 "specialist" hint
	// texts, leaving the actual master/novice boxes showing the template's
	// "xxx skill_five_a" placeholder. Now we fill the real boxes
	// and register them clickable so their mods/commands populate the panels.
	{
		applyTreeBox("graph.master.b", masterName,             playerSkills);
		applyTreeBox("graph.novice.b", novice->getSkillName(), playerSkills);
	}

	// Back-links at the bottom (graph.prev.*): the basic profession(s) this
	// elite branches FROM -- e.g. Pistoleer shows "To: Marksman" below Novice.
	// Found by reverse-searching every profession's branchLinks for this
	// profession's root; clickable via m_linkSkills (handled in OnMessage),
	// mirroring the forward "To: X" elite links up top.
	{
		std::string const rootName = stripNoviceSuffix(novice->getSkillName());

		// Clear all four bottom slots first (profession switch may leave stale).
		for (int s = 0; s < 4; ++s)
		{
			char path[64];
			snprintf(path, sizeof(path), "graph.prev.%d", s);
			UIBaseObject * const obj = m_pageGraph4x4->GetObjectFromPath(path, TUIText);
			if (obj)
			{
				UIText * const text = static_cast<UIText *>(obj);
				text->SetLocalText(Unicode::emptyString);
				text->SetVisible(false);
			}
		}

		int prevSlot = 0;
		for (int i = 0; i < k_professionDefCount && prevSlot < 4; ++i)
		{
			ProfessionDef const & d = k_professionDefs[i];
			bool linksToUs = false;
			for (int col = 0; col < 4 && !linksToUs; ++col)
				for (int slot = 0; slot < 3; ++slot)
				{
					char const * const lr = d.branchLinks[col][slot];
					if (lr && lr[0] && rootName == lr)
					{
						linksToUs = true;
						break;
					}
				}
			if (!linksToUs)
				continue;

			char path[64];
			snprintf(path, sizeof(path), "graph.prev.%d", prevSlot);
			UIBaseObject * const obj = m_pageGraph4x4->GetObjectFromPath(path, TUIText);
			if (obj && d.displayName && d.noviceSkill)
			{
				UIText * const t = static_cast<UIText *>(obj);
				Unicode::String label = Unicode::narrowToWide("To: ");
				label += Unicode::narrowToWide(d.displayName);
				t->SetLocalText(label);
				t->SetVisible(true);
				m_linkSkills[t] = d.noviceSkill;
				if (!isRegisteredMediatorObject(*t))
					registerMediatorObject(*t, true);
			}
			++prevSlot;
		}
	}

	if (m_pageGraphs)
	{
		m_pageGraphs->SetVisible(true);
		REPORT_LOG(true, ("SwgCuiSkills::tryPopulateGraph4x4:   set m_pageGraphs visible (isVisible=%d)\n",
			m_pageGraphs->IsVisible() ? 1 : 0));
	}
	if (m_pageProfession)
		m_pageProfession->SetVisible(true);
	m_pageGraph4x4->SetVisible(true);
	REPORT_LOG(true, ("SwgCuiSkills::tryPopulateGraph4x4:   set m_pageGraph4x4 visible (isVisible=%d size=%dx%d)\n",
		m_pageGraph4x4->IsVisible() ? 1 : 0,
		static_cast<int>(m_pageGraph4x4->GetSize().x), static_cast<int>(m_pageGraph4x4->GetSize().y)));

	// Preserve the selected box during live XP/skill refresh. Fall back to the
	// novice only when switching profession or when the old box disappeared.
	bool selectionStillVisible = false;
	for (std::map<UIWidget *, std::string>::const_iterator it = m_buttonSkills.begin();
		it != m_buttonSkills.end(); ++it)
	{
		if (it->second == m_selectedSkill)
		{
			selectionStillVisible = true;
			break;
		}
	}
	if (!selectionStillVisible)
		m_selectedSkill = novice->getSkillName();
	populateSelectedSkill();

	return true;
}

//-----------------------------------------------------------------------

void SwgCuiSkills::populateSelectedSkill()
{
	updateSurrenderButton();

	// Clear all four per-skill detail data sources first.
	if (m_dsInfoModsName)   m_dsInfoModsName->Clear();
	if (m_dsInfoModsPoints) m_dsInfoModsPoints->Clear();
	if (m_dsInfoCmdsName)   m_dsInfoCmdsName->Clear();
	if (m_dsInfoCmdsIcons)  m_dsInfoCmdsIcons->Clear();

	// Header text above the bottom panels (e.g. "Strategy III: Volley Fire").
	if (m_textProfessionBody)
	{
		Unicode::String header;
		if (!m_selectedSkill.empty())
		{
			if (!CuiSkillManager::localizeSkillName(m_selectedSkill, header) || header.empty())
				header = Unicode::narrowToWide(m_selectedSkill);
		}
		m_textProfessionBody->SetLocalText(header);
	}

	CreatureObject const * const player = Game::getPlayerCreature();
	SkillObject const * const selectedSkill = m_selectedSkill.empty()
		? 0
		: SkillManager::getInstance().getSkill(m_selectedSkill);
	bool const ownsSelectedSkill = player && selectedSkill &&
		findOwnedSkill(*player, m_selectedSkill);
	int const selectedSkillCost = selectedSkill
		? std::max(0, selectedSkill->getSkillPointsRequired())
		: 0;

	// Drive the authentic learning-capacity strip. Green is the player's
	// bounded available capacity, orange previews an unowned selection's cost,
	// and cyan overlays the points recovered by surrendering an owned selection.
	int usedSkillPoints = 0;
	if (player)
	{
		CreatureObject::SkillList const & playerSkills = player->getSkills();
		for (CreatureObject::SkillList::const_iterator it = playerSkills.begin();
			it != playerSkills.end(); ++it)
		{
			if (*it)
				usedSkillPoints += std::max(0, (*it)->getSkillPointsRequired());
		}
	}
	usedSkillPoints = std::max(0, std::min(k_skillPointCap, usedSkillPoints));
	int const availableSkillPoints = calculateAvailableSkillPoints(usedSkillPoints);

	UIWidget * const learningBar = m_pageLearningCurrent
		? m_pageLearningCurrent->GetParentWidget()
		: (m_pageLearningCost ? m_pageLearningCost->GetParentWidget() : 0);
	UIScalar const learningBarWidth = learningBar ? std::max<UIScalar>(0, learningBar->GetWidth()) : 0;
	UIScalar const currentWidth = calculateProportionalWidth(
		learningBarWidth, availableSkillPoints, k_skillPointCap);
	UIScalar const selectedCostWidth = calculateProportionalWidth(
		learningBarWidth, selectedSkillCost, k_skillPointCap);
	setHorizontalBarRange(m_pageLearningCurrent, 0, currentWidth, learningBarWidth);
	if (m_pageLearningCurrent)
		m_pageLearningCurrent->SetVisible(player != 0);

	bool const showAcquisition = selectedSkill && !ownsSelectedSkill;
	setHorizontalBarRange(m_pageLearningCost, currentWidth, selectedCostWidth, learningBarWidth);
	if (m_pageLearningCost)
		m_pageLearningCost->SetVisible(showAcquisition && selectedSkillCost > 0);

	setHorizontalBarRange(m_pageLearningRecover, currentWidth - selectedCostWidth,
		selectedCostWidth, learningBarWidth);
	if (m_pageLearningRecover)
		m_pageLearningRecover->SetVisible(ownsSelectedSkill && selectedSkillCost > 0);

	Unicode::String acquireText;
	Unicode::String surrenderText;
	if (selectedSkill)
	{
		CuiStringVariablesData pointVariables;
		pointVariables.digit_i = selectedSkillCost;
		CuiStringVariablesManager::process(CuiStringIdsSkill::acquire_skill_points_prose,
			pointVariables, acquireText);
		CuiStringVariablesManager::process(CuiStringIdsSkill::surrender_prose,
			pointVariables, surrenderText);
		if (acquireText.empty())
			acquireText = Unicode::narrowToWide(FormattedString<128>().sprintf(
				"This skill costs %d skill points.", selectedSkillCost));
		if (surrenderText.empty())
			surrenderText = Unicode::narrowToWide(FormattedString<128>().sprintf(
				"Surrender this skill to recover %d skill points.", selectedSkillCost));
	}
	if (m_textSurrender)
	{
		m_textSurrender->SetLocalText(surrenderText);
		m_textSurrender->SetVisible(ownsSelectedSkill);
	}
	if (m_textAcquire)
	{
		m_textAcquire->SetLocalText(acquireText);
		m_textAcquire->SetVisible(showAcquisition);
	}

	// The selected-skill XP bar is the one horizontal bar supplied by the
	// retail right-panel CodeData. Owned skills and zero-XP boxes do not need
	// acquisition progress, so clear and hide both pieces together.
	SkillObject::ExperiencePair const * const experience = selectedSkill
		? selectedSkill->getPrerequisiteExperience()
		: 0;
	int const experienceRequired = experience ? std::max(0, experience->second.first) : 0;
	int experienceCurrent = 0;
	if (player && experience && !player->getExperience(experience->first, experienceCurrent))
		experienceCurrent = 0;
	bool const hasPrerequisites = player && selectedSkill &&
		hasAllPrerequisiteSkills(*player, *selectedSkill);
	bool const showExperience = showAcquisition && hasPrerequisites &&
		experience && experienceRequired > 0;

	Unicode::String experienceText;
	if (showExperience)
	{
		Unicode::String localizedExperience;
		if (!CuiSkillManager::localizeExpName(experience->first, localizedExperience) ||
			localizedExperience.empty())
		{
			localizedExperience = Unicode::narrowToWide(experience->first);
		}

		CuiStringVariablesData experienceVariables;
		experienceVariables.sourceName = localizedExperience;
		experienceVariables.digit_i = experienceRequired;
		CuiStringVariablesManager::process(CuiStringIdsSkill::acquire_exp_prose,
			experienceVariables, experienceText);

		experienceVariables.digit_i = std::max(0, experienceCurrent);
		Unicode::String currentExperienceText;
		CuiStringVariablesManager::process(CuiStringIdsSkill::exp_prose,
			experienceVariables, currentExperienceText);
		if (!currentExperienceText.empty())
		{
			experienceText.append(2, ' ');
			experienceText += currentExperienceText;
		}
		if (experienceText.empty())
		{
			experienceText = Unicode::narrowToWide(FormattedString<256>().sprintf(
				"This skill requires %d %s experience; you have %d.",
				experienceRequired, experience->first.c_str(), std::max(0, experienceCurrent)));
		}
	}
	if (m_textExpRequired)
	{
		m_textExpRequired->SetLocalText(experienceText);
		m_textExpRequired->SetVisible(showExperience);
	}
	if (m_barExp)
	{
		UIWidget * const experienceBarParent = m_barExp->GetParentWidget();
		UIScalar const experienceBarWidth = experienceBarParent
			? std::max<UIScalar>(0, experienceBarParent->GetWidth())
			: 0;
		setHorizontalBarRange(m_barExp, 0,
			calculateProportionalWidth(experienceBarWidth, experienceCurrent, experienceRequired),
			experienceBarWidth);
		m_barExp->SetVisible(showExperience);
	}

	if (!selectedSkill)
		return;

	// The restored runtime SkillObject is authoritative for every selected-box
	// grant. This keeps the UI aligned with the same skills table used for
	// acquisition and surrender instead of a second generated grant snapshot.
	if (m_dsInfoModsName && m_dsInfoModsPoints)
	{
		int rowIdx = 0;
		SkillObject::GenericModVector const & modifiers =
			selectedSkill->getStatisticModifiers();
		for (SkillObject::GenericModVector::const_iterator modifier = modifiers.begin();
			modifier != modifiers.end(); ++modifier)
		{
			std::string const & modifierName = modifier->first;
			if (isPrivateName(modifierName))
				continue;

			Unicode::String localizedName;
			if (!CuiSkillManager::localizeSkillModName(modifierName, localizedName) ||
				localizedName.empty())
			{
				localizedName = prettifyKey(modifierName);
			}

			appendTableRow(m_dsInfoModsName, m_dsInfoModsPoints, localizedName,
				Unicode::narrowToWide(FormattedString<32>().sprintf("%+d", modifier->second)), rowIdx);
			++rowIdx;
		}
	}

	// Commands, abilities, and draft schematics granted by this box.
	if (m_dsInfoCmdsName)
	{
		SkillObject::StringVector const & grantedSchematicGroups =
			selectedSkill->getSchematicsGranted();
		std::set<std::string> schematicGroupNames;
		for (SkillObject::StringVector::const_iterator group = grantedSchematicGroups.begin();
			group != grantedSchematicGroups.end(); ++group)
		{
			schematicGroupNames.insert(Unicode::toLower(*group));
		}

		// Two passes so CERTIFICATIONS group together as a visible block first
		// (Pre-CU surfaced certs prominently; they were previously interleaved +
		// scrolled off), abilities second. Both localize to SOE's real Pre-CU
		// names (e.g. cert_rifle_dlt20 -> "DLT20 Rifle Certification").
		SkillObject::StringVector const & commands = selectedSkill->getCommandsProvided();
		int rowIdx = 0;
		for (int pass = 0; pass < 2; ++pass)
		for (SkillObject::StringVector::const_iterator command = commands.begin();
			command != commands.end(); ++command)
		{
			std::string const & cmd = *command;
			std::string const cmdGrantKey = Unicode::toLower(cmd);
			std::string::size_type const argumentSeparator = cmdGrantKey.find('+');
			std::string const cmdKey = cmdGrantKey.substr(0, argumentSeparator);
			std::string const cmdArgument = argumentSeparator == std::string::npos
				? std::string()
				: cmdGrantKey.substr(argumentSeparator + 1);
			if (isPrivateName(cmdKey))
				continue;
			// Some historical tables duplicated draft group ids in COMMANDS. They
			// are groups, not executable commands; render their member drafts below.
			if (schematicGroupNames.find(cmdGrantKey) != schematicGroupNames.end())
				continue;

			Command const & commandDefinition = CommandTable::getCommand(
				Crc::normalizeAndCalculate(cmdKey.c_str()));
			if (!commandDefinition.isNull() && !commandDefinition.m_visibleToClients)
				continue;
			bool const isCert = (cmdKey.compare(0, 5, "cert_") == 0);
			if ((pass == 0) != isCert)   // pass 0: certs only; pass 1: the rest
				continue;

			Unicode::String localizedName;
			// cmd_n STF keys are all lowercase (SOE convention); our command strings
			// are camelCase (e.g. overChargeShot1) -> lowercase for the lookup (mirrors
			// CuiSkillManager.cpp:450) so we show SOE's real Pre-CU command names instead
			// of the prettifyKey fallback. Keep camelCase cmd for the fallback path.
			bool const localizedGrant =
				CuiSkillManager::localizeCmdName(cmdGrantKey, localizedName) &&
				!localizedName.empty();
			if (!localizedGrant)
			{
				if (!CuiSkillManager::localizeCmdName(cmdKey, localizedName) ||
					localizedName.empty())
				{
					// Unknown, unlocalized grants are internal bookkeeping rather than
					// player commands. Known visible commands retain a readable fallback.
					if (commandDefinition.isNull())
						continue;
					localizedName = prettifyKey(cmdKey);
				}

				// Publish 14 encodes command variants as command+argument. The base
				// command owns visibility and icon policy; retain the argument in the
				// label when no variant-specific cmd_n entry exists.
				if (!cmdArgument.empty())
				{
					localizedName += Unicode::narrowToWide(" (");
					localizedName += Unicode::narrowToWide(cmdArgument);
					localizedName.push_back(')');
				}
			}

			// /styles.icon.command.<lowercasecmd> (blank until Pre-CU icon styles ship).
			appendGrantedDetailRow(m_dsInfoCmdsName, m_dsInfoCmdsIcons, localizedName,
				std::string("/styles.icon.command.") + cmdKey, rowIdx);
			++rowIdx;
		}

		DraftSchematicGroupManager::SchematicVector drafts;
		std::set<std::pair<uint32, uint32> > displayedDrafts;
		for (SkillObject::StringVector::const_iterator group = grantedSchematicGroups.begin();
			group != grantedSchematicGroups.end(); ++group)
		{
			std::string const groupName = Unicode::toLower(*group);
			drafts.clear();
			if (!DraftSchematicGroupManager::getSchematicsForGroup(groupName, drafts))
			{
				WARNING(true, ("SwgCuiSkills selected skill [%s] calls for invalid schematic group [%s]",
					m_selectedSkill.c_str(), groupName.c_str()));
				continue;
			}

			for (DraftSchematicGroupManager::SchematicVector::const_iterator draft = drafts.begin();
				draft != drafts.end(); ++draft)
			{
				if (!displayedDrafts.insert(*draft).second)
					continue;

				DraftSchematicInfo const * const info =
					DraftSchematicManager::cacheDraftSchematic(*draft);
				if (!info || info->getLocalizedName().empty())
				{
					WARNING(true, ("SwgCuiSkills unable to localize draft [%lu,%lu] from group [%s] on skill [%s]",
						static_cast<unsigned long>(draft->first),
						static_cast<unsigned long>(draft->second), groupName.c_str(),
						m_selectedSkill.c_str()));
					continue;
				}

				Unicode::String localizedDraftName;
				localizedDraftName.push_back('+');
				localizedDraftName += info->getLocalizedName();
				appendGrantedDetailRow(m_dsInfoCmdsName, m_dsInfoCmdsIcons,
					localizedDraftName, "/styles.icon.misc.granted", rowIdx);
				++rowIdx;
			}
		}
	}
}
