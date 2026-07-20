// ======================================================================
//
// SwgCuiSkills.h
// Pre-CU skill window mediator. Drives the legacy /Skill.skills page
// from ui_skill.inc (My Skills / All Skills tabs, profession TreeView,
// per-profession 4x4 skill tree pyramid, XP + skill mod tables).
//
// ======================================================================

#ifndef SwgCuiSkills_H
#define SwgCuiSkills_H

#include "UIEventCallback.h"
#include "clientGame/ClientCommandQueue.h"
#include "clientUserInterface/CuiMediator.h"
#include "sharedFoundation/NetworkId.h"

#include <map>
#include <set>
#include <string>

class UIButton;
class UIDataSource;
class UIDataSourceBase;
class UIDataSourceContainer;
class UIPage;
class UITabbedPane;
class UIText;
class UITreeView;
class SkillObject;
class CreatureObject;
class PlayerObject;

namespace MessageDispatch
{
	class Callback;
}

class SwgCuiSkills :
public CuiMediator,
public UIEventCallback
{
public:
	explicit                   SwgCuiSkills          (UIPage & page);

	virtual bool               OnMessage             (UIWidget * context, const UIMessage & msg);
	virtual void               OnButtonPressed       (UIWidget * context);
	virtual void               OnGenericSelectionChanged (UIWidget * context);
	virtual void               OnTabbedPaneChanged   (UIWidget * context);
	virtual void               update                (float deltaTimeSecs);

	void                       onSkillsChanged       (CreatureObject const & creature);
	void                       onExperienceChanged   (PlayerObject const & player);
	void                       onSkillModsChanged    (CreatureObject const & creature);
	void                       onCommandsChanged     (CreatureObject const & creature);
	void                       onDeleteSkillConfirmation (std::string const & skillName);
	void                       onSceneChanged        (bool const &);
	int                        showAllProfessionsForBackgroundValidation ();
	int                        selectAllProfessionForBackgroundValidation (int row);

protected:
	virtual void               performActivate       ();
	virtual void               performDeactivate     ();

private:
	                          ~SwgCuiSkills          ();
	                           SwgCuiSkills          (const SwgCuiSkills &);
	SwgCuiSkills &             operator=             (const SwgCuiSkills &);

	void                       populateProfessionList ();
	void                       synchronizeProfessionTreeSelection ();
	void                       populateExperience    ();
	void                       populateSkillMods     ();
	void                       populateCertifications ();
	void                       populateSelectedProfession ();
	void                       populateSelectedSkill  ();
	void                       updateSkillPointsDisplay ();
	void                       updateSurrenderButton ();
	void                       clearConfirmationSnapshot ();
	void                       clearPendingSurrender ();
	void                       reconcilePendingSurrender ();
	void                       onCommandRemoving     (ClientCommandQueue::Messages::Removing::Payload const & payload);
	bool                       tryPopulateGraph4x4    (SkillObject const * novice, std::set<std::string> const & playerSkills);
	void                       applyTreeBox           (char const * path, std::string const & skillName, std::set<std::string> const & playerSkills);
	void                       hideAllGraphs          ();
	void                       resetGraph4x4Presentation ();

	// Top-level page bindings (resolved via CodeData on the /Skill.skills page).
	UITabbedPane *             m_tabs;
	UIPage *                   m_pageProfessionList;
	UIPage *                   m_pageMyStats;
	UIPage *                   m_pageProfession;
	UIButton *                 m_buttonClose;
	UIText *                   m_textProfessionListMy;
	UIText *                   m_textProfessionListAll;

	// Right-panel text + graph layout containers.
	UIText *                   m_textProfName;
	UIText *                   m_textProfessionBody;
	UIPage *                   m_pageGraphs;
	UIPage *                   m_pageGraph4x4;
	UIPage *                   m_pageGraph2x4;
	UIPage *                   m_pageGraph1x4;
	UIPage *                   m_pageGraphPyramid;

	// Profession TreeView's data source (children are UIDataSourceContainer
	// entries, one per profession, with Name + Text properties).
	UIDataSourceContainer *    m_dsProfTree;
	UITreeView *               m_treeProf;

	// Left-side per-player TableExp / TableMods DataSources.
	UIDataSource *             m_dsExpName;
	UIDataSource *             m_dsExpPoints;
	UIDataSource *             m_dsModsName;
	UIDataSource *             m_dsModsPoints;
	UIDataSource *             m_dsCertsName;

	// Right-side per-selected-skill detail panels (under both.right.all.info).
	UIDataSource *             m_dsInfoModsName;
	UIDataSource *             m_dsInfoModsPoints;
	UIDataSource *             m_dsInfoCmdsName;
	UIDataSource *             m_dsInfoCmdsIcons;

	// Authentic Publish 14 selected-skill presentation contract from the nested
	// both.right CodeData block in ui_skill.inc.
	UIText *                   m_textSkillPoints;
	UIText *                   m_textAcquire;
	UIText *                   m_textSurrender;
	UIText *                   m_textExpRequired;
	UIPage *                   m_pageLearningCurrent;
	UIPage *                   m_pageLearningCost;
	UIPage *                   m_pageLearningRecover;
	UIPage *                   m_barExp;
	UIButton *                 m_buttonSurrender;

	// Map from a tree-cell button widget to its currently-displayed skill
	// name, rebuilt per tryPopulateGraph4x4 call. Used by OnButtonPressed
	// to identify which tree cell the user clicked.
	std::map<UIWidget *, std::string> m_buttonSkills;

	// Map from a "To: <profession>" link text widget to the novice skill of the
	// profession it links to; clicking navigates the tree there (shortcut).
	std::map<UIWidget *, std::string> m_linkSkills;

	std::string                m_selectedProfession;
	std::string                m_selectedSkill;

	// Confirmation and command snapshots keep the request tied to the skill
	// that was validated, even if the player changes tree selection while the
	// dialog or server command is in flight. Player ids prevent a snapshot from
	// crossing a scene/character transition.
	std::string                m_confirmationSkill;
	NetworkId                  m_confirmationPlayerId;
	std::string                m_pendingSurrenderSkill;
	NetworkId                  m_pendingSurrenderPlayerId;
	uint32                     m_surrenderSequenceId;
	MessageDispatch::Callback * m_callback;
};

#endif // SwgCuiSkills_H
