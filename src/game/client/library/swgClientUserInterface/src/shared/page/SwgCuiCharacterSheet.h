// ======================================================================
//
// SwgCuiCharacterSheet.h
// copyright (c) 2001 Sony Online Entertainment
//
// Publish 14 Character Sheet compatibility mediator.
//
// ======================================================================

#ifndef SwgCuiCharacterSheet_H
#define SwgCuiCharacterSheet_H

//-----------------------------------------------------------------

#include "UIEventCallback.h"
#include "clientGame/PlayerCreatureController.h"
#include "clientUserInterface/CuiMediator.h"
#include "sharedMessageDispatch/Receiver.h"

#include <map>

//-----------------------------------------------------------------

class CreatureObject;
class PlayerObject;
class UIButton;
class UIImage;
class UITable;
class UITableModelDefault;
class UITabbedPane;
class UIText;
class UIPage;

//-----------------------------------------------------------------

namespace MessageDispatch
{
	class Callback;
}

template <typename T> class Watcher;

//-----------------------------------------------------------------

class SwgCuiCharacterSheet :
public CuiMediator,
public UIEventCallback,
public MessageDispatch::Receiver
{
public:

	struct Messages
	{
		struct ShowStatMigration
		{
			typedef NetworkId Payload;
		};
	};

	static SwgCuiCharacterSheet * createInto(UIPage & parent);

public:
	explicit             SwgCuiCharacterSheet(UIPage & page);
	void                 OnButtonPressed(UIWidget * context);
	void                 receiveMessage(const MessageDispatch::Emitter & source, const MessageDispatch::MessageBase & message);
	void                 setExamineMode(CreatureObject * playerToExamine);
	bool                 isExaminingSelf() const;
	CreatureObject *     getCreatureToExamine() const;

protected:
	void                 update(float deltaTimeSecs);
	void                 performActivate();
	void                 performDeactivate();

private:
	                    ~SwgCuiCharacterSheet();
	                     SwgCuiCharacterSheet(const SwgCuiCharacterSheet &);
	SwgCuiCharacterSheet & operator=(const SwgCuiCharacterSheet &);

	void                 updateGuild(const std::string & guildName, const std::string & memberTitle);
	void                 requestServerInfo() const;
	void                 updateFactionTable();
	void                 updateIdentity();
	void                 updatePvpStatus();
	void                 updateHamTable();
	void                 updateStatusBars();
	void                 refreshBadgeWindow();
	void                 clearPrivateFields();
	void                 onBiographyRetrieved(PlayerCreatureController::Messages::BiographyRetrieved::BiographyOwner const & biographyOwner);

private:
	UITabbedPane *        m_tabbedPane;
	long                  m_lastActiveTab;
	float                 m_updateTimer;

	UIText *              m_characterName;
	UIText *              m_rank;
	UIText *              m_pvpStatus;
	UITable *             m_attributeTable;
	UITableModelDefault * m_attributeModel;
	UIText *              m_shockWounds;

	UIImage *             m_foodBar;
	UIImage *             m_foodBarBack;
	UIImage *             m_drinkBar;
	UIImage *             m_drinkBarBack;
	UIImage *             m_forcePowerBar;
	UIText *              m_forcePowerText;

	UIText *              m_born;
	UIText *              m_species;
	UIText *              m_played;
	UIText *              m_home;
	UIText *              m_married;
	UIText *              m_bindLocation;
	UIText *              m_bankLocation;
	UIText *              m_lotsAvailable;
	UIText *              m_guild;
	UIText *              m_guildAbbreviation;
	UIText *              m_guildTitle;
	UIText *              m_badgeWindow;
	UIText *              m_bio;

	UIText *              m_rebelFaction;
	UIText *              m_imperialFaction;
	UITable *             m_tableFactions;
	std::map<std::string, int> m_factions;

	UIButton *            m_statMigrationButton;
	MessageDispatch::Callback * m_callBack;

	typedef Watcher<PlayerObject> PlayerObjectWatcher;
	PlayerObjectWatcher * m_playerObjectWatcher;

	typedef Watcher<CreatureObject> CreatureObjectWatcher;
	CreatureObjectWatcher * m_creatureObjectWatcher;
};

//-----------------------------------------------------------------

#endif
